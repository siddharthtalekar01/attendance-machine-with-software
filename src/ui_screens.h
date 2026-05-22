#pragma once

#include <TFT_eSPI.h>
#include "config.h"
#include "display.h"
#include "fingerprint.h"
#include "attendance.h"
#include "storage.h"
#include "wifi_manager.h"

class UiScreens {
public:
    void begin();
    void setScreen(AppScreen screen);
    AppScreen currentScreen() const { return _screen; }
    void loop();

private:
    AppScreen _screen = AppScreen::Splash;
    uint32_t _splashStart = 0;

    TFT_eSprite _pulseSprite;
    bool _pulseSpriteReady = false;
    uint32_t _lastPulseMs = 0;
    uint32_t _lastClockMs = 0;
    char _lastClockStr[8] = {};
    char _lastDateStr[12] = {};
    int8_t _activeNavTab = -1;  // 0=Enroll 1=Records 2=Settings, -1=home dashboard

    void drawSplash();
    void drawHomeScreen();
    void updateHomeClock();
    void updateHomePulse();
    void drawHomeStatusCard();
    void drawHomeBottomNav();

    void drawScan();
    void drawEnroll();
    void drawRecords();
    void drawSettings();

    bool hitButton(int16_t tx, int16_t ty, int16_t x, int16_t y, int16_t w, int16_t h);
    void handleHomeTouch(const TouchPoint &tp);
    int homeNavTabAt(int16_t x, int16_t y) const;
};

extern UiScreens gUi;
