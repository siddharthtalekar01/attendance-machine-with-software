#include "storage.h"

#include <TimeLib.h>
#include <cstring>

StorageManager gStorage;

namespace {

constexpr const char *DIR_CONFIG = "/config";
constexpr const char *DIR_USERS = "/users";
constexpr const char *DIR_RECORDS = "/records";
constexpr const char *FILE_SETTINGS = "/config/settings.json";
constexpr const char *FILE_USERS_INDEX = "/users/index.json";
constexpr const char *FILE_SUMMARY = "/records/summary.json";
constexpr const char *FILE_ADMIN_DAT = "/config/admin.dat";
constexpr const char *LEGACY_USERS = "/users.json";
constexpr const char *LEGACY_LOG = "/attendance.json";

constexpr size_t DAY_FILE_MAX_BYTES = 32 * 1024;
constexpr size_t VERIFY_BUF_SIZE = 512;

static bool s_mounted = false;

bool ensureDir(const char *path) {
    if (LittleFS.exists(path)) return true;
    return LittleFS.mkdir(path);
}

bool ensureDirs() {
    return ensureDir(DIR_CONFIG) && ensureDir(DIR_USERS) && ensureDir(DIR_RECORDS);
}

bool writeFileVerified(const char *path, const uint8_t *data, size_t len) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    File wf = LittleFS.open(tmp, FILE_WRITE);
    if (!wf) return false;
    const size_t written = wf.write(data, len);
    wf.close();
    if (written != len) {
        LittleFS.remove(tmp);
        return false;
    }

    File rf = LittleFS.open(tmp, FILE_READ);
    if (!rf) {
        LittleFS.remove(tmp);
        return false;
    }

    uint8_t verify[VERIFY_BUF_SIZE];
    size_t offset = 0;
    while (offset < len) {
        const size_t chunk = min(len - offset, VERIFY_BUF_SIZE);
        const size_t rd = rf.read(verify, chunk);
        if (rd != chunk || memcmp(verify, data + offset, chunk) != 0) {
            rf.close();
            LittleFS.remove(tmp);
            return false;
        }
        offset += chunk;
    }
    rf.close();

    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
    }
    return LittleFS.rename(tmp, path);
}

bool writeJsonVerified(const char *path, JsonDocument &doc) {
    const size_t need = measureJson(doc);
    if (need == 0) return false;

    uint8_t *buf = (uint8_t *)malloc(need + 4);
    if (!buf) return false;

    const size_t len = serializeJson(doc, buf, need + 4);
    const bool ok = (len > 0) && writeFileVerified(path, buf, len);
    free(buf);
    return ok;
}

bool readJsonFile(const char *path, JsonDocument &doc) {
    if (!LittleFS.exists(path)) return false;
    File f = LittleFS.open(path, FILE_READ);
    if (!f) return false;
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    return !err;
}

void dayPath(time_t date, char *buf, size_t len, int part = 0) {
    if (timeStatus() == timeNotSet) {
        snprintf(buf, len, "%s/unknown.json", DIR_RECORDS);
        return;
    }
    if (part <= 0) {
        snprintf(buf, len, "%s/%04d-%02d-%02d.json", DIR_RECORDS,
                 year(date), month(date), day(date));
    } else {
        snprintf(buf, len, "%s/%04d-%02d-%02d_p%d.json", DIR_RECORDS,
                 year(date), month(date), day(date), part);
    }
}

time_t parseDayPath(const char *name) {
    int y = 0, m = 0, d = 0;
    if (sscanf(name, "%d-%d-%d", &y, &m, &d) != 3) return 0;
    tmElements_t te;
    te.Year = y - 1970;
    te.Month = m;
    te.Day = d;
    te.Hour = 0;
    te.Minute = 0;
    te.Second = 0;
    return makeTime(te);
}

void recordToJson(JsonObject o, const AttendanceRecord &r) {
    o["id"] = r.recordId;
    o["userId"] = r.userId;
    o["in"] = r.checkInTime;
    o["out"] = r.checkOutTime;
    o["status"] = r.status;
}

void recordFromJson(JsonObject o, AttendanceRecord &r) {
    r.recordId = o["id"] | 0;
    r.userId = o["userId"] | 0;
    r.checkInTime = o["in"] | 0;
    r.checkOutTime = o["out"] | 0;
    r.status = o["status"] | ATT_STATUS_PRESENT;
}

