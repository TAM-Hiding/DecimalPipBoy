#include "FMPage.h"

#include "Display.h"
#include "FMRadio.h"
#include "Input.h"
#include "Globals.h"
#include "Touch.h"
#include "Haptics.h"

#include <Arduino.h>

//======================================================
// Local UI constants
//======================================================

constexpr uint16_t COLOR_DARK_GREY = 0x4208;
constexpr uint16_t COLOR_MED_GREY  = 0x8410;
constexpr uint16_t COLOR_ORANGE    = 0xFD20;

// FM control-state indicators.
constexpr uint16_t COLOR_NEON_GREEN = 0x07E0;
constexpr uint16_t COLOR_MUSTARD    = 0xD5A0;

// FM broadcast band.
// Frequency units are 10 kHz:
// 8750 = 87.5 MHz
// 10800 = 108.0 MHz
constexpr uint16_t FM_MIN = 8750;
constexpr uint16_t FM_MAX = 10800;
constexpr uint16_t FM_STEP = 10;

//======================================================
// Touch hitboxes
//======================================================

constexpr int POWER_X = 360;
constexpr int POWER_Y = 108;
constexpr int POWER_W = 90;
constexpr int POWER_H = 34;

//======================================================
// Preset button geometry
//======================================================

constexpr int PRESET_COUNT = 6;

constexpr int PRESET_START_X = 60;
constexpr int PRESET_Y = 160;

constexpr int PRESET_W = 50;
constexpr int PRESET_H = 32;

constexpr int PRESET_GAP = 12;

//======================================================
// FM presets
//======================================================

static uint16_t presetFrequencies[PRESET_COUNT] =
{
    9030,   // 90.3 MHz
    9490,   // 94.9 MHz
    9990,   // 99.9 MHz
    10150,  // 101.5 MHz
    10370,  // 103.7 MHz
    10770   // 107.7 MHz
};


//======================================================
// Preset press state
//======================================================

static int8_t heldPreset = -1;
static uint32_t presetPressStart = 0;
static bool presetSaveCompleted = false;

constexpr uint32_t PRESET_SAVE_HOLD_MS = 750;

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
    gfx->setCursor(170, 112);

    gfx->print(frequencyMHz, 1);

    gfx->setTextSize(2);
    gfx->setTextColor(COLOR_MED_GREY);
    gfx->print(" MHz");
}

static void drawVolume()
{
    uint8_t volume = fmRadioVolume();

    // Clear only the volume display area.
    gfx->fillRect(
        105,
        232,
        270,
        36,
        RGB565_BLACK
    );

    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(1);

    //--------------------------------------------------
    // Label and numeric value
    //--------------------------------------------------

    gfx->setTextColor(COLOR_MED_GREY);
    gfx->setCursor(105, 235);
    gfx->print("VOLUME");

    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(340, 235);
    gfx->print(volume);
    gfx->print("/15");

    //--------------------------------------------------
    // Volume bar
    //--------------------------------------------------

    constexpr int BAR_X = 105;
    constexpr int BAR_Y = 250;
    constexpr int BAR_W = 270;
    constexpr int BAR_H = 10;

    gfx->drawRect(
        BAR_X,
        BAR_Y,
        BAR_W,
        BAR_H,
        RGB565_WHITE
    );

    int fillWidth =
        map(
            volume,
            0,
            15,
            0,
            BAR_W - 4
        );

    // Clear the interior first so decreasing volume
    // doesn't leave the old bar behind.
    gfx->fillRect(
        BAR_X + 2,
        BAR_Y + 2,
        BAR_W - 4,
        BAR_H - 4,
        RGB565_BLACK
    );

    if (fillWidth > 0)
    {
        gfx->fillRect(
            BAR_X + 2,
            BAR_Y + 2,
            fillWidth,
            BAR_H - 4,
            COLOR_ORANGE
        );
    }
}

static int presetX(uint8_t index)
{
    return
        PRESET_START_X +
        index * (PRESET_W + PRESET_GAP);
}

static int8_t presetAt(
    uint16_t x,
    uint16_t y
)
{
    if (
        y < PRESET_Y ||
        y >= PRESET_Y + PRESET_H
    )
    {
        return -1;
    }

    for (uint8_t i = 0; i < PRESET_COUNT; i++)
    {
        int x0 = presetX(i);

        if (
            x >= x0 &&
            x < x0 + PRESET_W
        )
        {
            return static_cast<int8_t>(i);
        }
    }

    return -1;
}

