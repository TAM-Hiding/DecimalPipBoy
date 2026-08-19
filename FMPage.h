#pragma once

#include <Arduino.h>

// Load saved presets from persistent storage.
void fmPageInit();

void fmPageDraw();
void fmPageUpdate();
void fmPageSelect();

// Returns true if the FM page used this touch.
bool fmPageHandleTouch(
    uint16_t x,
    uint16_t y
);
