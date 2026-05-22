#pragma once

#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include "config.h"

enum class FpResult : uint8_t {
    Ok,
    NoFinger,
    NotFound,
    EnrollFail,
    ImageFail,
    SensorError,
    PacketError,
};

class FingerprintManager {
public:
    bool begin();
    bool isReady() const { return _ready; }

    FpResult waitForFinger(uint32_t timeoutMs = 10000);
    FpResult identify(uint8_t &slotId, uint16_t &confidence);
    FpResult enroll(uint8_t slotId);
    bool deleteFinger(uint8_t slotId);
    uint8_t templateCount();

    const char *resultString(FpResult r) const;

private:
    HardwareSerial _serial{2};
    Adafruit_Fingerprint _sensor{&_serial};
    bool _ready = false;

    FpResult mapError(uint8_t code);
};

extern FingerprintManager gFingerprint;