static bool volumePickupReached(uint16_t knob);

static void drawQuarterCircle(
    int cx,
    int cy,
    int r,
    uint8_t quadrant,
    uint16_t color
)
{
    int x = 0;
    int y = r;
    int d = 3 - (2 * r);

    while (y >= x)
    {
        switch (quadrant)
        {
            case 1: // top-left
                gfx->drawPixel(cx - x, cy - y, color);
                gfx->drawPixel(cx - y, cy - x, color);
                break;

            case 2: // top-right
                gfx->drawPixel(cx + x, cy - y, color);
                gfx->drawPixel(cx + y, cy - x, color);
                break;

            case 3: // bottom-left
                gfx->drawPixel(cx - x, cy + y, color);
                gfx->drawPixel(cx - y, cy + x, color);
                break;

            case 4: // bottom-right
                gfx->drawPixel(cx + x, cy + y, color);
                gfx->drawPixel(cx + y, cy + x, color);
                break;
        }

        x++;

        if (d > 0)
        {
            y--;
            d += 4 * (x - y) + 10;
        }
        else
        {
            d += 4 * x + 6;
        }
    }
}

static void drawDiagonalRoundRect(
    int x,
    int y,
    int w,
    int h,
    int r,
    uint16_t color,
    bool mirror
)
{
    //--------------------------------------------------
    // Presets 1–3
    // Rounded: top-left + bottom-right
    //--------------------------------------------------

    if (!mirror)
    {
        // Top edge:
        // rounded at left, square at right.
        gfx->drawFastHLine(
            x + r,
            y,
            w - r,
            color
        );

        // Bottom edge:
        // square at left, rounded at right.
        gfx->drawFastHLine(
            x,
            y + h - 1,
            w - r,
            color
        );

        // Left edge:
        // rounded at top, square at bottom.
        gfx->drawFastVLine(
            x,
            y + r,
            h - r,
            color
        );

        // Right edge:
        // square at top, rounded at bottom.
        gfx->drawFastVLine(
            x + w - 1,
            y,
            h - r,
            color
        );

        // Rounded top-left.
        drawQuarterCircle(
            x + r,
            y + r,
            r,
            1,
            color
        );

        // Rounded bottom-right.
        drawQuarterCircle(
            x + w - r - 1,
            y + h - r - 1,
            r,
            4,
            color
        );
    }

    //--------------------------------------------------
    // Presets 4–6
    // Rounded: top-right + bottom-left
    //--------------------------------------------------

    else
    {
        // Top edge:
        // square at left, rounded at right.
        gfx->drawFastHLine(
            x,
            y,
            w - r,
            color
        );

        // Bottom edge:
        // rounded at left, square at right.
        gfx->drawFastHLine(
            x + r,
            y + h - 1,
            w - r,
            color
        );

        // Left edge:
        // square at top, rounded at bottom.
        gfx->drawFastVLine(
            x,
            y,
            h - r,
            color
        );

        // Right edge:
        // rounded at top, square at bottom.
        gfx->drawFastVLine(
            x + w - 1,
            y + r,
            h - r,
            color
        );

        // Rounded top-right.
        drawQuarterCircle(
            x + w - r - 1,
            y + r,
            r,
            2,
            color
        );

        // Rounded bottom-left.
        drawQuarterCircle(
            x + r,
            y + h - r - 1,
            r,
            3,
            color
        );
    }
}

static void drawPresetButtons()
{
    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(2);

    constexpr int CORNER_RADIUS = 10;

    for (uint8_t i = 0; i < PRESET_COUNT; i++)
    {
        const int x = presetX(i);

        // Clear the complete button area first.
        gfx->fillRect(
            x,
            PRESET_Y,
            PRESET_W,
            PRESET_H,
            RGB565_BLACK
        );

        // 1–3 use TL + BR rounded corners.
        // 4–6 use TR + BL rounded corners.
        const bool mirror = (i >= 3);

        uint16_t outlineColor =
            presetFrequencies[i] == fmRadioFrequency()
                ? COLOR_ORANGE
                : COLOR_MED_GREY;


        drawDiagonalRoundRect(
            x,
            PRESET_Y,
            PRESET_W,
            PRESET_H,
            CORNER_RADIUS,
            outlineColor,
            mirror
        );

        //--------------------------------------------------
        // Number
        //--------------------------------------------------

        gfx->setTextColor(RGB565_WHITE);

        gfx->setCursor(
            x + 20,
            PRESET_Y + 9
        );

        gfx->print(i + 1);
    }
}

