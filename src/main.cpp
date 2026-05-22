#include <Arduino.h>
#include <TimeLib.h>
#include <cstring>

#include "app_state.h"
#include "attendance.h"
#include "config.h"
#include "display.h"
#include "fingerprint.h"
#include "settings_manager.h"
#include "settings_ui.h"
#include "storage.h"
#include "ui_screens.h"
#include "wifi_manager.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

AppState currentState = STATE_BOOT;
bool gAdminUnlocked = false;

static uint32_t s_stateEnteredMs = 0;

static char s_errorTitle[32] = {};
static char s_errorBody[64] = {};

// Admin PIN entry
static char s_adminPin[8] = {};
static int s_adminPinLen = 0;
static bool s_adminPinError = false;
static bool s_adminMenuVisible = false;

// Home: long-press Settings tab for admin
static bool s_settingsTabHeld = false;
static uint32_t s_settingsHoldMs = 0;
static int16_t s_settingsHoldX = 0;
static int16_t s_settingsHoldY = 0;

// Scanning poll throttle
static uint32_t s_lastScanPollMs = 0;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void stateOnEnter(AppState state);
static void stateOnUpdate(AppState state);
static void stateOnExit(AppState state);
static void dispatchTouch(const TouchPoint &tp);
static void serviceMidnightReset();
static bool initHardware();

// ---------------------------------------------------------------------------
// State lifecycle API
// ---------------------------------------------------------------------------
void changeState(AppState next) {
    if (next == currentState) return;

    stateOnExit(currentState);
    currentState = next;
    s_stateEnteredMs = millis();
    stateOnEnter(currentState);
}

static void stateOnEnter(AppState state) {
    if (state == STATE_HOME) {
        gAdminUnlocked = false;
        s_adminMenuVisible = false;
        s_settingsTabHeld = false;
    }
    if (state == STATE_ADMIN_AUTH) {
        s_adminPinLen = 0;
        s_adminPin[0] = '\0';
        s_adminPinError = false;
        s_adminMenuVisible = false;
    }
    gUi.enterState(state);

    if (state == STATE_ERROR) {
        uiShowErrorScreen(s_errorTitle, s_errorBody);
    }
}

static void stateOnExit(AppState state) {
    gUi.exitState(state);
}

// ---------------------------------------------------------------------------
// Per-state onEnter / update / touch (update + touch invoked from loop)
// ---------------------------------------------------------------------------
static void stateBootOnUpdate() {
    if (millis() - s_stateEnteredMs >= BOOT_ANIM_MS) {
        changeState(STATE_HOME);
    }
}

static void stateHomeOnUpdate() {
    gUi.updateState(STATE_HOME);

    if (fingerprintFingerPresent()) {
        changeState(STATE_SCANNING);
        return;
    }

    if (s_settingsTabHeld) {
        TouchPoint held;
        const uint32_t heldMs = millis() - s_settingsHoldMs;
        if (touchReadHeld(held) && gUi.homeNavTabAt(held.x, held.y) == 2) {
            if (heldMs >= ADMIN_HOLD_MS) {
                s_settingsTabHeld = false;
                changeState(STATE_ADMIN_AUTH);
            }
        } else {
            if (heldMs < ADMIN_HOLD_MS) {
                changeState(STATE_SETTINGS);
            }
            s_settingsTabHeld = false;
        }
    }
}

static void stateHomeOnTouch(const TouchPoint &tp) {
    const int tab = gUi.homeNavTabAt(tp.x, tp.y);
    if (tab == 2) {
        s_settingsTabHeld = true;
        s_settingsHoldMs = millis();
        return;
    }
    gUi.handleTouch(STATE_HOME, tp);
}

static void stateScanningOnUpdate() {
    gUi.updateState(STATE_SCANNING);

    const uint32_t nowMs = millis();
    if (nowMs - s_lastScanPollMs < SCAN_POLL_MS) return;
    s_lastScanPollMs = nowMs;

    if (!fingerprintFingerPresent()) return;

    uint16_t slot = 0;
    uint16_t conf = 0;
    const int fp = fingerprintSearch(slot, conf);
    if (fp == FP_SEARCH_NO_FINGER) return;

    if (fp == FP_SEARCH_OK) {
        processAttendance(slot, ::now());
        changeState(STATE_SCAN_RESULT);
        return;
    }

    gLastScanResult.type = SCAN_UNKNOWN;
    gLastScanResult.hasUser = false;
    gLastScanResult.hasRecord = false;
    strlcpy(gLastScanResult.user.name, "", sizeof(gLastScanResult.user.name));
    changeState(STATE_SCAN_RESULT);
}

static void stateScanResultOnUpdate() {
    if (millis() - s_stateEnteredMs >= SCAN_RESULT_MS) {
        changeState(STATE_HOME);
    }
}

static void stateEnrollOnUpdate() {
    gUi.updateState(currentState);
}

static void stateAdminAuthOnUpdate() {
    // PIN UI is touch-driven; menu uses handleAdminMenuTouch
}

