#include "display.h"

DisplayManager gDisplay;

bool DisplayManager::begin() {
    pinMode(PIN_LCD_BL, OUTPUT);
    setBacklight(true);

    pinMode(PIN_TOUCH_IRQ, INPUT_PULLUP);
    pinMode(PIN_TOUCH_CS, OUTPUT);
    digitalWrite(PIN_TOUCH_CS, HIGH);

    // Shared bus: SCLK=12, MOSI=13, MISO=14 (touch T_DO)
    SPI.begin(PIN_LCD_SCLK, PIN_TOUCH_DO, PIN_LCD_MOSI, -1);

    _tft.init();
    _tft.setRotation(0);
    _tft.fillScreen(COLOR_BG);
    _ready = true;
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

// XPT2046 raw read (12-bit); shares SPI with TFT_eSPI
static uint16_t xpt2046Transfer(uint8_t command) {
    digitalWrite(PIN_TOUCH_CS, LOW);
    SPI.beginTransaction(SPISettings(SPI_TOUCH_FREQUENCY, MSBFIRST, SPI_MODE0));
    uint16_t value = SPI.transfer(command) << 8;
    value |= SPI.transfer(0x00);
    SPI.endTransaction();
    digitalWrite(PIN_TOUCH_CS, HIGH);
    return value >> 3;
}

bool DisplayManager::getTouchPoint(int16_t &x, int16_t &y) {
    if (digitalRead(PIN_TOUCH_IRQ) == HIGH) return false;

    const uint16_t rawX = xpt2046Transfer(0xD0);
    const uint16_t rawY = xpt2046Transfer(0x90);

    x = map(rawX, 200, 3800, 0, SCREEN_WIDTH);
    y = map(rawY, 200, 3800, 0, SCREEN_HEIGHT);
    x = constrain(x, 0, SCREEN_WIDTH - 1);
    y = constrain(y, 0, SCREEN_HEIGHT - 1);
    return true;
}
