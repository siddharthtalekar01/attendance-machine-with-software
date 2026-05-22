#include "admin_auth.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <cstring>

#include "app_state.h"
#include "config.h"
#include "storage.h"

namespace {

constexpr const char *ADMIN_PIN_LEGACY = "/config/admin_pin.json";
constexpr uint32_t RTC_MAGIC = 0xAD111001;

constexpr int KEY_W = 60;
constexpr int KEY_H = 50;
constexpr int KEY_GAP = 10;
constexpr int KEY_START_X = 15;
constexpr int KEY_START_Y = 145;
constexpr int KEY_COLS = 3;
constexpr int KEY_ROWS = 4;

constexpr uint32_t KEY_FLASH_MS = 120;
constexpr uint32_t SHAKE_MS = 420;

RTC_DATA_ATTR static uint8_t s_rtcFailCount = 0;
RTC_DATA_ATTR static uint32_t s_rtcMagic = 0;

struct PinHash {
    uint8_t xorKey[8] = {};
    uint8_t checksum = 0;
    uint8_t length = 0;
};

struct {
    PinHash stored{};
    char entry[ADMIN_PIN_MAX_LEN + 1] = {};
    int entryLen = 0;
    uint32_t lockoutUntilMs = 0;
    uint32_t sessionUntilMs = 0;
    AppState pendingTarget = STATE_HOME;
    bool pendingSet = false;

    int shakePhase = 0;
    uint32_t shakeStartMs = 0;
    bool shaking = false;

    int flashKey = -1;
    uint32_t flashUntilMs = 0;

    bool pinErrorVisible = false;
    uint32_t lastCountdownDrawMs = 0;
} s;

const char *keyLabel(int index) {
    static const char *labels[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "<", "0", "OK"};
    if (index < 0 || index >= 12) return "";
    return labels[index];
}

int keyIndexAt(int16_t x, int16_t y) {
    for (int i = 0; i < 12; i++) {
        const int col = i % KEY_COLS;
        const int row = i / KEY_COLS;
        const int kx = KEY_START_X + col * (KEY_W + KEY_GAP);
        const int ky = KEY_START_Y + row * (KEY_H + KEY_GAP);
        if (x >= kx && x < kx + KEY_W && y >= ky && y < ky + KEY_H) {
            return i;
        }
    }
    return -1;
}

void pinHashCompute(const char *pin, PinHash &out) {
    const size_t len = strlen(pin);
    out.length = (uint8_t)min(len, (size_t)ADMIN_PIN_MAX_LEN);
    memset(out.xorKey, 0, sizeof(out.xorKey));
    uint8_t cs = (uint8_t)(out.length ^ 0xA5);
    for (size_t i = 0; i < len; i++) {
        out.xorKey[i % 8] ^= (uint8_t)(pin[i] ^ (uint8_t)(i + 1) * 31);
        cs ^= (uint8_t)(pin[i] + out.xorKey[i % 8]);
    }
    out.checksum = cs;
}

bool pinHashMatches(const PinHash &a, const PinHash &b) {
    return a.length == b.length && a.checksum == b.checksum &&
           memcmp(a.xorKey, b.xorKey, sizeof(a.xorKey)) == 0;
}

bool loadStoredHash() {
    s.stored = {};

    if (storageLoadAdminPin(s.stored.xorKey, sizeof(s.stored.xorKey), s.stored.checksum,
                          s.stored.length)) {
        return s.stored.length >= ADMIN_PIN_MIN_LEN;
    }

    if (!LittleFS.exists(ADMIN_PIN_LEGACY)) return false;

    File f = LittleFS.open(ADMIN_PIN_LEGACY, FILE_READ);
    if (!f) return false;

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close();
        return false;
    }
    f.close();

    JsonArray arr = doc["xor"].as<JsonArray>();
    for (size_t i = 0; i < 8 && i < arr.size(); i++) {
        s.stored.xorKey[i] = arr[i] | 0;
    }
    s.stored.checksum = doc["cs"] | 0;
    s.stored.length = doc["len"] | 0;

    if (s.stored.length >= ADMIN_PIN_MIN_LEN) {
        storageSaveAdminPin(s.stored.xorKey, sizeof(s.stored.xorKey), s.stored.checksum,
                          s.stored.length);
        LittleFS.remove(ADMIN_PIN_LEGACY);
    }
    return s.stored.length >= ADMIN_PIN_MIN_LEN;
}

