#include "users_ui.h"

#include <TimeLib.h>
#include <ctype.h>
#include <cstring>

#include "display.h"
#include "fingerprint.h"
#include "ui_screens.h"

UsersUiState gUsersUi;

namespace {

constexpr int LIST_W = SCREEN_WIDTH;
constexpr int KB_H = 96;

int listAreaHeight() {
    return gUsersUi.kbVisible ? (USR_LIST_H - KB_H) : USR_LIST_H;
}

int listAreaY() { return USR_LIST_Y; }

void drawBackArrow(TFT_eSPI &tft, int16_t x, int16_t y, uint16_t color) {
    tft.fillTriangle(x, y + 8, x + 10, y, x + 10, y + 16, color);
    tft.drawLine(x + 10, y + 8, x + 22, y + 8, color);
}

uint16_t deptColor(const char *dept) {
    static const char *names[] = {"HR", "Engineering", "Sales", "Operations", "Management"};
    for (int i = 0; i < 5; i++) {
        if (dept && strcmp(dept, names[i]) == 0) return USR_DEPT_COLORS[i];
    }
    return USR_DEPT_COLORS[5];
}

void drawInitials(TFT_eSPI &tft, int16_t cx, int16_t cy, const char *name, uint16_t bg) {
    char ini[3] = "??";
    if (name && name[0]) {
        ini[0] = toupper(name[0]);
        const char *sp = strchr(name, ' ');
        ini[1] = (sp && sp[1]) ? toupper(sp[1]) : (name[1] ? toupper(name[1]) : ' ');
    }
    tft.fillCircle(cx, cy, 18, bg);
    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, bg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(ini, cx, cy);
    tft.setTextDatum(TL_DATUM);
}

void drawFingerprintIconSmall(TFT_eSPI &tft, int x, int y, uint16_t color) {
    for (int i = 0; i < 2; i++) {
        const int r = 6 + i * 5;
        tft.drawArc(x, y, r, r - 2, 200, 320, color, COLOR_BG_DARK, false);
        tft.drawArc(x, y, r, r - 2, 20, 140, color, COLOR_BG_DARK, false);
    }
}

void drawWarningIcon(TFT_eSPI &tft, int x, int y) {
    tft.fillTriangle(x, y - 8, x - 7, y + 6, x + 7, y + 6, STATUS_AMBER);
    tft.drawLine(x, y - 2, x, y + 2, COLOR_BG_DARK);
    tft.fillCircle(x, y + 4, 1, COLOR_BG_DARK);
}

static bool matchesSearch(const EnrolledUser &u) {
    if (gUsersUi.searchLen == 0) return true;
    char idbuf[8];
    snprintf(idbuf, sizeof(idbuf), "%u", u.fingerId);
    char nameLower[MAX_NAME_LEN];
    char queryLower[MAX_NAME_LEN];
    strlcpy(nameLower, u.name, sizeof(nameLower));
    strlcpy(queryLower, gUsersUi.searchBuf, sizeof(queryLower));
    for (int i = 0; nameLower[i]; i++) nameLower[i] = tolower(nameLower[i]);
    for (int i = 0; queryLower[i]; i++) queryLower[i] = tolower(queryLower[i]);
    if (strstr(nameLower, queryLower)) return true;
    if (strstr(idbuf, gUsersUi.searchBuf)) return true;
    snprintf(idbuf, sizeof(idbuf), "#%03u", u.fingerId);
    if (strstr(idbuf, gUsersUi.searchBuf)) return true;
    return false;
}

void rebuildFilter() {
    gUsersUi.filteredCount = 0;
    for (int i = 0; i < gUsersUi.userCount && gUsersUi.filteredCount < USR_MAX_USERS; i++) {
        if (matchesSearch(gUsersUi.users[i])) {
            gUsersUi.filtered[gUsersUi.filteredCount++] = i;
        }
    }
    gUsersUi.totalContentH = gUsersUi.filteredCount * USR_ROW_H;
    const int maxScroll = max(0, gUsersUi.totalContentH - listAreaHeight());
    if (gUsersUi.scrollY > maxScroll) gUsersUi.scrollY = maxScroll;
    if (gUsersUi.scrollY < 0) gUsersUi.scrollY = 0;
}

void renderListSprite() {
    if (!gUsersUi.spriteReady) return;

    const int lh = listAreaHeight();
    gUsersUi.listSprite.fillSprite(COLOR_BG_DARK);

    int y = -gUsersUi.scrollY;
    for (int fi = 0; fi < gUsersUi.filteredCount; fi++) {
        const int ui = gUsersUi.filtered[fi];
        const bool swiped = (gUsersUi.swipedRow == fi);
        if (y + USR_ROW_H > 0 && y < lh) {
            drawUserRow(y, gUsersUi.users[ui], swiped);
        }
        y += USR_ROW_H;
        if (y >= lh) break;
    }

    gUsersUi.listSprite.pushSprite(0, listAreaY());
}

int rowAtContentY(int contentY) {
    if (contentY < 0) return -1;
    return contentY / USR_ROW_H;
}

int filteredIndexAt(int x, int y) {
    if (x < 0 || x >= LIST_W) return -1;
    const int ly = y - listAreaY();
    const int lh = listAreaHeight();
    if (ly < 0 || ly >= lh) return -1;
    const int fi = rowAtContentY(ly + gUsersUi.scrollY);
    if (fi < 0 || fi >= gUsersUi.filteredCount) return -1;
    return fi;
}

int rowScreenY(int fi) {
    return listAreaY() + fi * USR_ROW_H - gUsersUi.scrollY;
}

void drawVirtualKeyboard(TFT_eSPI &tft) {
    const int y0 = SCREEN_HEIGHT - KB_H;
    tft.fillRect(0, y0, SCREEN_WIDTH, KB_H, 0x2104);
    tft.drawLine(0, y0, SCREEN_WIDTH, y0, TEXT_MUTED);

    const char *row1 = (gUsersUi.kbShift == 2) ? "1234567890" : "qwertyuiop";
    const char *row2 = (gUsersUi.kbShift == 2) ? "" : "asdfghjkl";
    const char *row3 = (gUsersUi.kbShift == 2) ? "" : "zxcvbnm";

    auto drawRow = [&](const char *keys, int row, int keyW) {
        if (!keys || !keys[0]) return;
        const int n = strlen(keys);
        const int totalW = n * keyW;
        int x = (SCREEN_WIDTH - totalW) / 2;
        const int ky = y0 + 8 + row * 26;
        for (int i = 0; i < n; i++) {
            tft.fillRoundRect(x, ky, keyW - 2, 22, 3, 0x4208);
            char ch[2] = {keys[i], 0};
            if (gUsersUi.kbShift == 1) {
                ch[0] = toupper(ch[0]);
            }
            tft.setTextFont(1);
            tft.setTextColor(TEXT_PRIMARY, 0x4208);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(ch, x + keyW / 2 - 1, ky + 11);
            x += keyW;
        }
        tft.setTextDatum(TL_DATUM);
    };

    drawRow(row1, 0, 22);
    if (gUsersUi.kbShift != 2) {
        drawRow(row2, 1, 22);
        drawRow(row3, 2, 24);
    }

    tft.fillRoundRect(4, y0 + KB_H - 28, 40, 22, 3, ACCENT_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(gUsersUi.kbShift == 2 ? "ABC" : "123", 24, y0 + KB_H - 17);
    tft.fillRoundRect(SCREEN_WIDTH - 44, y0 + KB_H - 28, 40, 22, 3, TEXT_MUTED);
    tft.drawString("Del", SCREEN_WIDTH - 24, y0 + KB_H - 17);
    tft.setTextDatum(TL_DATUM);
}

bool handleKeyboardTap(int x, int y) {
    if (!gUsersUi.kbVisible) return false;
    const int y0 = SCREEN_HEIGHT - KB_H;
    if (y < y0) return false;

    if (isTouchInRect({x, y, true}, 4, y0 + KB_H - 28, 40, 22)) {
        gUsersUi.kbShift = (gUsersUi.kbShift + 1) % 3;
        drawUsersScreen();
        return true;
    }
    if (isTouchInRect({x, y, true}, SCREEN_WIDTH - 44, y0 + KB_H - 28, 40, 22)) {
        if (gUsersUi.searchLen > 0) gUsersUi.searchBuf[--gUsersUi.searchLen] = '\0';
        rebuildFilter();
        drawUsersScreen();
        return true;
    }

    const char *rows[3];
    rows[0] = (gUsersUi.kbShift == 2) ? "1234567890" : "qwertyuiop";
    rows[1] = (gUsersUi.kbShift == 2) ? "" : "asdfghjkl";
    rows[2] = (gUsersUi.kbShift == 2) ? "" : "zxcvbnm";

    for (int r = 0; r < 3; r++) {
        if (!rows[r][0]) continue;
        const int n = strlen(rows[r]);
        const int keyW = (r == 2) ? 24 : 22;
        const int totalW = n * keyW;
        int kx = (SCREEN_WIDTH - totalW) / 2;
        const int ky = y0 + 8 + r * 26;
        for (int i = 0; i < n; i++) {
            if (isTouchInRect({x, y, true}, kx, ky, keyW - 2, 22)) {
                if (gUsersUi.searchLen < MAX_NAME_LEN - 1) {
                    char ch = rows[r][i];
                    if (gUsersUi.kbShift == 1) ch = toupper(ch);
                    gUsersUi.searchBuf[gUsersUi.searchLen++] = ch;
                    gUsersUi.searchBuf[gUsersUi.searchLen] = '\0';
                    rebuildFilter();
                    drawUsersScreen();
                }
                return true;
            }
            kx += keyW;
        }
    }
    return true;
}

}  // namespace

void usersReload() {
    gUsersUi.userCount = 0;
    JsonDocument doc;
    if (!gStorage.loadUsers(doc)) {
        rebuildFilter();
        return;
    }

    for (JsonObject u : doc["users"].as<JsonArray>()) {
        if (gUsersUi.userCount >= USR_MAX_USERS) break;
        EnrolledUser &usr = gUsersUi.users[gUsersUi.userCount++];
        usr.fingerId = u["fingerId"] | 0;
        strlcpy(usr.name, u["name"] | "", sizeof(usr.name));
        strlcpy(usr.department, u["department"] | "", sizeof(usr.department));
        usr.enrollDate = u["enrollTs"] | 0;
        usr.enrolled = fingerprintIdExists(usr.fingerId);
        usr.totalDays = 0;
        strlcpy(usr.avgArrival, "--:--", sizeof(usr.avgArrival));
    }

    rebuildFilter();
}

void drawUserRow(int y, const EnrolledUser &user, bool swiped) {
    TFT_eSPI &tft = gUsersUi.spriteReady ? (TFT_eSPI &)gUsersUi.listSprite : gDisplay.tft();
    if (y + USR_ROW_H < 0 || y > listAreaHeight()) return;

    tft.fillRect(0, y, LIST_W, USR_ROW_H, COLOR_BG_DARK);

    const int actionW = 76;
    if (swiped) {
        tft.fillRoundRect(LIST_W - actionW, y + 6, 34, USR_ROW_H - 12, 4, ACCENT_BLUE);
        tft.setTextFont(1);
        tft.setTextColor(TEXT_PRIMARY, ACCENT_BLUE);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Edit", LIST_W - actionW + 17, y + USR_ROW_H / 2);
        tft.fillRoundRect(LIST_W - 38, y + 6, 34, USR_ROW_H - 12, 4, ACCENT_RED);
        tft.drawString("Del", LIST_W - 21, y + USR_ROW_H / 2);
        tft.setTextDatum(TL_DATUM);
    }

    const int pad = swiped ? 8 : 0;
    drawInitials(tft, 22 + pad, y + USR_ROW_H / 2, user.name, deptColor(user.department));

    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, COLOR_BG_DARK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(user.name, 46 + pad, y + 10);

    tft.setTextFont(1);
    char idline[16];
    snprintf(idline, sizeof(idline), "#%03u", user.fingerId);
    tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
    tft.drawString(idline, 46 + pad, y + 28);
    if (user.department[0]) {
        tft.drawString(user.department, 100 + pad, y + 28);
    }

    if (user.enrolled) {
        drawFingerprintIconSmall(tft, LIST_W - 18 - pad, y + USR_ROW_H / 2, ACCENT_BLUE);
    } else {
        drawWarningIcon(tft, LIST_W - 18 - pad, y + USR_ROW_H / 2);
    }

    tft.drawLine(8, y + USR_ROW_H - 1, LIST_W - 8, y + USR_ROW_H - 1, 0x2104);
}

void drawUserDetailPopup(const EnrolledUser &user, bool confirmDelete) {
    TFT_eSPI &tft = gDisplay.tft();
    tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x0000);

