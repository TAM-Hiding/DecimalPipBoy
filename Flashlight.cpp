#include "Flashlight.h"
#include "Globals.h"

//======================================================
// Internal state
//======================================================

static uint8_t currentBrightness = 128;
static bool currentEnabled = false;

//======================================================
// Internal helpers
//======================================================

static void applyOutput()
{
    if (!currentEnabled || currentBrightness == 0)
    {
        digitalWrite(FLASHLIGHT_EN, LOW);
        return;
    }

    analogWrite(FLASHLIGHT_EN, currentBrightness);
}

//======================================================
// Public functions
//======================================================

void flashlightInit()
{
#if HAS_FLASHLIGHT
    pinMode(FLASHLIGHT_EN, OUTPUT);

    analogWriteResolution(8);
    analogWriteFrequency(FLASHLIGHT_EN, 1000);

    digitalWrite(FLASHLIGHT_EN, LOW);
#endif
}

void flashlightSetBrightness(uint8_t brightness)
{
    currentBrightness = brightness;

#if HAS_FLASHLIGHT
    applyOutput();
#endif
}

void flashlightSetEnabled(bool enabled)
{
    currentEnabled = enabled;

#if HAS_FLASHLIGHT
    applyOutput();
#endif
}

void flashlightToggle()
{
    flashlightSetEnabled(!currentEnabled);
}

uint8_t flashlightBrightness()
{
    return currentBrightness;
}

bool flashlightEnabled()
{
    return currentEnabled;
}
