#pragma once

#include <TFT_eSPI.h>
#include "config.h"
#include "storage.h"

// -------------------------------------------------------------------------
// Layout constants for the Users screen (240 × 320)
// -------------------------------------------------------------------------
constexpr int USR_HEADER_H     = 36;   // header bar height
constexpr int USR_SEARCH_Y     = USR_HEADER_H;
constexpr int USR_SEARCH_H     = 32;
constexpr int USR_LIST_Y       = USR_HEADER_H + USR_SEARCH_H;
constexpr int USR_LIST_H       = SCREEN_HEIGHT - USR_LIST_Y;  // 252 px
constexpr int USR_ROW_H        = 52;
constexpr int USR_MAX_USERS    = MAX_ENROLLED_FINGERS;        // 50
constexpr int USR_SWIPE_THRESH = 40;   // px horizontal drag to reveal actions

// Avatar palette — one colour per department (index 0-4)
// Must not conflict with COLOR_BG_DARK so initials are readable.
constexpr uint16_t USR_DEPT_COLORS[6] = {
    0x2196,  // HR         – primary blue
    0x07B6,  // Engineering– teal
    0xFD40,  // Sales      – amber
    0x8410,  // Operations – slate
    0xA81F,  // Management – purple
    0x3D6B,  // fallback
};

// -------------------------------------------------------------------------
// Rich user record used by the UI (loaded once per screen entry)
// -------------------------------------------------------------------------
struct EnrolledUser {
    uint8_t  fingerId   = 0;
    char     name[MAX_NAME_LEN] = {};
    char     department[24]     = {};
    bool     enrolled   = false;
    time_t   enrollDate = 0;
    int      totalDays  = 0;
    char     avgArrival[6] = {};
};

// -------------------------------------------------------------------------
// Module state (lives in users_ui.cpp)
// -------------------------------------------------------------------------
struct UsersUiState {
    EnrolledUser users[USR_MAX_USERS];
    int      userCount   = 0;

    // Scrollable list
    TFT_eSprite listSprite;
    bool     spriteReady = false;
    int      scrollY     = 0;
    float    velocity    = 0.0f;
    int      totalContentH = 0;

    // Drag/swipe
    bool     dragging    = false;
    int      dragStartX  = 0;
    int      dragStartY  = 0;
    int      lastTouchX  = 0;
    int      lastTouchY  = 0;
    int      swipedRow   = -1;   // index of row with actions revealed

    // Detail popup
    bool     popupVisible= false;
    int      popupUser   = -1;   // index into users[]
    bool     popupConfirmDelete = false;

    // Search / virtual keyboard
    char     searchBuf[MAX_NAME_LEN] = {};
    int      searchLen   = 0;
    bool     kbVisible   = false;
    int      kbShift     = 0;   // 0 = lowercase, 1 = upper, 2 = numbers

    // Filtered list (indices into users[])
    int      filtered[USR_MAX_USERS];
    int      filteredCount = 0;
};

extern UsersUiState gUsersUi;

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

/** Load users from storage + sensor, then draw the full screen. */
void drawUsersScreen();

/** Render one user row into the current draw target at pixel-y `y`. */
void drawUserRow(int y, const EnrolledUser &user, bool swiped);

/** Draw the full-screen detail popup (slide-up overlay). */
void drawUserDetailPopup(const EnrolledUser &user, bool confirmDelete);

/**
 * Handle a touch event on the Users screen.
 * @return true if the event was consumed (caller should not process further).
 */
bool handleUserListTouch(TouchPoint tp);

/** Called every loop() while Users screen is active (inertia + drag). */
void usersTickInertia();
void usersHandleTouchDown(int x, int y);
void usersHandleTouchMove(int x, int y);
void usersHandleTouchUp(int x, int y);

/** Reload the user list (call after enroll / delete). */
void usersReload();

/** Delete a user by index (removes from storage + fingerprint sensor). */
bool usersDeleteByIndex(int idx);