    const int panelH = confirmDelete ? (SCREEN_HEIGHT - 40) : 260;
    const int panelY = SCREEN_HEIGHT - panelH;
    tft.fillRoundRect(0, panelY, SCREEN_WIDTH, panelH, 10, 0x2104);

    drawInitials(tft, SCREEN_WIDTH / 2, panelY + 42, user.name, deptColor(user.department));

    tft.setTextFont(4);
    tft.setTextColor(TEXT_PRIMARY, 0x2104);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(user.name, SCREEN_WIDTH / 2, panelY + 72);

    tft.setTextFont(2);
    char line[48];
    snprintf(line, sizeof(line), "ID #%03u  |  %s", user.fingerId,
             user.department[0] ? user.department : "—");
    tft.setTextColor(TEXT_SECONDARY, 0x2104);
    tft.drawString(line, SCREEN_WIDTH / 2, panelY + 100);

    if (user.enrollDate > 0 && timeStatus() != timeNotSet) {
        snprintf(line, sizeof(line), "Enrolled: %02d/%02d/%04d", day(user.enrollDate),
                 month(user.enrollDate), year(user.enrollDate));
    } else {
        strlcpy(line, "Enrolled: —", sizeof(line));
    }
    tft.setTextFont(1);
    tft.drawString(line, SCREEN_WIDTH / 2, panelY + 120);

