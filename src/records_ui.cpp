#include "records_ui.h"

#include <TimeLib.h>
#include <ctype.h>
#include <cstring>
#include <math.h>

#include "display.h"

RecordsUiState gRecordsUi;

namespace {

constexpr int LIST_W = SCREEN_WIDTH;
constexpr int LIST_H = REC_LIST_H;

static time_t startOfDayLocal(time_t t) {
    if (timeStatus() == timeNotSet) return t;
    tmElements_t te;
    breakTime(t, te);
    te.Hour = 0;
    te.Minute = 0;
    te.Second = 0;
    return makeTime(te);
}

void formatDateLabel(time_t dayAnchor, char *buf, size_t len) {
    if (timeStatus() == timeNotSet) {
        strlcpy(buf, "Today", len);
        return;
    }
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    tmElements_t te;
    breakTime(dayAnchor, te);
    snprintf(buf, len, "%02d %s %04d", te.Day, months[te.Month - 1], tmYearToCalendar(te.Year));
}

void formatTimeAmPm(time_t ts, char *buf, size_t len) {
    if (timeStatus() == timeNotSet || ts == 0) {
        strlcpy(buf, "--:--", len);
        return;
    }
    int h = hour(ts);
    const int m = minute(ts);
    const char *ampm = "AM";
    if (h >= 12) {
        ampm = "PM";
        if (h > 12) h -= 12;
    }
    if (h == 0) h = 12;
    snprintf(buf, len, "%02d:%02d %s", h, m, ampm);
}

uint16_t departmentColor(const char *dept) {
    if (!dept || !dept[0]) return 0x4A69;
    if (strcmp(dept, "HR") == 0) return 0xFBE4;
    if (strcmp(dept, "Engineering") == 0) return 0x3A6A;
    if (strcmp(dept, "Sales") == 0) return 0xFE60;
    if (strcmp(dept, "Operations") == 0) return 0x07E0;
    if (strcmp(dept, "Management") == 0) return 0xC81F;
    return 0x632C;
}

void drawInitials(TFT_eSPI &tft, int16_t cx, int16_t cy, const char *name, uint16_t bg) {
    char initials[3] = "??";
    if (name && name[0]) {
        initials[0] = toupper(name[0]);
        const char *sp = strchr(name, ' ');
        initials[1] = sp && sp[1] ? toupper(sp[1]) : (name[1] ? toupper(name[1]) : ' ');
        initials[2] = '\0';
    }
    tft.fillCircle(cx, cy, 16, bg);
    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, bg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(initials, cx, cy);
    tft.setTextDatum(TL_DATUM);
}

void drawBackArrow(TFT_eSPI &tft, int16_t x, int16_t y, uint16_t color) {
    tft.fillTriangle(x, y + 8, x + 10, y, x + 10, y + 16, color);
    tft.drawLine(x + 10, y + 8, x + 22, y + 8, color);
}

void drawFilterIcon(TFT_eSPI &tft, int16_t x, int16_t y, uint16_t color) {
    tft.drawLine(x, y, x + 14, y, color);
    tft.drawLine(x + 2, y + 5, x + 12, y + 5, color);
    tft.drawLine(x + 4, y + 10, x + 10, y + 10, color);
}

void recomputeContentHeight() {
    int h = 0;
    for (int i = 0; i < gRecordsUi.itemCount; i++) {
        h += recordsRowHeight(i);
    }
    gRecordsUi.totalContentHeight = h;
    const int maxScroll = max(0, h - LIST_H);
    if (gRecordsUi.scrollY > maxScroll) gRecordsUi.scrollY = maxScroll;
    if (gRecordsUi.scrollY < 0) gRecordsUi.scrollY = 0;
}

void renderListSprite() {
    if (!gRecordsUi.spriteReady) return;

    gRecordsUi.listSprite.fillSprite(COLOR_BG_DARK);

    int yOff = -gRecordsUi.scrollY;
    for (int i = 0; i < gRecordsUi.itemCount; i++) {
        const int rh = recordsRowHeight(i);
        if (yOff + rh > 0 && yOff < LIST_H) {
            drawRecordRow(yOff, gRecordsUi.items[i], gRecordsUi.expandedIndex == i);
        }
        yOff += rh;
        if (yOff >= LIST_H) break;
    }

    gRecordsUi.listSprite.pushSprite(0, REC_LIST_Y);
}

}  // namespace

