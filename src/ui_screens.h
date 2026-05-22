#pragma once

#include <TFT_eSPI.h>
#include "app_state.h"
#include "config.h"
#include "display.h"
#include "fingerprint.h"
#include "attendance.h"
#include "storage.h"
#include "wifi_manager.h"
#include "users_ui.h"

/** Multi-step enrollment UI (step 1–4, progress dots 0–4). */
void drawEnrollScreen(int step, const char *status, int progress, uint16_t statusColor);

class UiScreens {
    friend void drawEnrollScreen(int step, const char *status, int progress, uint16_t statusColor);
public:
    void enterState(AppState state);
    void exitState(AppState state);
    void updateState(AppState state);
    bool handleTouch(AppState state, const TouchPoint &tp);

    /** @deprecated Use changeState() from main; kept for gradual migration. */
    void setScreen(AppScreen screen);

    int enrollWizardStep() const { return _enrollWizardStep; }
    void enrollCancelPoll() { _enrollPollActive = false; }
    int homeNavTabAt(int16_t x, int16_t y) const;
    void drawErrorScreen(const char *title, const char *body);
    void drawHomeScreenOffset(int yOffset = 0);
    void beginHomeBootCard(int userCount, const char *timeStr);
    void updateHomeBootCard();

private:
    AppScreen _screen = AppScreen::Splash;
    uint32_t _splashStart = 0;

    TFT_eSprite _pulseSprite;
    bool _pulseSpriteReady = false;
    uint32_t _lastPulseMs = 0;
    uint32_t _lastClockMs = 0;
    char _lastClockStr[8] = {};
    char _lastDateStr[12] = {};
    int8_t _activeNavTab = -1;

    TFT_eSprite _enrollSprite;
    bool _enrollSpriteReady = false;
    uint32_t _lastEnrollPulseMs = 0;

    int _enrollWizardStep = 1;
    char _enrollName[MAX_NAME_LEN] = {};
    uint16_t _enrollId = 1;
    int _enrollDeptIdx = 0;
    int _enrollDeptScroll = 0;
    int _enrollNamePreset = 0;
    char _enrollStatus[64] = {};
    uint16_t _enrollStatusColor = STATUS_AMBER;
    int _enrollProgress = 0;
    bool _enrollPollActive = false;
    bool _enrollSuccess = false;
    FingerprintEnrollContext _enrollCtx;

    void drawSplash();
    void drawHomeScreen();
    void updateHomeClock();
    void updateHomePulse();
    void drawHomeStatusCard();
    void drawHomeBottomNav();

    void drawScan();
    void beginEnrollWizard();
    void drawEnroll();
    void refreshEnrollUi();
    void updateEnrollPulse();
    void tickEnrollFingerprint();
    void handleEnrollTouch(const TouchPoint &tp);

    void drawRecords();
    void handleRecordsTouch(const TouchPoint &tp);
    void drawSettings();
    void handleSettingsTouch(const TouchPoint &tp);
    void handleUsersScreenTouch(const TouchPoint &tp);

    void drawScanResultScreen();
    void drawWifiSetupScreen();

    bool _settingsDragging = false;
    int _settingsLastX = 0;
    int _settingsLastY = 0;

    bool _usersDragging = false;
    int _usersLastX = 0;
    int _usersLastY = 0;

    bool _recordsDragging = false;
    int _recordsLastX = 0;
    int _recordsLastY = 0;

    bool _bootCardActive = false;
    uint32_t _bootCardStartMs = 0;
    char _bootCardLine1[32] = {};
    char _bootCardLine2[24] = {};

    bool hitButton(int16_t tx, int16_t ty, int16_t x, int16_t y, int16_t w, int16_t h);
    void handleHomeTouch(const TouchPoint &tp);
    static const char *enrollDepartmentName(int idx);
};

extern UiScreens gUi;

void uiShowErrorScreen(const char *title, const char *body);
