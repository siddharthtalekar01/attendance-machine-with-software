#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include "config.h"

class WiFiManagerService {
public:
    bool begin(const char *ssid, const char *password);
    bool connect(uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT_MS);
    bool isConnected() const;
    void syncTime();
    String formattedTime();
    String formattedTimeHHMM();
    String formattedDate();

    const char *lastError() const { return _lastError; }

private:
    WiFiUDP _udp;
    NTPClient _ntp{_udp, "pool.ntp.org", NTP_TIMEZONE_OFFSET_SEC, NTP_UPDATE_INTERVAL_MS};
    char _lastError[64] = {};
};

extern WiFiManagerService gWiFi;
