#pragma once

#include <Arduino.h>
#include "app_state.h"
#include "display.h"

// PIN policy
constexpr int ADMIN_PIN_MIN_LEN = 4;
constexpr int ADMIN_PIN_MAX_LEN = 6;
constexpr int ADMIN_PIN_DOT_COUNT = 6;

constexpr int ADMIN_MAX_FAILED_ATTEMPTS = 3;
constexpr uint32_t ADMIN_LOCKOUT_MS = 30000;
#ifndef ADMIN_SESSION_MS
#define ADMIN_SESSION_MS (5UL * 60UL * 1000UL)  // 5 minutes; override before include if needed
#endif

constexpr const char *ADMIN_PIN_DEFAULT = "1234";

extern bool gAdminUnlocked;

/** Initialize storage, RTC fail counter, default PIN if missing. Call after LittleFS mount. */
void adminAuthInit();

/** Call every main loop tick — animations, lockout countdown, session expiry. */
void adminAuthTick();

// -----------------------------------------------------------------------------
// PIN storage / verification
// -----------------------------------------------------------------------------
bool setAdminPin(const char *newPin);
bool verifyAdminPin(const char *attempt);
bool adminResetPinToDefault();

// -----------------------------------------------------------------------------
// Session (gates enroll / delete / settings)
// -----------------------------------------------------------------------------
bool adminIsSessionActive();
void adminBeginSession();
void adminEndSession();
void adminSessionTouch();

/**
 * If session active, returns true. Otherwise stores @p target, opens auth screen, false.
 */
bool adminRequestAccess(AppState target);

AppState adminPendingTarget();
void adminClearPending();

// -----------------------------------------------------------------------------
// Auth UI (240x320)
// -----------------------------------------------------------------------------
void drawAdminAuthScreen();
bool handleAdminAuthTouch(const TouchPoint &tp);
void showPinError();

bool adminIsLockedOut();
int adminLockoutSecondsRemaining();