bool saveStoredHash(const PinHash &hash) {
    return storageSaveAdminPin(hash.xorKey, sizeof(hash.xorKey), hash.checksum, hash.length);
}

bool pinValidFormat(const char *pin) {
    const size_t len = strlen(pin);
    if (len < ADMIN_PIN_MIN_LEN || len > ADMIN_PIN_MAX_LEN) return false;
    for (size_t i = 0; i < len; i++) {
        if (pin[i] < '0' || pin[i] > '9') return false;
    }
    return true;
}

void clearEntry() {
    s.entryLen = 0;
    s.entry[0] = '\0';
}

int shakeOffsetPx() {
    if (!s.shaking) return 0;
    static const int8_t pattern[] = {-12, 12, -10, 10, -6, 6, -3, 3, 0};
    const size_t n = sizeof(pattern) / sizeof(pattern[0]);
    const size_t idx = (size_t)s.shakePhase % n;
    return pattern[idx];
}

void drawLockIcon(TFT_eSPI &tft, int16_t cx, int16_t cy) {
    const uint16_t col = TEXT_PRIMARY;
    tft.drawRoundRect(cx - 14, cy - 10, 28, 22, 4, col);
    tft.drawArc(cx, cy - 4, 14, 10, 180, 360, col, COLOR_BG_DARK, false);
    tft.fillRect(cx - 3, cy + 4, 6, 8, col);
}

void drawPinDots(TFT_eSPI &tft, int16_t centerX, int16_t y) {
    const int dotR = 8;
    const int spacing = 28;
    const int totalW = (ADMIN_PIN_DOT_COUNT - 1) * spacing;
    const int x0 = centerX - totalW / 2 + shakeOffsetPx();
    const int filled = s.entryLen;

    tft.fillRect(0, y - 14, SCREEN_WIDTH, 28, COLOR_BG_DARK);

    for (int i = 0; i < ADMIN_PIN_DOT_COUNT; i++) {
        const int dx = x0 + i * spacing;
        if (i < filled) {
            tft.fillCircle(dx, y, dotR, ACCENT_BLUE);
        } else {
            tft.drawCircle(dx, y, dotR, TEXT_MUTED);
            tft.drawCircle(dx, y, dotR - 1, TEXT_MUTED);
        }
    }
}

void drawKey(int index, bool pressed) {
    if (index < 0 || index >= 12) return;

    TFT_eSPI &tft = gDisplay.tft();
    const int col = index % KEY_COLS;
    const int row = index / KEY_COLS;
    const int x = KEY_START_X + col * (KEY_W + KEY_GAP);
    const int y = KEY_START_Y + row * (KEY_H + KEY_GAP);

    const bool flash = (index == s.flashKey) && (millis() < s.flashUntilMs);
    const uint16_t bg = (pressed || flash) ? ACCENT_BLUE : BG_SECONDARY;
    const uint16_t fg = TEXT_PRIMARY;

    tft.fillRoundRect(x, y, KEY_W, KEY_H, 8, bg);
    tft.setTextFont(2);
    tft.setTextColor(fg, bg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(keyLabel(index), x + KEY_W / 2, y + KEY_H / 2);
    tft.setTextDatum(TL_DATUM);
}

void drawKeypad() {
    for (int i = 0; i < 12; i++) {
        drawKey(i, false);
    }
}

void drawLockoutOverlay() {
    TFT_eSPI &tft = gDisplay.tft();
    tft.fillRect(0, KEY_START_Y - 8, SCREEN_WIDTH, SCREEN_HEIGHT - KEY_START_Y, 0x0000);
    tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x0000);
    tft.setTextFont(2);
    tft.setTextColor(STATUS_RED, 0x0000);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Too many attempts", SCREEN_WIDTH / 2, 120);
    tft.setTextFont(4);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", adminLockoutSecondsRemaining());
    tft.drawString(buf, SCREEN_WIDTH / 2, 155);
    tft.setTextFont(1);
    tft.setTextColor(TEXT_MUTED, 0x0000);
    tft.drawString("seconds remaining", SCREEN_WIDTH / 2, 190);
    tft.setTextDatum(TL_DATUM);
}

}  // namespace

