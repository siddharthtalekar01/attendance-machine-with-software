#pragma once

#include <Arduino.h>

/** Call once after gDisplay.begin(); starts black screen + background init. */
void splashBegin();

/**
 * Drive splash animation and init tasks. Call every loop while in STATE_BOOT.
 * @return true when slide-out finished and app may enter STATE_HOME.
 */
bool splashTick();

/** True during final slide-up / home slide-in phase. */
bool splashIsTransitioning();

/** Number of enrolled users discovered during boot (for home welcome card). */
int splashEnrolledUserCount();

/** Any init step failed but boot continued (amber warnings were shown). */
bool splashHadWarnings();
