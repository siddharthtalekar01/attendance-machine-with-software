#include "settings_ui.h"

#include <TimeLib.h>
#include <WiFi.h>
#include <cstring>

#include <LittleFS.h>

#include "display.h"
#include "fingerprint.h"
#include "storage.h"
#include "wifi_manager.h"
#include "ui_screens.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

SettingsUiState gSettingsUi;

namespace {

constexpr int SET_HEADER_H = 36;
constexpr int SET_LIST_Y = SET_HEADER_H;
constexpr int SET_LIST_H = SCREEN_HEIGHT - SET_HEADER_H;
constexpr int SET_ROW_H = 40;
constexpr int SET_SECTION_H = 22;
constexpr int SET_LIST_W = SCREEN_WIDTH;

struct LayoutRow {
    SettingRowId id = SettingRowId::None;
    int y = 0;
    int h = 0;
};

static LayoutRow s_rows[24];
static int s_rowCount = 0;

void drawBackArrow(TFT_eSPI &tft, int16_t x, int16_t y, uint16_t color) {
    tft.fillTriangle(x, y + 8, x + 10, y, x + 10, y + 16, color);
    tft.drawLine(x + 10, y + 8, x + 22, y + 8, color);
}

void drawSectionHeader(TFT_eSPI &tft, int y, const char *title) {
    tft.setTextFont(1);
    tft.setTextColor(ACCENT_BLUE, COLOR_BG_DARK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(title, 12, y + 4);
    tft.drawLine(10, y + SET_SECTION_H - 2, SET_LIST_W - 10, y + SET_SECTION_H - 2, 0x2104);
}

void drawToggleSwitch(TFT_eSPI &tft, int x, int y, bool on) {
    const int w = 44;
    const int h = 22;
    const uint16_t bg = on ? ACCENT_GREEN : TEXT_MUTED;
    tft.fillRoundRect(x, y, w, h, h / 2, bg);
    const int knobX = on ? (x + w - 18) : (x + 4);
    tft.fillCircle(knobX + 7, y + h / 2, 8, TEXT_PRIMARY);
}

void drawRowIcon(TFT_eSPI &tft, int x, int y, const char *icon) {
    tft.fillRoundRect(x, y, 28, 28, 6, 0x2104);
    tft.setTextFont(1);
    tft.setTextColor(TEXT_SECONDARY, 0x2104);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(icon ? icon : "·", x + 14, y + 14);
    tft.setTextDatum(TL_DATUM);
}

void addRow(SettingRowId id, int &cursor, int h = SET_ROW_H) {
    if (s_rowCount < (int)(sizeof(s_rows) / sizeof(s_rows[0]))) {
        s_rows[s_rowCount++] = {id, cursor, h};
    }
    cursor += h;
}

void rebuildLayout() {
    s_rowCount = 0;
    int y = 0;
    y += SET_SECTION_H;
    addRow(SettingRowId::DeviceName, y);
    addRow(SettingRowId::CurrentTime, y);
    addRow(SettingRowId::AutoNtp, y);
    addRow(SettingRowId::CalibrateTouch, y);

    y += SET_SECTION_H;
    addRow(SettingRowId::WifiEnable, y);
    addRow(SettingRowId::WifiNetwork, y);
    addRow(SettingRowId::WifiStatus, y);

    y += SET_SECTION_H;
    addRow(SettingRowId::WorkStart, y);
    addRow(SettingRowId::WorkEnd, y);
    addRow(SettingRowId::LateThreshold, y);
    addRow(SettingRowId::CheckInMode, y);

    y += SET_SECTION_H;
    addRow(SettingRowId::TotalEnrolled, y);
    addRow(SettingRowId::ViewUsers, y);
    addRow(SettingRowId::DeleteAllUsers, y);

    y += SET_SECTION_H;
    addRow(SettingRowId::ExportData, y);
    addRow(SettingRowId::ClearRecords, y);
    addRow(SettingRowId::FactoryReset, y);

    gSettingsUi.totalContentHeight = y + 12;
}

void getRowDisplay(SettingRowId id, char *value, size_t vlen, bool &toggleOn, bool &danger,
                   SettingRowType &type, const char **icon) {
    danger = false;
    toggleOn = false;
    type = SettingRowType::Text;
    *icon = "·";
    value[0] = '\0';

    AppSettings &s = gSettingsUi.settings;

    switch (id) {
        case SettingRowId::DeviceName:
            *icon = "DN";
            strlcpy(value, s.deviceName, vlen);
            type = SettingRowType::Text;
            break;
        case SettingRowId::CurrentTime: {
            *icon = "TM";
            if (timeStatus() != timeNotSet) {
                snprintf(value, vlen, "%02d:%02d", hour(), minute());
            } else {
                strlcpy(value, "Set time", vlen);
            }
            type = SettingRowType::Text;
            break;
        }
        case SettingRowId::AutoNtp:
            *icon = "NT";
            strlcpy(value, s.autoNtp ? "ON" : "OFF", vlen);
            toggleOn = s.autoNtp;
            type = SettingRowType::Toggle;
            break;
        case SettingRowId::CalibrateTouch:
            *icon = "TC";
            strlcpy(value, "Run", vlen);
            type = SettingRowType::Arrow;
            break;
        case SettingRowId::WifiEnable:
            *icon = "WF";
            toggleOn = s.wifiEnabled;
            type = SettingRowType::Toggle;
            strlcpy(value, s.wifiEnabled ? "ON" : "OFF", vlen);
            break;
        case SettingRowId::WifiNetwork:
            *icon = "AP";
            if (s.ssid[0]) {
                strlcpy(value, s.ssid, vlen);
            } else if (strlen(WIFI_SSID) > 0) {
                strlcpy(value, WIFI_SSID, vlen);
            } else {
                strlcpy(value, "Tap to set", vlen);
            }
            type = SettingRowType::Arrow;
            break;
        case SettingRowId::WifiStatus:
            *icon = "ST";
            strlcpy(value, gWiFi.isConnected() ? "Connected" : "Disconnected", vlen);
            type = SettingRowType::Badge;
            break;
        case SettingRowId::WorkStart:
            *icon = "IN";
            settingsFormatTime(s.workStartMin, value, vlen);
            type = SettingRowType::Text;
            break;
        case SettingRowId::WorkEnd:
            *icon = "OUT";
            settingsFormatTime(s.workEndMin, value, vlen);
            type = SettingRowType::Text;
            break;
        case SettingRowId::LateThreshold:
            *icon = "LT";
            snprintf(value, vlen, "%d min", s.lateThresholdMin);
            type = SettingRowType::Text;
            break;
        case SettingRowId::CheckInMode:
            *icon = "MD";
            strlcpy(value, s.checkInAutoToggle ? "Auto Toggle" : "Manual IN/OUT", vlen);
            type = SettingRowType::Text;
            break;
        case SettingRowId::TotalEnrolled:
            *icon = "US";
            snprintf(value, vlen, "%d", settingsCountEnrolledUsers());
            type = SettingRowType::Badge;
            break;
        case SettingRowId::ViewUsers:
            *icon = "VL";
            strlcpy(value, "", vlen);
            type = SettingRowType::Arrow;
            break;
        case SettingRowId::DeleteAllUsers:
            *icon = "DU";
            strlcpy(value, "Confirm", vlen);
            danger = true;
            type = SettingRowType::Text;
            break;
        case SettingRowId::ExportData:
            *icon = "EX";
            strlcpy(value, "WiFi", vlen);
            type = SettingRowType::Arrow;
            break;
        case SettingRowId::ClearRecords:
            *icon = "CR";
            strlcpy(value, "Confirm", vlen);
            danger = true;
            type = SettingRowType::Text;
            break;
        case SettingRowId::FactoryReset:
            *icon = "FR";
            strlcpy(value, "Confirm", vlen);
            danger = true;
            type = SettingRowType::Text;
            break;
        default:
            break;
    }
}

const char *rowLabel(SettingRowId id) {
    switch (id) {
        case SettingRowId::DeviceName: return "Device Name";
        case SettingRowId::CurrentTime: return "Current Time";
        case SettingRowId::AutoNtp: return "Auto NTP Sync";
        case SettingRowId::CalibrateTouch: return "Calibrate Touch";
        case SettingRowId::WifiEnable: return "WiFi";
        case SettingRowId::WifiNetwork: return "Network";
        case SettingRowId::WifiStatus: return "Status";
        case SettingRowId::WorkStart: return "Work Start Time";
        case SettingRowId::WorkEnd: return "Work End Time";
        case SettingRowId::LateThreshold: return "Late Threshold";
        case SettingRowId::CheckInMode: return "Check-in Mode";
        case SettingRowId::TotalEnrolled: return "Total Enrolled";
        case SettingRowId::ViewUsers: return "View All Users";
        case SettingRowId::DeleteAllUsers: return "Delete All Users";
        case SettingRowId::ExportData: return "Export via WiFi";
        case SettingRowId::ClearRecords: return "Clear All Records";
        case SettingRowId::FactoryReset: return "Factory Reset";
        default: return "";
    }
}

void renderSettingsList() {
    if (!gSettingsUi.spriteReady) return;

    TFT_eSPI &spr = gSettingsUi.listSprite;
    spr.fillSprite(COLOR_BG_DARK);

    int y = -gSettingsUi.scrollY;

    auto section = [&](const char *title) {
        if (y + SET_SECTION_H > 0 && y < SET_LIST_H) {
            drawSectionHeader(spr, y, title);
        }
        y += SET_SECTION_H;
    };

    section("System");
    for (int i = 0; i < 4; i++) {
        const SettingRowId ids[] = {SettingRowId::DeviceName, SettingRowId::CurrentTime,
                                    SettingRowId::AutoNtp, SettingRowId::CalibrateTouch};
        char val[32];
        bool toggle, danger;
        SettingRowType type;
        const char *icon;
        getRowDisplay(ids[i], val, sizeof(val), toggle, danger, type, icon);
        if (y + SET_ROW_H > 0 && y < SET_LIST_H) {
            drawSettingRow(spr, y, icon, rowLabel(ids[i]), val, type, toggle, danger);
        }
        y += SET_ROW_H;
    }

    section("WiFi");
    for (int i = 0; i < 3; i++) {
        const SettingRowId ids[] = {SettingRowId::WifiEnable, SettingRowId::WifiNetwork,
                                    SettingRowId::WifiStatus};
        char val[32];
        bool toggle, danger;
        SettingRowType type;
        const char *icon;
        getRowDisplay(ids[i], val, sizeof(val), toggle, danger, type, icon);
        if (y + SET_ROW_H > 0 && y < SET_LIST_H) {
            drawSettingRow(spr, y, icon, rowLabel(ids[i]), val, type, toggle, danger);
        }
        y += SET_ROW_H;
    }

    section("Attendance");
    for (int i = 0; i < 4; i++) {
        const SettingRowId ids[] = {SettingRowId::WorkStart, SettingRowId::WorkEnd,
                                    SettingRowId::LateThreshold, SettingRowId::CheckInMode};
        char val[32];
        bool toggle, danger;
        SettingRowType type;
        const char *icon;
        getRowDisplay(ids[i], val, sizeof(val), toggle, danger, type, icon);
        if (y + SET_ROW_H > 0 && y < SET_LIST_H) {
            drawSettingRow(spr, y, icon, rowLabel(ids[i]), val, type, toggle, danger);
        }
        y += SET_ROW_H;
    }

    section("Users");
    for (int i = 0; i < 3; i++) {
        const SettingRowId ids[] = {SettingRowId::TotalEnrolled, SettingRowId::ViewUsers,
                                    SettingRowId::DeleteAllUsers};
        char val[32];
        bool toggle, danger;
        SettingRowType type;
        const char *icon;
        getRowDisplay(ids[i], val, sizeof(val), toggle, danger, type, icon);
        if (y + SET_ROW_H > 0 && y < SET_LIST_H) {
            drawSettingRow(spr, y, icon, rowLabel(ids[i]), val, type, toggle, danger);
        }
        y += SET_ROW_H;
    }

    section("Data");
    for (int i = 0; i < 3; i++) {
        const SettingRowId ids[] = {SettingRowId::ExportData, SettingRowId::ClearRecords,
                                    SettingRowId::FactoryReset};
        char val[32];
        bool toggle, danger;
        SettingRowType type;
        const char *icon;
        getRowDisplay(ids[i], val, sizeof(val), toggle, danger, type, icon);
        if (y + SET_ROW_H > 0 && y < SET_LIST_H) {
            drawSettingRow(spr, y, icon, rowLabel(ids[i]), val, type, toggle, danger);
        }
        y += SET_ROW_H;
    }

    gSettingsUi.listSprite.pushSprite(0, SET_LIST_Y);
}

void drawConfirmDialog(const char *title, const char *body, const char *btn1, const char *btn2) {
    TFT_eSPI &tft = gDisplay.tft();
    tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x0000);
    tft.fillRoundRect(16, 80, SCREEN_WIDTH - 32, 160, 8, 0x2104);
    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, 0x2104);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(title, SCREEN_WIDTH / 2, 100);
    tft.setTextFont(1);
    tft.setTextColor(TEXT_SECONDARY, 0x2104);
    tft.drawString(body, SCREEN_WIDTH / 2, 130);
    gDisplay.drawButton(30, 190, 80, 32, btn1, ACCENT_BLUE);
    gDisplay.drawButton(130, 190, 80, 32, btn2, COLOR_ERROR);
    tft.setTextDatum(TL_DATUM);
}

}  // namespace

