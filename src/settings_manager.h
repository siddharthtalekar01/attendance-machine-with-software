#pragma once

#include <Arduino.h>

struct AppSettings {
    char deviceName[32] = "Attendance";
    bool autoNtp = true;
    bool wifiEnabled = true;
    char ssid[32] = {};
    char wifiPassword[64] = {};
    int workStartMin = 9 * 60;      // 09:00
    int workEndMin = 18 * 60;       // 18:00
    int lateThresholdMin = 15;
    bool checkInAutoToggle = true;  // true = auto IN/OUT toggle
};

bool settingsLoad(AppSettings &out);
bool settingsSave(const AppSettings &settings);
void settingsFormatTime(int minutesFromMidnight, char *buf, size_t len);
int settingsCountEnrolledUsers();
