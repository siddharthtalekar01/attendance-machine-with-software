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

#define SPI_TOUCH_FREQUENCY 2500000

// R307S fingerprint — UART2
constexpr int PIN_FP_RX = 16;
constexpr int PIN_FP_TX = 17;
constexpr uint32_t FP_BAUD = 57600;

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

constexpr uint16_t COLOR_BG       = 0x1082;
constexpr uint16_t COLOR_PRIMARY  = 0x2196;
constexpr uint16_t COLOR_ACCENT   = 0x07E0;
constexpr uint16_t COLOR_TEXT     = 0xFFFF;
constexpr uint16_t COLOR_TEXT_DIM = 0xC618;
constexpr uint16_t COLOR_ERROR    = 0xF800;
constexpr uint16_t COLOR_SUCCESS  = 0x07E0;

enum class AppScreen : uint8_t {
    Splash,
    Home,
    Scan,
    Enroll,
    Admin,
    Settings,
    WiFiSetup,
};
