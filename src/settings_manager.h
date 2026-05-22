#pragma once

#include <Arduino.h>
#include "storage.h"

using AppSettings = AppConfig;

int settingsWorkStartMin(const AppSettings &s);
int settingsWorkEndMin(const AppSettings &s);
void settingsSetWorkStartMin(AppSettings &s, int minutesFromMidnight);
void settingsSetWorkEndMin(AppSettings &s, int minutesFromMidnight);

bool settingsLoad(AppSettings &out);
bool settingsSave(const AppSettings &settings);
void settingsApplyRuntime(const AppSettings &settings);
void settingsFormatTime(int minutesFromMidnight, char *buf, size_t len);
int settingsCountEnrolledUsers();
