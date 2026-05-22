#include "splash.h"

#include <cstring>

#include "admin_auth.h"
#include "attendance.h"
#include "config.h"
#include "display.h"
#include "fingerprint.h"
#include "settings_manager.h"
#include "settings_ui.h"
#include "storage.h"
#include "ui_screens.h"
#include "wifi_manager.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

namespace {

constexpr int ICON_BASE_R = 10;
constexpr int ICON_CY = 88;
constexpr int ICON_CX = SCREEN_WIDTH / 2;
constexpr int TITLE_Y = 138;
constexpr int SYSTEM_Y = 158;
constexpr int LINE_Y = 178;
constexpr int TASK_Y = 228;
constexpr int BAR_Y = 248;
constexpr int BAR_W = 200;
constexpr int BAR_H = 10;
constexpr int BAR_X = (SCREEN_WIDTH - BAR_W) / 2;

enum class InitStep : uint8_t {
    None,
    LittleFs,
    Config,
    Fingerprint,
    WiFi,
    Ntp,
    Attendance,
    Done,
};

struct {
    uint32_t startMs = 0;
    bool spriteReady = false;
    TFT_eSprite sprite = TFT_eSprite(&gDisplay.tft());

    InitStep initStep = InitStep::LittleFs;
    InitStep initDisplay = InitStep::None;
    bool initDone = false;
    bool hadWarnings = false;
    bool wifiStarted = false;
    bool ntpStarted = false;

    char taskLabel[40] = "Starting...";
    bool taskWarning = false;
    int progressPct = 0;
    int userCount = 0;

    bool sliding = false;
    uint32_t slideStartMs = 0;
    bool finished = false;

