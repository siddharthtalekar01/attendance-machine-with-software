#pragma once

#include <Arduino.h>
#include <time.h>

#include "config.h"

// -----------------------------------------------------------------------------
// Core domain types (attendance business layer)
// -----------------------------------------------------------------------------

struct User {
    uint16_t fingerprintId = 0;
    char name[32] = {};
    char department[24] = {};
    uint8_t avatarColorIndex = 0;
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

enum ScanResultType : uint8_t {
    CHECKIN_OK = 0,
    CHECKIN_LATE,
    CHECKOUT_OK,
    ALREADY_IN,
    ALREADY_OUT,
    SCAN_UNKNOWN,
};

struct ScanResult {
    ScanResultType type = SCAN_UNKNOWN;
    User user{};
    AttendanceRecord record{};
    bool hasUser = false;
    bool hasRecord = false;
};

constexpr int ATT_MAX_DAILY_RECORDS = MAX_ENROLLED_FINGERS;
constexpr int ATT_HALF_DAY_MINUTES = 4 * 60;
constexpr int ATT_DUPLICATE_IN_SEC = 300;

// Last scan outcome (for UI / logging)
extern ScanResult gLastScanResult;

void attendanceBegin();

ScanResultType processAttendance(uint16_t fingerprintId, time_t now);

bool isLate(time_t checkInTime);
int getWorkDurationMinutes(const AttendanceRecord &rec);
void generateDailySummary(time_t date, int &present, int &late, int &absent);
AttendanceRecord *getTodayRecord(uint16_t userId);

void midnightReset();

const char *scanResultTypeString(ScanResultType type);

// -----------------------------------------------------------------------------
// UI bridge (fingerprint scan flow)
// -----------------------------------------------------------------------------

struct ScanOutcome {
    bool success = false;
    char userName[MAX_NAME_LEN] = {};
    uint8_t fingerId = 0;
    bool checkIn = true;
    const char *message = "";
};

class AttendanceManager {
public:
    void begin();
    ScanOutcome processScan();
    bool recordManual(uint8_t fingerId, bool checkIn);

private:
    int _lastDay = -1;
};

extern AttendanceManager gAttendance;
