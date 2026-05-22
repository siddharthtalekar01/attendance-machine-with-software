#pragma once

#include <TFT_eSPI.h>
#include "display.h"

/** Shared on-screen QWERTY keyboard (10×3 key grid + shift / delete). */
struct VirtualKeyboardState {
    int shift = 0;  // 0 = lowercase, 1 = uppercase, 2 = numbers
};

constexpr int VIRTUAL_KEYBOARD_H = 96;

/**
 * Draw keyboard with top edge at @p y (full screen width).
 * @p active when true highlights the keyboard panel (caller draws buffer field separately).
 */
void drawKeyboard(TFT_eSPI &tft, int y, const char *buffer, int maxLen, bool active,
                  VirtualKeyboardState *kbState = nullptr);

/**
 * Handle tap inside keyboard area. Updates @p buffer and @p len.
 * @return true if the touch was consumed.
 */
bool handleKeyboardTouch(TouchPoint tp, int y, char *buffer, int &len, int maxLen,
                         VirtualKeyboardState *kbState = nullptr);

VirtualKeyboardState *keyboardDefaultState();
