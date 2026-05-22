#pragma once

#include <Arduino.h>

enum AppState : uint8_t {
    STATE_BOOT = 0,
    STATE_HOME,
    STATE_SCANNING,
    STATE_SCAN_RESULT,
    STATE_ENROLL_INFO,
    STATE_ENROLL_SCAN1,
    STATE_ENROLL_SCAN2,
    STATE_ENROLL_RESULT,
    STATE_RECORDS,
    STATE_USERS,
    STATE_SETTINGS,
    STATE_WIFI_SETUP,
    STATE_ADMIN_AUTH,
    STATE_ERROR,
};

/** After admin PIN, optional target (or menu from auth screen). */
enum AdminTarget : uint8_t {
    ADMIN_TARGET_NONE = 0,
    ADMIN_TARGET_SETTINGS,
    ADMIN_TARGET_USERS,
    ADMIN_TARGET_RECORDS,
};

extern AppState currentState;

void changeState(AppState next);
