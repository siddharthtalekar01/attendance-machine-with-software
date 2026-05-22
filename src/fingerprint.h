#pragma once

#include <Arduino.h>
#include <stdint.h>

/**
 * R307S on UART2 (RX=GPIO16, TX=GPIO17 @ 57600).
 * All polling uses short intervals; functions return status instead of blocking forever.
 */

// fingerprintSearch()
#define FP_SEARCH_OK        0
#define FP_SEARCH_NO_FINGER (-1)
#define FP_SEARCH_NO_MATCH  (-2)

// fingerprintEnroll() — negative error codes
#define FP_ENROLL_OK              0
#define FP_ENROLL_ERR_NO_FINGER   (-1)  // Timeout waiting for finger
#define FP_ENROLL_ERR_IMAGE       (-2)  // getImage / image2Tz failed
#define FP_ENROLL_ERR_NOT_INIT    (-3)  // Call fingerprintInit() first
#define FP_ENROLL_ERR_INVALID_ID  (-4)  // ID out of range
#define FP_ENROLL_ERR_CREATE      (-5)  // createModel failed
#define FP_ENROLL_ERR_STORE       (-6)  // storeModel failed
#define FP_ENROLL_ERR_COMM        (-7)  // Packet / sensor communication error
#define FP_ENROLL_ERR_REMOVE      (-8)  // Timeout waiting for finger removal

using FingerprintProgressCallback = void (*)(int step, const char *msg);

bool fingerprintInit();

int fingerprintEnroll(uint16_t id, FingerprintProgressCallback progressCallback = nullptr);

/** @return FP_SEARCH_OK, FP_SEARCH_NO_FINGER, or FP_SEARCH_NO_MATCH */
int fingerprintSearch(uint16_t &matchedId, uint16_t &confidence);

/** Single non-blocking sample — true if a finger is on the sensor now. */
bool fingerprintFingerPresent();

bool fingerprintDeleteId(uint16_t id);
bool fingerprintDeleteAll();
uint16_t fingerprintGetCount();
bool fingerprintIdExists(uint16_t id);
uint16_t fingerprintGetNextAvailableId();

/** Non-blocking enrollment for UI wizard (call enrollPoll each loop). */
enum class FpEnrollPhase : uint8_t {
    Idle,
    WaitFinger1,
    Capture1,
    WaitRemove,
    WaitFinger2,
    Capture2,
    MergeStore,
    Done,
    Failed,
};

struct FingerprintEnrollContext {
    uint16_t templateId = 0;
    FpEnrollPhase phase = FpEnrollPhase::Idle;
    uint32_t phaseStartMs = 0;
    int lastError = 0;
};

void fingerprintEnrollBegin(FingerprintEnrollContext &ctx, uint16_t id);
/** @return 0 in progress, 1 success, negative error */
int fingerprintEnrollPoll(FingerprintEnrollContext &ctx);
const char *fingerprintEnrollPhaseText(const FingerprintEnrollContext &ctx);
int fingerprintEnrollDotProgress(const FingerprintEnrollContext &ctx);

/** Human-readable string for enroll/search negative codes (for logging/UI). */
const char *fingerprintErrorString(int code);
