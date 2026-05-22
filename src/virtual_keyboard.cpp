#include "virtual_keyboard.h"

#include <ctype.h>
#include <cstring>

namespace {

VirtualKeyboardState s_defaultKb;

const char *rowKeys(const VirtualKeyboardState &kb, int row) {
    if (kb.shift == 2) {
        static const char *nums = "1234567890";
        if (row == 0) return nums;
        return "";
    }
    if (row == 0) return "qwertyuiop";
    if (row == 1) return "asdfghjkl";
    return "zxcvbnm";
}

int rowKeyWidth(int row) {
    return (row == 2) ? 24 : 22;
}

}  // namespace

VirtualKeyboardState *keyboardDefaultState() {
    return &s_defaultKb;
}

void drawKeyboard(TFT_eSPI &tft, int y, const char *buffer, int maxLen, bool active,
                  VirtualKeyboardState *kbState) {
    (void)buffer;
    (void)maxLen;

    VirtualKeyboardState &kb = kbState ? *kbState : s_defaultKb;
    const int h = VIRTUAL_KEYBOARD_H;
    const uint16_t panelBg = active ? 0x2945 : 0x2104;

    tft.fillRect(0, y, SCREEN_WIDTH, h, panelBg);
    tft.drawLine(0, y, SCREEN_WIDTH, y, TEXT_MUTED);

    for (int row = 0; row < 3; row++) {
        const char *keys = rowKeys(kb, row);
        if (!keys || !keys[0]) continue;

        const int n = (int)strlen(keys);
        const int keyW = rowKeyWidth(row);
        const int totalW = n * keyW;
        int x = (SCREEN_WIDTH - totalW) / 2;
        const int ky = y + 8 + row * 26;

        for (int i = 0; i < n; i++) {
            tft.fillRoundRect(x, ky, keyW - 2, 22, 3, BG_SECONDARY);
            char ch[2] = {keys[i], 0};
            if (kb.shift == 1) {
                ch[0] = (char)toupper((unsigned char)ch[0]);
            }
            tft.setTextFont(1);
            tft.setTextColor(TEXT_PRIMARY, BG_SECONDARY);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(ch, x + keyW / 2 - 1, ky + 11);
            x += keyW;
        }
    }
    tft.setTextDatum(TL_DATUM);

    tft.fillRoundRect(4, y + h - 28, 40, 22, 3, ACCENT_BLUE);
    tft.setTextFont(1);
    tft.setTextColor(TEXT_PRIMARY, ACCENT_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(kb.shift == 2 ? "ABC" : "123", 24, y + h - 17);

    tft.fillRoundRect(SCREEN_WIDTH - 44, y + h - 28, 40, 22, 3, TEXT_MUTED);
    tft.setTextColor(TEXT_PRIMARY, TEXT_MUTED);
    tft.drawString("Del", SCREEN_WIDTH - 24, y + h - 17);
    tft.setTextDatum(TL_DATUM);
}

bool handleKeyboardTouch(TouchPoint tp, int y, char *buffer, int &len, int maxLen,
                         VirtualKeyboardState *kbState) {
    if (!tp.pressed || !buffer || maxLen <= 0) return false;

    VirtualKeyboardState &kb = kbState ? *kbState : s_defaultKb;
    const int h = VIRTUAL_KEYBOARD_H;
    if (tp.y < y || tp.y >= y + h) return false;

    if (isTouchInRect(tp, 4, y + h - 28, 40, 22)) {
        kb.shift = (kb.shift + 1) % 3;
        return true;
    }
    if (isTouchInRect(tp, SCREEN_WIDTH - 44, y + h - 28, 40, 22)) {
        if (len > 0) {
            buffer[--len] = '\0';
        }
        return true;
    }

    for (int row = 0; row < 3; row++) {
        const char *keys = rowKeys(kb, row);
        if (!keys || !keys[0]) continue;

        const int n = (int)strlen(keys);
        const int keyW = rowKeyWidth(row);
        const int totalW = n * keyW;
        int kx = (SCREEN_WIDTH - totalW) / 2;
        const int ky = y + 8 + row * 26;

        for (int i = 0; i < n; i++) {
            if (isTouchInRect(tp, kx, ky, keyW - 2, 22)) {
                if (len < maxLen - 1) {
                    char ch = keys[i];
                    if (kb.shift == 1) {
                        ch = (char)toupper((unsigned char)ch);
                    }
                    buffer[len++] = ch;
                    buffer[len] = '\0';
                }
                return true;
            }
            kx += keyW;
        }
    }

    return false;
}
