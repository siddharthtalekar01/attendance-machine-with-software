#pragma once

#include "config.h"
#include "fingerprint.h"
#include "storage.h"

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
    bool _lastWasCheckIn[MAX_ENROLLED_FINGERS + 1] = {};
};

extern AttendanceManager gAttendance;
