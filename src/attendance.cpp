#include "attendance.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <TimeLib.h>
#include <cstring>

#include "fingerprint.h"
#include "settings_manager.h"
#include "storage.h"

AttendanceManager gAttendance;
ScanResult gLastScanResult;

namespace {

constexpr const char *DAILY_FILE = "/attendance/daily.json";
constexpr const char *ATT_DIR = "/attendance";

AppSettings s_settings;
AttendanceRecord s_todayRecords[ATT_MAX_DAILY_RECORDS];
int s_todayCount = 0;
uint32_t s_nextRecordId = 1;
int s_cachedDay = -1;
uint32_t s_lastMidnightCheck = 0;

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

static bool ensureAttendanceDir() {
    if (!LittleFS.exists(ATT_DIR)) {
        return LittleFS.mkdir(ATT_DIR);
    }
    return true;
}

static void syncDayCache(time_t now) {
    const int d = calendarDay(now);
    if (d == s_cachedDay) return;
    s_cachedDay = d;
    s_todayCount = 0;
    s_nextRecordId = 1;

    if (!LittleFS.exists(DAILY_FILE)) return;

    File f = LittleFS.open(DAILY_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close();
        return;
    }
    f.close();

    const int fileDay = doc["day"] | 0;
    if (fileDay != d) return;

    s_nextRecordId = doc["nextRecordId"] | 1;
    for (JsonObject o : doc["records"].as<JsonArray>()) {
        if (s_todayCount >= ATT_MAX_DAILY_RECORDS) break;
        AttendanceRecord &r = s_todayRecords[s_todayCount++];
        r.recordId = o["id"] | 0;
        r.userId = o["userId"] | 0;
        r.checkInTime = o["in"] | 0;
        r.checkOutTime = o["out"] | 0;
        r.status = o["status"] | ATT_STATUS_PRESENT;
    }
}

static bool saveTodayRecords(time_t now) {
    if (!ensureAttendanceDir()) return false;

    JsonDocument doc;
    doc["day"] = calendarDay(now);
    doc["nextRecordId"] = s_nextRecordId;
    JsonArray arr = doc["records"].to<JsonArray>();
    for (int i = 0; i < s_todayCount; i++) {
        const AttendanceRecord &r = s_todayRecords[i];
        JsonObject o = arr.add<JsonObject>();
        o["id"] = r.recordId;
        o["userId"] = r.userId;
        o["in"] = r.checkInTime;
        o["out"] = r.checkOutTime;
        o["status"] = r.status;
    }

    File f = LittleFS.open(DAILY_FILE, FILE_WRITE);
    if (!f) return false;
    const bool ok = serializeJson(doc, f) > 0;
    f.close();
    return ok;
}

static AttendanceRecord *findTodayRecordMutable(uint16_t userId) {
    for (int i = 0; i < s_todayCount; i++) {
        if (s_todayRecords[i].userId == userId) return &s_todayRecords[i];
    }
    return nullptr;
}

static bool lookupUser(uint16_t fingerprintId, User &out) {
    UserRecord rec;
    if (!gStorage.findUserByFingerId(static_cast<uint8_t>(fingerprintId), rec)) {
        return false;
    }
    out.fingerprintId = fingerprintId;
    strlcpy(out.name, rec.name, sizeof(out.name));
    strlcpy(out.department, rec.department, sizeof(out.department));
    out.avatarColorIndex = deptColorIndex(rec.department);

    JsonDocument doc;
    if (gStorage.loadUsers(doc)) {
        for (JsonObject u : doc["users"].as<JsonArray>()) {
            if ((u["fingerId"] | 0) == fingerprintId) {
                out.enrolledAt = u["enrollTs"] | 0;
                break;
            }
        }
    }
    return true;
}

static void appendLegacyEvent(uint16_t userId, time_t ts, bool checkIn) {
    AttendanceEvent ev;
    ev.fingerId = static_cast<uint8_t>(userId);
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

}  // namespace

void attendanceBegin() {
    settingsLoad(s_settings);
    s_todayCount = 0;
    s_cachedDay = -1;
    s_nextRecordId = 1;
    syncDayCache(now());
    s_lastMidnightCheck = millis();
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
    const int targetDay = calendarDay(date);

    if (!LittleFS.exists(DAILY_FILE)) return;

    File f = LittleFS.open(DAILY_FILE, FILE_READ);
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close();
        return;
    }
    f.close();

