#include "attendance.h"

#include <TimeLib.h>
#include <cstring>

#include "fingerprint.h"
#include "settings_manager.h"
#include "storage.h"

AttendanceManager gAttendance;
ScanResult gLastScanResult;

namespace {

AppConfig s_settings;
AttendanceRecord s_todayRecords[ATT_MAX_DAILY_RECORDS];
int s_todayCount = 0;
int s_cachedDay = -1;

static int calendarDay(time_t t) {
    if (timeStatus() == timeNotSet) return 0;
    return (int)year(t) * 10000 + (int)month(t) * 100 + (int)day(t);
}

static time_t dayStart(time_t t) {
    if (timeStatus() == timeNotSet) return t;
    tmElements_t te;
    breakTime(t, te);
    te.Hour = 0;
    te.Minute = 0;
    te.Second = 0;
    return makeTime(te);
}

static uint8_t deptColorIndex(const char *dept) {
    static const char *names[] = {"HR", "Engineering", "Sales", "Operations", "Management"};
    for (uint8_t i = 0; i < 5; i++) {
        if (dept && strcmp(dept, names[i]) == 0) return i;
    }
    return 5;
}

static void syncDayCache(time_t nowTs) {
    const int d = calendarDay(nowTs);
    if (d == s_cachedDay) return;
    s_cachedDay = d;
    s_todayCount = loadDayRecords(nowTs, s_todayRecords, ATT_MAX_DAILY_RECORDS);
}

static AttendanceRecord *findTodayRecordMutable(uint16_t fingerprintId) {
    User u;
    const uint16_t uid = loadUserByFingerprint(fingerprintId, u) ? u.id : fingerprintId;

    for (int i = 0; i < s_todayCount; i++) {
        if (s_todayRecords[i].userId == uid || s_todayRecords[i].userId == fingerprintId) {
            return &s_todayRecords[i];
        }
    }
    return nullptr;
}

static bool lookupUser(uint16_t fingerprintId, User &out) {
    if (!loadUserByFingerprint(fingerprintId, out)) {
        return false;
    }
    out.fingerprintId = fingerprintId;
    out.colorIndex = deptColorIndex(out.department);
    return true;
}

static void appendLegacyEvent(uint16_t fingerprintId, time_t ts, bool checkIn) {
    AttendanceEvent ev;
    ev.fingerId = static_cast<uint8_t>(fingerprintId);
    ev.timestamp = ts;
    ev.checkIn = checkIn;
    gStorage.appendAttendance(ev);
}

static void fillScanResult(ScanResultType type, const User *user, const AttendanceRecord *rec) {
    gLastScanResult.type = type;
    gLastScanResult.hasUser = (user != nullptr);
    gLastScanResult.hasRecord = (rec != nullptr);
    if (user) gLastScanResult.user = *user;
    if (rec) gLastScanResult.record = *rec;
}

static void reloadConfig() {
    AppConfig cfg;
    if (loadConfig(cfg)) {
        s_settings = cfg;
    }
}

}  // namespace

void attendanceBegin() {
    reloadConfig();
    s_cachedDay = -1;
    syncDayCache(now());
}

bool isLate(time_t checkInTime) {
    if (checkInTime == 0 || timeStatus() == timeNotSet) return false;

    const time_t day0 = dayStart(checkInTime);
    const int checkInMin = (int)((checkInTime - day0) / 60);
    const int deadline = s_settings.workStartMin + s_settings.lateThresholdMin;
    return checkInMin > deadline;
}

int getWorkDurationMinutes(const AttendanceRecord &rec) {
    if (rec.checkInTime == 0 || rec.checkOutTime == 0) return 0;
    if (rec.checkOutTime <= rec.checkInTime) return 0;
    return (int)((rec.checkOutTime - rec.checkInTime) / 60);
}

void generateDailySummary(time_t date, int &present, int &late, int &absent) {
    present = late = absent = 0;

    AttendanceRecord recs[ATT_MAX_DAILY_RECORDS];
    const int n = loadDayRecords(date, recs, ATT_MAX_DAILY_RECORDS);
    for (int i = 0; i < n; i++) {
        const uint8_t st = recs[i].status;
        if (st == ATT_STATUS_ABSENT) absent++;
        else if (st == ATT_STATUS_LATE) late++;
        else if (recs[i].checkInTime > 0) present++;
    }
}

AttendanceRecord *getTodayRecord(uint16_t userId) {
    syncDayCache(now());
    return findTodayRecordMutable(userId);
}

const char *scanResultTypeString(ScanResultType type) {
    switch (type) {
        case CHECKIN_OK: return "Checked in";
        case CHECKIN_LATE: return "Checked in (late)";
        case CHECKOUT_OK: return "Checked out";
        case ALREADY_IN: return "Already checked in";
        case ALREADY_OUT: return "Already checked out";
        case SCAN_UNKNOWN: return "Unknown user";
        default: return "Error";
    }
}

