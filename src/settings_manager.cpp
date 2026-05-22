#include "settings_manager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cstring>

#include "config.h"
#include "storage.h"

static const char *SETTINGS_FILE = "/config/settings.json";

bool settingsLoad(AppSettings &out) {
    out = AppSettings{};

    if (!LittleFS.exists(SETTINGS_FILE)) {
        return settingsSave(out);
    }

    File f = LittleFS.open(SETTINGS_FILE, FILE_READ);
    if (!f) return false;

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close();
        return false;
    }
    f.close();

    strlcpy(out.deviceName, doc["deviceName"] | "Attendance", sizeof(out.deviceName));
    out.autoNtp = doc["autoNtp"] | true;
    out.wifiEnabled = doc["wifiEnabled"] | true;
    strlcpy(out.ssid, doc["ssid"] | "", sizeof(out.ssid));
    strlcpy(out.wifiPassword, doc["wifiPassword"] | "", sizeof(out.wifiPassword));
    out.workStartMin = doc["workStartMin"] | (9 * 60);
    out.workEndMin = doc["workEndMin"] | (18 * 60);
    out.lateThresholdMin = doc["lateThresholdMin"] | 15;
    out.checkInAutoToggle = doc["checkInAutoToggle"] | true;
    return true;
}

bool settingsSave(const AppSettings &settings) {
    if (!LittleFS.exists(TOUCH_CAL_DIR)) {
        LittleFS.mkdir(TOUCH_CAL_DIR);
    }

    JsonDocument doc;
    doc["deviceName"] = settings.deviceName;
    doc["autoNtp"] = settings.autoNtp;
    doc["wifiEnabled"] = settings.wifiEnabled;
    doc["ssid"] = settings.ssid;
    doc["wifiPassword"] = settings.wifiPassword;
    doc["workStartMin"] = settings.workStartMin;
    doc["workEndMin"] = settings.workEndMin;
    doc["lateThresholdMin"] = settings.lateThresholdMin;
    doc["checkInAutoToggle"] = settings.checkInAutoToggle;

    File f = LittleFS.open(SETTINGS_FILE, FILE_WRITE);
    if (!f) return false;
    const bool ok = serializeJson(doc, f) > 0;
    f.close();
    return ok;
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
    JsonDocument doc;
    if (!gStorage.loadUsers(doc)) return 0;
    return doc["users"].as<JsonArray>().size();
}