int recordsRowHeight(int index) {
    if (index < 0 || index >= gRecordsUi.itemCount) return REC_ROW_H;
    if (gRecordsUi.expandedIndex == index) return REC_ROW_H + REC_ROW_EXPANDED;
    return REC_ROW_H;
}

void recordsReload() {
    if (gRecordsUi.dayAnchor == 0) {
        gRecordsUi.dayAnchor = now();
    }
    gRecordsUi.itemCount = gStorage.loadAttendanceFiltered(
        gRecordsUi.filter, gRecordsUi.dayAnchor, gRecordsUi.items, REC_MAX_ITEMS, &gRecordsUi.summary);
    gRecordsUi.expandedIndex = -1;

    const time_t reportDay = (gRecordsUi.filter == RecordFilter::All) ? now() : gRecordsUi.dayAnchor;
    gRecordsUi.dailyReport = generateDailyReport(reportDay);

    recomputeContentHeight();
}

void drawRecordsSummaryDashboard(TFT_eSPI &tft, const DailyReport &rep) {
    const int cardGap = 4;
    const int cardW = (SCREEN_WIDTH - 8 - cardGap * 3) / 4;
    const int y = REC_SUMMARY_Y;
    const int h = REC_SUMMARY_H;

    tft.fillRect(0, y, SCREEN_WIDTH, h, COLOR_BG_DARK);

    auto drawCard = [&](int idx, const char *label, const char *value, uint16_t accent) {
        const int x = 4 + idx * (cardW + cardGap);
        tft.fillRoundRect(x, y + 4, cardW, h - 8, 6, BG_SECONDARY);
        tft.drawRoundRect(x, y + 4, cardW, h - 8, 6, accent);

        tft.setTextFont(1);
        tft.setTextColor(TEXT_MUTED, BG_SECONDARY);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(label, x + cardW / 2, y + 10);

        tft.setTextFont(2);
        tft.setTextColor(accent, BG_SECONDARY);
        tft.drawString(value, x + cardW / 2, y + 28);
        tft.setTextDatum(TL_DATUM);
    };

    char buf[12];

    snprintf(buf, sizeof(buf), "%d", rep.presentToday);
    drawCard(0, "Present", buf, ACCENT_GREEN);

    snprintf(buf, sizeof(buf), "%d", rep.lateToday);
    drawCard(1, "Late", buf, STATUS_AMBER);

    snprintf(buf, sizeof(buf), "%d", rep.absentToday);
    drawCard(2, "Absent", buf, ACCENT_RED);

    if (rep.avgArrivalMin >= 0) {
        snprintf(buf, sizeof(buf), "%02d:%02d", rep.avgArrivalMin / 60, rep.avgArrivalMin % 60);
    } else if (rep.avgDurationMin >= 0) {
        snprintf(buf, sizeof(buf), "%dm", rep.avgDurationMin);
    } else {
        strlcpy(buf, "--:--", sizeof(buf));
    }
    drawCard(3, "Avg Time", buf, ACCENT_BLUE);
}

