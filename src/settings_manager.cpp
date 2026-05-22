#include "settings_manager.h"

#include "storage.h"

static void configToSettings(const AppConfig &cfg, AppSettings &out) {
    strlcpy(out.deviceName, cfg.deviceName, sizeof(out.deviceName));
    out.autoNtp = cfg.autoNtp;
    out.wifiEnabled = cfg.wifiEnabled;
    strlcpy(out.ssid, cfg.ssid, sizeof(out.ssid));
    strlcpy(out.wifiPassword, cfg.wifiPassword, sizeof(out.wifiPassword));
    out.workStartMin = cfg.workStartMin;
    out.workEndMin = cfg.workEndMin;
    out.lateThresholdMin = cfg.lateThresholdMin;
    out.checkInAutoToggle = cfg.checkInAutoToggle;
}

static void settingsToConfig(const AppSettings &s, AppConfig &out) {
    strlcpy(out.deviceName, s.deviceName, sizeof(out.deviceName));
    out.autoNtp = s.autoNtp;
    out.wifiEnabled = s.wifiEnabled;
    strlcpy(out.ssid, s.ssid, sizeof(out.ssid));
    strlcpy(out.wifiPassword, s.wifiPassword, sizeof(out.wifiPassword));
    out.workStartMin = s.workStartMin;
    out.workEndMin = s.workEndMin;
    out.lateThresholdMin = s.lateThresholdMin;
    out.checkInAutoToggle = s.checkInAutoToggle;
}

bool settingsLoad(AppSettings &out) {
    AppConfig cfg;
    if (!loadConfig(cfg)) {
        out = AppSettings{};
        return settingsSave(out);
    }
    configToSettings(cfg, out);
    return true;
}

bool settingsSave(const AppSettings &settings) {
    AppConfig cfg;
    settingsToConfig(settings, cfg);
    return saveConfig(cfg);
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