void settingsUiInit() {
    settingsLoad(gSettingsUi.settings);
    if (strlen(WIFI_SSID) > 0 && gSettingsUi.settings.ssid[0] == '\0') {
        strlcpy(gSettingsUi.settings.ssid, WIFI_SSID, sizeof(gSettingsUi.settings.ssid));
    }
    rebuildLayout();
    gSettingsUi.scrollY = 0;
    gSettingsUi.velocity = 0;
    gSettingsUi.dialog = SettingsUiState::Dialog::None;
}

void drawSettingRow(TFT_eSPI &tft, int y, const char *icon, const char *label, const char *value,
                    SettingRowType type, bool toggleOn, bool danger) {
    if (y + SET_ROW_H < 0 || y > SET_LIST_H) return;

    tft.fillRect(0, y, SET_LIST_W, SET_ROW_H, COLOR_BG_DARK);
    drawRowIcon(tft, 10, y + 6, icon);

    const uint16_t labelCol = danger ? COLOR_ERROR : TEXT_PRIMARY;
    tft.setTextFont(2);
    tft.setTextColor(labelCol, COLOR_BG_DARK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(label ? label : "", 46, y + 8);

    tft.setTextFont(1);
    const int rightX = SET_LIST_W - 12;

    switch (type) {
        case SettingRowType::Toggle:
            drawToggleSwitch(tft, rightX - 48, y + 9, toggleOn);
            break;
        case SettingRowType::Arrow:
            tft.setTextColor(TEXT_SECONDARY, COLOR_BG_DARK);
            tft.setTextDatum(MR_DATUM);
            if (value && value[0]) {
                tft.drawString(value, rightX - 16, y + 20);
            }
            tft.drawString(">", rightX, y + 20);
            tft.setTextDatum(TL_DATUM);
            break;
        case SettingRowType::Badge: {
            const bool ok = value && strcmp(value, "Connected") == 0;
            const uint16_t bg = ok ? ACCENT_GREEN : TEXT_MUTED;
            const int bw = 72;
            tft.fillRoundRect(rightX - bw, y + 10, bw, 18, 4, bg);
            tft.setTextColor(COLOR_BG_DARK, bg);
            tft.setTextDatum(MR_DATUM);
            tft.drawString(value ? value : "", rightX - 4, y + 19);
            tft.setTextDatum(TL_DATUM);
            break;
        }
        case SettingRowType::Text:
        default:
            tft.setTextColor(danger ? COLOR_ERROR : TEXT_SECONDARY, COLOR_BG_DARK);
            tft.setTextDatum(MR_DATUM);
            tft.drawString(value ? value : "", rightX, y + 22);
            if (danger) {
                /* red label already */
            } else if (strcmp(label, "Late Threshold") == 0) {
                tft.drawString("-  +", rightX - 56, y + 22);
            }
            tft.setTextDatum(TL_DATUM);
            break;
    }

    tft.drawLine(10, y + SET_ROW_H - 1, SET_LIST_W - 10, y + SET_ROW_H - 1, 0x2104);
}

void drawSettingsScreen() {
    rebuildLayout();
    TFT_eSPI &tft = gDisplay.tft();
    tft.fillScreen(COLOR_BG_DARK);

    tft.fillRect(0, 0, SCREEN_WIDTH, SET_HEADER_H, 0x2104);
    drawBackArrow(tft, 10, 10, TEXT_PRIMARY);
    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, 0x2104);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Settings", SCREEN_WIDTH / 2, 10);
    tft.setTextDatum(TL_DATUM);

    if (!gSettingsUi.spriteReady) {
        gSettingsUi.listSprite.setColorDepth(16);
        gSettingsUi.spriteReady = gSettingsUi.listSprite.createSprite(SET_LIST_W, SET_LIST_H);
    }

    if (gSettingsUi.dialog != SettingsUiState::Dialog::None) {
        const char *title = "Confirm?";
        const char *body = "This cannot be undone.";
        if (gSettingsUi.dialog == SettingsUiState::Dialog::ConfirmDeleteUsers ||
            gSettingsUi.dialog == SettingsUiState::Dialog::ConfirmDeleteUsers2) {
            title = "Delete All Users";
            body = gSettingsUi.dialog == SettingsUiState::Dialog::ConfirmDeleteUsers
                       ? "Remove all enrolled users?"
                       : "Tap YES to permanently delete.";
        } else if (gSettingsUi.dialog == SettingsUiState::Dialog::ConfirmClearRecords ||
                   gSettingsUi.dialog == SettingsUiState::Dialog::ConfirmClearRecords2) {
            title = "Clear Records";
            body = gSettingsUi.dialog == SettingsUiState::Dialog::ConfirmClearRecords
                       ? "Delete all attendance logs?"
                       : "Tap YES to clear all records.";
        } else if (gSettingsUi.dialog == SettingsUiState::Dialog::ConfirmFactoryReset ||
                   gSettingsUi.dialog == SettingsUiState::Dialog::ConfirmFactoryReset2) {
            title = "Factory Reset";
            body = gSettingsUi.dialog == SettingsUiState::Dialog::ConfirmFactoryReset
                       ? "Reset device to defaults?"
                       : "Tap YES to factory reset.";
        }
        drawConfirmDialog(title, body, "Cancel", "YES");
        return;
    }

    renderSettingsList();
}

