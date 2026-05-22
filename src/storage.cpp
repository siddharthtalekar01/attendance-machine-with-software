#include "storage.h"
#include <TimeLib.h>
#include <cstring>

StorageManager gStorage;

bool StorageManager::begin() {
    if (!LittleFS.begin(true)) {
        _mounted = false;
        return false;
    }
    _mounted = true;

    if (!LittleFS.exists(USERS_FILE)) {
        JsonDocument doc;
        doc["users"] = JsonArray();
        return saveUsers(doc);
    }
    if (!LittleFS.exists(LOG_FILE)) {
        File f = LittleFS.open(LOG_FILE, FILE_WRITE);
        if (!f) return false;
        f.print("[]");
        f.close();
    }
    return true;
}

bool StorageManager::loadUsers(JsonDocument &doc) {
    if (!_mounted) return false;
    File f = LittleFS.open(USERS_FILE, FILE_READ);
    if (!f) return false;
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    return !err && doc["users"].is<JsonArray>();
}

bool StorageManager::saveUsers(const JsonDocument &doc) {
    if (!_mounted) return false;
    File f = LittleFS.open(USERS_FILE, FILE_WRITE);
    if (!f) return false;
    if (serializeJson(doc, f) == 0) {
        f.close();
        return false;
    }
    f.close();
    return true;
}

bool StorageManager::findUserByFingerId(uint8_t fingerId, UserRecord &out) {
    JsonDocument doc;
    if (!loadUsers(doc)) return false;
    JsonArray users = doc["users"].as<JsonArray>();

    for (JsonObject u : users) {
        if (u["fingerId"].as<uint8_t>() == fingerId) {
            out.fingerId = fingerId;
            strlcpy(out.name, u["name"] | "", sizeof(out.name));
            strlcpy(out.department, u["department"] | "", sizeof(out.department));
            return true;
        }
    }
    return false;
}

bool StorageManager::upsertUser(const UserRecord &user) {
    JsonDocument doc;
    if (!loadUsers(doc)) {
        doc.clear();
        doc["users"].to<JsonArray>();
    }
    JsonArray users = doc["users"].as<JsonArray>();

    bool found = false;
    for (JsonObject u : users) {
        if (u["fingerId"].as<uint8_t>() == user.fingerId) {
            u["name"] = user.name;
            u["department"] = user.department;
            found = true;
            break;
        }
    }
    if (!found) {
        JsonObject u = users.add<JsonObject>();
        u["fingerId"] = user.fingerId;
        u["name"] = user.name;
        u["department"] = user.department;
        u["enrollTs"] = now();
    }
    return saveUsers(doc);
}

bool StorageManager::getUserAttendanceStats(uint8_t fingerId, int &totalDays,
                                            char *avgArrival, size_t avgLen) {
    totalDays = 0;
    if (avgArrival && avgLen > 0) strlcpy(avgArrival, "--:--", avgLen);

    if (!_mounted || !LittleFS.exists(LOG_FILE) || fingerId == 0) return false;

    File f = LittleFS.open(LOG_FILE, FILE_READ);
    if (!f) return false;

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close();
        return false;
    }
    f.close();

    if (!doc.is<JsonArray>()) return false;

    bool daysSeen[366] = {};
    int dayCount = 0;
    int totalCheckInMin = 0;
    int checkInCount = 0;

    for (JsonObject entry : doc.as<JsonArray>()) {
        if ((entry["fingerId"] | 0) != fingerId) continue;
        const time_t ts = entry["ts"] | 0;
        if (ts == 0) continue;

        const int d = day(ts);
        const int m = month(ts);
        if (d >= 1 && d <= 31 && m >= 1 && m <= 12) {
            const int key = m * 32 + d;
            if (!daysSeen[key]) {
                daysSeen[key] = true;
                dayCount++;
            }
        }

        if (entry["in"] | true) {
            totalCheckInMin += hour(ts) * 60 + minute(ts);
            checkInCount++;
        }
    }

    totalDays = dayCount;
    if (avgArrival && avgLen > 0 && checkInCount > 0) {
        const int avg = totalCheckInMin / checkInCount;
        snprintf(avgArrival, avgLen, "%02d:%02d", avg / 60, avg % 60);
    }
    return true;
}

bool StorageManager::removeUser(uint8_t fingerId) {
    if (!_mounted || fingerId == 0) return false;

    JsonDocument doc;
    if (!loadUsers(doc)) return false;

    JsonArray users = doc["users"].as<JsonArray>();
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i]["fingerId"].as<uint8_t>() == fingerId) {
            users.remove(i);
            return saveUsers(doc);
        }
    }
    return false;
}

bool StorageManager::appendAttendance(const AttendanceEvent &rec) {
    if (!_mounted) return false;

    JsonDocument doc;
    File f = LittleFS.open(LOG_FILE, FILE_READ);
    if (f) {
        deserializeJson(doc, f);
        f.close();
    }
    if (!doc.is<JsonArray>()) doc.to<JsonArray>();

    JsonObject entry = doc.as<JsonArray>().add<JsonObject>();
    entry["fingerId"] = rec.fingerId;
    entry["ts"] = rec.timestamp;
    entry["in"] = rec.checkIn;

    f = LittleFS.open(LOG_FILE, FILE_WRITE);
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

static bool isSameDay(time_t ts) {
    if (timeStatus() == timeNotSet) return true;
    return day(ts) == day() && month(ts) == month() && year(ts) == year();
}

bool StorageManager::getLastScanToday(LastScanInfo &out) {
    out = LastScanInfo{};
    if (!_mounted || !LittleFS.exists(LOG_FILE)) return false;

    File f = LittleFS.open(LOG_FILE, FILE_READ);
    if (!f) return false;

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close();
        return false;
    }
    f.close();

    if (!doc.is<JsonArray>()) return false;

    time_t bestTs = 0;
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

    if (!_mounted || !LittleFS.exists(LOG_FILE)) return 0;

    File f = LittleFS.open(LOG_FILE, FILE_READ);
    if (!f) return 0;

    JsonDocument doc;
    if (deserializeJson(doc, f)) {
        f.close();
        return 0;
    }
    f.close();

    if (!doc.is<JsonArray>()) return 0;

    int count = 0;
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
            strlcpy(view.department, "Unknown", sizeof(view.department));
        }

        if (count < maxOut) {
            out[count++] = view;
        }
        if (summary) {
            summary->total++;
            if (view.rec.checkIn) summary->checkIns++;
            else summary->checkOuts++;
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