void userToJson(JsonObject o, const User &u) {
    o["id"] = u.id;
    o["name"] = u.name;
    o["dept"] = u.department;
    o["fingerprintId"] = u.fingerprintId;
    o["enrolledAt"] = u.enrolledAt;
    o["colorIndex"] = u.colorIndex;
}

void userFromJson(JsonObject o, User &u) {
    u.id = o["id"] | 0;
    strlcpy(u.name, o["name"] | "", sizeof(u.name));
    strlcpy(u.department, o["dept"] | o["department"] | "", sizeof(u.department));
    u.fingerprintId = o["fingerprintId"] | o["fingerId"] | 0;
    u.enrolledAt = o["enrolledAt"] | o["enrollTs"] | 0;
    u.colorIndex = o["colorIndex"] | 0;
}

bool loadUsersIndex(JsonDocument &doc) {
    if (readJsonFile(FILE_USERS_INDEX, doc)) {
        return doc["users"].is<JsonArray>();
    }
    doc.clear();
    doc["users"].to<JsonArray>();
    return true;
}

bool saveUsersIndex(JsonDocument &doc) {
    return writeJsonVerified(FILE_USERS_INDEX, doc);
}

bool migrateLegacyUsers() {
    if (!LittleFS.exists(LEGACY_USERS)) return true;

    JsonDocument legacy;
    if (!readJsonFile(LEGACY_USERS, legacy)) return false;

    JsonDocument doc;
    loadUsersIndex(doc);
    JsonArray users = doc["users"].to<JsonArray>();

    uint16_t nextId = 1;
    for (JsonObject u : users) {
        nextId = max(nextId, (uint16_t)((u["id"] | 0) + 1));
    }

    for (JsonObject o : legacy["users"].as<JsonArray>()) {
        const uint16_t fp = o["fingerId"] | 0;
        bool exists = false;
        for (JsonObject u : users) {
            if ((u["fingerprintId"] | u["fingerId"] | 0) == fp) {
                exists = true;
                break;
            }
        }
        if (exists) continue;

        JsonObject nu = users.add<JsonObject>();
        nu["id"] = nextId++;
        nu["fingerprintId"] = fp;
        nu["name"] = o["name"] | "";
        nu["dept"] = o["department"] | "";
        nu["enrolledAt"] = o["enrollTs"] | (uint32_t)now();
        nu["colorIndex"] = 0;
    }

    if (!saveUsersIndex(doc)) return false;
    LittleFS.remove(LEGACY_USERS);
    return true;
}

bool loadDayDoc(time_t date, int part, JsonDocument &doc) {
    char path[48];
    dayPath(date, path, sizeof(path), part);
    if (!readJsonFile(path, doc)) {
        doc.clear();
        doc["nextRecordId"] = 1;
        doc["records"].to<JsonArray>();
        return false;
    }
    if (!doc["records"].is<JsonArray>()) {
        doc["records"].to<JsonArray>();
    }
    return true;
}

bool saveDayDoc(time_t date, int part, JsonDocument &doc) {
    char path[48];
    dayPath(date, path, sizeof(path), part);

    const size_t need = measureJson(doc);
    if (need > DAY_FILE_MAX_BYTES && part == 0) {
        return false;
    }

    return writeJsonVerified(path, doc);
}

int appendRecordToDay(time_t date, AttendanceRecord &rec) {
    for (int part = 0; part < 8; part++) {
        JsonDocument doc;
        loadDayDoc(date, part, doc);
        JsonArray arr = doc["records"].as<JsonArray>();

        if (rec.recordId == 0) {
            rec.recordId = doc["nextRecordId"] | 1;
            doc["nextRecordId"] = rec.recordId + 1;
        }

        JsonObject o = arr.add<JsonObject>();
        recordToJson(o, rec);

        if (measureJson(doc) <= DAY_FILE_MAX_BYTES) {
            if (!saveDayDoc(date, part, doc)) return -1;
            return part;
        }

        arr.remove(arr.size() - 1);
    }
    return -1;
}

