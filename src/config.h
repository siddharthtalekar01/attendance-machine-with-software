#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Board — Waveshare ESP32-S3-Touch-LCD-2.8
// Arduino board menu: "ESP32S3 Dev Module"
// TFT_eSPI pins: include/User_Setup.h
// -----------------------------------------------------------------------------

// Set to 1 to run RGB fill test on boot and halt (verify display, then set to 0)
#ifndef RUN_DISPLAY_TEST
#define RUN_DISPLAY_TEST 1
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
constexpr const char *USERS_FILE    = "/users.json";
constexpr const char *LOG_FILE      = "/attendance.json";

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr int      NTP_TIMEZONE_OFFSET_SEC = 0;
constexpr unsigned long NTP_UPDATE_INTERVAL_MS = 600000;

// Dark dashboard theme
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

constexpr int HOME_TOP_H      = 24;
constexpr int HOME_HERO_Y     = 24;
constexpr int HOME_HERO_H     = 136;
constexpr int HOME_STATUS_Y   = 160;
constexpr int HOME_STATUS_H   = 70;
constexpr int HOME_NAV_Y      = 230;
constexpr int HOME_NAV_H      = 90;
constexpr int HOME_PULSE_MS   = 50;
constexpr int HOME_CLOCK_MS   = 1000;

enum class AppScreen : uint8_t {
    Splash,
    Home,
    Scan,
    Enroll,
    Records,
    Admin,
    Settings,
    WiFiSetup,
};
