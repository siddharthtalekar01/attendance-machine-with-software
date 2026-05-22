#include "ui_screens.h"
#include <TimeLib.h>
#include <cstring>

UiScreens gUi;

namespace {

constexpr int PULSE_SPRITE_W = 88;
constexpr int PULSE_SPRITE_H = 88;
constexpr int ICON_CX = 120;
constexpr int ICON_CY = 88;

uint16_t blend565(uint16_t fg, uint16_t bg, float t) {
    t = constrain(t, 0.0f, 1.0f);
    const uint8_t fr = (fg >> 11) & 0x1F;
    const uint8_t fg_g = (fg >> 5) & 0x3F;
    const uint8_t fb = fg & 0x1F;
    const uint8_t br = (bg >> 11) & 0x1F;
    const uint8_t bg_g = (bg >> 5) & 0x3F;
    const uint8_t bb = bg & 0x1F;
    const uint8_t r = fr + (uint8_t)((br - fr) * t);
    const uint8_t g = fg_g + (uint8_t)((bg_g - fg_g) * t);
    const uint8_t b = fb + (uint8_t)((bb - fb) * t);
    return (r << 11) | (g << 5) | b;
}

void drawWifiIcon(TFT_eSPI &tft, int16_t x, int16_t y, bool connected, uint16_t color) {
    if (!connected) {
        tft.drawLine(x, y + 8, x + 14, y, color);
        tft.drawLine(x + 14, y, x + 28, y + 8, color);
        return;
    }
    tft.fillCircle(x + 14, y + 10, 2, color);
    tft.drawArc(x + 14, y + 10, 12, 10, 130, 230, color, COLOR_BG_DARK, false);
    tft.drawArc(x + 14, y + 10, 8, 6, 130, 230, color, COLOR_BG_DARK, false);
    tft.drawArc(x + 14, y + 10, 4, 2, 130, 230, color, COLOR_BG_DARK, false);
}

void drawFingerprintIcon(TFT_eSPI &tft, int16_t cx, int16_t cy) {
    const uint16_t col = TEXT_PRIMARY;
    for (int i = 0; i < 4; i++) {
        const int r = 10 + i * 7;
        const int ir = r - 3;
        tft.drawArc(cx, cy, r, ir, 200, 320, col, COLOR_BG_DARK, false);
        tft.drawArc(cx, cy, r, ir, 20, 140, col, COLOR_BG_DARK, false);
    }
    tft.fillCircle(cx, cy + 14, 4, col);
}

void drawNavIcon(TFT_eSPI &tft, int16_t cx, int16_t cy, int tab) {
    const uint16_t c = TEXT_SECONDARY;
    switch (tab) {
        case 0:
            tft.drawCircle(cx, cy - 2, 5, c);
            tft.drawLine(cx - 6, cy + 8, cx + 6, cy + 8, c);
            tft.drawLine(cx - 4, cy + 2, cx + 4, cy + 2, c);
            break;
        case 1:
            for (int i = 0; i < 3; i++) {
                tft.drawLine(cx - 7, cy - 4 + i * 5, cx + 7, cy - 4 + i * 5, c);
            }
            break;
        default:
            tft.drawCircle(cx, cy, 6, c);
            tft.fillCircle(cx, cy, 2, c);
            tft.drawLine(cx, cy - 9, cx, cy - 3, c);
            tft.drawLine(cx, cy + 3, cx, cy + 9, c);
            tft.drawLine(cx - 9, cy, cx - 3, cy, c);
            tft.drawLine(cx + 3, cy, cx + 9, cy, c);
            break;
    }
}

}  // namespace

void UiScreens::begin() {
    _splashStart = millis();
    _pulseSpriteReady = false;
    _lastClockStr[0] = '\0';
    _lastDateStr[0] = '\0';
    setScreen(AppScreen::Splash);
}

void UiScreens::setScreen(AppScreen screen) {
    touchQueueClear();
    _screen = screen;

    switch (screen) {
        case AppScreen::Splash:
            drawSplash();
            break;
        case AppScreen::Home:
            _activeNavTab = -1;
            drawHomeScreen();
            break;
        case AppScreen::Scan:
            drawScan();
            break;
        case AppScreen::Enroll:
            _activeNavTab = 0;
            drawEnroll();
            break;
        case AppScreen::Records:
            _activeNavTab = 1;
            drawRecords();
            break;
        case AppScreen::Settings:
            _activeNavTab = 2;
            drawSettings();
            break;
        default:
            drawHomeScreen();
            break;
    }
}

bool UiScreens::hitButton(int16_t tx, int16_t ty, int16_t x, int16_t y, int16_t w, int16_t h) {
    TouchPoint tp{tx, ty, true};
    return isTouchInRect(tp, x, y, w, h);
}

void UiScreens::drawSplash() {
    gDisplay.fillScreen(COLOR_BG_DARK);
    gDisplay.drawCenteredText(120, APP_NAME, 4, ACCENT_BLUE);
    gDisplay.drawCenteredText(160, APP_VERSION, 2, TEXT_MUTED);
}

