#include "wifi_setup_ui.h"

#include <WiFi.h>
#include <cstring>

#include "admin_auth.h"
#include "app_state.h"
#include "settings_manager.h"
#include "settings_ui.h"
#include "virtual_keyboard.h"
#include "wifi_manager.h"

namespace {

constexpr int HDR_H = 36;
constexpr int STATUS_Y = 36;
constexpr int STATUS_H = 48;
constexpr int LIST_Y = 84;
constexpr int LIST_H = SCREEN_HEIGHT - LIST_Y;
constexpr int ROW_H = 40;
constexpr int POPUP_H = 200;
constexpr int POPUP_Y = SCREEN_HEIGHT - POPUP_H;

constexpr int MAX_NETS = 24;
constexpr uint32_t CONNECT_UI_TIMEOUT_MS = 15000;
constexpr uint32_t SUCCESS_DISMISS_MS = 1500;

enum class PopupState : uint8_t {
    None = 0,
    Password,
    Connecting,
    Success,
    Failed,
};

struct {
    WiFiResult nets[MAX_NETS];
    int netCount = 0;
    int scrollY = 0;
    bool scanPending = true;

    PopupState popup = PopupState::None;
    char popupSsid[33] = {};
    char password[64] = {};
    int passwordLen = 0;
    bool showPassword = false;
    VirtualKeyboardState kb{};

    uint32_t connectStartMs = 0;
    uint32_t successDismissMs = 0;
    char connectedSsid[33] = {};

    bool dragging = false;
    bool dragMoved = false;
    int lastTouchY = 0;
} s;

void drawBackArrow(TFT_eSPI &tft, int16_t x, int16_t y, uint16_t color) {
    tft.fillTriangle(x, y + 8, x + 10, y, x + 10, y + 16, color);
    tft.drawLine(x + 10, y + 8, x + 22, y + 8, color);
}

void drawRefreshIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, uint16_t color) {
    tft.drawArc(cx, cy, 10, 8, 30, 300, color, COLOR_BG_DARK, false);
    tft.fillTriangle(cx + 8, cy - 10, cx + 12, cy - 4, cx + 4, cy - 6, color);
}

void drawLockIcon(TFT_eSPI &tft, int16_t x, int16_t y, uint16_t color) {
    tft.fillRoundRect(x, y + 6, 10, 8, 2, color);
    tft.drawRoundRect(x + 1, y, 8, 8, 3, color);
}

void drawCheckSmall(TFT_eSPI &tft, int16_t x, int16_t y, uint16_t color) {
    tft.drawLine(x, y + 6, x + 4, y + 10, color);
    tft.drawLine(x + 4, y + 10, x + 12, y, color);
}

void drawEyeIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, bool visible, uint16_t color) {
    if (visible) {
        tft.drawCircle(cx, cy, 6, color);
        tft.fillCircle(cx, cy, 2, color);
        tft.drawLine(cx - 10, cy, cx + 10, cy, color);
    } else {
        tft.drawLine(cx - 8, cy - 6, cx + 8, cy + 6, color);
        tft.drawLine(cx - 8, cy + 6, cx + 8, cy - 6, color);
    }
}

int rssiBarsForSetup(int32_t rssi) {
    if (rssi >= -50) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -75) return 2;
    return 1;
}

void drawSignalBarsSetup(TFT_eSPI &tft, int16_t x, int16_t y, int32_t rssi, uint16_t color) {
    const int bars = rssiBarsForSetup(rssi);
    const int barW = 3;
    const int gap = 2;
    const int baseY = y + 14;
    for (int i = 0; i < 4; i++) {
        const int h = 4 + i * 3;
        const int bx = x + i * (barW + gap);
        const int by = baseY - h;
        if (i < bars) {
            tft.fillRect(bx, by, barW, h, color);
        } else {
            tft.drawRect(bx, by, barW, h, TEXT_MUTED);
        }
    }
}

void beginScan() {
    s.scanPending = true;
    s.netCount = 0;
    startWifiScan();
}

