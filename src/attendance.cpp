#include "attendance.h"
#include <TimeLib.h>
#include <cstring>

AttendanceManager gAttendance;

void AttendanceManager::begin() {
    memset(_lastWasCheckIn, 0, sizeof(_lastWasCheckIn));
}

ScanOutcome AttendanceManager::processScan() {
    ScanOutcome out;
    uint16_t slot = 0;
    uint16_t conf = 0;

    const int fp = fingerprintSearch(slot, conf);
    if (fp != FP_SEARCH_OK) {
        out.message = fingerprintErrorString(fp);
        return out;
    }

    UserRecord user;
    if (!gStorage.findUserByFingerId(static_cast<uint8_t>(slot), user)) {
        out.message = "Finger not registered";
        out.fingerId = static_cast<uint8_t>(slot);
        return out;
    }

    const uint8_t fingerSlot = static_cast<uint8_t>(slot);
    out.checkIn = !_lastWasCheckIn[fingerSlot];
    _lastWasCheckIn[fingerSlot] = out.checkIn;

    AttendanceRecord rec;
    rec.fingerId = fingerSlot;
    rec.timestamp = now();
    rec.checkIn = out.checkIn;

    if (!gStorage.appendAttendance(rec)) {
        out.message = "Log save failed";
        return out;
    }

    out.success = true;
    out.fingerId = fingerSlot;
    strlcpy(out.userName, user.name, sizeof(out.userName));
    out.message = out.checkIn ? "Checked in" : "Checked out";
    return out;
}

bool AttendanceManager::recordManual(uint8_t fingerId, bool checkIn) {
    AttendanceRecord rec;
    rec.fingerId = fingerId;
    rec.timestamp = now();
    rec.checkIn = checkIn;
    if (gStorage.appendAttendance(rec)) {
        _lastWasCheckIn[fingerId] = checkIn;
        return true;
    }
    return false;
}
