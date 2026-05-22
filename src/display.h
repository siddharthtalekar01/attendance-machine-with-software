#pragma once

#include <TFT_eSPI.h>
#include <SPI.h>
#include "config.h"

class DisplayManager {
public:
    bool begin();
    /** Fills screen red → green → blue (1.5 s each). Serial logs progress. */
    void displayTest();
    TFT_eSPI &tft();

    void fillScreen(uint16_t color);
    void setBacklight(bool on);
    void drawCenteredText(int16_t y, const char *text, uint8_t font = 2, uint16_t color = COLOR_TEXT);
    void drawHeader(const char *title);
    void drawStatusBar(const char *left, const char *right = nullptr);
    void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const char *label, uint16_t fill = COLOR_PRIMARY);
    void showMessage(const char *title, const char *body, uint16_t accent = COLOR_PRIMARY);

    bool getTouchPoint(int16_t &x, int16_t &y);

private:
    TFT_eSPI _tft = TFT_eSPI();
    bool _ready = false;
};

extern DisplayManager gDisplay;
