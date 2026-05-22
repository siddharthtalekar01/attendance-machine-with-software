#include "wifi_manager.h"

WiFiManagerService gWiFi;

bool WiFiManagerService::begin(const char *ssid, const char *password) {
    WiFi.mode(WIFI_STA);
    if (ssid && strlen(ssid) > 0) {
        WiFi.begin(ssid, password ? password : "");
    }
    return true;
}

bool WiFiManagerService::connect(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        strlcpy(_lastError, "WiFi connect timeout", sizeof(_lastError));
        return false;
    }
    _lastError[0] = '\0';
    syncTime();
    return true;
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
