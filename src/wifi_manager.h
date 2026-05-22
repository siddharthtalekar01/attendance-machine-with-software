#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TimeLib.h>
#include <time.h>

#include "config.h"

// -----------------------------------------------------------------------------
// WiFi link status (status bar icon)
// -----------------------------------------------------------------------------
enum class WiFiLinkStatus : uint8_t {
    Off = 0,        // WiFi disabled in config
    Connecting,     // dashed arcs
    Connected,      // solid arcs
    Failed,         // X overlay
};

struct WiFiResult {
    char ssid[33] = {};
    int32_t rssi = -100;
    uint8_t channel = 0;
    bool encrypted = false;
};

// -----------------------------------------------------------------------------
// WiFi connection (non-blocking — call wifiUpdate() from loop)
// -----------------------------------------------------------------------------
void wifiSetEnabled(bool enabled);
void wifiSetCredentials(const char *ssid, const char *password);
void wifiApplyNtpConfig(const char *server, int timezoneMinutes);

/** Apply WiFi + NTP settings from AppConfig and start connect if enabled. */
void wifiConfigure(bool enabled, const char *ssid, const char *password, const char *ntpServer,
                   int timezoneMinutes);

void wifiUpdate();

WiFiLinkStatus wifiGetLinkStatus();
bool wifiIsConnected();
const char *wifiLastError();

/** Start non-blocking connect (polled by wifiUpdate). */
void wifiConnectBegin(uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT_MS);
bool wifiConnectInProgress();
bool wifiConnectSucceeded();

void drawWifiStatusIcon(TFT_eSPI &tft, int16_t x, int16_t y, uint16_t color);

// -----------------------------------------------------------------------------
// NTP
// -----------------------------------------------------------------------------
bool syncNTP();

/** Non-blocking NTP (splash / background). */
void wifiNtpSyncBegin();
bool wifiNtpSyncPoll();
bool wifiNtpSyncInProgress();

// -----------------------------------------------------------------------------
// Time helpers (ESP32 RTC / TimeLib after sync)
// -----------------------------------------------------------------------------
time_t getCurrentTime();
String formatTime(time_t t);
String formatDate(time_t t);
String formatShortDate(time_t t);
String formatDuration(int minutes);
bool isSameDay(time_t a, time_t b);
time_t startOfDay(time_t t);

// -----------------------------------------------------------------------------
// WiFi scan (settings network picker)
// -----------------------------------------------------------------------------
void startWifiScan();
bool wifiScanInProgress();
bool wifiScanComplete();
int getWifiScanResults(WiFiResult *buf, int maxCount);
void connectToNetwork(const char *ssid, const char *password);

/** 4-bar signal icon from RSSI (TFT primitives). */
void drawWifiSignalBars(TFT_eSPI &tft, int16_t x, int16_t y, int32_t rssi, uint16_t color);

// -----------------------------------------------------------------------------
// Legacy wrapper (existing call sites)
// -----------------------------------------------------------------------------
class WiFiManagerService {
public:
    bool begin(const char *ssid, const char *password);
    bool connect(uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT_MS);
    void connectBegin(uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT_MS);
    bool connectPoll();
    bool connectInProgress() const { return wifiConnectInProgress(); }
    bool connectSucceeded() const { return wifiConnectSucceeded(); }
    bool isConnected() const { return wifiIsConnected(); }
    void syncTime();
    void applyNtpConfig(const char *server, int timezoneMinutes) {
        wifiApplyNtpConfig(server, timezoneMinutes);
    }
    void ntpSyncBegin() { wifiNtpSyncBegin(); }
    bool ntpSyncPoll() { return wifiNtpSyncPoll(); }
    bool ntpSyncInProgress() const { return wifiNtpSyncInProgress(); }
    String formattedTime() { return formatTime(getCurrentTime()); }
    String formattedTimeHHMM();
    String formattedDate() { return formatShortDate(getCurrentTime()); }
    const char *lastError() const { return wifiLastError(); }
};

extern WiFiManagerService gWiFi;
