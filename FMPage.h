#pragma once

#include <Arduino.h>

void fmPageDraw();
void fmPageUpdate();
void fmPageSelect();

// Returns true if the FM page used this touch.
bool fmPageHandleTouch(
    uint16_t x,
    uint16_t y
);
