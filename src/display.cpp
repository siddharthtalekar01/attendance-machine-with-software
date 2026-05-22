#include "display.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SPI.h>

DisplayManager gDisplay;

// --- Touch calibration & state ---
static TouchCalibration s_cal;
static bool s_fingerDown = false;
static bool s_pressLatched = false;

static TouchPoint s_queue[TOUCH_QUEUE_DEPTH];
static uint8_t s_queueHead = 0;
static uint8_t s_queueTail = 0;
static uint8_t s_queueCount = 0;

/** Raw XPT2046 ADC via TFT_eSPI (before screen mapping). */
static bool readRawTouch(uint16_t &rawX, uint16_t &rawY) {
    return gDisplay.tft().validTouch(&rawX, &rawY, TOUCH_RAW_THRESHOLD) != 0;
}

static TouchPoint mapRawToScreen(uint16_t rawX, uint16_t rawY, bool pressed) {
    TouchPoint tp;
    tp.pressed = pressed;

    if (!pressed) return tp;

    if (s_cal.valid) {
        tp.x = map(static_cast<int>(rawX), s_cal.rawXMin, s_cal.rawXMax, 0, SCREEN_WIDTH - 1);
        tp.y = map(static_cast<int>(rawY), s_cal.rawYMin, s_cal.rawYMax, 0, SCREEN_HEIGHT - 1);
    } else {
        tp.x = map(static_cast<int>(rawX), 200, 3800, 0, SCREEN_WIDTH - 1);
        tp.y = map(static_cast<int>(rawY), 200, 3800, 0, SCREEN_HEIGHT - 1);
    }

    if (tp.x < 0 || tp.x >= SCREEN_WIDTH || tp.y < 0 || tp.y >= SCREEN_HEIGHT) {
        tp.pressed = false;
        tp.x = 0;
        tp.y = 0;
    }
    return tp;
}

static void queuePush(const TouchPoint &tp) {
    if (!tp.pressed || s_queueCount >= TOUCH_QUEUE_DEPTH) return;
    s_queue[s_queueHead] = tp;
    s_queueHead = (s_queueHead + 1) % TOUCH_QUEUE_DEPTH;
    s_queueCount++;
}

static TouchPoint sampleTouchDebounced() {
    uint16_t rawX = 0;
    uint16_t rawY = 0;
    const bool touching = readRawTouch(rawX, rawY);

    if (!touching) {
        s_fingerDown = false;
        s_pressLatched = false;
        return {};
    }

    if (!s_fingerDown) {
        s_fingerDown = true;
        if (!s_pressLatched) {
            s_pressLatched = true;
            return mapRawToScreen(rawX, rawY, true);
        }
    }

    return mapRawToScreen(rawX, rawY, false);
}

TouchPoint getTouchPoint() {
    return sampleTouchDebounced();
}

bool touchReadHeld(TouchPoint &tp) {
    uint16_t rawX = 0;
    uint16_t rawY = 0;
    if (!readRawTouch(rawX, rawY)) {
        tp = {};
        return false;
    }
    tp = mapRawToScreen(rawX, rawY, true);
    return tp.pressed;
}

bool isTouchInRect(TouchPoint tp, int x, int y, int w, int h) {
    if (!tp.pressed) return false;
    return tp.x >= x && tp.x < x + w && tp.y >= y && tp.y < y + h;
}

void touchUpdate() {
    const TouchPoint tp = sampleTouchDebounced();
    if (tp.pressed) {
        queuePush(tp);
    }
}

bool touchEventAvailable() {
    return s_queueCount > 0;
}

bool touchEventPop(TouchPoint &out) {
    if (s_queueCount == 0) return false;
    out = s_queue[s_queueTail];
    s_queueTail = (s_queueTail + 1) % TOUCH_QUEUE_DEPTH;
    s_queueCount--;
    return true;
}

void touchQueueClear() {
    s_queueHead = 0;
    s_queueTail = 0;
    s_queueCount = 0;
}

static bool ensureCalDir() {
    if (!LittleFS.exists(TOUCH_CAL_DIR)) {
        return LittleFS.mkdir(TOUCH_CAL_DIR);
    }
    return true;
}