void UiScreens::drawHomeStatusCard() {
    TFT_eSPI &tft = gDisplay.tft();
    const int y = HOME_STATUS_Y;
    tft.fillRect(8, y, SCREEN_WIDTH - 16, HOME_STATUS_H, COLOR_BG_DARK);

    LastScanInfo last;
    if (!gStorage.getLastScanToday(last) || !last.found) {
        tft.setTextFont(2);
        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("No records today", SCREEN_WIDTH / 2, y + HOME_STATUS_H / 2);
        tft.setTextDatum(TL_DATUM);
        return;
    }

    tft.fillRoundRect(10, y + 4, SCREEN_WIDTH - 20, HOME_STATUS_H - 8, 6, 0x2104);

    tft.setTextFont(2);
    tft.setTextColor(ACCENT_GREEN, 0x2104);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(last.name, 18, y + 12);

    tft.setTextFont(1);
    tft.setTextColor(TEXT_SECONDARY, 0x2104);
    tft.drawString(last.timeStr, 18, y + 34);

    const char *badge = last.checkIn ? "CHECK-IN" : "CHECK-OUT";
    const uint16_t badgeCol = last.checkIn ? ACCENT_GREEN : 0xFC60;
    const int bw = last.checkIn ? 62 : 72;
    tft.fillRoundRect(SCREEN_WIDTH - bw - 14, y + 14, bw, 18, 4, badgeCol);
    tft.setTextColor(COLOR_BG_DARK, badgeCol);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(badge, SCREEN_WIDTH - bw / 2 - 14, y + 23);
    tft.setTextDatum(TL_DATUM);
}

void UiScreens::drawHomeBottomNav() {
    TFT_eSPI &tft = gDisplay.tft();
    const int y = HOME_NAV_Y;
    const int tabW = SCREEN_WIDTH / 3;

    tft.fillRect(0, y, SCREEN_WIDTH, HOME_NAV_H, 0x2104);
    tft.drawLine(0, y, SCREEN_WIDTH, y, TEXT_MUTED);

    const char *labels[] = {"Enroll", "Records", "Settings"};
    for (int i = 0; i < 3; i++) {
        const int tx = i * tabW;
        const bool active = (_activeNavTab == i);

        if (active) {
            tft.fillCircle(tx + tabW / 2, y + 6, 3, ACCENT_BLUE);
        }

        const bool highlight = active;
        if (highlight) {
            tft.fillRoundRect(tx + 6, y + 12, tabW - 12, HOME_NAV_H - 20, 6, ACCENT_BLUE);
        }

        const int cx = tx + tabW / 2;
        const int cy = y + 36;
        if (highlight) {
            drawNavIcon(tft, cx, cy, i);
            tft.setTextColor(TEXT_PRIMARY, ACCENT_BLUE);
        } else {
            drawNavIcon(tft, cx, cy, i);
            tft.setTextFont(1);
            tft.setTextColor(TEXT_SECONDARY, 0x2104);
        }

        tft.setTextFont(1);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(labels[i], cx, y + HOME_NAV_H - 22);
        tft.setTextDatum(TL_DATUM);
    }
}