void pollScan() {
    wifiUpdate();
    if (!wifiScanComplete()) return;
    if (s.netCount == 0) {
        s.netCount = getWifiScanResults(s.nets, MAX_NETS);
    }
    s.scanPending = false;
}

void drawHeader(TFT_eSPI &tft) {
    tft.fillRect(0, 0, SCREEN_WIDTH, HDR_H, 0x2104);
    drawBackArrow(tft, 10, 10, TEXT_PRIMARY);
    drawRefreshIcon(tft, SCREEN_WIDTH - 22, 18, TEXT_SECONDARY);

    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, 0x2104);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("WiFi Setup", SCREEN_WIDTH / 2, 10);
    tft.setTextDatum(TL_DATUM);
}

void drawStatusCard(TFT_eSPI &tft) {
    tft.fillRoundRect(8, STATUS_Y + 4, SCREEN_WIDTH - 16, STATUS_H - 8, 6, BG_SECONDARY);

    if (wifiIsConnected() && WiFi.SSID()[0]) {
        strlcpy(s.connectedSsid, WiFi.SSID().c_str(), sizeof(s.connectedSsid));

        char line[48];
        snprintf(line, sizeof(line), "Connected to %s", s.connectedSsid);
        tft.setTextFont(1);
        tft.setTextColor(ACCENT_GREEN, BG_SECONDARY);
        tft.drawString(line, 14, STATUS_Y + 10);

        drawSignalBarsSetup(tft, SCREEN_WIDTH - 52, STATUS_Y + 6, WiFi.RSSI(), ACCENT_GREEN);

        tft.setTextFont(1);
        tft.setTextColor(TEXT_SECONDARY, BG_SECONDARY);
        char ipLine[24];
        snprintf(ipLine, sizeof(ipLine), "IP %s", WiFi.localIP().toString().c_str());
        tft.drawString(ipLine, 14, STATUS_Y + 26);
    } else {
        tft.setTextFont(1);
        tft.setTextColor(TEXT_MUTED, BG_SECONDARY);
        tft.drawString("Not connected", 14, STATUS_Y + 18);
    }
}

int listVisibleHeight() {
    if (s.popup != PopupState::None) {
        return POPUP_Y - LIST_Y;
    }
    return LIST_H;
}

void drawNetworkList(TFT_eSPI &tft) {
    const int visH = listVisibleHeight();
    tft.fillRect(0, LIST_Y, SCREEN_WIDTH, visH, COLOR_BG_DARK);

    if (s.scanPending || wifiScanInProgress()) {
        tft.setTextFont(2);
        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Scanning...", SCREEN_WIDTH / 2, LIST_Y + visH / 2);
        tft.setTextDatum(TL_DATUM);
        return;
    }

    if (s.netCount == 0) {
        tft.setTextFont(2);
        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("No networks", SCREEN_WIDTH / 2, LIST_Y + visH / 2);
        tft.setTextDatum(TL_DATUM);
        return;
    }

    const int maxScroll = max(0, s.netCount * ROW_H - visH);
    if (s.scrollY > maxScroll) s.scrollY = maxScroll;
    if (s.scrollY < 0) s.scrollY = 0;

    const bool curConnected = wifiIsConnected();

    for (int i = 0; i < s.netCount; i++) {
        const int rowY = LIST_Y + i * ROW_H - s.scrollY;
        if (rowY + ROW_H < LIST_Y || rowY >= LIST_Y + visH) continue;

        const WiFiResult &net = s.nets[i];
        const bool isCurrent =
            curConnected && s.connectedSsid[0] && strcmp(net.ssid, s.connectedSsid) == 0;

        tft.fillRoundRect(6, rowY + 2, SCREEN_WIDTH - 12, ROW_H - 4, 4,
                          isCurrent ? 0x2945 : BG_SECONDARY);

        drawSignalBarsSetup(tft, 12, rowY, net.rssi, isCurrent ? ACCENT_BLUE : TEXT_SECONDARY);

        tft.setTextFont(2);
        tft.setTextColor(isCurrent ? ACCENT_BLUE : TEXT_PRIMARY,
                         isCurrent ? 0x2945 : BG_SECONDARY);
        tft.drawString(net.ssid, 40, rowY + 12);

        if (net.encrypted) {
            drawLockIcon(tft, SCREEN_WIDTH - 36, rowY + 10, TEXT_MUTED);
        }
        if (isCurrent) {
            drawCheckSmall(tft, SCREEN_WIDTH - 20, rowY + 14, ACCENT_BLUE);
        }
    }
}