static void drawControlMode()
{
    // Clear the complete mode-control row.
    gfx->fillRect(
        120,
        202,
        240,
        27,
        RGB565_BLACK
    );

    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(1);

    //--------------------------------------------------
    // VOLUME
    //--------------------------------------------------

    gfx->setTextColor(
        controlMode == FM_CONTROL_VOLUME
            ? RGB565_WHITE
            : COLOR_MED_GREY
    );

    gfx->setCursor(150, 214);
    gfx->print("[ VOLUME ]");

    //--------------------------------------------------
    // State indicator
    //--------------------------------------------------

    bool controlActive = true;

    if (controlMode == FM_CONTROL_VOLUME)
    {
        controlActive = knobCaptured;
    }

    gfx->fillCircle(
        240,
        217,
        4,
        controlActive
            ? COLOR_NEON_GREEN
            : COLOR_MUSTARD
    );

    //--------------------------------------------------
    // TUNE
    //--------------------------------------------------

    gfx->setTextColor(
        controlMode == FM_CONTROL_TUNING
            ? RGB565_WHITE
            : COLOR_MED_GREY
    );

    gfx->setCursor(275, 214);
    gfx->print("[ TUNE ]");
}

//======================================================
// FM power button
//======================================================

static void drawPowerButton()
{
    const bool powered = fmRadioPowered();

    gfx->fillRect(
        POWER_X,
        POWER_Y,
        POWER_W,
        POWER_H,
        RGB565_BLACK
    );

    gfx->drawRect(
        POWER_X,
        POWER_Y,
        POWER_W,
        POWER_H,
        powered
            ? COLOR_ORANGE
            : COLOR_MED_GREY
    );

    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(1);

    gfx->setTextColor(
        powered
            ? COLOR_ORANGE
            : COLOR_MED_GREY
    );

    gfx->setCursor(
        POWER_X + 13,
        POWER_Y + 13
    );

    gfx->print(
        powered
            ? "RADIO ON"
            : "RADIO OFF"
    );
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
// Preset press handling
//======================================================

static void updatePresetPress()
{
    if (heldPreset < 0)
    {
        return;
    }

    //--------------------------------------------------
    // Long hold -> save
    //--------------------------------------------------

    if (
        touchHeld() &&
        !presetSaveCompleted &&
        millis() - presetPressStart >= PRESET_SAVE_HOLD_MS
    )
    {
        presetFrequencies[heldPreset] =
            fmRadioFrequency();

        presetSaveCompleted = true;

        drawPresetButtons();

        hapticPulse(100);

        return;
    }

    //--------------------------------------------------
    // Finger lifted -> recall if this was a tap
    //--------------------------------------------------

    if (!touchHeld())
    {
        if (!presetSaveCompleted)
        {
            fmRadioSetFrequency(
                presetFrequencies[heldPreset]
            );

            //--------------------------------------------------
            // If TUNE owns the knob, make the recalled
            // station the new tuning anchor.
            //--------------------------------------------------

            if (controlMode == FM_CONTROL_TUNING)
            {
                tuningAnchorKnob =
                    inputKnobValue();

                tuningAnchorFrequency =
                    fmRadioFrequency();
            }

            drawFrequency();
            drawPresetButtons();

            previousFrequency =
                fmRadioFrequency();
        }

        heldPreset = -1;
        presetPressStart = 0;
        presetSaveCompleted = false;
    }
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
    drawPresetButtons();
    drawControlMode();
    drawVolume();
    drawPowerButton();

    previousFrequency = fmRadioFrequency();
    previousVolume = fmRadioVolume();
    previousMode = controlMode;
}

void fmPageUpdate()
{
    updatePresetPress();
 
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
        drawPresetButtons();
        
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

bool fmPageHandleTouch(
    uint16_t x,
    uint16_t y
)
{

    //--------------------------------------------------
    // Radio power
    //--------------------------------------------------

    if (
        x >= POWER_X &&
        x < POWER_X + POWER_W &&
        y >= POWER_Y &&
        y < POWER_Y + POWER_H
    )
    {
        fmRadioTogglePower();
        drawPowerButton();

        return true;
    }

    //--------------------------------------------------
    // Presets
    //--------------------------------------------------

    int8_t preset = presetAt(x, y);

    if (preset >= 0)
    {
        heldPreset = preset;
        presetPressStart = millis();
        presetSaveCompleted = false;

        return true;
    }

    return false;
}