void UiScreens::drawHomeScreen() {
    TFT_eSPI &tft = gDisplay.tft();
    tft.fillScreen(COLOR_BG_DARK);

    tft.fillRect(0, 0, SCREEN_WIDTH, HOME_TOP_H, 0x2104);
    _lastClockStr[0] = '\0';
    _lastDateStr[0] = '\0';
    updateHomeClock();

    const int heroY = HOME_HERO_Y;
    tft.fillRect(0, heroY, SCREEN_WIDTH, HOME_HERO_H, COLOR_BG_DARK);

    drawFingerprintIcon(tft, ICON_CX, ICON_CY);

    tft.setTextFont(4);
    tft.setTextColor(TEXT_PRIMARY, COLOR_BG_DARK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Place Finger", ICON_CX, ICON_CY + 48);

    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
    tft.drawString("Scan to mark attendance", ICON_CX, ICON_CY + 72);
    tft.setTextDatum(TL_DATUM);

    drawHomeStatusCard();
    drawHomeBottomNav();

    if (!_pulseSpriteReady) {
        _pulseSprite.setColorDepth(16);
        _pulseSpriteReady = _pulseSprite.createSprite(PULSE_SPRITE_W, PULSE_SPRITE_H);
    }
    _lastPulseMs = 0;
    updateHomePulse();
}

void UiScreens::updateHomeClock() {
    if (_screen != AppScreen::Home) return;

    const String hhmm = gWiFi.formattedTimeHHMM();
    const String dateStr = gWiFi.formattedDate();

    if (hhmm.equals(_lastClockStr) && dateStr.equals(_lastDateStr)) return;

    hhmm.toCharArray(_lastClockStr, sizeof(_lastClockStr));
    dateStr.toCharArray(_lastDateStr, sizeof(_lastDateStr));

    TFT_eSPI &tft = gDisplay.tft();

    tft.fillRect(0, 0, SCREEN_WIDTH, HOME_TOP_H, 0x2104);

    tft.setTextFont(1);
    tft.setTextSize(2);
    tft.setTextColor(TEXT_SECONDARY, 0x2104);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(_lastClockStr, 6, HOME_TOP_H / 2);

    drawWifiIcon(tft, SCREEN_WIDTH - 118, 4, gWiFi.isConnected(), TEXT_SECONDARY);

    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(_lastDateStr, SCREEN_WIDTH - 6, HOME_TOP_H / 2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
}

void UiScreens::updateHomePulse() {
    if (_screen != AppScreen::Home || !_pulseSpriteReady) return;

    const uint32_t now = millis();
    if (now - _lastPulseMs < HOME_PULSE_MS) return;
    _lastPulseMs = now;

    const float phase = (now % 1800) / 1800.0f;
    const int radius = 22 + (int)(phase * 18);
    const float fade = 1.0f - phase;

    _pulseSprite.fillSprite(COLOR_BG_DARK);

    const int cx = PULSE_SPRITE_W / 2;
    const int cy = PULSE_SPRITE_H / 2;
    const uint16_t ringColor = blend565(ACCENT_BLUE, COLOR_BG_DARK, fade);

    if (radius > 20) {
        _pulseSprite.drawCircle(cx, cy, radius, ringColor);
        _pulseSprite.drawCircle(cx, cy, radius - 1, ringColor);
    }

    const int16_t sx = ICON_CX - PULSE_SPRITE_W / 2;
    const int16_t sy = ICON_CY - PULSE_SPRITE_H / 2;
    _pulseSprite.pushSprite(sx, sy);
}

int UiScreens::homeNavTabAt(int16_t x, int16_t y) const {
    if (y < HOME_NAV_Y || y >= SCREEN_HEIGHT) return -1;
    const int tabW = SCREEN_WIDTH / 3;
    const int idx = x / tabW;
    if (idx < 0 || idx > 2) return -1;
    return idx;
}

void UiScreens::handleHomeTouch(const TouchPoint &tp) {
    if (isTouchInRect(tp, 30, HOME_HERO_Y + 10, 180, HOME_HERO_H - 20)) {
        setScreen(AppScreen::Scan);
        return;
    }

    const int tab = homeNavTabAt(tp.x, tp.y);
    if (tab == 0) setScreen(AppScreen::Enroll);
    else if (tab == 1) setScreen(AppScreen::Records);
    else if (tab == 2) setScreen(AppScreen::Settings);
}

void UiScreens::drawScan() {
    gDisplay.fillScreen(COLOR_BG_DARK);
    gDisplay.drawHeader("Scan");
    gDisplay.drawCenteredText(140, "Place finger on sensor", 2, TEXT_PRIMARY);
    gDisplay.drawButton(20, 260, 90, 40, "Back", 0x4208);
}

void UiScreens::drawEnroll() {
    gDisplay.fillScreen(COLOR_BG_DARK);
    gDisplay.drawHeader("Enroll");
    gDisplay.drawCenteredText(120, "Assign slot in admin", 2, TEXT_MUTED);
    gDisplay.drawCenteredText(150, "Use serial/API next", 2, TEXT_PRIMARY);
    drawHomeBottomNav();
}

void UiScreens::drawRecords() {
    gDisplay.fillScreen(COLOR_BG_DARK);
    gDisplay.drawHeader("Records");
    gDisplay.drawCenteredText(140, "Today's log", 2, TEXT_MUTED);
    gDisplay.drawCenteredText(170, "(coming soon)", 2, TEXT_SECONDARY);
    drawHomeBottomNav();
}

void UiScreens::drawSettings() {
    gDisplay.fillScreen(COLOR_BG_DARK);
    gDisplay.drawHeader("Settings");
    gDisplay.drawCenteredText(100, "WiFi: secrets.h", 2, TEXT_MUTED);
    gDisplay.drawButton(20, 140, 200, 40, "Calibrate Touch", 0x4A69);
    drawHomeBottomNav();
}

void UiScreens::loop() {
    touchUpdate();

    if (_screen == AppScreen::Splash) {
        if (millis() - _splashStart > 2000) setScreen(AppScreen::Home);
        return;
    }

    if (_screen == AppScreen::Home) {
        if (millis() - _lastClockMs >= HOME_CLOCK_MS) {
            _lastClockMs = millis();
            updateHomeClock();
        }
        updateHomePulse();
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
            } else {
                const int tab = homeNavTabAt(tp.x, tp.y);
                if (tab == 0) setScreen(AppScreen::Enroll);
                else if (tab == 1) setScreen(AppScreen::Records);
                else if (tab == 2) { /* already settings */ }
                else setScreen(AppScreen::Home);
            }
            continue;
        }

        if (_screen == AppScreen::Enroll || _screen == AppScreen::Records) {
            const int tab = homeNavTabAt(tp.x, tp.y);
            if (tab == 0) setScreen(AppScreen::Enroll);
            else if (tab == 1) setScreen(AppScreen::Records);
            else if (tab == 2) setScreen(AppScreen::Settings);
            else setScreen(AppScreen::Home);
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
