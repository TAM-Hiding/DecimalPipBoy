#pragma once

#include <Arduino_GFX_Library.h>

// Global display object.
// Defined once in Display.cpp and used by the UI modules.
extern Arduino_GFX* gfx;

// Initialize the ST7796S display and backlight.
bool displayInit();

// Clear the full screen to black.
void displayClear();

// Turn the backlight on or off.
void displaySetBacklight(bool enabled);

// Set backlight brightness from 0 to 255.
// This uses PWM on TFT_BL.
void displaySetBrightness(uint8_t brightness);
