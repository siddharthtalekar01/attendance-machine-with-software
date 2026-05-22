#include "fingerprint.h"

#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

static HardwareSerial s_fpSerial(2);
static Adafruit_Fingerprint s_sensor(&s_fpSerial);
static bool s_initialized = false;

static void notify(FingerprintProgressCallback cb, int step, const char *msg) {
    if (cb) cb(step, msg);
}

static bool timedOut(uint32_t startMs, uint32_t timeoutMs) {
    return (millis() - startMs) >= timeoutMs;
}

static int mapCommError(uint8_t code) {
    if (code == FINGERPRINT_OK) return FP_ENROLL_OK;
    if (code == FINGERPRINT_NOTFOUND) return FP_SEARCH_NO_MATCH;
    if (code == FINGERPRINT_NOFINGER) return FP_SEARCH_NO_FINGER;
    if (code == FINGERPRINT_PACKETRECIEVEERR || code == FINGERPRINT_TIMEOUT ||
        code == FINGERPRINT_COMMUNICATIONERROR) {
        return FP_ENROLL_ERR_COMM;
    }
    if (code == FINGERPRINT_IMAGEFAIL || code == FINGERPRINT_IMAGEMESS ||
        code == FINGERPRINT_FEATUREFAIL || code == FINGERPRINT_INVALIDIMAGE) {
        return FP_ENROLL_ERR_IMAGE;
    }
    return FP_ENROLL_ERR_COMM;
}

/** Wait until finger detected or timeout. Returns true if finger present. */
static bool waitForFingerPresent(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (!timedOut(start, timeoutMs)) {
        const uint8_t p = s_sensor.getImage();
        if (p == FINGERPRINT_OK) return true;
        if (p == FINGERPRINT_NOFINGER) {
            delay(FP_POLL_INTERVAL_MS);
            continue;
        }
        if (p != FINGERPRINT_NOFINGER && p != FINGERPRINT_OK) {
            Serial.printf("[FP] waitForFingerPresent error: 0x%02X\n", p);
            return false;
        }
        delay(FP_POLL_INTERVAL_MS);
    }
    return false;
}

/** Wait until finger removed or timeout. */
static bool waitForFingerRemoved(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (!timedOut(start, timeoutMs)) {
        if (s_sensor.getImage() == FINGERPRINT_NOFINGER) return true;
        delay(FP_POLL_INTERVAL_MS);
    }
    return false;
}

/** Capture one image with retries (caller must ensure finger is on sensor). */
static int captureImageOnce() {
    for (int attempt = 0; attempt < FP_MAX_RETRIES; attempt++) {
        const uint8_t p = s_sensor.getImage();
        if (p == FINGERPRINT_OK) return FP_ENROLL_OK;
        if (p == FINGERPRINT_NOFINGER) return FP_ENROLL_ERR_NO_FINGER;
        if (p != FINGERPRINT_PACKETRECIEVEERR) {
            Serial.printf("[FP] getImage error: 0x%02X (attempt %d)\n", p, attempt + 1);
        }
        delay(FP_POLL_INTERVAL_MS);
    }
    return FP_ENROLL_ERR_IMAGE;
}

static void printSensorInfo() {
    if (s_sensor.getParameters() == FINGERPRINT_OK) {
        Serial.println("[FP] --- Sensor parameters ---");
        Serial.printf("[FP] Status:         0x%04X\n", s_sensor.status);
        Serial.printf("[FP] Capacity:       %u templates\n", s_sensor.capacity);
        Serial.printf("[FP] Security level: %u\n", s_sensor.security_level);
        Serial.printf("[FP] Device addr:    0x%08lX\n", (unsigned long)s_sensor.device_addr);
        Serial.printf("[FP] Packet size:    %u\n", s_sensor.packet_len);
    } else {
        Serial.println("[FP] Warning: getParameters failed");
    }

    if (s_sensor.getTemplateCount() == FINGERPRINT_OK) {
        Serial.printf("[FP] Templates stored: %u\n", s_sensor.templateCount);
    }
    Serial.println("[FP] ---------------------------");
}