    EnrolledUser statsUser = user;
    gStorage.getUserAttendanceStats(statsUser.fingerId, statsUser.totalDays, statsUser.avgArrival,
                                    sizeof(statsUser.avgArrival));
    snprintf(line, sizeof(line), "Total days: %d | Avg arrival: %s", statsUser.totalDays,
             statsUser.avgArrival);
    tft.drawString(line, SCREEN_WIDTH / 2, panelY + 138);

    if (confirmDelete) {
        tft.setTextColor(STATUS_AMBER, 0x2104);
        tft.drawString("Delete this user?", SCREEN_WIDTH / 2, panelY + 160);
        gDisplay.drawButton(24, panelY + 185, 90, 32, "Cancel", TEXT_MUTED);
        gDisplay.drawButton(126, panelY + 185, 90, 32, "Delete", ACCENT_RED);
    } else {
        gDisplay.drawButton(20, panelY + 165, SCREEN_WIDTH - 40, 32, "Re-enroll Finger", ACCENT_BLUE);
        gDisplay.drawButton(20, panelY + 205, SCREEN_WIDTH - 40, 32, "Delete User", ACCENT_RED);
        gDisplay.drawButton(20, panelY + 245, SCREEN_WIDTH - 40, 28, "Close", 0x4208);
    }
    tft.setTextDatum(TL_DATUM);
}

