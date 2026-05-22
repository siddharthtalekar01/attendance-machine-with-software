#include "ui_screens.h"
#include "records_ui.h"
#include "settings_ui.h"
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

void drawProgressDots(TFT_eSPI &tft, int16_t cx, int16_t y, int filled, int total, uint16_t activeColor) {
    const int spacing = 18;
    const int startX = cx - ((total - 1) * spacing) / 2;
    for (int i = 0; i < total; i++) {
        const int x = startX + i * spacing;
        if (i < filled) {
            tft.fillCircle(x, y, 5, activeColor);
        } else {
            tft.drawCircle(x, y, 5, TEXT_MUTED);
        }
    }
}

void drawBigCheck(TFT_eSPI &tft, int16_t cx, int16_t cy, uint16_t color) {
    tft.drawLine(cx - 18, cy, cx - 4, cy + 16, color);
    tft.drawLine(cx - 4, cy + 16, cx + 22, cy - 14, color);
    tft.drawLine(cx - 18, cy + 1, cx - 4, cy + 17, color);
    tft.drawLine(cx - 4, cy + 17, cx + 22, cy - 13, color);
}

void drawBigX(TFT_eSPI &tft, int16_t cx, int16_t cy, uint16_t color) {
    tft.drawLine(cx - 16, cy - 16, cx + 16, cy + 16, color);
    tft.drawLine(cx + 16, cy - 16, cx - 16, cy + 16, color);
    tft.drawLine(cx - 15, cy - 16, cx + 17, cy + 16, color);
    tft.drawLine(cx + 17, cy - 16, cx - 15, cy + 16, color);
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
            beginEnrollWizard();
            drawEnroll();
            break;
        case AppScreen::Records:
            _activeNavTab = 1;
            drawRecords();
            break;
        case AppScreen::Settings:
            _activeNavTab = 2;
            _onUserList = false;
            drawSettings();
            break;
        case AppScreen::UserList:
            _onUserList = true;
            drawUsersListScreen();
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

const char *UiScreens::enrollDepartmentName(int idx) {
    static const char *depts[] = {"HR", "Engineering", "Sales", "Operations", "Management"};
    if (idx < 0 || idx >= ENROLL_DEPT_COUNT) return depts[0];
    return depts[idx];
}

void drawEnrollScreen(int step, const char *status, int progress, uint16_t statusColor) {
    TFT_eSPI &tft = gDisplay.tft();
    tft.fillScreen(COLOR_BG_DARK);

    tft.fillRect(0, 0, SCREEN_WIDTH, 28, 0x2104);
    tft.setTextFont(1);
    tft.setTextColor(TEXT_SECONDARY, 0x2104);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("< Back", 8, 8);

    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, 0x2104);
    tft.setTextDatum(TC_DATUM);
    const char *titles[] = {"", "New User", "First Scan", "Confirm Scan", "Result"};
    if (step >= 1 && step <= 4) {
        tft.drawString(titles[step], SCREEN_WIDTH / 2, 10);
    }
    tft.setTextDatum(TL_DATUM);

    if (step == 1) {
        const int labelX = 14;
        tft.setTextFont(1);
        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.drawString("Name", labelX, 38);

        tft.fillRoundRect(12, 52, SCREEN_WIDTH - 24, 28, 4, 0x2104);
        tft.setTextColor(TEXT_PRIMARY, 0x2104);
        tft.drawString(gUi._enrollName, 20, 60);

        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.drawString("ID", labelX, 88);
        tft.fillRoundRect(12, 102, SCREEN_WIDTH - 24, 32, 4, 0x2104);
        tft.setTextColor(ACCENT_GREEN, 0x2104);
        tft.setTextDatum(MC_DATUM);
        char idBuf[8];
        snprintf(idBuf, sizeof(idBuf), "%u", gUi._enrollId);
        tft.drawString(idBuf, SCREEN_WIDTH / 2, 118);
        tft.fillRoundRect(20, 108, 36, 24, 4, ACCENT_BLUE);
        tft.setTextColor(TEXT_PRIMARY, ACCENT_BLUE);
        tft.drawString("-", 38, 118);
        tft.fillRoundRect(SCREEN_WIDTH - 56, 108, 36, 24, 4, ACCENT_BLUE);
        tft.drawString("+", SCREEN_WIDTH - 38, 118);
        tft.setTextDatum(TL_DATUM);

        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.drawString("Department", labelX, 142);
        const int listY = 158;
        const int rowH = 22;
        const int visible = 3;
        tft.fillRoundRect(12, listY, SCREEN_WIDTH - 24, rowH * visible + 8, 4, 0x2104);
        for (int row = 0; row < visible; row++) {
            const int idx = gUi._enrollDeptScroll + row;
            if (idx >= ENROLL_DEPT_COUNT) break;
            const int ry = listY + 4 + row * rowH;
            const bool sel = (idx == gUi._enrollDeptIdx);
            if (sel) {
                tft.fillRoundRect(16, ry, SCREEN_WIDTH - 32, rowH - 2, 3, ACCENT_BLUE);
            }
            tft.setTextColor(sel ? TEXT_PRIMARY : TEXT_SECONDARY, sel ? ACCENT_BLUE : 0x2104);
            tft.drawString(UiScreens::enrollDepartmentName(idx), 22, ry + 4);
        }
        tft.fillTriangle(220, listY + 8, 228, listY + 8, 224, listY, TEXT_SECONDARY);
        tft.fillTriangle(220, listY + visible * rowH, 228, listY + visible * rowH, 224, listY + visible * rowH + 6,
                         TEXT_SECONDARY);

        gDisplay.drawButton(20, 272, SCREEN_WIDTH - 40, 38, "Next ->", ACCENT_BLUE);
        return;
    }

    if (step == 2 || step == 3) {
        const int iconCy = 105;
        drawFingerprintIcon(tft, ICON_CX, iconCy);

        tft.setTextFont(2);
        tft.setTextColor(statusColor, COLOR_BG_DARK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(status ? status : "", ICON_CX, iconCy + 52);
        tft.setTextFont(1);
        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        if (step == 3) {
            tft.drawString("Second impression", ICON_CX, iconCy + 72);
        }
        tft.setTextDatum(TL_DATUM);

        drawProgressDots(tft, SCREEN_WIDTH / 2, 292, progress, 4, statusColor);
        return;
    }

    if (step == 4) {
        const bool ok = gUi._enrollSuccess;
        const uint16_t iconCol = ok ? STATUS_GREEN : STATUS_RED;
        if (ok) {
            drawBigCheck(tft, SCREEN_WIDTH / 2, 88, iconCol);
        } else {
            drawBigX(tft, SCREEN_WIDTH / 2, 88, iconCol);
        }

        tft.setTextFont(2);
        tft.setTextColor(statusColor, COLOR_BG_DARK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(status ? status : (ok ? "Enrolled!" : "Failed"), SCREEN_WIDTH / 2, 118);

        tft.fillRoundRect(16, 140, SCREEN_WIDTH - 32, 72, 6, 0x2104);
        tft.setTextFont(2);
        tft.setTextColor(ACCENT_GREEN, 0x2104);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(gUi._enrollName, 24, 152);
        tft.setTextFont(1);
        tft.setTextColor(TEXT_SECONDARY, 0x2104);
        char line[48];
        snprintf(line, sizeof(line), "ID: %u  |  %s", gUi._enrollId,
                 UiScreens::enrollDepartmentName(gUi._enrollDeptIdx));
        tft.drawString(line, 24, 176);
        tft.setTextDatum(TC_DATUM);

        gDisplay.drawButton(14, 228, 100, 36, "Enroll Another", ACCENT_BLUE);
        gDisplay.drawButton(126, 228, 100, 36, "Done", 0x4208);
        tft.setTextDatum(TL_DATUM);
    }
}

void UiScreens::beginEnrollWizard() {
    _enrollWizardStep = 1;
    _enrollDeptIdx = 0;
    _enrollDeptScroll = 0;
    _enrollNamePreset = 0;
    _enrollPollActive = false;
    _enrollSuccess = false;
    _enrollProgress = 0;
    strlcpy(_enrollStatus, "Enter user details", sizeof(_enrollStatus));
    _enrollStatusColor = STATUS_AMBER;
    _enrollId = fingerprintGetNextAvailableId();

#if ENROLL_KIOSK_MODE
    strlcpy(_enrollName, KIOSK_DEFAULT_NAME, sizeof(_enrollName));
#else
    _enrollName[0] = '\0';
#endif
}

void UiScreens::refreshEnrollUi() {
    drawEnrollScreen(_enrollWizardStep, _enrollStatus, _enrollProgress, _enrollStatusColor);
    if (_enrollWizardStep == 2 || _enrollWizardStep == 3) {
        updateEnrollPulse();
    }
}

void UiScreens::updateEnrollPulse() {
    if ((_enrollWizardStep != 2 && _enrollWizardStep != 3) || !_enrollSpriteReady) return;

    const uint32_t now = millis();
    if (now - _lastEnrollPulseMs < HOME_PULSE_MS) return;
    _lastEnrollPulseMs = now;

    const float phase = (now % 1600) / 1600.0f;
    const int radius = 20 + (int)(phase * 20);
    const float fade = 1.0f - phase;
    const uint16_t ringColor = blend565(_enrollStatusColor, COLOR_BG_DARK, fade);

    _enrollSprite.fillSprite(COLOR_BG_DARK);
    const int cx = PULSE_SPRITE_W / 2;
    const int cy = PULSE_SPRITE_H / 2;
    if (radius > 18) {
        _enrollSprite.drawCircle(cx, cy, radius, ringColor);
        _enrollSprite.drawCircle(cx, cy, radius - 1, ringColor);
    }
    const int iconCy = 105;
    _enrollSprite.pushSprite(ICON_CX - PULSE_SPRITE_W / 2, iconCy - PULSE_SPRITE_H / 2);
}

void UiScreens::tickEnrollFingerprint() {
    if (!_enrollPollActive) return;

    const int r = fingerprintEnrollPoll(_enrollCtx);
    strlcpy(_enrollStatus, fingerprintEnrollPhaseText(_enrollCtx), sizeof(_enrollStatus));
    _enrollProgress = fingerprintEnrollDotProgress(_enrollCtx);

    if (r == 0) {
        if (_enrollCtx.phase == FpEnrollPhase::WaitFinger2 || _enrollCtx.phase == FpEnrollPhase::Capture2) {
            if (_enrollWizardStep != 3) {
                _enrollWizardStep = 3;
                refreshEnrollUi();
            }
        } else if (_enrollWizardStep == 2 || _enrollWizardStep == 3) {
            _enrollStatusColor = (_enrollCtx.phase == FpEnrollPhase::WaitRemove) ? STATUS_GREEN : STATUS_AMBER;
            updateEnrollPulse();
            TFT_eSPI &tft = gDisplay.tft();
            tft.fillRect(0, 150, SCREEN_WIDTH, 30, COLOR_BG_DARK);
            tft.setTextFont(2);
            tft.setTextColor(_enrollStatusColor, COLOR_BG_DARK);
            tft.setTextDatum(TC_DATUM);
            tft.drawString(_enrollStatus, ICON_CX, 157);
            tft.setTextDatum(TL_DATUM);
            drawProgressDots(tft, SCREEN_WIDTH / 2, 292, _enrollProgress, 4, _enrollStatusColor);
        }
        return;
    }

    _enrollPollActive = false;
    _enrollWizardStep = 4;
    _enrollSuccess = (r == FP_ENROLL_OK);

    if (_enrollSuccess) {
        _enrollStatusColor = STATUS_GREEN;
        strlcpy(_enrollStatus, "Success!", sizeof(_enrollStatus));
        UserRecord user;
        user.fingerId = static_cast<uint8_t>(_enrollId);
        strlcpy(user.name, _enrollName, sizeof(user.name));
        strlcpy(user.department, enrollDepartmentName(_enrollDeptIdx), sizeof(user.department));
        gStorage.upsertUser(user);
    } else {
        _enrollStatusColor = STATUS_RED;
        snprintf(_enrollStatus, sizeof(_enrollStatus), "%s", fingerprintErrorString(r));
        fingerprintDeleteId(_enrollId);
    }
    refreshEnrollUi();
}

void UiScreens::drawEnroll() {
    if (!_enrollSpriteReady) {
        _enrollSprite.setColorDepth(16);
        _enrollSpriteReady = _enrollSprite.createSprite(PULSE_SPRITE_W, PULSE_SPRITE_H);
    }
    _lastEnrollPulseMs = 0;
    refreshEnrollUi();
}

void UiScreens::handleEnrollTouch(const TouchPoint &tp) {
    if (_enrollWizardStep == 1) {
        if (isTouchInRect(tp, 0, 0, 70, 28)) {
            setScreen(AppScreen::Home);
            return;
        }
        if (isTouchInRect(tp, 20, 108, 36, 24)) {
            if (_enrollId > 1) _enrollId--;
            refreshEnrollUi();
            return;
        }
        if (isTouchInRect(tp, SCREEN_WIDTH - 56, 108, 36, 24)) {
            if (_enrollId < MAX_ENROLLED_FINGERS) _enrollId++;
            refreshEnrollUi();
            return;
        }
        if (isTouchInRect(tp, 12, 52, SCREEN_WIDTH - 24, 28)) {
#if ENROLL_KIOSK_MODE
            static const char *presets[] = {KIOSK_DEFAULT_NAME, "Visitor", "Staff", "Contractor"};
            _enrollNamePreset = (_enrollNamePreset + 1) % 4;
            strlcpy(_enrollName, presets[_enrollNamePreset], sizeof(_enrollName));
            refreshEnrollUi();
#endif
            return;
        }
        const int listY = 158;
        const int rowH = 22;
        if (isTouchInRect(tp, 12, listY, SCREEN_WIDTH - 24, rowH * 3 + 8)) {
            const int row = (tp.y - listY - 4) / rowH;
            const int idx = _enrollDeptScroll + row;
            if (idx >= 0 && idx < ENROLL_DEPT_COUNT) {
                _enrollDeptIdx = idx;
                refreshEnrollUi();
            }
            return;
        }
        if (isTouchInRect(tp, 210, listY, 28, 12) && _enrollDeptScroll > 0) {
            _enrollDeptScroll--;
            refreshEnrollUi();
            return;
        }
        if (isTouchInRect(tp, 210, listY + rowH * 3, 28, 12) &&
            _enrollDeptScroll < ENROLL_DEPT_COUNT - 3) {
            _enrollDeptScroll++;
            refreshEnrollUi();
            return;
        }
        if (isTouchInRect(tp, 20, 272, SCREEN_WIDTH - 40, 38)) {
            _enrollWizardStep = 2;
            _enrollStatusColor = STATUS_AMBER;
            _enrollProgress = 0;
            strlcpy(_enrollStatus, "Place finger firmly", sizeof(_enrollStatus));
            fingerprintEnrollBegin(_enrollCtx, _enrollId);
            _enrollPollActive = true;
            refreshEnrollUi();
        }
        return;
    }

    if (_enrollWizardStep == 4) {
        if (isTouchInRect(tp, 14, 228, 100, 36)) {
            beginEnrollWizard();
            drawEnroll();
            return;
        }
        if (isTouchInRect(tp, 126, 228, 100, 36)) {
            setScreen(AppScreen::Home);
        }
        return;
    }
}

void UiScreens::drawRecords() {
    _activeNavTab = 1;
    gRecordsUi.scrollY = 0;
    gRecordsUi.velocity = 0;
    gRecordsUi.expandedIndex = -1;
    gRecordsUi.dayAnchor = now();
    drawRecordsScreen(RecordFilter::Today);
}

void UiScreens::handleRecordsTouch(const TouchPoint &tp) {
    if (recordsHandleChromeTap(tp.x, tp.y)) {
        if (isTouchInRect(tp, 0, 0, 40, REC_HEADER_H)) {
            setScreen(AppScreen::Home);
        }
        return;
    }

    if (isTouchInRect(tp, 0, REC_LIST_Y, SCREEN_WIDTH, REC_LIST_H)) {
        recordsHandleTouchDown(tp.x, tp.y);
        _recordsDragging = true;
        _recordsLastX = tp.x;
        _recordsLastY = tp.y;
    }
}

void UiScreens::drawSettings() {
    _activeNavTab = 2;
    settingsUiInit();
    drawSettingsScreen();
}

void UiScreens::handleSettingsTouch(const TouchPoint &tp) {
    if (_onUserList) {
        if (isTouchInRect(tp, 0, 0, 44, 36) || isTouchInRect(tp, 20, 280, 90, 32)) {
            _onUserList = false;
            setScreen(AppScreen::Settings);
        }
        return;
    }

    if (gSettingsUi.dialog != SettingsUiState::Dialog::None) {
        settingsHandleDialogTap(tp.x, tp.y);
        return;
    }

    if (settingsHandleTap(tp.x, tp.y)) {
        if (isTouchInRect(tp, 0, 0, 44, SETTINGS_HEADER_H)) {
            setScreen(AppScreen::Home);
        }
        return;
    }

    if (isTouchInRect(tp, 0, SETTINGS_LIST_Y, SCREEN_WIDTH, SETTINGS_LIST_H)) {
        settingsHandleTouchDown(tp.x, tp.y);
        _settingsDragging = true;
        _settingsLastX = tp.x;
        _settingsLastY = tp.y;
    }
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

    if (_screen == AppScreen::Enroll) {
        tickEnrollFingerprint();
        if (_enrollWizardStep == 2 || _enrollWizardStep == 3) {
            updateEnrollPulse();
        }
    }

    if (_screen == AppScreen::Settings && !_onUserList &&
        gSettingsUi.dialog == SettingsUiState::Dialog::None) {
        settingsTickInertia();
        TouchPoint held;
        if (touchReadHeld(held)) {
            if (_settingsDragging) {
                settingsHandleTouchMove(held.x, held.y);
            } else if (isTouchInRect(held, 0, SETTINGS_LIST_Y, SCREEN_WIDTH, SETTINGS_LIST_H)) {
                settingsHandleTouchDown(held.x, held.y);
                _settingsDragging = true;
            }
            _settingsLastX = held.x;
            _settingsLastY = held.y;
        } else if (_settingsDragging) {
            settingsHandleTouchUp(_settingsLastX, _settingsLastY);
            _settingsDragging = false;
        }
    }

    if (_screen == AppScreen::Records) {
        recordsTickInertia();

        TouchPoint held;
        if (touchReadHeld(held)) {
            if (_recordsDragging) {
                recordsHandleTouchMove(held.x, held.y);
            } else if (isTouchInRect(held, 0, REC_LIST_Y, SCREEN_WIDTH, REC_LIST_H)) {
                recordsHandleTouchDown(held.x, held.y);
                _recordsDragging = true;
            }
            _recordsLastX = held.x;
            _recordsLastY = held.y;
        } else if (_recordsDragging) {
            recordsHandleTouchUp(_recordsLastX, _recordsLastY);
            _recordsDragging = false;
        }
    }

    TouchPoint tp;
    while (touchEventPop(tp)) {
        if (!tp.pressed) continue;

        if (_screen == AppScreen::Home) {
            handleHomeTouch(tp);
            continue;
        }

        if (_screen == AppScreen::Enroll) {
            if (isTouchInRect(tp, 0, 0, 70, 28) && _enrollWizardStep != 1) {
                setScreen(AppScreen::Home);
                _enrollPollActive = false;
                continue;
            }
            handleEnrollTouch(tp);
            continue;
        }

        if (_screen == AppScreen::Settings || _screen == AppScreen::UserList) {
            handleSettingsTouch(tp);
            continue;
        }

        if (_screen == AppScreen::Records) {
            handleRecordsTouch(tp);
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