void settingsScrollList(int delta) {
    if (!gSettingsUi.spriteReady) return;
    gSettingsUi.scrollY += delta;
    const int maxScroll = max(0, gSettingsUi.totalContentHeight - SET_LIST_H);
    if (gSettingsUi.scrollY < 0) gSettingsUi.scrollY = 0;
    if (gSettingsUi.scrollY > maxScroll) gSettingsUi.scrollY = maxScroll;
    renderSettingsList();
}

void settingsTickInertia() {
    if (fabs((double)gSettingsUi.velocity) < 0.35) {
        gSettingsUi.velocity = 0;
        return;
    }
    settingsScrollList((int)gSettingsUi.velocity);
    gSettingsUi.velocity *= 0.90f;
}

SettingRowId settingsRowAt(int x, int y) {
    if (x < 0 || x >= SET_LIST_W || y < SET_LIST_Y || y >= SET_LIST_Y + SET_LIST_H) {
        return SettingRowId::None;
    }
    const int localY = y - SET_LIST_Y + gSettingsUi.scrollY;
    for (int i = 0; i < s_rowCount; i++) {
        if (localY >= s_rows[i].y && localY < s_rows[i].y + s_rows[i].h) {
            return s_rows[i].id;
        }
    }
    return SettingRowId::None;
}