bool findRecordInDay(time_t date, uint32_t recordId, AttendanceRecord &out, int &partOut) {
    for (int part = 0; part < 8; part++) {
        char path[48];
        dayPath(date, path, sizeof(path), part);
        if (!LittleFS.exists(path)) {
            if (part == 0) return false;
            break;
        }

        JsonDocument doc;
        if (!readJsonFile(path, doc)) continue;

        for (JsonObject o : doc["records"].as<JsonArray>()) {
            if ((o["id"] | 0) == recordId) {
                recordFromJson(o, out);
                partOut = part;
                return true;
            }
        }
    }
    return false;
}

void summaryEnsureUser(JsonDocument &doc, uint16_t userId) {
    JsonArray users = doc["users"].to<JsonArray>();
    for (JsonObject o : users) {
        if ((o["userId"] | 0) == userId) return;
    }
    JsonObject nu = users.add<JsonObject>();
    nu["userId"] = userId;
    nu["totalDays"] = 0;
    nu["totalLate"] = 0;
    nu["totalPresent"] = 0;
    nu["totalHalfDay"] = 0;
    nu["totalAbsent"] = 0;
    nu["totalWorkMinutes"] = 0;
}

}  // namespace

bool storageInit() {
    if (!LittleFS.begin(true)) {
        s_mounted = false;
        return false;
    }
    s_mounted = true;

    if (!ensureDirs()) return false;

    if (!LittleFS.exists(FILE_SETTINGS)) {
        AppConfig cfg;
        saveConfig(cfg);
    }
    if (!LittleFS.exists(FILE_USERS_INDEX)) {
        JsonDocument doc;
        doc["users"].to<JsonArray>();
        saveUsersIndex(doc);
    }
    if (!LittleFS.exists(FILE_SUMMARY)) {
        JsonDocument doc;
        doc["users"].to<JsonArray>();
        writeJsonVerified(FILE_SUMMARY, doc);
    }

    migrateLegacyUsers();

    if (!LittleFS.exists(LEGACY_LOG)) {
        File f = LittleFS.open(LEGACY_LOG, FILE_WRITE);
        if (f) {
            f.print("[]");
            f.close();
        }
    }

    return true;
}

bool loadConfig(AppConfig &cfg) {
    cfg = AppConfig{};
    if (!s_mounted) return false;

    JsonDocument doc;
    if (!readJsonFile(FILE_SETTINGS, doc)) {
        return saveConfig(cfg);
    }

    strlcpy(cfg.deviceName, doc["deviceName"] | "Attendance", sizeof(cfg.deviceName));
    cfg.autoNtp = doc["autoNtp"] | true;
    cfg.wifiEnabled = doc["wifiEnabled"] | true;
    strlcpy(cfg.ssid, doc["ssid"] | "", sizeof(cfg.ssid));
    strlcpy(cfg.wifiPassword, doc["wifiPassword"] | "", sizeof(cfg.wifiPassword));
    cfg.workStartMin = doc["workStartMin"] | (9 * 60);
    cfg.workEndMin = doc["workEndMin"] | (18 * 60);
    cfg.lateThresholdMin = doc["lateThresholdMin"] | 15;
    cfg.checkInAutoToggle = doc["checkInAutoToggle"] | true;
    return true;
}

bool saveConfig(const AppConfig &cfg) {
    if (!s_mounted) return false;

    JsonDocument doc;
    doc["deviceName"] = cfg.deviceName;
    doc["autoNtp"] = cfg.autoNtp;
    doc["wifiEnabled"] = cfg.wifiEnabled;
    doc["ssid"] = cfg.ssid;
    doc["wifiPassword"] = cfg.wifiPassword;
    doc["workStartMin"] = cfg.workStartMin;
    doc["workEndMin"] = cfg.workEndMin;
    doc["lateThresholdMin"] = cfg.lateThresholdMin;
    doc["checkInAutoToggle"] = cfg.checkInAutoToggle;
    return writeJsonVerified(FILE_SETTINGS, doc);
}

uint16_t getNextUserId() {
    JsonDocument doc;
    if (!loadUsersIndex(doc)) return 1;

    uint16_t maxId = 0;
    for (JsonObject u : doc["users"].as<JsonArray>()) {
        maxId = max(maxId, (uint16_t)(u["id"] | 0));
    }
    return maxId + 1;
}

