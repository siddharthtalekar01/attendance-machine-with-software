#pragma once

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include "config.h"

struct UserRecord {
    uint8_t fingerId = 0;
    char name[MAX_NAME_LEN] = {};
};

struct AttendanceRecord {
    uint8_t fingerId = 0;
    time_t timestamp = 0;
    bool checkIn = true;  // true = in, false = out
};

class StorageManager {
public:
    bool begin();
    bool loadUsers(JsonDocument &doc);
    bool saveUsers(const JsonDocument &doc);
    bool appendAttendance(const AttendanceRecord &rec);
    bool findUserByFingerId(uint8_t fingerId, UserRecord &out);
    bool upsertUser(const UserRecord &user);

private:
    bool _mounted = false;
};

extern StorageManager gStorage;
