#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>

// Initialize GT911 touch support.
// Wire.begin() must already have been called.
bool touchInit();

// Poll the GT911.
// Call once every main loop.
void touchUpdate();

// True once when a new touch begins.
bool touchPressed();

// True once when the finger is released.
bool touchReleased();

// True while a finger remains on the screen.
bool touchHeld();

// Latest raw GT911 coordinates.
uint16_t touchX();
uint16_t touchY();

// Touch-point identifier reported by the GT911.
uint8_t touchID();

// Number of active touch points.
uint8_t touchPointCount();

// Whether the GT911 responded during initialization.
bool touchAvailable();

#endif
