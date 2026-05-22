#pragma once

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include "config.h"

struct UserRecord {
    uint8_t fingerId = 0;
    char name[MAX_NAME_LEN] = {};
    char department[24] = {};
};

struct AttendanceRecord {
    uint8_t fingerId = 0;
    time_t timestamp = 0;
    bool checkIn = true;  // true = in, false = out
};

struct LastScanInfo {
    bool found = false;
    char name[MAX_NAME_LEN] = {};
    char timeStr[12] = {};   // HH:MM:SS
    bool checkIn = true;
};

enum class RecordFilter : uint8_t {
    Today = 0,
    Week = 1,
    All = 2,
};

struct AttendanceRecordView {
    AttendanceRecord rec{};
    char name[MAX_NAME_LEN] = {};
    char department[24] = {};
};

struct RecordsSummary {
    int total = 0;
    int checkIns = 0;
    int checkOuts = 0;
};

class StorageManager {
public:
    bool begin();
    bool loadUsers(JsonDocument &doc);
    bool saveUsers(const JsonDocument &doc);
    bool appendAttendance(const AttendanceRecord &rec);
    bool findUserByFingerId(uint8_t fingerId, UserRecord &out);
    bool upsertUser(const UserRecord &user);
    bool getLastScanToday(LastScanInfo &out);
    int loadAttendanceFiltered(RecordFilter filter, time_t dayAnchor,
                               AttendanceRecordView *out, int maxOut,
                               RecordsSummary *summary);

private:
    bool _mounted = false;
};

extern StorageManager gStorage;
