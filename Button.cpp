#include "Button.h"

Button::Button(int pin) : pin(pin) {}

void Button::begin() {
    pinMode(pin, INPUT_PULLUP);
    stableState = digitalRead(pin);
    lastReading = stableState;
}

void Button::update() {
    bool reading = digitalRead(pin);

    pressedEvent = false;
    releasedEvent = false;

    if (reading != lastReading) {
        lastChangeTime = millis();
        lastReading = reading;
    }

    if ((millis() - lastChangeTime) > debounceMs) {
        if (reading != stableState) {
            stableState = reading;

            if (stableState == LOW) {
                pressedEvent = true;
            } else {
                releasedEvent = true;
            }
        }
    }
}

bool Button::pressed() {
    return pressedEvent;
}

bool Button::released() {
    return releasedEvent;
}

bool Button::isDown() {
    return stableState == LOW;
}