void drawUsersScreen() {
    TFT_eSPI &tft = gDisplay.tft();
    tft.fillScreen(COLOR_BG_DARK);

    tft.fillRect(0, 0, SCREEN_WIDTH, USR_HEADER_H, 0x2104);
    drawBackArrow(tft, 10, 10, TEXT_PRIMARY);
    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, 0x2104);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Users", SCREEN_WIDTH / 2, 10);
    tft.fillCircle(SCREEN_WIDTH - 22, 18, 12, ACCENT_BLUE);
    tft.setTextColor(TEXT_PRIMARY, ACCENT_BLUE);
    tft.drawString("+", SCREEN_WIDTH - 22, 18);
    tft.setTextDatum(TL_DATUM);

    tft.fillRect(8, USR_SEARCH_Y + 4, SCREEN_WIDTH - 16, USR_SEARCH_H - 8, 0x2104);
    tft.drawRect(8, USR_SEARCH_Y + 4, SCREEN_WIDTH - 16, USR_SEARCH_H - 8, TEXT_MUTED);
    tft.setTextFont(1);
    if (gUsersUi.searchLen > 0) {
        tft.setTextColor(TEXT_PRIMARY, 0x2104);
        tft.drawString(gUsersUi.searchBuf, 14, USR_SEARCH_Y + 12);
    } else {
        tft.setTextColor(TEXT_MUTED, 0x2104);
        tft.drawString("Search name or ID...", 14, USR_SEARCH_Y + 12);
    }

    if (gUsersUi.popupVisible && gUsersUi.popupUser >= 0 &&
        gUsersUi.popupUser < gUsersUi.userCount) {
        drawUserDetailPopup(gUsersUi.users[gUsersUi.popupUser], gUsersUi.popupConfirmDelete);
        return;
    }

    const int lh = listAreaHeight();
    if (!gUsersUi.spriteReady) {
        gUsersUi.listSprite.setColorDepth(16);
        gUsersUi.spriteReady = gUsersUi.listSprite.createSprite(LIST_W, USR_LIST_H);
    }

    if (gUsersUi.filteredCount == 0) {
        tft.fillRect(0, listAreaY(), LIST_W, lh, COLOR_BG_DARK);
        tft.setTextFont(2);
        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("No users found", SCREEN_WIDTH / 2, listAreaY() + lh / 2);
        tft.setTextDatum(TL_DATUM);
    } else {
        renderListSprite();
    }

    if (gUsersUi.kbVisible) {
        drawVirtualKeyboard(tft);
    }
}

