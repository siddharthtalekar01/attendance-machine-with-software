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
            found = true;
            break;
        }
    }
    if (!found) {
        JsonObject u = users.add<JsonObject>();
        u["fingerId"] = user.fingerId;
        u["name"] = user.name;
    }
    return saveUsers(doc);
}

bool StorageManager::appendAttendance(const AttendanceRecord &rec) {
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