static void applyRowAction(SettingRowId id, int tapX) {
    AppSettings &s = gSettingsUi.settings;

    switch (id) {
        case SettingRowId::DeviceName: {
            static const char *names[] = {"Attendance", "Office Kiosk", "Main Gate", "Lab Unit"};
            static int idx = 0;
            idx = (idx + 1) % 4;
            strlcpy(s.deviceName, names[idx], sizeof(s.deviceName));
            break;
        }
        case SettingRowId::CurrentTime:
            if (timeStatus() != timeNotSet) {
                setTime(now() + 3600);
            }
            if (s.autoNtp) {
                gWiFi.syncTime();
            }
            break;
        case SettingRowId::AutoNtp:
            s.autoNtp = !s.autoNtp;
            if (s.autoNtp) gWiFi.syncTime();
            break;
        case SettingRowId::CalibrateTouch:
            calibrateTouch();
            break;
        case SettingRowId::WifiEnable:
            s.wifiEnabled = !s.wifiEnabled;
            if (s.wifiEnabled && s.ssid[0]) {
                gWiFi.begin(s.ssid, s.wifiPassword);
                gWiFi.connect();
            } else if (!s.wifiEnabled) {
                WiFi.disconnect(true);
            }
            break;
        case SettingRowId::WifiNetwork:
            if (strlen(WIFI_SSID) > 0) {
                strlcpy(s.ssid, WIFI_SSID, sizeof(s.ssid));
                strlcpy(s.wifiPassword, WIFI_PASSWORD, sizeof(s.wifiPassword));
                gWiFi.begin(s.ssid, s.wifiPassword);
                gWiFi.connect();
            }
            break;
        case SettingRowId::WorkStart:
            s.workStartMin += 30;
            if (s.workStartMin >= 24 * 60) s.workStartMin = 0;
            break;
        case SettingRowId::WorkEnd:
            s.workEndMin += 30;
            if (s.workEndMin >= 24 * 60) s.workEndMin = 0;
            break;
        case SettingRowId::LateThreshold:
            if (tapX < SET_LIST_W / 2) {
                s.lateThresholdMin = max(0, s.lateThresholdMin - 5);
            } else {
                s.lateThresholdMin = min(120, s.lateThresholdMin + 5);
            }
            break;
        case SettingRowId::CheckInMode:
            s.checkInAutoToggle = !s.checkInAutoToggle;
            break;
        case SettingRowId::WifiStatus:
        case SettingRowId::TotalEnrolled:
            drawSettingsScreen();
            return;
        case SettingRowId::ViewUsers:
            gUi.setScreen(AppScreen::UserList);
            return;
        case SettingRowId::DeleteAllUsers:
            gSettingsUi.dialog = SettingsUiState::Dialog::ConfirmDeleteUsers;
            drawSettingsScreen();
            return;
        case SettingRowId::ExportData:
            Serial.println("[Settings] Export via WiFi (stub)");
            break;
        case SettingRowId::ClearRecords:
            gSettingsUi.dialog = SettingsUiState::Dialog::ConfirmClearRecords;
            drawSettingsScreen();
            return;
        case SettingRowId::FactoryReset:
            gSettingsUi.dialog = SettingsUiState::Dialog::ConfirmFactoryReset;
            drawSettingsScreen();
            return;
        default:
            break;
    }

    settingsSave(s);
    drawSettingsScreen();
}