bool usersDeleteByIndex(int idx) {
    if (idx < 0 || idx >= gUsersUi.userCount) return false;
    const uint8_t fid = gUsersUi.users[idx].fingerId;
    fingerprintDeleteId(fid);
    gStorage.removeUser(fid);
    usersReload();
    return true;
}

void usersScrollList(int delta) {
    if (!gUsersUi.spriteReady) return;
    gUsersUi.scrollY += delta;
    const int maxScroll = max(0, gUsersUi.totalContentH - listAreaHeight());
    if (gUsersUi.scrollY < 0) gUsersUi.scrollY = 0;
    if (gUsersUi.scrollY > maxScroll) gUsersUi.scrollY = maxScroll;
    renderListSprite();
}

void usersTickInertia() {
    if (fabs((double)gUsersUi.velocity) < 0.35) {
        gUsersUi.velocity = 0;
        return;
    }
    usersScrollList((int)gUsersUi.velocity);
    gUsersUi.velocity *= 0.90f;
}

void usersHandleTouchDown(int x, int y) {
    gUsersUi.dragStartX = x;
    gUsersUi.dragStartY = y;
    gUsersUi.lastTouchX = x;
    gUsersUi.lastTouchY = y;
    gUsersUi.dragging = isTouchInRect({x, y, true}, 0, listAreaY(), LIST_W, listAreaHeight());
    gUsersUi.velocity = 0;
}

void usersHandleTouchMove(int x, int y) {
    if (!gUsersUi.dragging) return;

    const int dx = x - gUsersUi.dragStartX;
    const int dy = y - gUsersUi.lastTouchY;
    gUsersUi.lastTouchX = x;
    gUsersUi.lastTouchY = y;

    if (abs(dx) > abs(dy) + 8 && dx < -USR_SWIPE_THRESH / 2) {
        const int fi = filteredIndexAt(gUsersUi.dragStartX, gUsersUi.dragStartY);
        if (fi >= 0) {
            gUsersUi.swipedRow = fi;
            renderListSprite();
            return;
        }
    }

    if (abs(dy) > 2) {
        usersScrollList(dy);
        gUsersUi.velocity = (float)dy;
    }
}

void usersHandleTouchUp(int x, int y) {
    if (!gUsersUi.dragging) return;
    gUsersUi.dragging = false;

    const int dx = x - gUsersUi.dragStartX;
    const int dy = y - gUsersUi.dragStartY;

    if (abs(dx) < 8 && abs(dy) < 8) {
        gUsersUi.velocity = 0;
        handleUserListTouch({x, y, true});
        return;
    }

    if (abs(dx) > USR_SWIPE_THRESH && dx < 0) {
        const int fi = filteredIndexAt(x, y);
        if (fi >= 0) gUsersUi.swipedRow = fi;
        renderListSprite();
    }
}

