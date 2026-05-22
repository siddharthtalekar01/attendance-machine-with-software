#include "wifi_manager.h"

#include <LittleFS.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <sys/time.h>
#include <cstring>

WiFiManagerService gWiFi;

namespace {

constexpr int NTP_SYNC_MAX_ATTEMPTS = 3;
constexpr uint32_t NTP_RETRY_DELAY_MS = 2000;
constexpr uint32_t NTP_POLL_TIMEOUT_MS = 8000;

WiFiUDP s_udp;
NTPClient s_ntp(s_udp, "pool.ntp.org", NTP_TIMEZONE_OFFSET_SEC, NTP_UPDATE_INTERVAL_MS);

bool s_wifiEnabled = false;
char s_ssid[64] = {};
char s_password[64] = {};
char s_ntpServer[64] = "pool.ntp.org";
int s_tzOffsetSec = 0;

WiFiLinkStatus s_link = WiFiLinkStatus::Off;
char s_lastError[64] = {};

bool s_connecting = false;
bool s_connectOk = false;
uint32_t s_connectStartMs = 0;
uint32_t s_connectTimeoutMs = WIFI_CONNECT_TIMEOUT_MS;
uint32_t s_lastRetryMs = 0;

bool s_ntpSyncing = false;
uint32_t s_ntpSyncStartMs = 0;

int s_scanResult = -2;  // -2 idle, -1 scanning, >=0 count when done

void setSystemTimeEpoch(time_t epoch) {
    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    setTime(epoch);
}

bool logNtpLastSync(time_t epoch) {
    if (!LittleFS.exists("/config")) {
        LittleFS.mkdir("/config");
    }
    File f = LittleFS.open(NTP_LAST_SYNC_FILE, FILE_WRITE);
    if (!f) return false;

    struct tm tmUtc;
    gmtime_r(&epoch, &tmUtc);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tmUtc.tm_year + 1900,
             tmUtc.tm_mon + 1, tmUtc.tm_mday, tmUtc.tm_hour, tmUtc.tm_min, tmUtc.tm_sec);
    f.println(buf);
    f.close();
    return true;
}

void updateLinkFromDriver() {
    if (!s_wifiEnabled) {
        s_link = WiFiLinkStatus::Off;
        return;
    }
    if (s_connecting) {
        s_link = WiFiLinkStatus::Connecting;
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        s_link = WiFiLinkStatus::Connected;
        s_connectOk = true;
        return;
    }
    if (s_connectOk && WiFi.status() != WL_CONNECTED) {
        s_connectOk = false;
    }
    if (s_lastError[0] != '\0' || s_link == WiFiLinkStatus::Failed) {
        s_link = WiFiLinkStatus::Failed;
    } else if (s_ssid[0] != '\0') {
        s_link = WiFiLinkStatus::Failed;
    } else {
        s_link = WiFiLinkStatus::Off;
    }
}

int rssiToBars(int32_t rssi) {
    if (rssi >= -50) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -75) return 2;
    return 1;
}

}  // namespace

void wifiSetEnabled(bool enabled) {
    s_wifiEnabled = enabled;
    if (!enabled) {
        WiFi.disconnect(true);
        s_connecting = false;
        s_connectOk = false;
        s_link = WiFiLinkStatus::Off;
        s_lastError[0] = '\0';
    } else {
        updateLinkFromDriver();
    }
}

void wifiSetCredentials(const char *ssid, const char *password) {
    if (ssid) {
        strlcpy(s_ssid, ssid, sizeof(s_ssid));
    } else {
        s_ssid[0] = '\0';
    }
    if (password) {
        strlcpy(s_password, password, sizeof(s_password));
    } else {
        s_password[0] = '\0';
    }
}

void wifiApplyNtpConfig(const char *server, int timezoneMinutes) {
    timezoneMinutes = constrain(timezoneMinutes, -720, 840);
    s_tzOffsetSec = timezoneMinutes * 60;
    if (server && server[0]) {
        strlcpy(s_ntpServer, server, sizeof(s_ntpServer));
        s_ntp.setPoolServerName(s_ntpServer);
    }
    s_ntp.setTimeOffset(s_tzOffsetSec);
}