bool fingerprintInit() {
    s_initialized = false;

    s_fpSerial.end();
    delay(10);
    s_fpSerial.begin(FP_BAUD, SERIAL_8N1, PIN_FP_RX, PIN_FP_TX);
    delay(FP_UART_BEGIN_DELAY_MS);

    s_sensor.begin(FP_BAUD);

    for (int attempt = 0; attempt < FP_MAX_RETRIES; attempt++) {
        if (s_sensor.verifyPassword()) {
            s_sensor.setPacketSize(128);
            s_initialized = true;
            Serial.println("[FP] R307S detected (password OK)");
            printSensorInfo();
            return true;
        }
        Serial.printf("[FP] verifyPassword failed (attempt %d/%d)\n", attempt + 1, FP_MAX_RETRIES);
        delay(100);
    }

    Serial.println("[FP] ERROR: Sensor not found on UART2 (GPIO16/17 @ 57600)");
    return false;
}

int fingerprintEnroll(uint16_t id, FingerprintProgressCallback progressCallback) {
    if (!s_initialized) return FP_ENROLL_ERR_NOT_INIT;
    if (id == 0 || id > MAX_ENROLLED_FINGERS) return FP_ENROLL_ERR_INVALID_ID;

    notify(progressCallback, 1, "Place finger");
    if (!waitForFingerPresent(FP_FINGER_PRESENT_TIMEOUT_MS)) {
        return FP_ENROLL_ERR_NO_FINGER;
    }

    int err = captureImageOnce();
    if (err != FP_ENROLL_OK) return err;

    uint8_t p = s_sensor.image2Tz(1);
    if (p != FINGERPRINT_OK) {
        Serial.printf("[FP] image2Tz(1) error: 0x%02X\n", p);
        return mapCommError(p) == FP_ENROLL_ERR_COMM ? FP_ENROLL_ERR_COMM : FP_ENROLL_ERR_IMAGE;
    }

    notify(progressCallback, 2, "Remove finger");
    if (!waitForFingerRemoved(FP_FINGER_REMOVE_TIMEOUT_MS)) {
        return FP_ENROLL_ERR_REMOVE;
    }

    notify(progressCallback, 3, "Place again");
    if (!waitForFingerPresent(FP_FINGER_PRESENT_TIMEOUT_MS)) {
        return FP_ENROLL_ERR_NO_FINGER;
    }

    err = captureImageOnce();
    if (err != FP_ENROLL_OK) return err;

    p = s_sensor.image2Tz(2);
    if (p != FINGERPRINT_OK) {
        Serial.printf("[FP] image2Tz(2) error: 0x%02X\n", p);
        return mapCommError(p) == FP_ENROLL_ERR_COMM ? FP_ENROLL_ERR_COMM : FP_ENROLL_ERR_IMAGE;
    }

    notify(progressCallback, 4, "Enrolling...");
    p = s_sensor.createModel();
    if (p == FINGERPRINT_ENROLLMISMATCH) {
        Serial.println("[FP] createModel: prints did not match");
        return FP_ENROLL_ERR_CREATE;
    }
    if (p != FINGERPRINT_OK) {
        Serial.printf("[FP] createModel error: 0x%02X\n", p);
        return FP_ENROLL_ERR_CREATE;
    }

    p = s_sensor.storeModel(id);
    if (p != FINGERPRINT_OK) {
        Serial.printf("[FP] storeModel(%u) error: 0x%02X\n", id, p);
        return FP_ENROLL_ERR_STORE;
    }

    notify(progressCallback, 5, "Success");
    Serial.printf("[FP] Enrolled template ID %u\n", id);
    return FP_ENROLL_OK;
}

