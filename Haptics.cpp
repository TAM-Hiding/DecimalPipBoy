#include "Haptics.h"

#include "Globals.h"

#include <Arduino.h>

//======================================================
// Internal state
//======================================================

static bool hapticActive = false;
static uint32_t hapticStopTime = 0;

//======================================================
// Public functions
//======================================================

void hapticsInit()
{
    pinMode(HAPTIC_PIN, OUTPUT);

    // Always begin with the motor off.
    digitalWrite(HAPTIC_PIN, LOW);

    hapticActive = false;
    hapticStopTime = 0;

    Serial.println("Haptics initialized.");
}

void hapticPulse(uint16_t durationMs)
{
    if (durationMs == 0)
    {
        return;
    }

    digitalWrite(HAPTIC_PIN, HIGH);

    hapticActive = true;
    hapticStopTime = millis() + durationMs;
}

void hapticsUpdate()
{
    if (!hapticActive)
    {
        return;
    }

    // Signed subtraction keeps this safe across millis() rollover.
    if ((int32_t)(millis() - hapticStopTime) >= 0)
    {
        digitalWrite(HAPTIC_PIN, LOW);

        hapticActive = false;
    }
}