    if ((doc["day"] | 0) != targetDay) return;

    for (JsonObject o : doc["records"].as<JsonArray>()) {
        const uint8_t st = o["status"] | ATT_STATUS_PRESENT;
        if (st == ATT_STATUS_ABSENT) absent++;
        else if (st == ATT_STATUS_LATE) late++;
        else present++;
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

ScanResultType processAttendance(uint16_t fingerprintId, time_t now) {
    settingsLoad(s_settings);

    if (fingerprintId == 0) {
        fillScanResult(SCAN_UNKNOWN, nullptr, nullptr);
        return SCAN_UNKNOWN;
    }

    syncDayCache(now);

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
        rec->recordId = s_nextRecordId++;
        rec->userId = fingerprintId;
        rec->checkInTime = (uint32_t)now;
        rec->checkOutTime = 0;
        rec->status = isLate(now) ? ATT_STATUS_LATE : ATT_STATUS_PRESENT;

        saveTodayRecords(now);
        appendLegacyEvent(fingerprintId, now, true);

        const ScanResultType type = (rec->status == ATT_STATUS_LATE) ? CHECKIN_LATE : CHECKIN_OK;
        fillScanResult(type, &user, rec);
        return type;
    }

    if (rec->checkInTime > 0 && rec->checkOutTime == 0) {
        if ((uint32_t)now - rec->checkInTime < ATT_DUPLICATE_IN_SEC) {
            fillScanResult(ALREADY_IN, &user, rec);
            return ALREADY_IN;
        }

        rec->checkOutTime = (uint32_t)now;
        const int duration = getWorkDurationMinutes(*rec);
        if (duration > 0 && duration < ATT_HALF_DAY_MINUTES) {
            rec->status = ATT_STATUS_HALF_DAY;
        }

        saveTodayRecords(now);
        appendLegacyEvent(fingerprintId, now, false);

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
        int present = 0, late = 0, absent = 0;

        for (int i = 0; i < s_todayCount; i++) {
            AttendanceRecord &r = s_todayRecords[i];
            if (r.checkInTime > 0 && r.checkOutTime == 0) {
                r.checkOutTime = (uint32_t)dayStart(t) - 1;
                const int duration = getWorkDurationMinutes(r);
                if (duration > 0 && duration < ATT_HALF_DAY_MINUTES) {
                    r.status = ATT_STATUS_HALF_DAY;
                }
            }
            if (r.status == ATT_STATUS_ABSENT) absent++;
            else if (r.status == ATT_STATUS_LATE) late++;
            else if (r.checkInTime > 0) present++;
        }

        char path[48];
        snprintf(path, sizeof(path), "/attendance/summary_%d.json", s_cachedDay);
        if (ensureAttendanceDir()) {
            JsonDocument sum;
            sum["present"] = present;
            sum["late"] = late;
            sum["absent"] = absent;
            File f = LittleFS.open(path, FILE_WRITE);
            if (f) {
                serializeJson(sum, f);
                f.close();
            }
        }

        saveTodayRecords(t - 86400);

        Serial.printf("[Attendance] Midnight reset: %d -> %d (P:%d L:%d A:%d)\n", s_cachedDay, today,
                      present, late, absent);

        s_todayCount = 0;
        s_nextRecordId = 1;
        s_cachedDay = today;

        if (LittleFS.exists(DAILY_FILE)) {
            LittleFS.remove(DAILY_FILE);
        }
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