void drawRecordRow(int y, const AttendanceRecordView &view, bool expanded) {
    TFT_eSPI &tft = gRecordsUi.spriteReady ? (TFT_eSPI &)gRecordsUi.listSprite : gDisplay.tft();
    const int rowH = expanded ? (REC_ROW_H + REC_ROW_EXPANDED) : REC_ROW_H;

    if (y + rowH < 0 || y > LIST_H) return;

    tft.fillRect(0, y, LIST_W, rowH, COLOR_BG_DARK);

    const uint16_t avatarCol = departmentColor(view.department);
    drawInitials(tft, 22, y + REC_ROW_H / 2, view.name, avatarCol);

    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, COLOR_BG_DARK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(view.name, 46, y + 8);

    tft.setTextFont(1);
    tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
    const char *dept = view.department[0] ? view.department : "—";
    tft.drawString(dept, 46, y + 26);

    char timeBuf[16];
    formatTimeAmPm(view.rec.timestamp, timeBuf, sizeof(timeBuf));
    tft.setTextColor(TEXT_SECONDARY, COLOR_BG_DARK);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(timeBuf, LIST_W - 10, y + 10);

    const char *badge = view.rec.checkIn ? "IN" : "OUT";
    const uint16_t badgeCol = view.rec.checkIn ? ACCENT_GREEN : 0xFC60;
    const int bw = 28;
    tft.fillRoundRect(LIST_W - bw - 10, y + 22, bw, 16, 3, badgeCol);
    tft.setTextColor(COLOR_BG_DARK, badgeCol);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(badge, LIST_W - bw / 2 - 10, y + 30);
    tft.setTextDatum(TL_DATUM);

    if (expanded) {
        const int dy = y + REC_ROW_H;
        tft.drawLine(8, dy, LIST_W - 8, dy, TEXT_MUTED);
        tft.setTextFont(1);
        tft.setTextColor(TEXT_SECONDARY, COLOR_BG_DARK);
        char line[64];
        snprintf(line, sizeof(line), "ID: %u", view.rec.fingerId);
        tft.drawString(line, 46, dy + 6);
        formatTimeAmPm(view.rec.timestamp, timeBuf, sizeof(timeBuf));
        snprintf(line, sizeof(line), "Time: %s  %s", timeBuf, view.rec.checkIn ? "Check-in" : "Check-out");
        tft.drawString(line, 46, dy + 20);
        tft.drawString("Duration: —", 46, dy + 32);
    }

    tft.drawLine(8, y + rowH - 1, LIST_W - 8, y + rowH - 1, 0x2104);
}

