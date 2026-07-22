#pragma once
#include <Arduino.h>

class Button {
public:
    Button(int pin);

    void begin();
    void update();

    bool pressed();
    bool released();
    bool isDown();

private:
    int pin;

    bool stableState = HIGH;
    bool lastReading = HIGH;

    bool pressedEvent = false;
    bool releasedEvent = false;

    unsigned long lastChangeTime = 0;
    const unsigned long debounceMs = 20;
};