    int typeOnChars = 0;
    uint32_t lastDrawMs = 0;
} s;

float easeOutCubic(float t) {
    t = constrain(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

void drawFingerprintScaled(TFT_eSPI &tft, int16_t cx, int16_t cy, int radius) {
    const uint16_t col = TEXT_PRIMARY;
    const int base = max(ICON_BASE_R, radius / 4);
    for (int i = 0; i < 4; i++) {
        const int r = base + i * max(2, radius / 5);
        const int ir = max(1, r - max(2, radius / 12));
        tft.drawArc(cx, cy, r, ir, 200, 320, col, COLOR_BG_DARK, false);
        tft.drawArc(cx, cy, r, ir, 20, 140, col, COLOR_BG_DARK, false);
    }
    const int dotR = max(2, radius / 8);
    tft.fillCircle(cx, cy + radius / 4, dotR, col);
}

void setTask(const char *label, bool warning = false) {
    strlcpy(s.taskLabel, label ? label : "", sizeof(s.taskLabel));
    s.taskWarning = warning;
}

int initProgressPct() {
    switch (s.initDisplay) {
        case InitStep::None: return 0;
        case InitStep::LittleFs: return 12;
        case InitStep::Config: return 30;
        case InitStep::Fingerprint: return 50;
        case InitStep::WiFi: return 70;
        case InitStep::Ntp: return 85;
        case InitStep::Attendance: return 95;
        case InitStep::Done: return 100;
        default: return 0;
    }
}

void pumpInit() {
    if (s.initDone) return;

    switch (s.initStep) {
        case InitStep::LittleFs:
            setTask("Mounting storage...");
            if (!storageInit()) {
                setTask("Storage warning", true);
                s.hadWarnings = true;
            }
            s.initDisplay = InitStep::LittleFs;
            s.initStep = InitStep::Config;
            break;

        case InitStep::Config:
            setTask("Loading config...");
            touchLoadCalibration();
            settingsLoad(gSettingsUi.settings);
            adminAuthInit();
            s.initDisplay = InitStep::Config;
            s.initStep = InitStep::Fingerprint;
            break;

        case InitStep::Fingerprint:
            setTask("Fingerprint sensor...");
            if (!fingerprintInit()) {
                setTask("Sensor not found", true);
                s.hadWarnings = true;
            } else {
                s.userCount = (int)fingerprintGetCount();
            }
            s.initDisplay = InitStep::Fingerprint;
            s.initStep = InitStep::WiFi;
            break;

        case InitStep::WiFi: {
            const AppSettings &cfg = gSettingsUi.settings;
            const char *ssid = cfg.ssid[0] ? cfg.ssid : WIFI_SSID;
            const char *pass = cfg.wifiPassword[0] ? cfg.wifiPassword : WIFI_PASSWORD;

            if (!s.wifiStarted) {
                s.wifiStarted = true;
                if (cfg.wifiEnabled && ssid[0]) {
                    setTask("Connecting WiFi...");
                    gWiFi.begin(ssid, pass);
                    gWiFi.connectBegin(8000);
                } else {
                    setTask("WiFi skipped");
                    s.initDisplay = InitStep::WiFi;
                    s.initStep = InitStep::Ntp;
                }
                break;
            }

            if (gWiFi.connectInProgress()) {
                gWiFi.connectPoll();
                if (gWiFi.connectInProgress()) {
                    return;
                }
            }
            if (s.wifiStarted && cfg.wifiEnabled && ssid[0] && !gWiFi.connectSucceeded()) {
                setTask("WiFi unavailable", true);
                s.hadWarnings = true;
            }
            s.initDisplay = InitStep::WiFi;
            s.initStep = InitStep::Ntp;
            break;
        }

        case InitStep::Ntp:
            if (gSettingsUi.settings.autoNtp && gWiFi.isConnected()) {
                if (!s.ntpStarted) {
                    setTask("Syncing time...");
                    gWiFi.ntpSyncBegin();
                    s.ntpStarted = true;
                    break;
                }
                if (gWiFi.ntpSyncInProgress()) {
                    gWiFi.ntpSyncPoll();
                    if (gWiFi.ntpSyncInProgress()) {
                        return;
                    }
                }
                if (timeStatus() == timeNotSet) {
                    setTask("Time not synced", true);
                    s.hadWarnings = true;
                }
            } else if (gSettingsUi.settings.autoNtp) {
                setTask("Time not synced", true);
                s.hadWarnings = true;
            }
            s.initDisplay = InitStep::Ntp;
            s.initStep = InitStep::Attendance;
            break;

        case InitStep::Attendance:
            setTask("Ready");
            gAttendance.begin();
            {
                const int fromJson = settingsCountEnrolledUsers();
                if (fromJson > 0) {
                    s.userCount = fromJson;
                } else if (s.userCount == 0) {
                    s.userCount = fromJson;
                }
            }
            s.initDisplay = InitStep::Done;
            s.initStep = InitStep::Done;
            s.initDone = true;
            break;

        default:
            s.initDone = true;
            break;
    }
}

void renderSplashFrame(int slideOffsetY) {
    if (!s.spriteReady) return;

    TFT_eSprite &sp = s.sprite;
    sp.fillSprite(COLOR_BG_DARK);

    const uint32_t elapsed = millis() - s.startMs;
    const float iconT = easeOutCubic((float)elapsed / (float)SPLASH_ICON_MS);
    const int iconR = (int)(20 + (80 - 20) * iconT);

    drawFingerprintScaled(sp, ICON_CX, ICON_CY + slideOffsetY, iconR);

    if (elapsed >= SPLASH_TYPE_START_MS) {
        const int msPerChar = 42;
        const int maxChars = 10;
        s.typeOnChars = min(maxChars, (int)((elapsed - SPLASH_TYPE_START_MS) / msPerChar) + 1);
        char buf[12] = {};
        memcpy(buf, "ATTENDANCE", min(s.typeOnChars, 10));
        sp.setTextFont(4);
        sp.setTextColor(TEXT_PRIMARY, COLOR_BG_DARK);
        sp.setTextDatum(TC_DATUM);
        sp.drawString(buf, ICON_CX, TITLE_Y + slideOffsetY);
    }

    if (elapsed >= SPLASH_SYSTEM_MS) {
        sp.setTextFont(2);
        sp.setTextColor(ACCENT_BLUE, COLOR_BG_DARK);
        sp.setTextDatum(TC_DATUM);
        sp.drawString("SYSTEM", ICON_CX, SYSTEM_Y + slideOffsetY);
    }

    if (elapsed >= SPLASH_LINE_MS) {
        const float lineT = easeOutCubic((float)(elapsed - SPLASH_LINE_MS) / 400.0f);
        const int half = (int)((SCREEN_WIDTH / 2 - 20) * lineT);
        sp.drawLine(ICON_CX - half, LINE_Y + slideOffsetY, ICON_CX + half, LINE_Y + slideOffsetY, ACCENT_BLUE);
    }

    if (elapsed >= SPLASH_PROGRESS_MS || s.initDone) {
        pumpInit();

        const int actual = initProgressPct();
        const uint32_t progElapsed = elapsed > SPLASH_PROGRESS_MS ? elapsed - SPLASH_PROGRESS_MS : 0;
        const int visual = (int)min(100.0f, (progElapsed * 100.0f) / 1200.0f);
        s.progressPct = max(actual, visual);
        if (s.initDone) s.progressPct = 100;

        sp.setTextFont(1);
        sp.setTextColor(s.taskWarning ? STATUS_AMBER : TEXT_SECONDARY, COLOR_BG_DARK);
        sp.setTextDatum(TC_DATUM);
        sp.drawString(s.taskLabel, ICON_CX, TASK_Y + slideOffsetY);

        sp.fillRoundRect(BAR_X, BAR_Y + slideOffsetY, BAR_W, BAR_H, 4, BG_SECONDARY);
        const int fillW = (BAR_W * s.progressPct) / 100;
        if (fillW > 0) {
            sp.fillRoundRect(BAR_X, BAR_Y + slideOffsetY, fillW, BAR_H, 4, ACCENT_BLUE);
        }
    }

    sp.setTextFont(1);
    sp.setTextColor(TEXT_MUTED, COLOR_BG_DARK);
    sp.setTextDatum(TC_DATUM);
    sp.drawString(APP_VERSION, ICON_CX, SCREEN_HEIGHT - 14 + slideOffsetY);
    sp.setTextDatum(TL_DATUM);
    sp.pushSprite(0, slideOffsetY);
}

}  // namespace

void splashBegin() {
    s = {};
    s.startMs = millis();

    gDisplay.fillScreen(TFT_BLACK);
    gDisplay.setBacklight(true);

    s.sprite.setColorDepth(16);
    s.spriteReady = s.sprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);

    setTask("Starting...");
    renderSplashFrame(0);
}

bool splashHadWarnings() {
    return s.hadWarnings;
}

int splashEnrolledUserCount() {
    return s.userCount;
}

bool splashIsTransitioning() {
    return s.sliding;
}

bool splashTick() {
    if (s.finished) return true;

    const uint32_t elapsed = millis() - s.startMs;

    if (!s.sliding) {
        pumpInit();

        const bool readySlide = s.initDone && elapsed >= SPLASH_SLIDE_START_MS;
        if (readySlide) {
            s.sliding = true;
            s.slideStartMs = millis();
        }

        if (millis() - s.lastDrawMs >= 33) {
            s.lastDrawMs = millis();
            renderSplashFrame(0);
        }
        return false;
    }

    const uint32_t slideElapsed = millis() - s.slideStartMs;
    const float st = easeOutCubic((float)slideElapsed / (float)SPLASH_SLIDE_MS);
    const int splashY = -(int)(SCREEN_HEIGHT * st);
    const int homeY = SCREEN_HEIGHT - (int)(SCREEN_HEIGHT * st);

    if (millis() - s.lastDrawMs >= 33) {
        s.lastDrawMs = millis();
        gDisplay.fillScreen(COLOR_BG_DARK);
        renderSplashFrame(splashY);
        gUi.drawHomeScreenOffset(homeY);
    }

    if (slideElapsed >= SPLASH_SLIDE_MS) {
        s.finished = true;
        if (s.spriteReady) {
            s.sprite.deleteSprite();
            s.spriteReady = false;
        }
        return true;
    }

    return false;
}
