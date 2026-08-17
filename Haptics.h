#pragma once

#include <Arduino.h>

// Initialize the haptic feedback motor.
void hapticsInit();

// Begin a non-blocking vibration pulse.
// durationMs = pulse duration in milliseconds.
void hapticPulse(uint16_t durationMs);

// Update haptic timing.
// Call every pass through loop().
void hapticsUpdate();
