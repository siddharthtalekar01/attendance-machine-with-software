#include "storage.h"

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
