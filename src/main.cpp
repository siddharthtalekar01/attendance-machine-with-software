#include <Arduino.h>
#include <TimeLib.h>
#include <cstring>

#include "admin_auth.h"
#include "app_state.h"
#include "attendance.h"
#include "config.h"
#include "display.h"
#include "fingerprint.h"
#include "settings_manager.h"
#include "settings_ui.h"
#include "splash.h"
#include "storage.h"
#include "ui_screens.h"
#include "wifi_manager.h"

AppState currentState = STATE_BOOT;

static uint32_t s_stateEnteredMs = 0;

static char s_errorTitle[32] = {};
static char s_errorBody[64] = {};

static bool s_settingsTabHeld = false;
static uint32_t s_settingsHoldMs = 0;
static uint32_t s_lastScanPollMs = 0;

static void stateOnEnter(AppState state);
static void stateOnUpdate(AppState state);
static void stateOnExit(AppState state);
static void dispatchTouch(const TouchPoint &tp);
static void serviceMidnightReset();

void changeState(AppState next) {
    if (next == currentState) return;

    stateOnExit(currentState);
    currentState = next;
    s_stateEnteredMs = millis();
    stateOnEnter(currentState);
}

static void stateOnEnter(AppState state) {
    if (state == STATE_HOME) {
        adminEndSession();
        s_settingsTabHeld = false;
    }
    if (state == STATE_SETTINGS || state == STATE_USERS || state == STATE_ENROLL_INFO) {
        adminSessionTouch();
    }

    if (state != STATE_BOOT) {
        gUi.enterState(state);
    }

    if (state == STATE_ERROR) {
        uiShowErrorScreen(s_errorTitle, s_errorBody);
    }
}

static void stateOnExit(AppState state) {
    if (state != STATE_BOOT) {
        gUi.exitState(state);
    }
}

static void stateBootOnUpdate() {
    if (splashTick()) {
        char timeBuf[16] = "--:--";
        if (timeStatus() != timeNotSet) {
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hour(), minute());
        } else {
            const String t = formatTime(getCurrentTime()).substring(0, 5);
            t.toCharArray(timeBuf, sizeof(timeBuf));
        }
        changeState(STATE_HOME);
        gUi.beginHomeBootCard(splashEnrolledUserCount(), timeBuf);
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
                adminRequestAccess(STATE_SETTINGS);
            }
        } else {
            if (heldMs < ADMIN_HOLD_MS) {
                adminRequestAccess(STATE_SETTINGS);
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
        default:
            break;
    }
}

static void dispatchTouch(const TouchPoint &tp) {
    if (!tp.pressed) return;
    if (currentState == STATE_BOOT || splashIsTransitioning()) return;

    adminSessionTouch();

    switch (currentState) {
        case STATE_HOME:
            stateHomeOnTouch(tp);
            break;
        case STATE_ADMIN_AUTH:
            handleAdminAuthTouch(tp);
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

    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.printf("[%s] %s starting...\n", APP_NAME, APP_VERSION);

    if (!gDisplay.begin()) {
        Serial.println("Display init failed");
        strlcpy(s_errorTitle, "Display", sizeof(s_errorTitle));
        strlcpy(s_errorBody, "TFT init failed", sizeof(s_errorBody));
        changeState(STATE_ERROR);
        uiShowErrorScreen(s_errorTitle, s_errorBody);
        return;
    }

    splashBegin();
    changeState(STATE_BOOT);
}

void loop() {
    touchUpdate();
    adminAuthTick();
    serviceMidnightReset();

    wifiUpdate();

    if (gSettingsUi.settings.ntpEnabled && wifiIsConnected() && !wifiNtpSyncInProgress()) {
        static uint32_t lastNtpMs = 0;
        const uint32_t nowMs = millis();
        if (nowMs - lastNtpMs >= NTP_UPDATE_INTERVAL_MS) {
            lastNtpMs = nowMs;
            wifiNtpSyncBegin();
        }
    }

    stateOnUpdate(currentState);

    TouchPoint tp;
    while (touchEventPop(tp)) {
        dispatchTouch(tp);
    }
}