bool settingsHandleDialogTap(int x, int y) {
    if (gSettingsUi.dialog == SettingsUiState::Dialog::None) return false;

    TouchPoint tp{x, y, true};
    if (isTouchInRect(tp, 30, 190, 80, 32)) {
        gSettingsUi.dialog = SettingsUiState::Dialog::None;
        drawSettingsScreen();
        return true;
    }
    if (isTouchInRect(tp, 130, 190, 80, 32)) {
        auto d = gSettingsUi.dialog;
        gSettingsUi.dialog = SettingsUiState::Dialog::None;

        if (d == SettingsUiState::Dialog::ConfirmDeleteUsers) {
            gSettingsUi.dialog = SettingsUiState::Dialog::ConfirmDeleteUsers2;
            drawSettingsScreen();
            return true;
        }
        if (d == SettingsUiState::Dialog::ConfirmClearRecords) {
            gSettingsUi.dialog = SettingsUiState::Dialog::ConfirmClearRecords2;
            drawSettingsScreen();
            return true;
        }
        if (d == SettingsUiState::Dialog::ConfirmFactoryReset) {
            gSettingsUi.dialog = SettingsUiState::Dialog::ConfirmFactoryReset2;
            drawSettingsScreen();
            return true;
        }

        if (d == SettingsUiState::Dialog::ConfirmDeleteUsers2) {
            fingerprintDeleteAll();
            JsonDocument doc;
            doc["users"] = JsonArray();
            gStorage.saveUsers(doc);
        } else if (d == SettingsUiState::Dialog::ConfirmClearRecords2) {
            File f = LittleFS.open(LOG_FILE, FILE_WRITE);
            if (f) {
                f.print("[]");
                f.close();
            }
        } else if (d == SettingsUiState::Dialog::ConfirmFactoryReset2) {
            fingerprintDeleteAll();
            JsonDocument udoc;
            udoc["users"] = JsonArray();
            gStorage.saveUsers(udoc);
            File f = LittleFS.open(LOG_FILE, FILE_WRITE);
            if (f) {
                f.print("[]");
                f.close();
            }
            gSettingsUi.settings = AppSettings{};
            settingsSave(gSettingsUi.settings);
        }
        drawSettingsScreen();
        return true;
    }
    return true;
}

