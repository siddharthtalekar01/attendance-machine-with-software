#include "attendance.h"
#include <TimeLib.h>
#include <cstring>

AttendanceManager gAttendance;

void AttendanceManager::begin() {
    memset(_lastWasCheckIn, 0, sizeof(_lastWasCheckIn));
}

ScanOutcome AttendanceManager::processScan() {
    ScanOutcome out;
    uint8_t slot = 0;
    uint16_t conf = 0;

    const FpResult fp = gFingerprint.identify(slot, conf);
    if (fp != FpResult::Ok) {
        out.message = gFingerprint.resultString(fp);
        return out;
    }

    UserRecord user;
    if (!gStorage.findUserByFingerId(slot, user)) {
        out.message = "Finger not registered";
        out.fingerId = slot;
        return out;
    }

    out.checkIn = !_lastWasCheckIn[slot];
    _lastWasCheckIn[slot] = out.checkIn;

    AttendanceRecord rec;
    rec.fingerId = slot;
    rec.timestamp = now();
    rec.checkIn = out.checkIn;

    if (!gStorage.appendAttendance(rec)) {
        out.message = "Log save failed";
        return out;
    }

    out.success = true;
    out.fingerId = slot;
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