static bool verifyAdminPin() {
    return strcmp(s_adminPin, ADMIN_PIN_DEFAULT) == 0;
}

static void stateAdminAuthOnTouch(const TouchPoint &tp) {
    if (s_adminMenuVisible) {
        gUi.handleAdminMenuTouch(tp);
        return;
    }

    bool submitted = false;
    if (gUi.handleAdminAuthTouch(tp, s_adminPin, s_adminPinLen, submitted)) {
        if (submitted) {
            if (verifyAdminPin()) {
                gAdminUnlocked = true;
                s_adminPinError = false;
                s_adminMenuVisible = true;
                gUi.drawAdminMenuScreen();
            } else {
                s_adminPinError = true;
                s_adminPinLen = 0;
                s_adminPin[0] = '\0';
                gUi.drawAdminAuthScreen("", true);
            }
        }
    }
}

static void stateOnUpdate(AppState state) {
    switch (state) {
        case STATE_BOOT:
            stateBootOnUpdate();
            break;
        case STATE_HOME:
            stateHomeOnUpdate();
            break;
        case STATE_SCANNING:
            stateScanningOnUpdate();
            break;
        case STATE_SCAN_RESULT:
            stateScanResultOnUpdate();
            break;
        case STATE_ENROLL_INFO:
        case STATE_ENROLL_SCAN1:
        case STATE_ENROLL_SCAN2:
        case STATE_ENROLL_RESULT:
            stateEnrollOnUpdate();
            break;
        case STATE_RECORDS:
        case STATE_USERS:
        case STATE_SETTINGS:
            gUi.updateState(state);
            break;
        case STATE_ADMIN_AUTH:
            stateAdminAuthOnUpdate();
            break;
        default:
            break;
    }
}

static void dispatchTouch(const TouchPoint &tp) {
    if (!tp.pressed) return;

    switch (currentState) {
        case STATE_HOME:
            stateHomeOnTouch(tp);
            break;
        case STATE_ADMIN_AUTH:
            stateAdminAuthOnTouch(tp);
            break;
        case STATE_ERROR:
            if (isTouchInRect(tp, 20, 260, (int)SCREEN_WIDTH - 40, 40)) {
                changeState(STATE_HOME);
            }
            break;
        default:
            gUi.handleTouch(currentState, tp);
            break;
    }
}

static void serviceMidnightReset() {
    static int lastCalDay = -1;
    if (timeStatus() == timeNotSet) return;

    const int today = (int)year() * 10000 + (int)month() * 100 + (int)day();
    if (lastCalDay >= 0 && today != lastCalDay) {
        midnightReset();
    }
    lastCalDay = today;
}

static bool initHardware() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.printf("[%s] %s starting...\n", APP_NAME, APP_VERSION);

    if (!gDisplay.begin()) {
        Serial.println("Display init failed");
        strlcpy(s_errorTitle, "Display", sizeof(s_errorTitle));
        strlcpy(s_errorBody, "TFT init failed", sizeof(s_errorBody));
        return false;
    }

    if (!gStorage.begin()) {
        Serial.println("LittleFS init failed");
        strlcpy(s_errorTitle, "Storage", sizeof(s_errorTitle));
        strlcpy(s_errorBody, "LittleFS mount failed", sizeof(s_errorBody));
        return false;
    }

    touchLoadCalibration();
    settingsLoad(gSettingsUi.settings);

    if (!fingerprintInit()) {
        Serial.println("Fingerprint sensor not found on UART2");
        strlcpy(s_errorTitle, "Sensor", sizeof(s_errorTitle));
        strlcpy(s_errorBody, "R307S not detected\nCheck wiring 16/17", sizeof(s_errorBody));
        return false;
    }

    Serial.printf("Fingerprint templates: %u\n", fingerprintGetCount());
    return true;
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
#if RUN_DISPLAY_TEST
    Serial.begin(115200);
    delay(500);
    if (!gDisplay.begin()) {
        Serial.println("Display init failed");
    }
    gDisplay.displayTest();
    while (true) {
        delay(1000);
    }
#endif

    const bool hwOk = initHardware();

    gAttendance.begin();

    gWiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    if (strlen(WIFI_SSID) > 0) {
        gWiFi.connect();
    }
    if (gSettingsUi.settings.autoNtp) {
        gWiFi.syncTime();
    }

    if (!hwOk) {
        changeState(STATE_ERROR);
        uiShowErrorScreen(s_errorTitle, s_errorBody);
    } else {
        changeState(STATE_BOOT);
    }
}

void loop() {
    touchUpdate();
    serviceMidnightReset();

    if (gSettingsUi.settings.autoNtp && gWiFi.isConnected()) {
        static uint32_t lastNtpMs = 0;
        const uint32_t nowMs = millis();
        if (nowMs - lastNtpMs >= NTP_UPDATE_INTERVAL_MS) {
            lastNtpMs = nowMs;
            gWiFi.syncTime();
        }
    }

    stateOnUpdate(currentState);

    TouchPoint tp;
    while (touchEventPop(tp)) {
        dispatchTouch(tp);
    }
}