bool gAdminUnlocked = false;

void adminAuthInit() {
    const esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_POWERON || s_rtcMagic != RTC_MAGIC) {
        s_rtcFailCount = 0;
    }
    s_rtcMagic = RTC_MAGIC;

    if (s_rtcFailCount >= ADMIN_MAX_FAILED_ATTEMPTS) {
        s.lockoutUntilMs = millis() + ADMIN_LOCKOUT_MS;
    }

    if (!loadStoredHash()) {
        setAdminPin(ADMIN_PIN_DEFAULT);
    }

    adminEndSession();
    clearEntry();
}

void adminAuthTick() {
    const uint32_t now = millis();

    if (s.sessionUntilMs != 0 && now >= s.sessionUntilMs) {
        adminEndSession();
    }

    if (s.lockoutUntilMs != 0 && now >= s.lockoutUntilMs) {
        s.lockoutUntilMs = 0;
        s_rtcFailCount = 0;
        if (currentState == STATE_ADMIN_AUTH) {
            drawAdminAuthScreen();
        }
    }

    if (s.shaking) {
        if (now - s.shakeStartMs >= SHAKE_MS) {
            s.shaking = false;
            s.shakePhase = 0;
            if (currentState == STATE_ADMIN_AUTH) {
                drawPinDots(gDisplay.tft(), SCREEN_WIDTH / 2, 108);
            }
        } else {
            const uint32_t step = (now - s.shakeStartMs) / 45;
            if ((int)step != s.shakePhase) {
                s.shakePhase = (int)step;
                drawPinDots(gDisplay.tft(), SCREEN_WIDTH / 2, 108);
            }
        }
    }

    if (s.flashKey >= 0 && now >= s.flashUntilMs) {
        const int k = s.flashKey;
        s.flashKey = -1;
        drawKey(k, false);
    }

    if (adminIsLockedOut() && currentState == STATE_ADMIN_AUTH) {
        if (now - s.lastCountdownDrawMs >= 250) {
            s.lastCountdownDrawMs = now;
            drawLockoutOverlay();
        }
    }
}

bool setAdminPin(const char *newPin) {
    if (!pinValidFormat(newPin)) return false;
    PinHash h;
    pinHashCompute(newPin, h);
    if (!saveStoredHash(h)) return false;
    s.stored = h;
    return true;
}

bool verifyAdminPin(const char *attempt) {
    if (!pinValidFormat(attempt)) return false;
    PinHash attemptHash;
    pinHashCompute(attempt, attemptHash);
    return pinHashMatches(attemptHash, s.stored);
}

bool adminResetPinToDefault() {
    s_rtcFailCount = 0;
    s.lockoutUntilMs = 0;
    return setAdminPin(ADMIN_PIN_DEFAULT);
}

bool adminIsSessionActive() {
    return gAdminUnlocked && millis() < s.sessionUntilMs;
}

void adminBeginSession() {
    gAdminUnlocked = true;
    s.sessionUntilMs = millis() + ADMIN_SESSION_MS;
    clearEntry();
    s.pinErrorVisible = false;
}

void adminEndSession() {
    gAdminUnlocked = false;
    s.sessionUntilMs = 0;
}

void adminSessionTouch() {
    if (gAdminUnlocked) {
        s.sessionUntilMs = millis() + ADMIN_SESSION_MS;
    }
}

bool adminRequestAccess(AppState target) {
    if (adminIsSessionActive()) {
        if (currentState != target) {
            changeState(target);
        }
        return true;
    }
    s.pendingTarget = target;
    s.pendingSet = true;
    clearEntry();
    s.pinErrorVisible = false;
    if (currentState == STATE_ADMIN_AUTH) {
        drawAdminAuthScreen();
    } else {
        changeState(STATE_ADMIN_AUTH);
    }
    return false;
}

AppState adminPendingTarget() {
    return s.pendingSet ? s.pendingTarget : STATE_HOME;
}

void adminClearPending() {
    s.pendingSet = false;
    s.pendingTarget = STATE_HOME;
}

bool adminIsLockedOut() {
    return s.lockoutUntilMs != 0 && millis() < s.lockoutUntilMs;
}

