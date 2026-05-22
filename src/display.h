#pragma once

#include <TFT_eSPI.h>
#include "config.h"

// -----------------------------------------------------------------------------
// Touch input (XPT2046 via TFT_eSPI built-in driver, TOUCH_CS in User_Setup.h)
//
// CALIBRATION PROCEDURE (run calibrateTouch() once, e.g. from Settings):
//   1. Three crosshairs appear: top-left, top-right, bottom-right.
//   2. Tap each crosshair firmly with a stylus/finger; wait for the next target.
//   3. Raw ADC values are stored and written to LittleFS: /config/touch_cal.json
//   4. On boot, touchLoadCalibration() loads the file; defaults are used if missing.
//   5. Re-run calibration if touches are offset or axes are swapped.
//
// RUNTIME:
//   - Call touchUpdate() every loop() — fast poll, enqueues press edges.
//   - During slow draws, use touchEventPop() so taps are not lost.
//   - getTouchPoint() returns the current debounced state (one press per lift).
// -----------------------------------------------------------------------------

struct TouchPoint {
    int x = 0;
    int y = 0;
    bool pressed = false;
};

struct TouchCalibration {
    uint16_t rawXMin = 200;
    uint16_t rawXMax = 3800;
    uint16_t rawYMin = 200;
    uint16_t rawYMax = 3800;
    bool valid = false;
};

constexpr int TOUCH_QUEUE_DEPTH = 4;

TouchPoint getTouchPoint();
bool isTouchInRect(TouchPoint tp, int x, int y, int w, int h);

/** Sample touch and push press-edge events into the global queue. Call every loop. */
void touchUpdate();

bool touchEventAvailable();
bool touchEventPop(TouchPoint &out);
void touchQueueClear();

/** Load /config/touch_cal.json from LittleFS (call after LittleFS mount). */
bool touchLoadCalibration();

/** Interactive 3-point calibration wizard; saves /config/touch_cal.json. */
void calibrateTouch();

class DisplayManager {
public:
    bool begin();
    void displayTest();
    TFT_eSPI &tft();

    void fillScreen(uint16_t color);
    void setBacklight(bool on);
    void drawCenteredText(int16_t y, const char *text, uint8_t font = 2, uint16_t color = COLOR_TEXT);
    void drawHeader(const char *title);
    void drawStatusBar(const char *left, const char *right = nullptr);
    void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char *label, uint16_t fill = COLOR_PRIMARY);
    void showMessage(const char *title, const char *body, uint16_t accent = COLOR_PRIMARY);

private:
    TFT_eSPI _tft = TFT_eSPI();
    bool _ready = false;
};

extern DisplayManager gDisplay;