void wifiConfigure(bool enabled, const char *ssid, const char *password, const char *ntpServer,
                   int timezoneMinutes) {
    wifiSetCredentials(ssid, password);
    wifiApplyNtpConfig(ntpServer, timezoneMinutes);
    wifiSetEnabled(enabled);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    if (enabled && s_ssid[0]) {
        s_lastRetryMs = millis();
        wifiConnectBegin(WIFI_CONNECT_TIMEOUT_MS);
    }
}

void wifiConnectBegin(uint32_t timeoutMs) {
    if (!s_wifiEnabled || !s_ssid[0]) {
        s_connecting = false;
        return;
    }

    s_connecting = true;
    s_connectOk = false;
    s_connectStartMs = millis();
    s_connectTimeoutMs = timeoutMs;
    s_lastError[0] = '\0';
    s_link = WiFiLinkStatus::Connecting;

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(s_ssid, s_password);
}

bool wifiConnectInProgress() {
    return s_connecting;
}

bool wifiConnectSucceeded() {
    return s_connectOk && WiFi.status() == WL_CONNECTED;
}

void wifiUpdate() {
    if (!s_wifiEnabled) {
        s_link = WiFiLinkStatus::Off;
        return;
    }

    const uint32_t nowMs = millis();

    if (s_connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            s_connecting = false;
            s_connectOk = true;
            s_lastError[0] = '\0';
            s_link = WiFiLinkStatus::Connected;
        } else if (nowMs - s_connectStartMs >= s_connectTimeoutMs) {
            s_connecting = false;
            s_connectOk = false;
            strlcpy(s_lastError, "WiFi connect timeout", sizeof(s_lastError));
            s_link = WiFiLinkStatus::Failed;
            s_lastRetryMs = nowMs;
        } else {
            s_link = WiFiLinkStatus::Connecting;
        }
    } else if (WiFi.status() == WL_CONNECTED) {
        s_connectOk = true;
        s_link = WiFiLinkStatus::Connected;
        s_lastError[0] = '\0';
    } else {
        s_connectOk = false;
        if (s_ssid[0]) {
            s_link = WiFiLinkStatus::Failed;
            if (s_lastError[0] == '\0') {
                strlcpy(s_lastError, "WiFi disconnected", sizeof(s_lastError));
            }
            if (!s_connecting && (nowMs - s_lastRetryMs >= WIFI_RETRY_INTERVAL_MS)) {
                s_lastRetryMs = nowMs;
                wifiConnectBegin(WIFI_CONNECT_TIMEOUT_MS);
            }
        }
    }

    if (s_ntpSyncing) {
        wifiNtpSyncPoll();
    }

    if (s_scanResult == -1 && WiFi.scanComplete() >= 0) {
        s_scanResult = WiFi.scanComplete();
    }
}

WiFiLinkStatus wifiGetLinkStatus() {
    return s_link;
}

bool wifiIsConnected() {
    return s_wifiEnabled && WiFi.status() == WL_CONNECTED;
}

const char *wifiLastError() {
    return s_lastError;
}

void drawWifiStatusIcon(TFT_eSPI &tft, int16_t x, int16_t y, uint16_t color) {
    const WiFiLinkStatus st = wifiGetLinkStatus();
    const int cx = x + 14;
    const int cy = y + 10;

    if (st == WiFiLinkStatus::Off) {
        tft.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
        tft.setTextFont(1);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("--", cx, cy);
        tft.setTextDatum(TL_DATUM);
        return;
    }

    if (st == WiFiLinkStatus::Failed) {
        tft.drawLine(x, y + 8, x + 28, y + 20, color);
        tft.drawLine(x + 28, y + 8, x, y + 20, color);
        return;
    }

    if (st == WiFiLinkStatus::Connecting) {
        for (int arc = 0; arc < 3; arc++) {
            const int r = 12 - arc * 4;
            const int ir = r - 2;
            for (int seg = 0; seg < 3; seg++) {
                const int a0 = 130 + seg * 34;
                const int a1 = a0 + 22;
                tft.drawArc(cx, cy, r, ir, a0, a1, color, COLOR_BG_DARK, false);
            }
        }
        tft.drawCircle(cx, cy, 2, color);
        return;
    }

    tft.fillCircle(cx, cy, 2, color);
    tft.drawArc(cx, cy, 12, 10, 130, 230, color, COLOR_BG_DARK, false);
    tft.drawArc(cx, cy, 8, 6, 130, 230, color, COLOR_BG_DARK, false);
    tft.drawArc(cx, cy, 4, 2, 130, 230, color, COLOR_BG_DARK, false);
}