bool saveUser(User &user) {
    if (!s_mounted) return false;

    JsonDocument doc;
    if (!loadUsersIndex(doc)) return false;
    JsonArray users = doc["users"].as<JsonArray>();

    if (user.id == 0) {
        user.id = getNextUserId();
        if (user.enrolledAt == 0) {
            user.enrolledAt = (uint32_t)now();
        }
        JsonObject o = users.add<JsonObject>();
        userToJson(o, user);
        return saveUsersIndex(doc);
    }

    for (JsonObject o : users) {
        if ((o["id"] | 0) == user.id) {
            userToJson(o, user);
            return saveUsersIndex(doc);
        }
    }

    JsonObject o = users.add<JsonObject>();
    userToJson(o, user);
    return saveUsersIndex(doc);
}

bool loadUser(uint16_t id, User &out) {
    out = User{};
    if (!s_mounted || id == 0) return false;

    JsonDocument doc;
    if (!loadUsersIndex(doc)) return false;

    for (JsonObject o : doc["users"].as<JsonArray>()) {
        if ((o["id"] | 0) == id) {
            userFromJson(o, out);
            return true;
        }
    }
    return false;
}

bool loadUserByFingerprint(uint16_t fingerprintId, User &out) {
    out = User{};
    if (!s_mounted || fingerprintId == 0) return false;

    JsonDocument doc;
    if (!loadUsersIndex(doc)) return false;

    for (JsonObject o : doc["users"].as<JsonArray>()) {
        if ((o["fingerprintId"] | o["fingerId"] | 0) == fingerprintId) {
            userFromJson(o, out);
            return true;
        }
    }
    return false;
}

bool deleteUser(uint16_t id) {
    if (!s_mounted || id == 0) return false;

    JsonDocument doc;
    if (!loadUsersIndex(doc)) return false;

    JsonArray users = doc["users"].as<JsonArray>();
    for (size_t i = 0; i < users.size(); i++) {
        if ((users[i]["id"] | 0) == id) {
            users.remove(i);
            return saveUsersIndex(doc);
        }
    }
    return false;
}

bool loadAllUsers(User *buf, int maxCount, int &count) {
    count = 0;
    if (!buf || maxCount <= 0 || !s_mounted) return false;

    JsonDocument doc;
    if (!loadUsersIndex(doc)) return false;

    for (JsonObject o : doc["users"].as<JsonArray>()) {
        if (count >= maxCount) break;
        userFromJson(o, buf[count++]);
    }
    return true;
}

uint32_t getNextRecordIdForDay(time_t date) {
    JsonDocument doc;
    loadDayDoc(date, 0, doc);
    return doc["nextRecordId"] | 1;
}

bool saveAttendanceRecord(AttendanceRecord &rec) {
    if (!s_mounted) return false;
    const time_t day = (rec.checkInTime > 0) ? (time_t)rec.checkInTime : now();
    if (appendRecordToDay(day, rec) < 0) return false;
    updateUserSummary(rec.userId, rec);
    return true;
}

bool updateAttendanceRecord(const AttendanceRecord &rec) {
    if (!s_mounted || rec.recordId == 0) return false;

    const time_t day = (rec.checkInTime > 0) ? (time_t)rec.checkInTime : now();
    int part = 0;
    AttendanceRecord existing;
    if (!findRecordInDay(day, rec.recordId, existing, part)) {
        AttendanceRecord mut = rec;
        return saveAttendanceRecord(mut);
    }

    char path[48];
    dayPath(day, path, sizeof(path), part);

    JsonDocument doc;
    if (!readJsonFile(path, doc)) return false;

    JsonArray arr = doc["records"].as<JsonArray>();
    for (JsonObject o : arr) {
        if ((o["id"] | 0) == rec.recordId) {
            recordToJson(o, rec);
            if (!saveDayDoc(day, part, doc)) return false;
            updateUserSummary(rec.userId, rec);
            return true;
        }
    }
    return false;
}

int loadDayRecords(time_t date, AttendanceRecord *buf, int maxCount) {
    if (!buf || maxCount <= 0 || !s_mounted) return 0;

    int count = 0;
    for (int part = 0; part < 8; part++) {
        char path[48];
        dayPath(date, path, sizeof(path), part);
        if (!LittleFS.exists(path)) {
            if (part == 0) break;
            continue;
        }

        JsonDocument doc;
        if (!readJsonFile(path, doc)) continue;

        for (JsonObject o : doc["records"].as<JsonArray>()) {
            if (count >= maxCount) return count;
            recordFromJson(o, buf[count++]);
        }
    }
    return count;
}

