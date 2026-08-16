#include "FMPage.h"

#include "Display.h"
#include "FMRadio.h"
#include "Input.h"
#include "Globals.h"

#include <Arduino.h>

//======================================================
// Local UI constants
//======================================================

constexpr uint16_t COLOR_DARK_GREY = 0x4208;
constexpr uint16_t COLOR_MED_GREY  = 0x8410;
constexpr uint16_t COLOR_ORANGE    = 0xFD20;

// FM broadcast band.
// Frequency units are 10 kHz:
// 8750 = 87.5 MHz
// 10800 = 108.0 MHz
constexpr uint16_t FM_MIN = 8750;
constexpr uint16_t FM_MAX = 10800;
constexpr uint16_t FM_STEP = 10;

//======================================================
// Control mode
//======================================================

enum FMControlMode
{
    FM_CONTROL_VOLUME,
    FM_CONTROL_TUNING
};

static FMControlMode controlMode = FM_CONTROL_VOLUME;

//======================================================
// Soft takeover state
//======================================================

static bool knobCaptured = false;

static uint16_t tuningAnchorKnob = 0;
static uint16_t tuningAnchorFrequency = 9990;

//======================================================
// Previous UI state
//======================================================

static uint16_t previousFrequency = 0;
static uint8_t previousVolume = 255;
static FMControlMode previousMode = FM_CONTROL_TUNING;

//======================================================
// Internal helpers
//======================================================

static uint8_t knobToVolume(uint16_t knob)
{
    return static_cast<uint8_t>(
        map(knob, 0, 1023, 0, 15)
    );
}

static void drawFrequency()
{
    uint16_t frequency = fmRadioFrequency();

    // Clear only frequency display area.
    gfx->fillRect(
        125,
        105,
        230,
        45,
        RGB565_BLACK
    );

    float frequencyMHz =
        frequency / 100.0f;

    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(4);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(140, 112);

    gfx->print(frequencyMHz, 1);

    gfx->setTextSize(2);
    gfx->setTextColor(COLOR_MED_GREY);
    gfx->print(" MHz");
}

static void drawVolume()
{
    uint8_t volume = fmRadioVolume();

    // Clear dynamic volume region.
    gfx->fillRect(
        105,
        180,
        270,
        32,
        RGB565_BLACK
    );

    gfx->setFont((const GFXfont*)nullptr);

    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setCursor(105, 183);
    gfx->print("VOLUME");

    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(335, 183);
    gfx->print(volume);
    gfx->print("/15");

    constexpr int barX = 105;
    constexpr int barY = 198;
    constexpr int barWidth = 270;
    constexpr int barHeight = 12;

    gfx->drawRect(
        barX,
        barY,
        barWidth,
        barHeight,
        COLOR_MED_GREY
    );

    gfx->fillRect(
        barX + 1,
        barY + 1,
        barWidth - 2,
        barHeight - 2,
        RGB565_BLACK
    );

    int fillWidth =
        map(
            volume,
            0,
            15,
            0,
            barWidth - 4
        );

    if (fillWidth > 0)
    {
        gfx->fillRect(
            barX + 2,
            barY + 2,
            fillWidth,
            barHeight - 4,
            COLOR_ORANGE
        );
    }
}

static void drawControlMode()
{
    // Clear only control-mode area.
    gfx->fillRect(
        100,
        225,
        280,
        32,
        RGB565_BLACK
    );

    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(1);

    //----------------------------------------------
    // Volume
    //----------------------------------------------

    gfx->setTextColor(
        controlMode == FM_CONTROL_VOLUME
            ? COLOR_ORANGE
            : COLOR_DARK_GREY
    );

    gfx->setCursor(135, 230);
    gfx->print("[ VOLUME ]");

    //----------------------------------------------
    // Tuning
    //----------------------------------------------

    gfx->setTextColor(
        controlMode == FM_CONTROL_TUNING
            ? COLOR_ORANGE
            : COLOR_DARK_GREY
    );

    gfx->setCursor(275, 230);
    gfx->print("[ TUNE ]");

    //----------------------------------------------
    // Pickup indicator
    //----------------------------------------------

    gfx->setTextColor(COLOR_MED_GREY);
    gfx->setCursor(185, 248);

    if (knobCaptured)
    {
        gfx->print("CONTROL ACTIVE");
    }
    else
    {
        gfx->print("MOVE KNOB TO PICKUP");
    }
}

static bool volumePickupReached(uint16_t knob)
{
    uint8_t knobVolume = knobToVolume(knob);
    uint8_t currentVolume = fmRadioVolume();

    return abs(
        static_cast<int>(knobVolume) -
        static_cast<int>(currentVolume)
    ) <= 1;
}

//======================================================
// Public functions
//======================================================

void fmPageDraw()
{
    gfx->drawFastHLine(
        22,
        94,
        SCREEN_WIDTH - 44,
        COLOR_DARK_GREY
    );

    drawFrequency();
    drawVolume();
    drawControlMode();

    previousFrequency = fmRadioFrequency();
    previousVolume = fmRadioVolume();
    previousMode = controlMode;
}

void fmPageUpdate()
{
    uint16_t knob = inputKnobValue();

    //--------------------------------------------------
    // Wait for soft takeover
    //--------------------------------------------------

    if (
        !knobCaptured &&
        controlMode == FM_CONTROL_VOLUME
    )
    {
        if (volumePickupReached(knob))
        {
            knobCaptured = true;
            drawControlMode();
        }
    }

    //--------------------------------------------------
    // Apply knob control
    //--------------------------------------------------

    if (controlMode == FM_CONTROL_VOLUME)
    {
        if (knobCaptured)
        {
            uint8_t newVolume =
                knobToVolume(knob);

            if (newVolume != fmRadioVolume())
            {
                fmRadioSetVolume(newVolume);
            }
        }
    }
    else
    {
        int knobDelta =
            static_cast<int>(knob) -
            static_cast<int>(tuningAnchorKnob);

        // About 5 ADC counts per 0.1 MHz step.
        int frequencySteps = knobDelta / 5;

        int newFrequency =
            static_cast<int>(tuningAnchorFrequency) +
            (frequencySteps * FM_STEP);

        newFrequency =
            constrain(
                newFrequency,
                static_cast<int>(FM_MIN),
                static_cast<int>(FM_MAX)
            );

        if (
            static_cast<uint16_t>(newFrequency) !=
            fmRadioFrequency()
        )
        {
            fmRadioSetFrequency(
                static_cast<uint16_t>(newFrequency)
            );
        }
    }

    //--------------------------------------------------
    // Surgical UI updates
    //--------------------------------------------------

    if (fmRadioFrequency() != previousFrequency)
    {
        drawFrequency();
        previousFrequency = fmRadioFrequency();
    }

    if (fmRadioVolume() != previousVolume)
    {
        drawVolume();
        previousVolume = fmRadioVolume();
    }

    if (controlMode != previousMode)
    {
        drawControlMode();
        previousMode = controlMode;
    }
}

void fmPageSelect()
{
    if (controlMode == FM_CONTROL_VOLUME)
    {
        controlMode = FM_CONTROL_TUNING;

        // Wherever the knob currently sits becomes
        // the zero point for tuning.
        tuningAnchorKnob = inputKnobValue();
        tuningAnchorFrequency = fmRadioFrequency();

        knobCaptured = true;
    }
    else
    {
        controlMode = FM_CONTROL_VOLUME;

        // Volume still uses soft takeover.
        knobCaptured = false;
    }

    drawControlMode();
}