void drawSpinner(TFT_eSPI &tft, int16_t cx, int16_t cy, uint16_t color) {
    const int phase = (int)((millis() / 120) % 12);
    const int a0 = phase * 30;
    tft.drawArc(cx, cy, 14, 11, a0, a0 + 80, color, COLOR_BG_DARK, false);
}

void drawPasswordPopup(TFT_eSPI &tft) {
    tft.fillRect(0, POPUP_Y, SCREEN_WIDTH, POPUP_H, 0x2104);
    tft.drawLine(0, POPUP_Y, SCREEN_WIDTH, POPUP_Y, ACCENT_BLUE);

    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, 0x2104);
    tft.drawString(s.popupSsid, 12, POPUP_Y + 8);

    const int fieldY = POPUP_Y + 30;
    const int fieldH = 24;
    tft.fillRoundRect(10, fieldY, SCREEN_WIDTH - 56, fieldH, 4, BG_SECONDARY);
    tft.setTextFont(1);
    tft.setTextColor(TEXT_PRIMARY, BG_SECONDARY);

    if (s.showPassword) {
        tft.drawString(s.password, 16, fieldY + 7);
    } else {
        char mask[64];
        const int n = min(s.passwordLen, (int)sizeof(mask) - 1);
        memset(mask, '*', (size_t)n);
        mask[n] = '\0';
        tft.drawString(mask, 16, fieldY + 7);
    }

    drawEyeIcon(tft, SCREEN_WIDTH - 28, fieldY + 12, s.showPassword, TEXT_SECONDARY);

    const int kbY = POPUP_Y + 58;
    const bool kbActive = (s.popup == PopupState::Password);
    drawKeyboard(tft, kbY, s.password, (int)sizeof(s.password), kbActive, &s.kb);

    const int btnY = POPUP_Y + POPUP_H - 34;
    gDisplay.drawButton(12, btnY, 88, 28, "Cancel", TEXT_MUTED);
    gDisplay.drawButton(140, btnY, 88, 28, "Connect", ACCENT_BLUE);

    if (s.popup == PopupState::Connecting) {
        tft.fillRect(0, POPUP_Y, SCREEN_WIDTH, POPUP_H, 0x2104);
        drawSpinner(tft, SCREEN_WIDTH / 2, POPUP_Y + 70, ACCENT_BLUE);
        tft.setTextFont(2);
        tft.setTextColor(TEXT_PRIMARY, 0x2104);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Connecting...", SCREEN_WIDTH / 2, POPUP_Y + 100);
        tft.setTextDatum(TL_DATUM);
    } else if (s.popup == PopupState::Success) {
        tft.fillRect(0, POPUP_Y, SCREEN_WIDTH, POPUP_H, 0x2104);
        const int cx = SCREEN_WIDTH / 2;
        const int cy = POPUP_Y + 68;
        tft.drawLine(cx - 14, cy, cx - 4, cy + 12, ACCENT_GREEN);
        tft.drawLine(cx - 4, cy + 12, cx + 16, cy - 10, ACCENT_GREEN);
        tft.drawLine(cx - 13, cy + 1, cx - 3, cy + 13, ACCENT_GREEN);
        tft.drawLine(cx - 3, cy + 13, cx + 17, cy - 9, ACCENT_GREEN);
        tft.setTextFont(2);
        tft.setTextColor(ACCENT_GREEN, 0x2104);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Connected!", SCREEN_WIDTH / 2, POPUP_Y + 100);
        tft.setTextDatum(TL_DATUM);
    } else if (s.popup == PopupState::Failed) {
        tft.fillRect(0, POPUP_Y, SCREEN_WIDTH, POPUP_H, 0x2104);
        tft.drawLine(SCREEN_WIDTH / 2 - 12, POPUP_Y + 52, SCREEN_WIDTH / 2 + 12, POPUP_Y + 76,
                     ACCENT_RED);
        tft.drawLine(SCREEN_WIDTH / 2 + 12, POPUP_Y + 52, SCREEN_WIDTH / 2 - 12, POPUP_Y + 76,
                     ACCENT_RED);
        tft.setTextFont(2);
        tft.setTextColor(ACCENT_RED, 0x2104);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Wrong password", SCREEN_WIDTH / 2, POPUP_Y + 92);
        tft.drawString("or timeout", SCREEN_WIDTH / 2, POPUP_Y + 110);
        tft.setTextDatum(TL_DATUM);
        gDisplay.drawButton(76, POPUP_Y + POPUP_H - 36, 88, 28, "OK", ACCENT_BLUE);
    }
}

