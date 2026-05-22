#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Board — Waveshare ESP32-S3-Touch-LCD-2.8
// Arduino board menu: "ESP32S3 Dev Module"
// TFT_eSPI pins: include/User_Setup.h
// -----------------------------------------------------------------------------

// Set to 1 to run RGB fill test on boot and halt (verify display, then set to 0)
#ifndef RUN_DISPLAY_TEST
#define RUN_DISPLAY_TEST 0
#endif

constexpr uint16_t SCREEN_WIDTH  = 240;
constexpr uint16_t SCREEN_HEIGHT = 320;

// LCD SPI (matches User_Setup.h)
constexpr int PIN_LCD_MOSI = 13;
constexpr int PIN_LCD_SCLK = 12;
constexpr int PIN_LCD_CS   = 10;
constexpr int PIN_LCD_DC   = 11;
constexpr int PIN_LCD_RST  = 9;
constexpr int PIN_LCD_BL   = 46;  // Active HIGH

// XPT2046 (shared SPI: CLK=12, DIN=13; separate CS/MISO/IRQ)
constexpr int PIN_TOUCH_CLK  = 12;
constexpr int PIN_TOUCH_CS   = 38;
constexpr int PIN_TOUCH_DIN  = 13;
constexpr int PIN_TOUCH_DO   = 14;
constexpr int PIN_TOUCH_IRQ  = 15;  // Active LOW when pressed

#define SPI_TOUCH_FREQUENCY  2500000
#define TOUCH_RAW_THRESHOLD  400
#define TOUCH_CAL_FILE       "/config/touch_cal.json"
#define TOUCH_CAL_DIR        "/config"

// R307S fingerprint — UART2 (Serial2)
constexpr int PIN_FP_RX = 16;  // ESP32 RX2 <- sensor TX
constexpr int PIN_FP_TX = 17;  // ESP32 TX2 -> sensor RX
constexpr uint32_t FP_BAUD = 57600;

// Fingerprint timeouts & retries (non-blocking polling)
#define FP_MAX_RETRIES              3
#define FP_POLL_INTERVAL_MS         50
#define FP_UART_BEGIN_DELAY_MS      150
#define FP_FINGER_PRESENT_TIMEOUT_MS  8000
#define FP_FINGER_REMOVE_TIMEOUT_MS   6000
#define FP_IMAGE_CAPTURE_TIMEOUT_MS   3000
#define FP_SEARCH_POLL_TIMEOUT_MS     5000

// Spare header GPIO (avoid 15 — used by touch IRQ)
constexpr int PIN_HEADER_IO18 = 18;

// -----------------------------------------------------------------------------
// Application constants
// -----------------------------------------------------------------------------
constexpr const char *APP_NAME    = "Attendance";
constexpr const char *APP_VERSION = "0.1.0";

constexpr uint8_t MAX_ENROLLED_FINGERS = 50;
constexpr uint8_t MAX_NAME_LEN         = 32;

constexpr const char *STORAGE_MOUNT = "/littlefs";
/** @deprecated Legacy paths — use storage.h API (/users/index.json, /records/). */
constexpr const char *USERS_FILE = "/users.json";
constexpr const char *LOG_FILE   = "/attendance.json";

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr int      NTP_TIMEZONE_OFFSET_SEC = 0;
constexpr unsigned long NTP_UPDATE_INTERVAL_MS = 600000;

// Splash / boot animation (millis timelines)
constexpr uint32_t SPLASH_ICON_MS       = 600;
constexpr uint32_t SPLASH_TYPE_START_MS = 600;
constexpr uint32_t SPLASH_SYSTEM_MS     = 1000;
constexpr uint32_t SPLASH_LINE_MS       = 1500;
constexpr uint32_t SPLASH_PROGRESS_MS   = 2000;
constexpr uint32_t SPLASH_SLIDE_START_MS = 3500;
constexpr uint32_t SPLASH_SLIDE_MS      = 400;
constexpr uint32_t HOME_BOOT_CARD_MS    = 2000;

// Application state machine timings (non-blocking; millis() only in loop)
constexpr uint32_t BOOT_ANIM_MS        = SPLASH_SLIDE_START_MS + SPLASH_SLIDE_MS;
constexpr uint32_t SCAN_RESULT_MS      = 3000;
constexpr uint32_t ADMIN_HOLD_MS       = 2000;
constexpr uint32_t SCAN_POLL_MS        = 80;

// Dark dashboard theme
constexpr uint16_t BG_SECONDARY        = 0x4208;
constexpr uint16_t COLOR_BG_DARK    = 0x0841;
constexpr uint16_t COLOR_BG         = COLOR_BG_DARK;
constexpr uint16_t COLOR_PRIMARY    = 0x2196;
constexpr uint16_t COLOR_ACCENT     = 0x07E0;
constexpr uint16_t COLOR_TEXT       = 0xFFFF;
constexpr uint16_t COLOR_TEXT_DIM   = 0xC618;
constexpr uint16_t COLOR_ERROR      = 0xF800;
constexpr uint16_t COLOR_SUCCESS    = 0x07E0;

constexpr uint16_t TEXT_SECONDARY   = 0x9CD3;
constexpr uint16_t TEXT_PRIMARY    = 0xFFFF;
constexpr uint16_t TEXT_MUTED      = 0x632C;
constexpr uint16_t ACCENT_GREEN    = 0x07E0;
constexpr uint16_t ACCENT_BLUE     = 0x3A6A;
constexpr uint16_t ACCENT_RED      = COLOR_ERROR;
constexpr uint16_t STATUS_AMBER    = 0xFD20;
constexpr uint16_t STATUS_GREEN    = ACCENT_GREEN;
constexpr uint16_t STATUS_RED      = COLOR_ERROR;

// Kiosk enrollment: fixed display name (tap name field to cycle presets if disabled)
#ifndef ENROLL_KIOSK_MODE
#define ENROLL_KIOSK_MODE 1
#endif
#ifndef KIOSK_DEFAULT_NAME
#define KIOSK_DEFAULT_NAME "Kiosk User"
#endif
constexpr int ENROLL_DEPT_COUNT = 5;

constexpr int HOME_TOP_H      = 24;
constexpr int HOME_HERO_Y     = 24;
constexpr int HOME_HERO_H     = 136;
constexpr int HOME_STATUS_Y   = 160;
constexpr int HOME_STATUS_H   = 70;
constexpr int HOME_NAV_Y      = 230;
constexpr int HOME_NAV_H      = 90;
constexpr int HOME_PULSE_MS   = 50;
constexpr int HOME_CLOCK_MS   = 1000;

// Records screen layout
constexpr int REC_HEADER_H     = 45;
constexpr int REC_FILTER_Y     = 45;
constexpr int REC_FILTER_H     = 25;
constexpr int REC_LIST_Y       = 70;
constexpr int REC_LIST_H       = 220;
constexpr int REC_BOTTOM_Y     = 290;
constexpr int REC_ROW_H        = 44;
constexpr int REC_ROW_EXPANDED = 40;
constexpr int REC_VISIBLE_ROWS = 5;
constexpr int REC_MAX_ITEMS    = 100;

enum class AppScreen : uint8_t {
    Splash,
    Home,
    Scan,
    Enroll,
    Records,
    Admin,
    Settings,
    UserList,
    WiFiSetup,
};