bool handleUserListTouch(TouchPoint tp) {
    if (gUsersUi.popupVisible && gUsersUi.popupUser >= 0) {
        const EnrolledUser &u = gUsersUi.users[gUsersUi.popupUser];
        const int panelY = gUsersUi.popupConfirmDelete ? (SCREEN_HEIGHT - (SCREEN_HEIGHT - 40))
                                                       : (SCREEN_HEIGHT - 260);

        if (gUsersUi.popupConfirmDelete) {
            if (isTouchInRect(tp, 24, panelY + 185, 90, 32)) {
                gUsersUi.popupConfirmDelete = false;
                drawUsersScreen();
                return true;
            }
            if (isTouchInRect(tp, 126, panelY + 185, 90, 32)) {
                usersDeleteByIndex(gUsersUi.popupUser);
                gUsersUi.popupVisible = false;
                gUsersUi.popupConfirmDelete = false;
                gUsersUi.popupUser = -1;
                drawUsersScreen();
                return true;
            }
            return true;
        }

        if (isTouchInRect(tp, 20, panelY + 245, SCREEN_WIDTH - 40, 28)) {
            gUsersUi.popupVisible = false;
            gUsersUi.popupUser = -1;
            drawUsersScreen();
            return true;
        }
        if (isTouchInRect(tp, 20, panelY + 205, SCREEN_WIDTH - 40, 32)) {
            gUsersUi.popupConfirmDelete = true;
            drawUsersScreen();
            return true;
        }
        if (isTouchInRect(tp, 20, panelY + 165, SCREEN_WIDTH - 40, 32)) {
            gUsersUi.popupVisible = false;
            gUi.setScreen(AppScreen::Enroll);
            return true;
        }
        if (tp.y < panelY) {
            gUsersUi.popupVisible = false;
            gUsersUi.popupUser = -1;
            drawUsersScreen();
            return true;
        }
        return true;
    }

    if (handleKeyboardTap(tp.x, tp.y)) return true;

    if (isTouchInRect(tp, 0, 0, 44, USR_HEADER_H)) {
        gUi.setScreen(AppScreen::Settings);
        return true;
    }

    if (isTouchInRect(tp, SCREEN_WIDTH - 40, 0, 40, USR_HEADER_H)) {
        gUi.setScreen(AppScreen::Enroll);
        return true;
    }

    if (isTouchInRect(tp, 8, USR_SEARCH_Y, SCREEN_WIDTH - 16, USR_SEARCH_H)) {
        gUsersUi.kbVisible = !gUsersUi.kbVisible;
        drawUsersScreen();
        return true;
    }

    const int fi = filteredIndexAt(tp.x, tp.y);
    if (fi >= 0 && gUsersUi.swipedRow == fi) {
        const int ry = rowScreenY(fi);
        if (isTouchInRect(tp, LIST_W - 76, ry + 6, 34, USR_ROW_H - 12)) {
            gUsersUi.popupUser = gUsersUi.filtered[fi];
            gUsersUi.popupVisible = true;
            gUsersUi.popupConfirmDelete = false;
            drawUsersScreen();
            return true;
        }
        if (isTouchInRect(tp, LIST_W - 38, ry + 6, 34, USR_ROW_H - 12)) {
            gUsersUi.popupUser = gUsersUi.filtered[fi];
            gUsersUi.popupVisible = true;
            gUsersUi.popupConfirmDelete = true;
            drawUsersScreen();
            return true;
        }
    }

    if (fi >= 0) {
        if (gUsersUi.swipedRow == fi) {
            gUsersUi.swipedRow = -1;
            renderListSprite();
            return true;
        }
        gUsersUi.popupUser = gUsersUi.filtered[fi];
        gUsersUi.popupVisible = true;
        gUsersUi.popupConfirmDelete = false;
        drawUsersScreen();
        return true;
    }

    if (gUsersUi.swipedRow >= 0) {
        gUsersUi.swipedRow = -1;
        renderListSprite();
        return true;
    }

    return false;
}
