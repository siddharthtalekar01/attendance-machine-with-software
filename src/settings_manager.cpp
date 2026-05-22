#include "settings_manager.h"

#include "admin_auth.h"
#include "wifi_manager.h"

int settingsWorkStartMin(const AppSettings &s) {
    return s.workStartHour * 60 + s.workStartMinute;
}

int settingsWorkEndMin(const AppSettings &s) {
    return s.workEndHour * 60 + s.workEndMinute;
}

void settingsSetWorkStartMin(AppSettings &s, int minutesFromMidnight) {
    minutesFromMidnight = constrain(minutesFromMidnight, 0, 23 * 60 + 59);
    s.workStartHour = minutesFromMidnight / 60;
    s.workStartMinute = minutesFromMidnight % 60;
}

void settingsSetWorkEndMin(AppSettings &s, int minutesFromMidnight) {
    minutesFromMidnight = constrain(minutesFromMidnight, 0, 23 * 60 + 59);
    s.workEndHour = minutesFromMidnight / 60;
    s.workEndMinute = minutesFromMidnight % 60;
}

bool settingsLoad(AppSettings &out) {
    if (!loadConfig(out)) {
        resetConfigToDefaults(out);
        return saveConfig(out);
    }
    return true;
}

bool settingsSave(const AppSettings &settings) {
    const bool ok = saveConfig(settings);
    if (ok) {
        settingsApplyRuntime(settings);
    }
    return ok;
}

void settingsApplyRuntime(const AppSettings &settings) {
    adminAuthApplyConfig(settings);
    wifiConfigure(settings.wifiEnabled, settings.wifiSSID, settings.wifiPassword,
                  settings.ntpServer, settings.timezone);
}

void settingsFormatTime(int minutesFromMidnight, char *buf, size_t len) {
    minutesFromMidnight = constrain(minutesFromMidnight, 0, 23 * 60 + 59);
    int h = minutesFromMidnight / 60;
    const int m = minutesFromMidnight % 60;
    const char *ampm = "AM";
    if (h >= 12) {
        ampm = "PM";
        if (h > 12) h -= 12;
    }
    if (h == 0) h = 12;
    snprintf(buf, len, "%02d:%02d %s", h, m, ampm);
}

int settingsCountEnrolledUsers() {
    User users[MAX_ENROLLED_FINGERS];
    int count = 0;
    loadAllUsers(users, MAX_ENROLLED_FINGERS, count);
    return count;
}
