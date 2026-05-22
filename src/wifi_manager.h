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

    /** Non-blocking connect (call connectPoll from loop). */
    void connectBegin(uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT_MS);
    bool connectPoll();
    bool connectInProgress() const { return _connecting; }
    bool connectSucceeded() const { return _connectOk; }

    bool isConnected() const;
    void syncTime();

    void ntpSyncBegin();
    bool ntpSyncPoll();
    bool ntpSyncInProgress() const { return _ntpSyncing; }
    String formattedTime();
    String formattedTimeHHMM();
    String formattedDate();

    const char *lastError() const { return _lastError; }

private:
    WiFiUDP _udp;
    NTPClient _ntp{_udp, "pool.ntp.org", NTP_TIMEZONE_OFFSET_SEC, NTP_UPDATE_INTERVAL_MS};
    char _lastError[64] = {};
    bool _connecting = false;
    bool _connectOk = false;
    uint32_t _connectStartMs = 0;
    uint32_t _connectTimeoutMs = 0;
    bool _ntpSyncing = false;
    bool _ntpOk = false;
    uint32_t _ntpStartMs = 0;
};

extern WiFiManagerService gWiFi;
