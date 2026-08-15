#pragma once

#include <Arduino.h>

// Initialize flashlight hardware.
void flashlightInit();

// Set brightness from 0-255.
// 0 = off, 255 = full output.
void flashlightSetBrightness(uint8_t brightness);

// Enable or disable the flashlight while preserving
// the selected brightness level.
void flashlightSetEnabled(bool enabled);

// Toggle current enabled state.
void flashlightToggle();

// Current state accessors.
uint8_t flashlightBrightness();
bool flashlightEnabled();
