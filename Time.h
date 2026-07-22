#pragma once

#include <Arduino.h>

// Initialize the time system.
// Uses the DS3231 when available, otherwise falls back
// to compile time plus millis().
void timeInit();

// True when the DS3231 was detected and initialized.
bool rtcPresent();

// Seconds elapsed since local midnight.
uint32_t getSecondsToday();

// STR-GZR decimal time:
// 0.00000 through 9.99999
uint32_t getDecimalTime();

// Set software time from the sketch compile time.
// Used as a fallback and to initialize an unset RTC.
void setTimeFromCompileTime();
