#include "FMRadio.h"

#include "Globals.h"

#include <Wire.h>
#include <RDA5807.h>

//======================================================
// Radio hardware
//======================================================

static RDA5807 radio;

//======================================================
// Internal state
//======================================================

static uint16_t currentFrequency = 9990;
static uint8_t currentVolume = 8;
static bool currentMuted = false;

//======================================================
// Public functions
//======================================================

void fmRadioInit()
{
#if HAS_FM_RADIO
    radio.setup();

    radio.setFrequency(currentFrequency);
    radio.setVolume(currentVolume);

    // Start unmuted.
    currentMuted = false;
#endif
}

void fmRadioSetFrequency(uint16_t frequency)
{
    currentFrequency = frequency;

#if HAS_FM_RADIO
    radio.setFrequency(currentFrequency);
#endif
}

uint16_t fmRadioFrequency()
{
    return currentFrequency;
}

void fmRadioSetVolume(uint8_t volume)
{
    if (volume > 15)
    {
        volume = 15;
    }

    currentVolume = volume;

#if HAS_FM_RADIO
    radio.setVolume(currentVolume);
#endif
}

uint8_t fmRadioVolume()
{
    return currentVolume;
}

void fmRadioSetMuted(bool muted)
{
    currentMuted = muted;

#if HAS_FM_RADIO
    radio.setMute(currentMuted);
#endif
}

void fmRadioToggleMute()
{
    fmRadioSetMuted(!currentMuted);
}

bool fmRadioMuted()
{
    return currentMuted;
}
