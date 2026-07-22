#include "Logo.h"
#include "Globals.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <math.h>

// Display object lives in Display.cpp
extern U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2;

void drawLogoFrame(int cx, int cy, int radius, int litDots)
{
    const int dotCount = 10;

    for (int i = 0; i < dotCount; i++)
    {
        // Start at top, move clockwise
        float angle = -HALF_PI + (TWO_PI * i / dotCount);

        int x = cx + cos(angle) * radius;
        int y = cy + sin(angle) * radius;

        int dotRadius = 1;

        // Top dot is larger
        if (i == 0)
        {
            dotRadius = 2;
        }

        if (i < litDots)
        {
            u8g2.drawDisc(x, y, dotRadius);
        }
        else
        {
            u8g2.drawCircle(x, y, dotRadius);
        }
    }
}

void drawLogo(int cx, int cy, int radius)
{
    drawLogoFrame(cx, cy, radius, 10);
}