bool syncNTP() {
    if (!wifiIsConnected()) {
        return false;
    }

    s_ntp.setPoolServerName(s_ntpServer);
    s_ntp.setTimeOffset(s_tzOffsetSec);

    for (int attempt = 0; attempt < NTP_SYNC_MAX_ATTEMPTS; attempt++) {
        s_ntp.begin();
        const uint32_t start = millis();
        while (millis() - start < 5000) {
            if (s_ntp.update()) {
                const time_t epoch = s_ntp.getEpochTime();
                setSystemTimeEpoch(epoch);
                logNtpLastSync(epoch);
                Serial.printf("[NTP] Synced: %lu\n", (unsigned long)epoch);
                return true;
            }
            delay(50);
        }
        if (attempt < NTP_SYNC_MAX_ATTEMPTS - 1) {
            delay(NTP_RETRY_DELAY_MS);
        }
    }

    Serial.println("[NTP] Sync failed after 3 attempts");
    return false;
}

void wifiNtpSyncBegin() {
    if (!wifiIsConnected()) return;
    s_ntpSyncing = true;
    s_ntpSyncStartMs = millis();
    s_ntp.begin();
}

bool wifiNtpSyncPoll() {
    if (!s_ntpSyncing) return true;

    if (s_ntp.update()) {
        const time_t epoch = s_ntp.getEpochTime();
        setSystemTimeEpoch(epoch);
        logNtpLastSync(epoch);
        s_ntpSyncing = false;
        return true;
    }

    if (millis() - s_ntpSyncStartMs >= NTP_POLL_TIMEOUT_MS) {
        s_ntpSyncing = false;
        return true;
    }

    return false;
}

bool wifiNtpSyncInProgress() {
    return s_ntpSyncing;
}

time_t getCurrentTime() {
    return time(nullptr);
}

String formatTime(time_t t) {
    if (t <= 0) return "--:--:--";
    struct tm tmLocal;
    localtime_r(&t, &tmLocal);
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min, tmLocal.tm_sec);
    return String(buf);
}

String formatDate(time_t t) {
    if (t <= 0) return "-- --- ----";
    static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    struct tm tmLocal;
    localtime_r(&t, &tmLocal);
    char buf[24];
    snprintf(buf, sizeof(buf), "%s, %d %s %04d", days[tmLocal.tm_wday], tmLocal.tm_mday,
             months[tmLocal.tm_mon], tmLocal.tm_year + 1900);
    return String(buf);
}

String formatShortDate(time_t t) {
    if (t <= 0) return "-- ---";
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    struct tm tmLocal;
    localtime_r(&t, &tmLocal);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d %s", tmLocal.tm_mday, months[tmLocal.tm_mon]);
    return String(buf);
}

String formatDuration(int minutes) {
    if (minutes < 0) minutes = 0;
    const int h = minutes / 60;
    const int m = minutes % 60;
    char buf[16];
    if (h > 0) {
        snprintf(buf, sizeof(buf), "%dh %dm", h, m);
    } else {
        snprintf(buf, sizeof(buf), "%dm", m);
    }
    return String(buf);
}