bool loadUserSummary(uint16_t userId, UserSummary &out) {
    out = UserSummary{};
    out.userId = userId;
    if (!s_mounted || userId == 0) return false;

    JsonDocument doc;
    if (!readJsonFile(FILE_SUMMARY, doc)) return false;

    for (JsonObject o : doc["users"].as<JsonArray>()) {
        if ((o["userId"] | 0) == userId) {
            out.totalDays = o["totalDays"] | 0;
            out.totalLate = o["totalLate"] | 0;
            out.totalPresent = o["totalPresent"] | 0;
            out.totalHalfDay = o["totalHalfDay"] | 0;
            out.totalAbsent = o["totalAbsent"] | 0;
            out.totalWorkMinutes = o["totalWorkMinutes"] | 0;
            return true;
        }
    }
    return false;
}

void updateUserSummary(uint16_t userId, const AttendanceRecord &rec) {
    if (!s_mounted || userId == 0) return;

    JsonDocument doc;
    if (!readJsonFile(FILE_SUMMARY, doc)) {
        doc["users"].to<JsonArray>();
    }

    summaryEnsureUser(doc, userId);
    JsonArray users = doc["users"].as<JsonArray>();

    for (JsonObject o : users) {
        if ((o["userId"] | 0) != userId) continue;

        if (rec.checkOutTime > 0 && rec.checkInTime > 0) {
            const uint32_t mins = (rec.checkOutTime - rec.checkInTime) / 60;
            o["totalWorkMinutes"] = (o["totalWorkMinutes"] | 0) + mins;
            o["totalDays"] = (o["totalDays"] | 0) + 1;

            if (rec.status == ATT_STATUS_LATE) {
                o["totalLate"] = (o["totalLate"] | 0) + 1;
            } else if (rec.status == ATT_STATUS_HALF_DAY) {
                o["totalHalfDay"] = (o["totalHalfDay"] | 0) + 1;
            } else if (rec.status == ATT_STATUS_ABSENT) {
                o["totalAbsent"] = (o["totalAbsent"] | 0) + 1;
            } else {
                o["totalPresent"] = (o["totalPresent"] | 0) + 1;
            }
        }
        break;
    }

    writeJsonVerified(FILE_SUMMARY, doc);
}

String exportDayCSV(time_t date) {
    String csv = "recordId,userId,checkIn,checkOut,status\n";

    AttendanceRecord recs[64];
    const int n = loadDayRecords(date, recs, 64);
    for (int i = 0; i < n; i++) {
        const AttendanceRecord &r = recs[i];
        csv += String(r.recordId) + ",";
        csv += String(r.userId) + ",";
        csv += String(r.checkInTime) + ",";
        csv += String(r.checkOutTime) + ",";
        csv += String(r.status) + "\n";
    }
    return csv;
}

size_t getUsedBytes() {
    if (!s_mounted) return 0;
    return LittleFS.usedBytes();
}

size_t getTotalBytes() {
    if (!s_mounted) return 0;
    return LittleFS.totalBytes();
}

int getUsedPercent() {
    const size_t total = getTotalBytes();
    if (total == 0) return 0;
    return (int)((getUsedBytes() * 100) / total);
}

bool storageLoadAdminPin(uint8_t *xorKey, size_t xorLen, uint8_t &checksum, uint8_t &length) {
    if (!s_mounted || !xorKey || xorLen < 8) return false;
    if (!LittleFS.exists(FILE_ADMIN_DAT)) return false;

    File f = LittleFS.open(FILE_ADMIN_DAT, FILE_READ);
    if (!f || f.size() < 10) {
        if (f) f.close();
        return false;
    }

    length = f.read();
    checksum = f.read();
    const size_t rd = f.read(xorKey, 8);
    f.close();
    return rd == 8 && length >= 4;
}

bool storageSaveAdminPin(const uint8_t *xorKey, size_t xorLen, uint8_t checksum, uint8_t length) {
    if (!s_mounted || !xorKey || xorLen < 8) return false;

    uint8_t buf[10];
    buf[0] = length;
    buf[1] = checksum;
    memcpy(buf + 2, xorKey, 8);
    return writeFileVerified(FILE_ADMIN_DAT, buf, sizeof(buf));
}

// --- Legacy StorageManager ---