bool settingsHandleTap(int x, int y) {
    if (settingsHandleDialogTap(x, y)) return true;

    if (isTouchInRect({x, y, true}, 0, 0, 44, SET_HEADER_H)) {
        return true;
    }

    const SettingRowId id = settingsRowAt(x, y);
    if (id != SettingRowId::None) {
        applyRowAction(id, x);
        return true;
    }
    return false;
}

void settingsHandleTouchDown(int x, int y) {
    gSettingsUi.dragging = isTouchInRect({x, y, true}, 0, SET_LIST_Y, SET_LIST_W, SET_LIST_H);
    gSettingsUi.lastTouchY = y;
    gSettingsUi.velocity = 0;
}

void settingsHandleTouchMove(int x, int y) {
    if (!gSettingsUi.dragging) return;
    const int delta = y - gSettingsUi.lastTouchY;
    gSettingsUi.lastTouchY = y;
    settingsScrollList(delta);
    gSettingsUi.velocity = (float)delta;
}

void settingsHandleTouchUp(int x, int y) {
    if (!gSettingsUi.dragging) return;
    gSettingsUi.dragging = false;
    if (abs((int)gSettingsUi.velocity) < 5) {
        settingsHandleTap(x, y);
    }
}

void drawUsersListScreen() {
    TFT_eSPI &tft = gDisplay.tft();
    tft.fillScreen(COLOR_BG_DARK);
    drawBackArrow(tft, 10, 10, TEXT_PRIMARY);
    tft.setTextFont(2);
    tft.setTextColor(TEXT_PRIMARY, COLOR_BG_DARK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("All Users", SCREEN_WIDTH / 2, 10);
    tft.setTextDatum(TL_DATUM);

    JsonDocument doc;
    if (!gStorage.loadUsers(doc)) {
        gDisplay.drawCenteredText(160, "No users file", 2, TEXT_MUTED);
        return;
    }

    JsonArray users = doc["users"].as<JsonArray>();
    int y = 40;
    tft.setTextFont(1);
    for (JsonObject u : users) {
        if (y > 280) break;
        char line[48];
        snprintf(line, sizeof(line), "%s (ID %u)", u["name"].as<const char *>(),
                 u["fingerId"].as<uint8_t>());
        tft.setTextColor(TEXT_PRIMARY, COLOR_BG_DARK);
        tft.drawString(line, 12, y);
        const char *dept = u["department"] | "";
        if (dept[0]) {
            tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
            tft.drawString(dept, 12, y + 14);
        }
        y += 32;
    }

    gDisplay.drawButton(20, 280, 90, 32, "Back", 0x4208);
}
