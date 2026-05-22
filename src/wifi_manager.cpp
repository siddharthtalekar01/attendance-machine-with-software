#include "wifi_manager.h"

WiFiManagerService gWiFi;

bool WiFiManagerService::begin(const char *ssid, const char *password) {
    WiFi.mode(WIFI_STA);
    if (ssid && strlen(ssid) > 0) {
        WiFi.begin(ssid, password ? password : "");
    }
    return true;
}

void WiFiManagerService::connectBegin(uint32_t timeoutMs) {
    _connecting = true;
    _connectOk = false;
    _connectStartMs = millis();
    _connectTimeoutMs = timeoutMs;
    _lastError[0] = '\0';
}

bool WiFiManagerService::connectPoll() {
    if (!_connecting) return true;

    if (WiFi.status() == WL_CONNECTED) {
        _connecting = false;
        _connectOk = true;
        _lastError[0] = '\0';
        return true;
    }

    if (millis() - _connectStartMs >= _connectTimeoutMs) {
        _connecting = false;
        _connectOk = false;
        strlcpy(_lastError, "WiFi connect timeout", sizeof(_lastError));
        return true;
    }

    return false;
}

bool WiFiManagerService::connect(uint32_t timeoutMs) {
    connectBegin(timeoutMs);
    while (!connectPoll()) {
        delay(50);
    }
    if (_connectOk) {
        syncTime();
    }
    return _connectOk;
}

void WiFiManagerService::ntpSyncBegin() {
    _ntpSyncing = true;
    _ntpOk = false;
    _ntpStartMs = millis();
    _ntp.begin();
}

bool WiFiManagerService::ntpSyncPoll() {
    if (!_ntpSyncing) return true;

    if (_ntp.update()) {
        setTime(_ntp.getEpochTime());
        _ntpSyncing = false;
        _ntpOk = true;
        return true;
    }

    if (millis() - _ntpStartMs >= 8000) {
        _ntpSyncing = false;
        _ntpOk = false;
        return true;
    }

    return false;
}

bool WiFiManagerService::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WiFiManagerService::syncTime() {
    _ntp.begin();
    if (_ntp.update()) {
        setTime(_ntp.getEpochTime());
    }
}

String WiFiManagerService::formattedTime() {
    _ntp.update();
    if (timeStatus() == timeNotSet) return "--:--";
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour(), minute(), second());
    return String(buf);
}

String WiFiManagerService::formattedTimeHHMM() {
    _ntp.update();
    if (timeStatus() == timeNotSet) return "--:--";
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour(), minute());
    return String(buf);
}

String WiFiManagerService::formattedDate() {
    _ntp.update();
    if (timeStatus() == timeNotSet) return "-- ---";
    static const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    char buf[12];
    const int m = month();
    snprintf(buf, sizeof(buf), "%02d %s", day(), (m >= 1 && m <= 12) ? months[m - 1] : "---");
    return String(buf);
}
