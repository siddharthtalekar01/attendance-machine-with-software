// TFT_eSPI — Waveshare ESP32-S3-Touch-LCD-2.8 (ST7789 240x320 + XPT2046)
// Schematic pinout (user board revision)
// Loaded by platformio.ini: -DUSER_SETUP_LOADED=1 -include include/User_Setup.h

#define USER_SETUP_INFO "Waveshare_ESP32S3_Touch_LCD_2_8"

// -----------------------------------------------------------------------------
// Display driver
// -----------------------------------------------------------------------------
#define ST7789_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// -----------------------------------------------------------------------------
// SPI LCD (Waveshare schematic)
// -----------------------------------------------------------------------------
#define TFT_MOSI 13
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC   11
#define TFT_RST  9

// XPT2046 T_DO — display has no MISO; touch reads on GPIO14
#define TFT_MISO 14

#define TFT_BL 46
#define TFT_BACKLIGHT_ON HIGH

// -----------------------------------------------------------------------------
// Color order — if red/blue are swapped on screen, comment one and uncomment the other
// -----------------------------------------------------------------------------
#define TFT_RGB_ORDER TFT_RGB
// #define TFT_RGB_ORDER TFT_BGR

// -----------------------------------------------------------------------------
// SPI speed
// -----------------------------------------------------------------------------
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

// -----------------------------------------------------------------------------
// XPT2046 touch (shares SCLK/MOSI with LCD)
// -----------------------------------------------------------------------------
#define TOUCH_CS 38
#define SPI_TOUCH_FREQUENCY 2500000

// If the panel stays blank or crashes at init, try uncommenting:
// #define USE_HSPI_PORT