int adminLockoutSecondsRemaining() {
    if (!adminIsLockedOut()) return 0;
    const uint32_t left = s.lockoutUntilMs - millis();
    return (int)((left + 999) / 1000);
}

void drawAdminAuthScreen() {
    TFT_eSPI &tft = gDisplay.tft();
    tft.fillScreen(COLOR_BG_DARK);

    if (adminIsLockedOut()) {
        drawLockIcon(tft, SCREEN_WIDTH / 2, 42);
        tft.setTextFont(2);
        tft.setTextColor(TEXT_PRIMARY, COLOR_BG_DARK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("Admin Access", SCREEN_WIDTH / 2, 72);
        tft.setTextDatum(TL_DATUM);
        drawLockoutOverlay();
        tft.setTextFont(1);
        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("Forgot PIN? Hold power 10s to reset", SCREEN_WIDTH / 2, 302);
        tft.setTextDatum(TL_DATUM);
        return;
    }

    tft.setTextFont(1);
    tft.setTextColor(TEXT_SECONDARY, COLOR_BG_DARK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("< Back", 8, 8);

    drawLockIcon(tft, SCREEN_WIDTH / 2, 42);

    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, COLOR_BG_DARK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Admin Access", SCREEN_WIDTH / 2, 72);

    if (s.pinErrorVisible) {
        tft.setTextFont(1);
        tft.setTextColor(STATUS_RED, COLOR_BG_DARK);
        tft.drawString("Incorrect PIN", SCREEN_WIDTH / 2, 88);
    }

    drawPinDots(tft, SCREEN_WIDTH / 2, 108);
    drawKeypad();

    tft.setTextFont(1);
    tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
    tft.drawString("Forgot PIN? Hold power 10s to reset", SCREEN_WIDTH / 2, 302);
    tft.setTextDatum(TL_DATUM);
}

void showPinError() {
    s.pinErrorVisible = true;
    s.shaking = true;
    s.shakeStartMs = millis();
    s.shakePhase = 0;
    clearEntry();
    drawAdminAuthScreen();
}

bool handleAdminAuthTouch(const TouchPoint &tp) {
    if (!tp.pressed) return false;

    adminSessionTouch();

    if (adminIsLockedOut()) {
        return false;
    }

    if (isTouchInRect(tp, 0, 0, 50, 32)) {
        adminClearPending();
        changeState(STATE_HOME);
        return false;
    }

    const int key = keyIndexAt(tp.x, tp.y);
    if (key < 0) return false;

    s.flashKey = key;
    s.flashUntilMs = millis() + KEY_FLASH_MS;
    drawKey(key, true);

    if (key <= 8) {
        if (s.entryLen < ADMIN_PIN_MAX_LEN) {
            s.entry[s.entryLen++] = (char)('1' + key);
            s.entry[s.entryLen] = '\0';
        }
        drawPinDots(gDisplay.tft(), SCREEN_WIDTH / 2, 108);
        return false;
    }

    if (key == 9) {
        if (s.entryLen > 0) {
            s.entry[--s.entryLen] = '\0';
            drawPinDots(gDisplay.tft(), SCREEN_WIDTH / 2, 108);
        }
        return false;
    }

    if (key == 10) {
        if (s.entryLen < ADMIN_PIN_MAX_LEN) {
            s.entry[s.entryLen++] = '0';
            s.entry[s.entryLen] = '\0';
            drawPinDots(gDisplay.tft(), SCREEN_WIDTH / 2, 108);
        }
        return false;
    }

    // Confirm (key 11)
    if (s.entryLen < ADMIN_PIN_MIN_LEN) {
        showPinError();
        return false;
    }

    if (verifyAdminPin(s.entry)) {
        s_rtcFailCount = 0;
        s.lockoutUntilMs = 0;
        s.pinErrorVisible = false;
        adminBeginSession();
        const AppState target = adminPendingTarget();
        adminClearPending();
        changeState(target);
        return true;
    }

    s_rtcFailCount++;
    if (s_rtcFailCount >= ADMIN_MAX_FAILED_ATTEMPTS) {
        s.lockoutUntilMs = millis() + ADMIN_LOCKOUT_MS;
        drawAdminAuthScreen();
    } else {
        showPinError();
    }
    return false;
}