bool StorageManager::loadUsers(JsonDocument &doc) {
    doc.clear();
    if (!s_mounted) return false;

    JsonDocument index;
    if (!loadUsersIndex(index)) return false;

    JsonArray out = doc["users"].to<JsonArray>();
    for (JsonObject o : index["users"].as<JsonArray>()) {
        JsonObject u = out.add<JsonObject>();
        u["fingerId"] = o["fingerprintId"] | o["fingerId"] | 0;
        u["name"] = o["name"] | "";
        u["department"] = o["dept"] | o["department"] | "";
        u["enrollTs"] = o["enrolledAt"] | 0;
    }
    return true;
}

bool StorageManager::saveUsers(const JsonDocument &doc) {
    if (!s_mounted || !doc["users"].is<JsonArray>()) return false;

    JsonDocument index;
    loadUsersIndex(index);
    JsonArray users = index["users"].to<JsonArray>();

    for (JsonObject in : doc["users"].as<JsonArray>()) {
        const uint16_t fp = in["fingerId"] | 0;
        bool found = false;
        for (JsonObject o : users) {
            if ((o["fingerprintId"] | o["fingerId"] | 0) == fp) {
                o["name"] = in["name"] | "";
                o["dept"] = in["department"] | "";
                o["enrolledAt"] = in["enrollTs"] | o["enrolledAt"] | (uint32_t)now();
                found = true;
                break;
            }
        }
        if (!found) {
            JsonObject o = users.add<JsonObject>();
            o["id"] = getNextUserId();
            o["fingerprintId"] = fp;
            o["name"] = in["name"] | "";
            o["dept"] = in["department"] | "";
            o["enrolledAt"] = in["enrollTs"] | (uint32_t)now();
            o["colorIndex"] = 0;
        }
    }
    return saveUsersIndex(index);
}

bool StorageManager::findUserByFingerId(uint8_t fingerId, UserRecord &out) {
    User u;
    if (!loadUserByFingerprint(fingerId, u)) return false;
    out.fingerId = static_cast<uint8_t>(u.fingerprintId);
    strlcpy(out.name, u.name, sizeof(out.name));
    strlcpy(out.department, u.department, sizeof(out.department));
    return true;
}

bool StorageManager::upsertUser(const UserRecord &user) {
    User u;
    if (!loadUserByFingerprint(user.fingerId, u)) {
        u.id = 0;
        u.fingerprintId = user.fingerId;
        u.enrolledAt = (uint32_t)now();
    }
    strlcpy(u.name, user.name, sizeof(u.name));
    strlcpy(u.department, user.department, sizeof(u.department));
    return saveUser(u);
}

bool StorageManager::removeUser(uint8_t fingerId) {
    User u;
    if (!loadUserByFingerprint(fingerId, u)) return false;
    return deleteUser(u.id);
}

bool StorageManager::appendAttendance(const AttendanceEvent &ev) {
    if (!s_mounted) return false;

    JsonDocument doc;
    File f = LittleFS.open(LEGACY_LOG, FILE_READ);
    if (f) {
        deserializeJson(doc, f);
        f.close();
    }
    if (!doc.is<JsonArray>()) doc.to<JsonArray>();

    JsonObject entry = doc.as<JsonArray>().add<JsonObject>();
    entry["fingerId"] = ev.fingerId;
    entry["ts"] = ev.timestamp;
    entry["in"] = ev.checkIn;

    User u;
    uint16_t uid = 0;
    if (loadUserByFingerprint(ev.fingerId, u)) {
        uid = u.id;
    }

    AttendanceRecord rec;
    rec.userId = uid ? uid : ev.fingerId;
    rec.checkInTime = ev.checkIn ? (uint32_t)ev.timestamp : 0;
    rec.checkOutTime = ev.checkIn ? 0 : (uint32_t)ev.timestamp;
    rec.status = ATT_STATUS_PRESENT;
    if (ev.checkIn) {
        saveAttendanceRecord(rec);
    } else {
        AttendanceRecord dayRecs[32];
        const int n = loadDayRecords((time_t)ev.timestamp, dayRecs, 32);
        for (int i = n - 1; i >= 0; i--) {
            if (dayRecs[i].userId == rec.userId && dayRecs[i].checkOutTime == 0) {
                rec.recordId = dayRecs[i].recordId;
                rec.checkInTime = dayRecs[i].checkInTime;
                rec.checkOutTime = (uint32_t)ev.timestamp;
                updateAttendanceRecord(rec);
                break;
            }
        }
    }

    return writeJsonVerified(LEGACY_LOG, doc);
}

