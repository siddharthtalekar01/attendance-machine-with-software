#pragma once

#include <Arduino.h>
#include <time.h>

#include "config.h"
#include "storage.h"

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

extern ScanResult gLastScanResult;

void attendanceBegin();

ScanResultType processAttendance(uint16_t fingerprintId, time_t now);

bool isLate(time_t checkInTime);
int getWorkDurationMinutes(const AttendanceRecord &rec);
void generateDailySummary(time_t date, int &present, int &late, int &absent);
AttendanceRecord *getTodayRecord(uint16_t userId);

void midnightReset();

const char *scanResultTypeString(ScanResultType type);

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
