#pragma once

#include <Arduino.h>

// Initialize the RDA5807M radio.
void fmRadioInit();

// Set/get frequency in 10 kHz units.
// Example: 99.9 MHz = 9990.
void fmRadioSetFrequency(uint16_t frequency);
uint16_t fmRadioFrequency();

// Set/get volume.
// RDA5807 volume range is 0-15.
void fmRadioSetVolume(uint8_t volume);
uint8_t fmRadioVolume();

// Mute control.
void fmRadioSetMuted(bool muted);
void fmRadioToggleMute();
bool fmRadioMuted();