bool touchLoadCalibration() {
    s_cal = TouchCalibration{};

    if (!LittleFS.exists(TOUCH_CAL_FILE)) {
        Serial.println("[Touch] No calibration file — using defaults");
        return false;
    }

    File f = LittleFS.open(TOUCH_CAL_FILE, FILE_READ);
    if (!f) {
        Serial.println("[Touch] Failed to open calibration file");
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close();
        Serial.println("[Touch] Invalid calibration JSON");
        return false;
    }
    f.close();

    s_cal.rawXMin = doc["rawXMin"] | 200;
    s_cal.rawXMax = doc["rawXMax"] | 3800;
    s_cal.rawYMin = doc["rawYMin"] | 200;
    s_cal.rawYMax = doc["rawYMax"] | 3800;
    s_cal.valid = doc["valid"] | true;

    Serial.printf("[Touch] Loaded cal X[%u..%u] Y[%u..%u]\n",
                  s_cal.rawXMin, s_cal.rawXMax, s_cal.rawYMin, s_cal.rawYMax);
    return true;
}

static bool saveTouchCalibration() {
    if (!ensureCalDir()) {
        Serial.println("[Touch] mkdir /config failed");
        return false;
    }

    JsonDocument doc;
    doc["rawXMin"] = s_cal.rawXMin;
    doc["rawXMax"] = s_cal.rawXMax;
    doc["rawYMin"] = s_cal.rawYMin;
    doc["rawYMax"] = s_cal.rawYMax;
    doc["valid"] = true;
    doc["screenW"] = SCREEN_WIDTH;
    doc["screenH"] = SCREEN_HEIGHT;

    File f = LittleFS.open(TOUCH_CAL_FILE, FILE_WRITE);
    if (!f) return false;
    const bool ok = serializeJson(doc, f) > 0;
    f.close();
    return ok;
}

static void drawCrosshair(int16_t sx, int16_t sy) {
    TFT_eSPI &tft = gDisplay.tft();
    const int arm = 12;
    tft.drawLine(sx - arm, sy, sx + arm, sy, TFT_WHITE);
    tft.drawLine(sx, sy - arm, sx, sy + arm, TFT_WHITE);
    tft.fillCircle(sx, sy, 3, TFT_RED);
}

static bool waitForCalibrationTap(uint16_t &rawX, uint16_t &rawY, const char *label) {
    gDisplay.drawCenteredText(SCREEN_HEIGHT / 2 + 40, label, 2, COLOR_TEXT_DIM);

    while (!readRawTouch(rawX, rawY)) {
        delay(10);
    }
    delay(80);
    readRawTouch(rawX, rawY);

    const uint32_t releaseStart = millis();
    while (readRawTouch(rawX, rawY) && (millis() - releaseStart) < 5000) {
        delay(10);
    }

    s_fingerDown = false;
    s_pressLatched = false;
    return true;
}

void calibrateTouch() {
    /*
     * Calibration wizard — tap each crosshair once.
     * Maps raw ADC min/max to 240x320 using corner samples:
     *   TL sets rawXMin & rawYMin, TR sets rawXMax & rawYMin, BR sets rawYMax.
     */
    struct Target {
        int16_t sx;
        int16_t sy;
        const char *label;
    };

    const Target targets[] = {
        {25, 25, "Tap TOP-LEFT"},
        {SCREEN_WIDTH - 25, 25, "Tap TOP-RIGHT"},
        {SCREEN_WIDTH - 25, SCREEN_HEIGHT - 25, "Tap BOTTOM-RIGHT"},
    };

    uint16_t rawSamples[3][2] = {};

    gDisplay.fillScreen(TFT_BLACK);
    gDisplay.drawHeader("Touch Calibrate");
    gDisplay.drawCenteredText(70, "Tap 3 corner targets", 2, COLOR_TEXT);

    for (int i = 0; i < 3; i++) {
        gDisplay.fillRect(0, 90, SCREEN_WIDTH, SCREEN_HEIGHT - 90, TFT_BLACK);
        drawCrosshair(targets[i].sx, targets[i].sy);
        if (!waitForCalibrationTap(rawSamples[i][0], rawSamples[i][1], targets[i].label)) {
            return;
        }
        Serial.printf("[Touch] Cal point %d raw (%u, %u)\n", i + 1, rawSamples[i][0], rawSamples[i][1]);
    }

    s_cal.rawXMin = rawSamples[0][0];
    s_cal.rawXMax = rawSamples[1][0];
    s_cal.rawYMin = (rawSamples[0][1] + rawSamples[1][1]) / 2;
    s_cal.rawYMax = rawSamples[2][1];

    if (s_cal.rawXMin > s_cal.rawXMax) {
        const uint16_t t = s_cal.rawXMin;
        s_cal.rawXMin = s_cal.rawXMax;
        s_cal.rawXMax = t;
    }
    if (s_cal.rawYMin > s_cal.rawYMax) {
        const uint16_t t = s_cal.rawYMin;
        s_cal.rawYMin = s_cal.rawYMax;
        s_cal.rawYMax = t;
    }

    s_cal.valid = true;

    if (saveTouchCalibration()) {
        gDisplay.showMessage("Calibration", "Saved OK\nReboot optional", COLOR_SUCCESS);
        Serial.println("[Touch] Calibration saved to " TOUCH_CAL_FILE);
    } else {
        gDisplay.showMessage("Calibration", "Save failed", COLOR_ERROR);
        Serial.println("[Touch] Calibration save failed");
    }
    delay(2000);
}