int networkRowAt(int x, int y) {
    if (s.popup != PopupState::None && s.popup != PopupState::Password) return -1;
    const int visH = listVisibleHeight();
    if (y < LIST_Y || y >= LIST_Y + visH) return -1;

    const int localY = y - LIST_Y + s.scrollY;
    const int idx = localY / ROW_H;
    if (idx < 0 || idx >= s.netCount) return -1;
    return idx;
}

void openNetworkPopup(int idx) {
    if (idx < 0 || idx >= s.netCount) return;
    strlcpy(s.popupSsid, s.nets[idx].ssid, sizeof(s.popupSsid));
    s.password[0] = '\0';
    s.passwordLen = 0;
    if (strcmp(gSettingsUi.settings.wifiSSID, s.popupSsid) == 0) {
        strlcpy(s.password, gSettingsUi.settings.wifiPassword, sizeof(s.password));
        s.passwordLen = (int)strlen(s.password);
    }
    s.showPassword = false;
    s.kb = VirtualKeyboardState{};
    s.popup = PopupState::Password;
}

void closePopup() {
    s.popup = PopupState::None;
    s.popupSsid[0] = '\0';
}

void saveConnectedSettings() {
    AppSettings &cfg = gSettingsUi.settings;
    strlcpy(cfg.wifiSSID, s.popupSsid, sizeof(cfg.wifiSSID));
    strlcpy(cfg.wifiPassword, s.password, sizeof(cfg.wifiPassword));
    cfg.wifiEnabled = true;
    settingsSave(cfg);
}

void startConnect() {
    connectToNetwork(s.popupSsid, s.password);
    s.popup = PopupState::Connecting;
    s.connectStartMs = millis();
}

}  // namespace

void wifiSetupEnter() {
    memset(&s, 0, sizeof(s));
    if (wifiIsConnected()) {
        strlcpy(s.connectedSsid, WiFi.SSID().c_str(), sizeof(s.connectedSsid));
    }
    beginScan();
}

void wifiSetupExit() {
    closePopup();
}

void wifiSetupDraw() {
    pollScan();

    TFT_eSPI &tft = gDisplay.tft();
    tft.fillScreen(COLOR_BG_DARK);
    drawHeader(tft);
    drawStatusCard(tft);
    drawNetworkList(tft);

    if (s.popup != PopupState::None) {
        drawPasswordPopup(tft);
    }
}

