#include "ui_screens.h"



UiScreens gUi;



void UiScreens::begin() {

    _splashStart = millis();

    setScreen(AppScreen::Splash);

}



void UiScreens::setScreen(AppScreen screen) {
    touchQueueClear();
    _screen = screen;

    switch (screen) {

        case AppScreen::Splash: drawSplash(); break;

        case AppScreen::Home: drawHome(); break;

        case AppScreen::Scan: drawScan(); break;

        case AppScreen::Enroll: drawEnroll(); break;

        case AppScreen::Settings: drawSettings(); break;

        default: drawHome(); break;

    }

}



bool UiScreens::hitButton(int16_t tx, int16_t ty, int16_t x, int16_t y, int16_t w, int16_t h) {

    TouchPoint tp;

    tp.x = tx;

    tp.y = ty;

    tp.pressed = true;

    return isTouchInRect(tp, x, y, w, h);

}



void UiScreens::drawSplash() {

    gDisplay.fillScreen(COLOR_BG);

    gDisplay.drawCenteredText(120, APP_NAME, 4, COLOR_PRIMARY);

    gDisplay.drawCenteredText(160, APP_VERSION, 2, COLOR_TEXT_DIM);

}



void UiScreens::drawHome() {

    gDisplay.fillScreen(COLOR_BG);

    gDisplay.drawHeader(APP_NAME);

    gDisplay.drawButton(20, 60, 200, 44, "Scan Finger", COLOR_PRIMARY);

    gDisplay.drawButton(20, 120, 200, 44, "Enroll", COLOR_ACCENT);

    gDisplay.drawButton(20, 180, 200, 44, "Settings", 0x4A69);

    gDisplay.drawStatusBar(gWiFi.isConnected() ? "WiFi OK" : "Offline", gWiFi.formattedTime().c_str());

}



void UiScreens::drawScan() {

    gDisplay.fillScreen(COLOR_BG);

    gDisplay.drawHeader("Scan");

    gDisplay.drawCenteredText(140, "Place finger on sensor", 2, COLOR_TEXT);

    gDisplay.drawButton(20, 260, 90, 40, "Back", 0x4208);

}



void UiScreens::drawEnroll() {

    gDisplay.fillScreen(COLOR_BG);

    gDisplay.drawHeader("Enroll");

    gDisplay.drawCenteredText(120, "Assign slot in admin", 2, COLOR_TEXT_DIM);

    gDisplay.drawCenteredText(150, "Use serial/API next", 2, COLOR_TEXT);

    gDisplay.drawButton(20, 260, 90, 40, "Back", 0x4208);

}



void UiScreens::drawSettings() {

    gDisplay.fillScreen(COLOR_BG);

    gDisplay.drawHeader("Settings");

    gDisplay.drawCenteredText(100, "WiFi: secrets.h", 2, COLOR_TEXT_DIM);

    gDisplay.drawButton(20, 140, 200, 40, "Calibrate Touch", 0x4A69);

    gDisplay.drawButton(20, 260, 90, 40, "Back", 0x4208);

}



void UiScreens::handleHomeTouch(const TouchPoint &tp) {

    if (isTouchInRect(tp, 20, 60, 200, 44)) setScreen(AppScreen::Scan);

    else if (isTouchInRect(tp, 20, 120, 200, 44)) setScreen(AppScreen::Enroll);

    else if (isTouchInRect(tp, 20, 180, 200, 44)) setScreen(AppScreen::Settings);

}



void UiScreens::loop() {

    touchUpdate();



    if (_screen == AppScreen::Splash) {

        if (millis() - _splashStart > 2000) setScreen(AppScreen::Home);

        return;

    }



    TouchPoint tp;

    while (touchEventPop(tp)) {

        if (!tp.pressed) continue;



        if (_screen == AppScreen::Home) {

            handleHomeTouch(tp);

            continue;

        }



        if (_screen == AppScreen::Settings) {

            if (isTouchInRect(tp, 20, 140, 200, 40)) {

                calibrateTouch();

                setScreen(AppScreen::Settings);

            } else if (isTouchInRect(tp, 20, 260, 90, 40)) {

                setScreen(AppScreen::Home);

            }

            continue;

        }



        if (isTouchInRect(tp, 20, 260, 90, 40)) {

            setScreen(AppScreen::Home);

            continue;

        }



        if (_screen == AppScreen::Scan) {

            ScanOutcome result = gAttendance.processScan();

            if (result.success) {

                char msg[64];

                snprintf(msg, sizeof(msg), "%s\n%s", result.userName, result.message);

                gDisplay.showMessage("Success", msg, COLOR_SUCCESS);

            } else {

                gDisplay.showMessage("Scan", result.message, COLOR_ERROR);

            }

            delay(1500);

            setScreen(AppScreen::Home);

        }

    }

}