void drawRecordsScreen(RecordFilter filter) {
    gRecordsUi.filter = filter;
    recordsReload();

    TFT_eSPI &tft = gDisplay.tft();
    tft.fillScreen(COLOR_BG_DARK);

    tft.fillRect(0, 0, SCREEN_WIDTH, REC_HEADER_H, 0x2104);
    drawBackArrow(tft, 10, 12, TEXT_PRIMARY);
    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, 0x2104);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Records", SCREEN_WIDTH / 2, 10);
    drawFilterIcon(tft, SCREEN_WIDTH - 28, 12, TEXT_SECONDARY);
    tft.setTextDatum(TL_DATUM);

    const char *tabs[] = {"Today", "Week", "All"};
    const int tabW = 72;
    const int tabY = 28;
    for (int i = 0; i < 3; i++) {
        const int tx = 8 + i * (tabW + 4);
        const bool active = (static_cast<int>(filter) == i);
        const uint16_t bg = active ? ACCENT_BLUE : 0x2104;
        const uint16_t fg = active ? TEXT_PRIMARY : TEXT_SECONDARY;
        tft.fillRoundRect(tx, tabY, tabW, 14, 7, bg);
        tft.setTextFont(1);
        tft.setTextColor(fg, bg);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(tabs[i], tx + tabW / 2, tabY + 7);
    }
    tft.setTextDatum(TL_DATUM);

    tft.fillRect(0, REC_FILTER_Y, SCREEN_WIDTH, REC_FILTER_H, COLOR_BG_DARK);
    if (filter != RecordFilter::All) {
        tft.fillTriangle(14, REC_FILTER_Y + 12, 22, REC_FILTER_Y + 8, 22, REC_FILTER_Y + 16, TEXT_SECONDARY);
        tft.fillTriangle(SCREEN_WIDTH - 22, REC_FILTER_Y + 12, SCREEN_WIDTH - 14, REC_FILTER_Y + 8,
                         SCREEN_WIDTH - 14, REC_FILTER_Y + 16, TEXT_SECONDARY);
    }

    char dateBuf[20];
    if (filter == RecordFilter::Today) {
        formatDateLabel(gRecordsUi.dayAnchor, dateBuf, sizeof(dateBuf));
    } else if (filter == RecordFilter::Week) {
        snprintf(dateBuf, sizeof(dateBuf), "Last 7 days");
    } else {
        strlcpy(dateBuf, "All history", sizeof(dateBuf));
    }
    tft.setTextFont(1);
    tft.setTextColor(TEXT_SECONDARY, COLOR_BG_DARK);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(dateBuf, 28, REC_FILTER_Y + 12);

    char badge[24];
    snprintf(badge, sizeof(badge), "%d records", gRecordsUi.summary.total);
    const int badgeW = 72;
    tft.fillRoundRect(SCREEN_WIDTH - badgeW - 8, REC_FILTER_Y + 4, badgeW, 18, 4, ACCENT_BLUE);
    tft.setTextColor(TEXT_PRIMARY, ACCENT_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(badge, SCREEN_WIDTH - badgeW / 2 - 8, REC_FILTER_Y + 13);
    tft.setTextDatum(TL_DATUM);

    drawRecordsSummaryDashboard(tft, gRecordsUi.dailyReport);

    tft.fillRect(0, REC_BOTTOM_Y, SCREEN_WIDTH, SCREEN_HEIGHT - REC_BOTTOM_Y, 0x2104);
    char summary[56];
    snprintf(summary, sizeof(summary), "%d enrolled | %d events", gRecordsUi.dailyReport.totalEnrolled,
             gRecordsUi.summary.total);
    tft.setTextFont(1);
    tft.setTextColor(TEXT_MUTED, 0x2104);
    tft.drawString(summary, 10, REC_BOTTOM_Y + 6);

    tft.fillRoundRect(10, REC_BOTTOM_Y + 22, 100, 24, 4, ACCENT_BLUE);
    tft.setTextColor(TEXT_PRIMARY, ACCENT_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Export CSV", 60, REC_BOTTOM_Y + 34);
    tft.setTextDatum(TL_DATUM);

    if (!gRecordsUi.spriteReady) {
        gRecordsUi.listSprite.setColorDepth(16);
        gRecordsUi.spriteReady = gRecordsUi.listSprite.createSprite(LIST_W, LIST_H);
    }

    if (gRecordsUi.itemCount == 0) {
        tft.setTextFont(2);
        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("No records", SCREEN_WIDTH / 2, REC_LIST_Y + LIST_H / 2);
        tft.setTextDatum(TL_DATUM);
        tft.fillRect(0, REC_LIST_Y, LIST_W, LIST_H, COLOR_BG_DARK);
    } else {
        renderListSprite();
    }
}

void scrollRecordsList(int delta) {
    if (!gRecordsUi.spriteReady || gRecordsUi.itemCount == 0) return;

    gRecordsUi.scrollY += delta;
    const int maxScroll = max(0, gRecordsUi.totalContentHeight - LIST_H);
    if (gRecordsUi.scrollY < 0) gRecordsUi.scrollY = 0;
    if (gRecordsUi.scrollY > maxScroll) gRecordsUi.scrollY = maxScroll;

    renderListSprite();
}

void recordsTickInertia() {
    if (fabs((double)gRecordsUi.velocity) < 0.35) {
        gRecordsUi.velocity = 0;
        return;
    }
    scrollRecordsList((int)gRecordsUi.velocity);
    gRecordsUi.velocity *= 0.90f;
}

int recordsHitRow(int x, int y, int &outIndex) {
    if (x < 0 || x >= LIST_W || y < REC_LIST_Y || y >= REC_LIST_Y + LIST_H) return -1;

    int localY = y - REC_LIST_Y + gRecordsUi.scrollY;
    int cum = 0;
    for (int i = 0; i < gRecordsUi.itemCount; i++) {
        const int rh = recordsRowHeight(i);
        if (localY >= cum && localY < cum + rh) {
            outIndex = i;
            return 0;
        }
        cum += rh;
    }
    return -1;
}

void recordsHandleTouchDown(int x, int y) {
    gRecordsUi.dragging = isTouchInRect({x, y, true}, 0, REC_LIST_Y, LIST_W, LIST_H);
    gRecordsUi.lastTouchY = y;
    gRecordsUi.velocity = 0;
}

void recordsHandleTouchMove(int x, int y) {
    if (!gRecordsUi.dragging) return;
    const int delta = y - gRecordsUi.lastTouchY;
    gRecordsUi.lastTouchY = y;
    scrollRecordsList(delta);
    gRecordsUi.velocity = (float)delta;
}

void recordsHandleTouchUp(int x, int y) {
    if (!gRecordsUi.dragging) return;
    gRecordsUi.dragging = false;

    int idx = -1;
    if (recordsHitRow(x, y, idx) == 0 && abs((int)gRecordsUi.velocity) < 6) {
        gRecordsUi.expandedIndex = (gRecordsUi.expandedIndex == idx) ? -1 : idx;
        recomputeContentHeight();
        renderListSprite();
    }
}

bool recordsHandleChromeTap(int x, int y) {
    TouchPoint tp{x, y, true};

    if (isTouchInRect(tp, 0, 0, 40, REC_HEADER_H)) {
        return true;  // caller navigates back
    }

    const int tabW = 72;
    const int tabY = 28;
    for (int i = 0; i < 3; i++) {
        const int tx = 8 + i * (tabW + 4);
        if (isTouchInRect(tp, tx, tabY, tabW, 14)) {
            drawRecordsScreen(static_cast<RecordFilter>(i));
            return true;
        }
    }

    if (gRecordsUi.filter != RecordFilter::All) {
        if (isTouchInRect(tp, 4, REC_FILTER_Y, 24, REC_FILTER_H)) {
            gRecordsUi.dayAnchor -= (gRecordsUi.filter == RecordFilter::Week) ? 7 * 86400 : 86400;
            drawRecordsScreen(gRecordsUi.filter);
            return true;
        }
        if (isTouchInRect(tp, SCREEN_WIDTH - 28, REC_FILTER_Y, 24, REC_FILTER_H)) {
            gRecordsUi.dayAnchor += (gRecordsUi.filter == RecordFilter::Week) ? 7 * 86400 : 86400;
            drawRecordsScreen(gRecordsUi.filter);
            return true;
        }
    }

    if (isTouchInRect(tp, SCREEN_WIDTH - 36, 8, 28, 24)) {
        drawRecordsScreen(gRecordsUi.filter);
        return true;
    }

    if (isTouchInRect(tp, 10, REC_BOTTOM_Y + 22, 100, 24)) {
        const time_t day = startOfDayLocal(gRecordsUi.dayAnchor);
        String csv;
        if (gRecordsUi.filter == RecordFilter::Week) {
            csv = exportRangeCSV(day - 6 * 86400, day);
        } else if (gRecordsUi.filter == RecordFilter::All) {
            csv = exportRangeCSV(day - 30 * 86400, day);
        } else {
            csv = exportDayCSV(day);
        }

        char fname[40];
        snprintf(fname, sizeof(fname), "%04d-%02d-%02d_export.csv", year(day), month(day), day(day));
        if (saveExportToFS(csv, fname)) {
            Serial.printf("[Records] Saved /exports/%s (%u bytes)\n", fname, (unsigned)csv.length());
        } else {
            Serial.println("[Records] Export save failed");
        }
        return true;
    }

    return false;
}
