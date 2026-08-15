#include "FlashlightPage.h"

#include "Display.h"
#include "Flashlight.h"
#include "Input.h"
#include "Globals.h"

#include <Arduino.h>

//======================================================
// Local UI constants
//======================================================

constexpr uint16_t COLOR_DARK_GREY = 0x4208;
constexpr uint16_t COLOR_MED_GREY  = 0x8410;
constexpr uint16_t COLOR_ORANGE    = 0xFD20;

//======================================================
// Local state
//======================================================

static uint8_t previousBrightness = 255;
static bool previousEnabled = false;
static bool firstDraw = true;

//======================================================
// Internal helpers
//======================================================

static uint8_t readBrightnessFromKnob()
{
    uint16_t knobValue = inputKnobValue();

    return static_cast<uint8_t>(
        map(knobValue, 0, 1023, 0, 255)
    );
}

static void drawBrightnessBar(uint8_t brightness)
{
    constexpr int x = 90;
    constexpr int y = 190;
    constexpr int width = 300;
    constexpr int height = 22;

    // Clear inside of bar.
    gfx->fillRect(
        x + 1,
        y + 1,
        width - 2,
        height - 2,
        RGB565_BLACK
    );

    // Border.
    gfx->drawRect(
        x,
        y,
        width,
        height,
        COLOR_MED_GREY
    );

    int fillWidth =
        map(
            brightness,
            0,
            255,
            0,
            width - 4
        );

    if (fillWidth > 0)
    {
        gfx->fillRect(
            x + 2,
            y + 2,
            fillWidth,
            height - 4,
            COLOR_ORANGE
        );
    }
}

static void drawFlashlightState()
{
    uint8_t brightness = flashlightBrightness();
    bool enabled = flashlightEnabled();

    // Clear dynamic area.
    gfx->fillRect(
        80,
        105,
        320,
        115,
        RGB565_BLACK
    );

    gfx->setFont((const GFXfont*)nullptr);

    //--------------------------------------------------
    // Output state
    //--------------------------------------------------

    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setCursor(214, 112);
    gfx->print("OUTPUT");

    gfx->setTextSize(3);

    if (enabled)
    {
        gfx->setTextColor(RGB565_WHITE);
        gfx->setCursor(218, 132);
        gfx->print("ON");
    }
    else
    {
        gfx->setTextColor(COLOR_MED_GREY);
        gfx->setCursor(208, 132);
        gfx->print("OFF");
    }

    //--------------------------------------------------
    // Brightness
    //--------------------------------------------------

    int percent =
        map(
            brightness,
            0,
            255,
            0,
            100
        );

    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setCursor(174, 172);
    gfx->print("BRIGHTNESS ");

    gfx->setTextColor(RGB565_WHITE);
    gfx->print(percent);
    gfx->print("%");

    drawBrightnessBar(brightness);
}

//======================================================
// Public functions
//======================================================

void flashlightPageDraw()
{
    firstDraw = true;

    gfx->drawFastHLine(
        22,
        94,
        480 - 44,
        COLOR_DARK_GREY
    );

    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setCursor(180, 242);
    gfx->print("SELECT: TOGGLE OUTPUT");

    drawFlashlightState();

    previousBrightness = flashlightBrightness();
    previousEnabled = flashlightEnabled();
    firstDraw = false;
}

void flashlightPageUpdate()
{
    uint8_t brightness = readBrightnessFromKnob();

    if (brightness != flashlightBrightness())
    {
        flashlightSetBrightness(brightness);
    }

    bool enabled = flashlightEnabled();

    if (
        firstDraw ||
        brightness != previousBrightness ||
        enabled != previousEnabled
    )
    {
        drawFlashlightState();

        previousBrightness = brightness;
        previousEnabled = enabled;
        firstDraw = false;
    }
}

void flashlightPageSelect()
{
    flashlightToggle();
}
