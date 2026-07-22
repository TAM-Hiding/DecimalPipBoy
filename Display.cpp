#include "Display.h"
#include "Globals.h"

#include <Arduino.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>

//======================================================
// Display Bus
//======================================================

Arduino_DataBus* displayBus = new Arduino_HWSPI(
    TFT_DC,
    TFT_CS
);

//======================================================
// Display Driver
//======================================================

Arduino_GFX* gfx = new Arduino_ST7796(
    displayBus,
    TFT_RST,
    SCREEN_ROTATION,
    true
);

//======================================================
// Public Functions
//======================================================

bool displayInit()
{
    pinMode(TFT_BL, OUTPUT);

    // Keep the backlight off during controller startup.
    digitalWrite(TFT_BL, LOW);

    if (!gfx->begin())
    {
        return false;
    }

    gfx->setUTF8Print(true);

    gfx->fillScreen(RGB565_BLACK);

    displaySetBacklight(true);

    return true;
}

void displayClear()
{
    gfx->fillScreen(RGB565_BLACK);
}

void displaySetBacklight(bool enabled)
{
    digitalWrite(
        TFT_BL,
        enabled ? HIGH : LOW
    );
}

void displaySetBrightness(uint8_t brightness)
{
    analogWrite(TFT_BL, brightness);
}