int fingerprintSearch(uint16_t &matchedId, uint16_t &confidence) {
    matchedId = 0;
    confidence = 0;

    if (!s_initialized) return FP_SEARCH_NO_MATCH;

    const uint32_t start = millis();
    bool gotImage = false;

    while (!timedOut(start, FP_SEARCH_POLL_TIMEOUT_MS)) {
        const uint8_t p = s_sensor.getImage();
        if (p == FINGERPRINT_OK) {
            gotImage = true;
            break;
        }
        if (p == FINGERPRINT_NOFINGER) {
            delay(FP_POLL_INTERVAL_MS);
            continue;
        }
        Serial.printf("[FP] search getImage error: 0x%02X\n", p);
        return FP_SEARCH_NO_MATCH;
    }

    if (!gotImage) return FP_SEARCH_NO_FINGER;

    uint8_t p = s_sensor.image2Tz();
    if (p != FINGERPRINT_OK) {
        Serial.printf("[FP] search image2Tz error: 0x%02X\n", p);
        return FP_SEARCH_NO_MATCH;
    }

    p = s_sensor.fingerFastSearch();
    if (p == FINGERPRINT_NOTFOUND) return FP_SEARCH_NO_MATCH;
    if (p != FINGERPRINT_OK) {
        Serial.printf("[FP] fingerFastSearch error: 0x%02X\n", p);
        return FP_SEARCH_NO_MATCH;
    }

    matchedId = s_sensor.fingerID;
    confidence = s_sensor.confidence;
    Serial.printf("[FP] Match ID=%u confidence=%u\n", matchedId, confidence);
    return FP_SEARCH_OK;
}

bool fingerprintDeleteId(uint16_t id) {
    if (!s_initialized || id == 0 || id > MAX_ENROLLED_FINGERS) return false;
    const uint8_t p = s_sensor.deleteModel(id);
    if (p == FINGERPRINT_OK) {
        Serial.printf("[FP] Deleted template %u\n", id);
        return true;
    }
    Serial.printf("[FP] deleteModel(%u) error: 0x%02X\n", id, p);
    return false;
}

bool fingerprintDeleteAll() {
    if (!s_initialized) return false;
    const uint8_t p = s_sensor.emptyDatabase();
    if (p == FINGERPRINT_OK) {
        Serial.println("[FP] All templates deleted");
        return true;
    }
    Serial.printf("[FP] emptyDatabase error: 0x%02X\n", p);
    return false;
}

uint16_t fingerprintGetCount() {
    if (!s_initialized) return 0;
    if (s_sensor.getTemplateCount() != FINGERPRINT_OK) return 0;
    return s_sensor.templateCount;
}

bool fingerprintIdExists(uint16_t id) {
    if (!s_initialized || id == 0 || id > MAX_ENROLLED_FINGERS) return false;
    const uint8_t p = s_sensor.loadModel(id);
    return p == FINGERPRINT_OK;
}

const char *fingerprintErrorString(int code) {
    switch (code) {
        case FP_SEARCH_OK:
        case FP_ENROLL_OK: return "OK";
        case FP_SEARCH_NO_FINGER: return "No finger detected";
        case FP_SEARCH_NO_MATCH: return "No match in database";
        case FP_ENROLL_ERR_NO_FINGER: return "Finger not detected (timeout)";
        case FP_ENROLL_ERR_IMAGE: return "Bad fingerprint image";
        case FP_ENROLL_ERR_NOT_INIT: return "Sensor not initialized";
        case FP_ENROLL_ERR_INVALID_ID: return "Invalid template ID";
        case FP_ENROLL_ERR_CREATE: return "Could not merge prints";
        case FP_ENROLL_ERR_STORE: return "Could not store template";
        case FP_ENROLL_ERR_COMM: return "Communication error";
        case FP_ENROLL_ERR_REMOVE: return "Remove finger (timeout)";
        default: return "Unknown error";
    }
}