static bool isSameDay(time_t ts) {
    if (timeStatus() == timeNotSet) return true;
    return day(ts) == day() && month(ts) == month() && year(ts) == year();
}

bool StorageManager::getLastScanToday(LastScanInfo &out) {
    out = LastScanInfo{};

    AttendanceRecord recs[32];
    const int n = loadDayRecords(now(), recs, 32);
    time_t bestTs = 0;
    AttendanceRecord *best = nullptr;

    for (int i = 0; i < n; i++) {
        const time_t ts = max((time_t)recs[i].checkInTime, (time_t)recs[i].checkOutTime);
        if (ts >= bestTs) {
            bestTs = ts;
            best = &recs[i];
        }
    }

    if (!best || bestTs == 0) {
        if (!LittleFS.exists(LEGACY_LOG)) return false;
        JsonDocument doc;
        if (!readJsonFile(LEGACY_LOG, doc) || !doc.is<JsonArray>()) return false;

        uint8_t bestFinger = 0;
        bool bestIn = true;
        for (JsonObject entry : doc.as<JsonArray>()) {
            const time_t ts = entry["ts"] | 0;
            if (ts == 0 || !isSameDay(ts)) continue;
            if (ts >= bestTs) {
                bestTs = ts;
                bestFinger = entry["fingerId"] | 0;
                bestIn = entry["in"] | true;
            }
        }
        if (bestTs == 0) return false;

        UserRecord user;
        if (!findUserByFingerId(bestFinger, user)) {
            snprintf(out.name, sizeof(out.name), "ID %u", bestFinger);
        } else {
            strlcpy(out.name, user.name, sizeof(out.name));
        }
        snprintf(out.timeStr, sizeof(out.timeStr), "%02d:%02d:%02d",
                 hour(bestTs), minute(bestTs), second(bestTs));
        out.checkIn = bestIn;
        out.found = true;
        return true;
    }

    User u;
    if (!loadUser(best->userId, u)) {
        if (!loadUserByFingerprint(best->userId, u)) {
            snprintf(out.name, sizeof(out.name), "ID %u", best->userId);
        }
    }
    if (u.name[0]) {
        strlcpy(out.name, u.name, sizeof(out.name));
    }

    const bool checkIn = (best->checkOutTime == 0 || best->checkOutTime == best->checkInTime);
    snprintf(out.timeStr, sizeof(out.timeStr), "%02d:%02d:%02d",
             hour(bestTs), minute(bestTs), second(bestTs));
    out.checkIn = checkIn;
    out.found = true;
    return true;
}

static time_t startOfDay(time_t t) {
    if (timeStatus() == timeNotSet) return t;
    tmElements_t te;
    breakTime(t, te);
    te.Hour = 0;
    te.Minute = 0;
    te.Second = 0;
    return makeTime(te);
}

static bool inFilterRange(time_t ts, RecordFilter filter, time_t dayAnchor) {
    if (ts == 0) return false;
    if (filter == RecordFilter::All) return true;

    const time_t dayStart = startOfDay(dayAnchor);
    const time_t dayEnd = dayStart + 86400;

    if (filter == RecordFilter::Today) {
        return ts >= dayStart && ts < dayEnd;
    }

    const time_t weekStart = dayStart - 6 * 86400;
    return ts >= weekStart && ts < dayEnd;
}