ScanResultType processAttendance(uint16_t fingerprintId, time_t nowTs) {
    reloadConfig();

    if (fingerprintId == 0) {
        fillScanResult(SCAN_UNKNOWN, nullptr, nullptr);
        return SCAN_UNKNOWN;
    }

    syncDayCache(nowTs);

    User user;
    if (!lookupUser(fingerprintId, user)) {
        fillScanResult(SCAN_UNKNOWN, nullptr, nullptr);
        return SCAN_UNKNOWN;
    }

    AttendanceRecord *rec = findTodayRecordMutable(fingerprintId);

    if (rec == nullptr) {
        if (s_todayCount >= ATT_MAX_DAILY_RECORDS) {
            fillScanResult(SCAN_UNKNOWN, &user, nullptr);
            return SCAN_UNKNOWN;
        }
        rec = &s_todayRecords[s_todayCount++];
        rec->recordId = 0;
        rec->userId = user.id ? user.id : fingerprintId;
        rec->checkInTime = (uint32_t)nowTs;
        rec->checkOutTime = 0;
        rec->status = isLate(nowTs) ? ATT_STATUS_LATE : ATT_STATUS_PRESENT;

        saveAttendanceRecord(*rec);
        appendLegacyEvent(fingerprintId, nowTs, true);

        const ScanResultType type = (rec->status == ATT_STATUS_LATE) ? CHECKIN_LATE : CHECKIN_OK;
        fillScanResult(type, &user, rec);
        return type;
    }

    if (rec->checkInTime > 0 && rec->checkOutTime == 0) {
        if ((uint32_t)nowTs - rec->checkInTime < ATT_DUPLICATE_IN_SEC) {
            fillScanResult(ALREADY_IN, &user, rec);
            return ALREADY_IN;
        }

        rec->checkOutTime = (uint32_t)nowTs;
        const int duration = getWorkDurationMinutes(*rec);
        if (duration > 0 && duration < ATT_HALF_DAY_MINUTES) {
            rec->status = ATT_STATUS_HALF_DAY;
        }

        updateAttendanceRecord(*rec);
        appendLegacyEvent(fingerprintId, nowTs, false);

        fillScanResult(CHECKOUT_OK, &user, rec);
        return CHECKOUT_OK;
    }

    fillScanResult(ALREADY_OUT, &user, rec);
    return ALREADY_OUT;
}

void midnightReset() {
    const time_t t = now();
    const int today = calendarDay(t);

    if (s_cachedDay >= 0 && s_cachedDay != today) {
        for (int i = 0; i < s_todayCount; i++) {
            AttendanceRecord &r = s_todayRecords[i];
            if (r.checkInTime > 0 && r.checkOutTime == 0) {
                r.checkOutTime = (uint32_t)dayStart(t) - 1;
                const int duration = getWorkDurationMinutes(r);
                if (duration > 0 && duration < ATT_HALF_DAY_MINUTES) {
                    r.status = ATT_STATUS_HALF_DAY;
                }
                updateAttendanceRecord(r);
            }
        }

        Serial.printf("[Attendance] Midnight reset: %d -> %d\n", s_cachedDay, today);
        s_todayCount = 0;
        s_cachedDay = today;
    } else if (s_cachedDay < 0) {
        s_cachedDay = today;
    }

    syncDayCache(t);
}

void AttendanceManager::begin() {
    attendanceBegin();
    _lastDay = calendarDay(now());
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

    const ScanResultType type = processAttendance(slot, now());
    out.fingerId = static_cast<uint8_t>(slot);

    if (gLastScanResult.hasUser) {
        strlcpy(out.userName, gLastScanResult.user.name, sizeof(out.userName));
    }

    out.message = scanResultTypeString(type);
    out.success = (type == CHECKIN_OK || type == CHECKIN_LATE || type == CHECKOUT_OK);
    out.checkIn = (type == CHECKIN_OK || type == CHECKIN_LATE);

    return out;
}

bool AttendanceManager::recordManual(uint8_t fingerId, bool checkIn) {
    if (checkIn) {
        AttendanceRecord *rec = getTodayRecord(fingerId);
        if (rec && rec->checkInTime > 0 && rec->checkOutTime == 0) {
            return false;
        }
        processAttendance(fingerId, now());
        return gLastScanResult.type == CHECKIN_OK || gLastScanResult.type == CHECKIN_LATE;
    }

    AttendanceRecord *rec = getTodayRecord(fingerId);
    if (!rec || rec->checkInTime == 0 || rec->checkOutTime > 0) {
        return false;
    }
    processAttendance(fingerId, now());
    return gLastScanResult.type == CHECKOUT_OK;
}