void wifiSetupUpdate() {
    wifiUpdate();
    pollScan();

    if (s.popup == PopupState::Connecting) {
        if (wifiIsConnected()) {
            saveConnectedSettings();
            strlcpy(s.connectedSsid, s.popupSsid, sizeof(s.connectedSsid));
            s.popup = PopupState::Success;
            s.successDismissMs = millis() + SUCCESS_DISMISS_MS;
            wifiSetupDraw();
        } else if (!wifiConnectInProgress() &&
                   millis() - s.connectStartMs >= CONNECT_UI_TIMEOUT_MS) {
            s.popup = PopupState::Failed;
            wifiSetupDraw();
        } else {
            wifiSetupDraw();
        }
        return;
    }

    if (s.popup == PopupState::Success) {
        if (millis() >= s.successDismissMs) {
            closePopup();
            beginScan();
        }
        wifiSetupDraw();
        return;
    }

    if (s.scanPending || wifiScanInProgress() || s.popup == PopupState::Connecting) {
        wifiSetupDraw();
    }
}

void wifiSetupHandleTouchDown(int x, int y) {
    (void)x;
    if (s.popup != PopupState::None) return;
    const int visH = listVisibleHeight();
    if (y >= LIST_Y && y < LIST_Y + visH) {
        s.dragging = true;
        s.dragMoved = false;
        s.lastTouchY = y;
    }
}

void wifiSetupHandleTouchMove(int x, int y) {
    (void)x;
    if (!s.dragging || s.popup != PopupState::None) return;
    const int delta = y - s.lastTouchY;
    if (abs(delta) > 4) {
        s.dragMoved = true;
    }
    s.lastTouchY = y;
    s.scrollY -= delta;
    const int maxScroll = max(0, s.netCount * ROW_H - listVisibleHeight());
    s.scrollY = constrain(s.scrollY, 0, maxScroll);
    wifiSetupDraw();
}

void wifiSetupHandleTouchUp(int x, int y) {
    if (!s.dragging) return;
    s.dragging = false;
    if (!s.dragMoved && s.popup == PopupState::None) {
        TouchPoint tp{x, y, true};
        wifiSetupHandleTouch(tp);
    }
}

bool wifiSetupHandleTouch(const TouchPoint &tp) {
    if (!tp.pressed) return false;

    if (isTouchInRect(tp, 0, 0, 44, HDR_H)) {
        closePopup();
        adminRequestAccess(STATE_SETTINGS);
        return true;
    }

    if (isTouchInRect(tp, SCREEN_WIDTH - 44, 0, 44, HDR_H)) {
        beginScan();
        wifiSetupDraw();
        return true;
    }

    if (s.popup == PopupState::Failed) {
        if (isTouchInRect(tp, 76, POPUP_Y + POPUP_H - 36, 88, 28)) {
            s.popup = PopupState::Password;
            wifiSetupDraw();
            return true;
        }
        if (isTouchInRect(tp, 12, POPUP_Y + POPUP_H - 34, 88, 28)) {
            closePopup();
            wifiSetupDraw();
            return true;
        }
        return true;
    }

    if (s.popup == PopupState::Connecting || s.popup == PopupState::Success) {
        return true;
    }

    if (s.popup == PopupState::Password) {
        const int fieldY = POPUP_Y + 30;
        if (isTouchInRect(tp, SCREEN_WIDTH - 44, fieldY, 40, 24)) {
            s.showPassword = !s.showPassword;
            wifiSetupDraw();
            return true;
        }

        const int kbY = POPUP_Y + 58;
        if (handleKeyboardTouch(tp, kbY, s.password, s.passwordLen, (int)sizeof(s.password),
                                &s.kb)) {
            wifiSetupDraw();
            return true;
        }

        const int btnY = POPUP_Y + POPUP_H - 34;
        if (isTouchInRect(tp, 12, btnY, 88, 28)) {
            closePopup();
            wifiSetupDraw();
            return true;
        }
        if (isTouchInRect(tp, 140, btnY, 88, 28)) {
            startConnect();
            wifiSetupDraw();
            return true;
        }

        return true;
    }

    const int row = networkRowAt(tp.x, tp.y);
    if (row >= 0) {
        openNetworkPopup(row);
        wifiSetupDraw();
        return true;
    }

    return false;
}
