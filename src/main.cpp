#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "fingerprint.h"
#include "storage.h"
#include "attendance.h"
#include "ui_screens.h"
#include "wifi_manager.h"

// Copy secrets.example.h to secrets.h and set WiFi credentials
#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

static void printBootBanner() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.printf("[%s] %s starting...\n", APP_NAME, APP_VERSION);
}

void setup() {
    printBootBanner();

    if (!gDisplay.begin()) {
        Serial.println("Display init failed");
    }

#if RUN_DISPLAY_TEST
    gDisplay.displayTest();
    while (true) {
        delay(1000);
    }
#endif

    if (!gStorage.begin()) {
        Serial.println("LittleFS init failed");
        gDisplay.showMessage("Storage", "LittleFS mount failed", COLOR_ERROR);
        delay(3000);
    } else {
        touchLoadCalibration();
    }

    if (!fingerprintInit()) {
        Serial.println("Fingerprint sensor not found on UART2");
        gDisplay.showMessage("Sensor", "R307S not detected\nCheck wiring 16/17", COLOR_ERROR);
        delay(2500);
    } else {
        Serial.printf("Fingerprint templates: %u\n", fingerprintGetCount());
    }

    gAttendance.begin();
    gWiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    if (strlen(WIFI_SSID) > 0) {
        gWiFi.connect();
    }

    gUi.begin();
}

void loop() {
    gUi.loop();
    delay(20);
}
