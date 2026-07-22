#pragma once

// Initialize the three physical navigation buttons.
void inputInit();

// Poll and debounce all buttons.
// Call this every pass through loop().
void inputUpdate();

// One-shot press events.
// Each returns true only on the update where the press is detected.
bool inputLeftPressed();
bool inputSelectPressed();
bool inputRightPressed();