int StorageManager::loadAttendanceFiltered(RecordFilter filter, time_t dayAnchor,
                                           AttendanceRecordView *out, int maxOut,
                                           RecordsSummary *summary) {
    if (!out || maxOut <= 0) return 0;
    if (summary) *summary = RecordsSummary{};

    int count = 0;

    auto addView = [&](AttendanceRecordView &view) {
        if (count < maxOut) out[count++] = view;
        if (summary) {
            summary->total++;
            if (view.rec.checkIn) summary->checkIns++;
            else summary->checkOuts++;
        }
    };

    const time_t day0 = startOfDay(dayAnchor);
    for (int dayOff = 0; dayOff < 8; dayOff++) {
        const time_t d = (filter == RecordFilter::Today) ? day0 : day0 - dayOff * 86400;
        if (filter == RecordFilter::Week && dayOff > 6) break;
        if (filter == RecordFilter::Today && dayOff > 0) break;

        AttendanceRecord recs[48];
        const int n = loadDayRecords(d, recs, 48);
        for (int i = 0; i < n; i++) {
            const AttendanceRecord &r = recs[i];
            if (r.checkInTime > 0) {
                AttendanceRecordView v{};
                User u;
                uint16_t fp = r.userId;
                if (loadUser(r.userId, u)) fp = u.fingerprintId;
                v.rec.fingerId = static_cast<uint8_t>(fp);
                v.rec.timestamp = r.checkInTime;
                v.rec.checkIn = true;
                if (loadUser(r.userId, u) || loadUserByFingerprint(fp, u)) {
                    strlcpy(v.name, u.name, sizeof(v.name));
                    strlcpy(v.department, u.department, sizeof(v.department));
                } else {
                    snprintf(v.name, sizeof(v.name), "ID %u", fp);
                }
                if (inFilterRange(v.rec.timestamp, filter, dayAnchor)) addView(v);
            }
            if (r.checkOutTime > 0 && r.checkOutTime != r.checkInTime) {
                AttendanceRecordView v{};
                User u;
                uint16_t fp = r.userId;
                if (loadUser(r.userId, u)) fp = u.fingerprintId;
                v.rec.fingerId = static_cast<uint8_t>(fp);
                v.rec.timestamp = r.checkOutTime;
                v.rec.checkIn = false;
                if (loadUser(r.userId, u) || loadUserByFingerprint(fp, u)) {
                    strlcpy(v.name, u.name, sizeof(v.name));
                    strlcpy(v.department, u.department, sizeof(v.department));
                } else {
                    snprintf(v.name, sizeof(v.name), "ID %u", fp);
                }
                if (inFilterRange(v.rec.timestamp, filter, dayAnchor)) addView(v);
            }
        }
    }

    if (count == 0 && LittleFS.exists(LEGACY_LOG)) {
        JsonDocument doc;
        if (readJsonFile(LEGACY_LOG, doc) && doc.is<JsonArray>()) {
            for (JsonObject entry : doc.as<JsonArray>()) {
                AttendanceRecordView view{};
                view.rec.fingerId = entry["fingerId"] | 0;
                view.rec.timestamp = entry["ts"] | 0;
                view.rec.checkIn = entry["in"] | true;
                if (!inFilterRange(view.rec.timestamp, filter, dayAnchor)) continue;

                UserRecord user;
                if (findUserByFingerId(view.rec.fingerId, user)) {
                    strlcpy(view.name, user.name, sizeof(view.name));
                    strlcpy(view.department, user.department, sizeof(view.department));
                } else {
                    snprintf(view.name, sizeof(view.name), "ID %u", view.rec.fingerId);
                }
                addView(view);
            }
        }
    }

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (out[j].rec.timestamp > out[i].rec.timestamp) {
                const AttendanceRecordView tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }

    return count;
}

bool StorageManager::getUserAttendanceStats(uint8_t fingerId, int &totalDays, char *avgArrival,
                                            size_t avgLen) {
    totalDays = 0;
    if (avgArrival && avgLen > 0) strlcpy(avgArrival, "--:--", avgLen);

    User u;
    if (!loadUserByFingerprint(fingerId, u)) return false;

    UserSummary sum;
    if (loadUserSummary(u.id, sum)) {
        totalDays = (int)sum.totalDays;
    }

    AttendanceRecord recs[200];
    int checkInCount = 0;
    int totalCheckInMin = 0;

    for (int dayOff = 0; dayOff < 365; dayOff++) {
        const time_t d = now() - dayOff * 86400;
        const int n = loadDayRecords(d, recs, 200);
        for (int i = 0; i < n; i++) {
            if (recs[i].userId != u.id && recs[i].userId != fingerId) continue;
            if (recs[i].checkInTime > 0) {
                const time_t ts = recs[i].checkInTime;
                totalCheckInMin += hour(ts) * 60 + minute(ts);
                checkInCount++;
            }
        }
    }

    if (avgArrival && avgLen > 0 && checkInCount > 0) {
        const int avg = totalCheckInMin / checkInCount;
        snprintf(avgArrival, avgLen, "%02d:%02d", avg / 60, avg % 60);
    }
    return true;
}
