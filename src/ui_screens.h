#pragma once

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

    void drawSplash();
    void drawHome();
    void drawScan();
    void drawEnroll();
    void drawSettings();

    bool hitButton(int16_t tx, int16_t ty, int16_t x, int16_t y, int16_t w, int16_t h);
    void handleHomeTouch(int16_t x, int16_t y);
};

extern UiScreens gUi;
