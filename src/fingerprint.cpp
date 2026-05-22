#include "fingerprint.h"

FingerprintManager gFingerprint;

bool FingerprintManager::begin() {
    _serial.begin(FP_BAUD, SERIAL_8N1, PIN_FP_RX, PIN_FP_TX);
    delay(100);
    _sensor.begin(FP_BAUD);
    if (!_sensor.verifyPassword()) {
        _ready = false;
        return false;
    }
    _sensor.setPacketSize(128);
    _ready = true;
    return true;
}

FpResult FingerprintManager::mapError(uint8_t code) {
    switch (code) {
        case FINGERPRINT_OK: return FpResult::Ok;
        case FINGERPRINT_NOTFOUND: return FpResult::NotFound;
        case FINGERPRINT_PACKETRECIEVEERR: return FpResult::PacketError;
        case FINGERPRINT_IMAGEFAIL: return FpResult::ImageFail;
        default: return FpResult::SensorError;
    }
}

FpResult FingerprintManager::waitForFinger(uint32_t timeoutMs) {
    if (!_ready) return FpResult::SensorError;
    const uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        uint8_t p = _sensor.getImage();
        if (p == FINGERPRINT_OK) return FpResult::Ok;
        if (p == FINGERPRINT_NOFINGER) {
            delay(50);
            continue;
        }
        return mapError(p);
    }
    return FpResult::NoFinger;
}

FpResult FingerprintManager::identify(uint8_t &slotId, uint16_t &confidence) {
    if (!_ready) return FpResult::SensorError;

    uint8_t p = _sensor.getImage();
    if (p != FINGERPRINT_OK) return mapError(p);

    p = _sensor.image2Tz();
    if (p != FINGERPRINT_OK) return mapError(p);

    p = _sensor.fingerFastSearch();
    if (p != FINGERPRINT_OK) return mapError(p);

    slotId = _sensor.fingerID;
    confidence = _sensor.confidence;
    return FpResult::Ok;
}

FpResult FingerprintManager::enroll(uint8_t slotId) {
    if (!_ready || slotId > MAX_ENROLLED_FINGERS) return FpResult::SensorError;

    for (int step = 0; step < 2; step++) {
        if (waitForFinger(15000) != FpResult::Ok) return FpResult::NoFinger;

        uint8_t p = _sensor.getImage();
        if (p != FINGERPRINT_OK) return mapError(p);

        p = _sensor.image2Tz(step + 1);
        if (p != FINGERPRINT_OK) return mapError(p);

        while (_sensor.getImage() != FINGERPRINT_NOFINGER) delay(50);
    }

    uint8_t p = _sensor.createModel();
    if (p != FINGERPRINT_OK) return FpResult::EnrollFail;

    p = _sensor.storeModel(slotId);
    return mapError(p);
}

bool FingerprintManager::deleteFinger(uint8_t slotId) {
    if (!_ready) return false;
    return _sensor.deleteModel(slotId) == FINGERPRINT_OK;
}

uint8_t FingerprintManager::templateCount() {
    if (!_ready) return 0;
    _sensor.getTemplateCount();
    return _sensor.templateCount;
}

const char *FingerprintManager::resultString(FpResult r) const {
    switch (r) {
        case FpResult::Ok: return "OK";
        case FpResult::NoFinger: return "Place finger";
        case FpResult::NotFound: return "Unknown finger";
        case FpResult::EnrollFail: return "Enroll failed";
        case FpResult::ImageFail: return "Bad image";
        case FpResult::PacketError: return "Comm error";
        case FpResult::SensorError: return "Sensor error";
        default: return "Error";
    }
}
