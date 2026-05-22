#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <WString.h>

#include "config.h"

// -----------------------------------------------------------------------------
// Domain types (canonical persistence)
// -----------------------------------------------------------------------------

struct AppConfig {
    char deviceName[32] = "Attendance Terminal 1";
    char wifiSSID[64] = {};
    char wifiPassword[64] = {};
    bool wifiEnabled = true;
    bool ntpEnabled = true;
    char ntpServer[64] = "pool.ntp.org";
    int timezone = 0;              // UTC offset in minutes (e.g. 330 = IST)
    int workStartHour = 9;
    int workStartMinute = 0;
    int workEndHour = 18;
    int workEndMinute = 0;
    int lateThresholdMinutes = 15;
    int attendanceMode = 1;        // 0 = Manual IN/OUT, 1 = Auto Toggle
    int displayBrightness = 200;   // 0–255
    int screenTimeout = 0;         // seconds, 0 = never
    bool soundEnabled = true;
    int adminSessionMinutes = 5;
    uint32_t configVersion = 0;
};

constexpr uint32_t CONFIG_VERSION_CURRENT = 1;

void resetConfigToDefaults(AppConfig &cfg);
bool validateConfig(const AppConfig &cfg);
void printConfigToSerial(const AppConfig &cfg);

struct User {
    uint16_t id = 0;
    uint16_t fingerprintId = 0;
    char name[MAX_NAME_LEN] = {};
    char department[24] = {};
    uint8_t colorIndex = 0;
    uint32_t enrolledAt = 0;
};

enum AttendanceStatus : uint8_t {
    ATT_STATUS_PRESENT = 0,
    ATT_STATUS_LATE = 1,
    ATT_STATUS_ABSENT = 2,
    ATT_STATUS_HALF_DAY = 3,
};

struct AttendanceRecord {
    uint32_t recordId = 0;
    uint16_t userId = 0;
    uint32_t checkInTime = 0;
    uint32_t checkOutTime = 0;
    uint8_t status = ATT_STATUS_PRESENT;
};

struct UserSummary {
    uint16_t userId = 0;
    uint32_t totalDays = 0;
    uint32_t totalLate = 0;
    uint32_t totalPresent = 0;
    uint32_t totalHalfDay = 0;
    uint32_t totalAbsent = 0;
    uint32_t totalWorkMinutes = 0;
};

// -----------------------------------------------------------------------------
// Core API
// -----------------------------------------------------------------------------

bool storageInit();

bool loadConfig(AppConfig &cfg);
bool saveConfig(const AppConfig &cfg);

bool saveUser(User &user);
bool loadUser(uint16_t id, User &out);
bool loadUserByFingerprint(uint16_t fingerprintId, User &out);
bool deleteUser(uint16_t id);
bool loadAllUsers(User *buf, int maxCount, int &count);
uint16_t getNextUserId();

bool saveAttendanceRecord(AttendanceRecord &rec);
bool updateAttendanceRecord(const AttendanceRecord &rec);
int loadDayRecords(time_t date, AttendanceRecord *buf, int maxCount);
uint32_t getNextRecordIdForDay(time_t date);

bool loadUserSummary(uint16_t userId, UserSummary &out);
void updateUserSummary(uint16_t userId, const AttendanceRecord &rec);

struct DailyReport {
    int totalEnrolled = 0;
    int presentToday = 0;
    int lateToday = 0;
    int absentToday = 0;
    int avgArrivalMin = -1;   // minutes from midnight, -1 if N/A
    int avgDurationMin = -1;  // average completed session minutes
};

DailyReport generateDailyReport(time_t date);

String exportDayCSV(time_t date);
String exportRangeCSV(time_t startDate, time_t endDate);
String exportUserSummaryCSV(time_t startDate, time_t endDate);

bool saveExportToFS(const String &csv, const char *filename);

size_t getUsedBytes();
size_t getTotalBytes();
int getUsedPercent();

// Admin PIN blob (/config/admin.dat)
bool storageLoadAdminPin(uint8_t *xorKey, size_t xorLen, uint8_t &checksum, uint8_t &length);
bool storageSaveAdminPin(const uint8_t *xorKey, size_t xorLen, uint8_t checksum, uint8_t length);

// -----------------------------------------------------------------------------
// Legacy compatibility (records UI, gradual migration)
// -----------------------------------------------------------------------------

struct UserRecord {
    uint8_t fingerId = 0;
    char name[MAX_NAME_LEN] = {};
    char department[24] = {};
};

struct AttendanceEvent {
    uint8_t fingerId = 0;
    time_t timestamp = 0;
    bool checkIn = true;
};

struct LastScanInfo {
    bool found = false;
    char name[MAX_NAME_LEN] = {};
    char timeStr[12] = {};
    bool checkIn = true;
};

enum class RecordFilter : uint8_t {
    Today = 0,
    Week = 1,
    All = 2,
};

struct AttendanceRecordView {
    AttendanceEvent rec{};
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
    bool begin() { return storageInit(); }
    bool loadUsers(JsonDocument &doc);
    bool saveUsers(const JsonDocument &doc);
    bool appendAttendance(const AttendanceEvent &rec);
    bool findUserByFingerId(uint8_t fingerId, UserRecord &out);
    bool upsertUser(const UserRecord &user);
    bool getLastScanToday(LastScanInfo &out);
    int loadAttendanceFiltered(RecordFilter filter, time_t dayAnchor,
                               AttendanceRecordView *out, int maxOut,
                               RecordsSummary *summary);
    bool getUserAttendanceStats(uint8_t fingerId, int &totalDays, char *avgArrival,
                                size_t avgLen);
    bool removeUser(uint8_t fingerId);
};

extern StorageManager gStorage;