bool isSameDay(time_t a, time_t b) {
    if (a <= 0 || b <= 0) return false;
    struct tm ta;
    struct tm tb;
    localtime_r(&a, &ta);
    localtime_r(&b, &tb);
    return ta.tm_year == tb.tm_year && ta.tm_mon == tb.tm_mon && ta.tm_mday == tb.tm_mday;
}

time_t startOfDay(time_t t) {
    if (t <= 0) return t;
    struct tm tmLocal;
    localtime_r(&t, &tmLocal);
    tmLocal.tm_hour = 0;
    tmLocal.tm_min = 0;
    tmLocal.tm_sec = 0;
    return mktime(&tmLocal);
}

void startWifiScan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    s_scanResult = -1;
    WiFi.scanNetworks(true, true);
}

bool wifiScanInProgress() {
    return s_scanResult == -1 && WiFi.scanComplete() < 0;
}

bool wifiScanComplete() {
    if (s_scanResult >= 0) return true;
    const int n = WiFi.scanComplete();
    if (n >= 0) {
        s_scanResult = n;
        return true;
    }
    return false;
}

int getWifiScanResults(WiFiResult *buf, int maxCount) {
    if (!buf || maxCount <= 0) return 0;

    if (s_scanResult < 0) {
        const int n = WiFi.scanComplete();
        if (n < 0) return 0;
        s_scanResult = n;
    }

    const int total = s_scanResult;
    const int count = min(total, maxCount);
    for (int i = 0; i < count; i++) {
        strlcpy(buf[i].ssid, WiFi.SSID(i).c_str(), sizeof(buf[i].ssid));
        buf[i].rssi = WiFi.RSSI(i);
        buf[i].channel = (uint8_t)WiFi.channel(i);
        buf[i].encrypted = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }

    WiFi.scanDelete();
    s_scanResult = -2;
    return count;
}

void connectToNetwork(const char *ssid, const char *password) {
    if (!ssid || !ssid[0]) return;
    wifiSetCredentials(ssid, password);
    wifiSetEnabled(true);
    wifiConnectBegin(WIFI_CONNECT_TIMEOUT_MS);
}

void drawWifiSignalBars(TFT_eSPI &tft, int16_t x, int16_t y, int32_t rssi, uint16_t color) {
    const int bars = rssiToBars(rssi);
    const int barW = 3;
    const int gap = 2;
    const int baseY = y + 14;

    for (int i = 0; i < 4; i++) {
        const int h = 4 + i * 3;
        const int bx = x + i * (barW + gap);
        const int by = baseY - h;
        if (i < bars) {
            tft.fillRect(bx, by, barW, h, color);
        } else {
            tft.drawRect(bx, by, barW, h, TEXT_MUTED);
        }
    }
}

// -----------------------------------------------------------------------------
// Legacy WiFiManagerService
// -----------------------------------------------------------------------------

bool WiFiManagerService::begin(const char *ssid, const char *password) {
    wifiSetCredentials(ssid, password);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    if (ssid && ssid[0]) {
        WiFi.begin(ssid, password ? password : "");
    }
    return true;
}

bool WiFiManagerService::connect(uint32_t timeoutMs) {
    wifiConnectBegin(timeoutMs);
    const uint32_t start = millis();
    while (wifiConnectInProgress() && (millis() - start < timeoutMs + 500)) {
        wifiUpdate();
        delay(50);
    }
    if (wifiConnectSucceeded()) {
        syncTime();
    }
    return wifiConnectSucceeded();
}

void WiFiManagerService::connectBegin(uint32_t timeoutMs) {
    wifiConnectBegin(timeoutMs);
}

bool WiFiManagerService::connectPoll() {
    wifiUpdate();
    return !wifiConnectInProgress();
}

void WiFiManagerService::syncTime() {
    syncNTP();
}

String WiFiManagerService::formattedTimeHHMM() {
    const time_t t = getCurrentTime();
    if (t <= 0 && timeStatus() == timeNotSet) {
        return "--:--";
    }
    time_t use = t;
    if (use <= 0) {
        use = now();
    }
    struct tm tmLocal;
    localtime_r(&use, &tmLocal);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
    return String(buf);
}
