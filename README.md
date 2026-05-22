# ESP32-S3 Fingerprint Attendance Machine

PlatformIO project for a **Waveshare ESP32-S3 2.8" IPS LCD** board with **R307S** fingerprint sensor.

## Hardware

| Component | Connection |
|-----------|------------|
| Board | Waveshare ESP32-S3-Touch-LCD-2.8 (Arduino: **ESP32S3 Dev Module**) |
| Display | ST7789 240×320 SPI — MOSI=13, SCLK=12, CS=10, DC=11, RST=9, BL=46 |
| Touch | XPT2046 — CS=38, IRQ=15, shared CLK/DIN (12/13), DO=14 |
| Fingerprint | R307S on **UART2**: RX=GPIO16, TX=GPIO17 @ 57600 baud |

**Pin conflict:** GPIO16/17 are used by the onboard **TF card** on this board. Use external UART wiring for the R307S; do not mount SD on those pins.

**Touch note:** The Waveshare SKU uses **CST328** (I2C). This project is configured for **XPT2046** as requested; adjust `PIN_TOUCH_CS` / calibration in `display.cpp` if needed.

## Setup

1. Install [PlatformIO](https://platformio.org/).
2. Copy `src/secrets.example.h` → `src/secrets.h` and set WiFi credentials.
3. First boot runs **display test** (red → green → blue). Set `RUN_DISPLAY_TEST` to `0` in `src/config.h` when colors look correct.
4. Build & upload:

```bash
pio run -t upload
pio device monitor
```

## Layout

```
include/User_Setup.h   # TFT_eSPI ST7789 pins (Waveshare schematic)
src/
  config.h             # GPIO, screen size, app constants
  display.*            # TFT + XPT2046
  fingerprint.*        # R307S / Adafruit library
  storage.*            # LittleFS + ArduinoJson
  attendance.*         # Check-in/out logic
  ui_screens.*         # Screen flow
  wifi_manager.*       # WiFi + NTP + Time
  main.cpp
```

## Libraries

Declared in `platformio.ini`: TFT_eSPI, Adafruit Fingerprint, ArduinoJson v7, NTPClient, Time. LittleFS is provided by the ESP32 Arduino core.