// --- DisplayManager ---

bool DisplayManager::begin() {
    pinMode(PIN_LCD_BL, OUTPUT);
    setBacklight(true);

    pinMode(PIN_TOUCH_IRQ, INPUT_PULLUP);

    SPI.begin(PIN_LCD_SCLK, PIN_TOUCH_DO, PIN_LCD_MOSI, -1);

    _tft.init();
    _tft.setRotation(0);
    _tft.fillScreen(COLOR_BG);
    _ready = true;

    s_queueHead = s_queueTail = s_queueCount = 0;
    s_fingerDown = false;
    s_pressLatched = false;

    Serial.println("[Touch] XPT2046 via TFT_eSPI built-in driver");
    return true;
}

void DisplayManager::displayTest() {
    if (!_ready) {
        pinMode(PIN_LCD_BL, OUTPUT);
        setBacklight(true);
        SPI.begin(PIN_LCD_SCLK, PIN_TOUCH_DO, PIN_LCD_MOSI, -1);
        _tft.init();
        _tft.setRotation(0);
        _ready = true;
    }

    struct {
        uint16_t color;
        const char *name;
    } steps[] = {
        {TFT_RED, "RED"},
        {TFT_GREEN, "GREEN"},
        {TFT_BLUE, "BLUE"},
    };

    Serial.println("[displayTest] RGB sequence — check colors on LCD");
    for (const auto &step : steps) {
        Serial.printf("[displayTest] %s\n", step.name);
        _tft.fillScreen(step.color);
        _tft.setTextColor(TFT_WHITE, step.color);
        _tft.setTextDatum(MC_DATUM);
        _tft.drawString(step.name, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 4);
        _tft.setTextDatum(TL_DATUM);
        delay(1500);
    }
    Serial.println("[displayTest] Done — set RUN_DISPLAY_TEST to 0 in config.h to run app");
}

TFT_eSPI &DisplayManager::tft() { return _tft; }

void DisplayManager::setBacklight(bool on) {
    digitalWrite(PIN_LCD_BL, on ? HIGH : LOW);
}

void DisplayManager::fillScreen(uint16_t color) {
    if (_ready) _tft.fillScreen(color);
}

void DisplayManager::drawCenteredText(int16_t y, const char *text, uint8_t font, uint16_t color) {
    if (!_ready || !text) return;
    _tft.setTextFont(font);
    _tft.setTextColor(color, COLOR_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(text, SCREEN_WIDTH / 2, y);
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::drawHeader(const char *title) {
    if (!_ready) return;
    _tft.fillRect(0, 0, SCREEN_WIDTH, 36, COLOR_PRIMARY);
    _tft.setTextFont(2);
    _tft.setTextColor(COLOR_TEXT, COLOR_PRIMARY);
    _tft.setTextDatum(ML_DATUM);
    _tft.drawString(title ? title : APP_NAME, 8, 18);
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::drawStatusBar(const char *left, const char *right) {
    if (!_ready) return;
    const int y = SCREEN_HEIGHT - 22;
    _tft.fillRect(0, y, SCREEN_WIDTH, 22, 0x0000);
    _tft.setTextFont(1);
    _tft.setTextColor(COLOR_TEXT_DIM, 0x0000);
    if (left) _tft.drawString(left, 4, y + 4);
    if (right) {
        _tft.setTextDatum(MR_DATUM);
        _tft.drawString(right, SCREEN_WIDTH - 4, y + 11);
        _tft.setTextDatum(TL_DATUM);
    }
}

void DisplayManager::drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char *label, uint16_t fill) {
    if (!_ready) return;
    _tft.fillRoundRect(x, y, w, h, 6, fill);
    _tft.setTextFont(2);
    _tft.setTextColor(COLOR_TEXT, fill);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(label ? label : "", x + w / 2, y + h / 2);
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::showMessage(const char *title, const char *body, uint16_t accent) {
    if (!_ready) return;
    fillScreen(COLOR_BG);
    drawHeader(title ? title : "Notice");
    _tft.setTextFont(2);
    _tft.setTextColor(COLOR_TEXT, COLOR_BG);
    _tft.setTextDatum(TC_DATUM);
    _tft.drawString(body ? body : "", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    _tft.setTextDatum(TL_DATUM);
    _tft.drawRoundRect(10, 50, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 80, 8, accent);
}
