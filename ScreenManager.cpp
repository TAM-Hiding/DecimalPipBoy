#include "ScreenManager.h"
#include "Display.h"
#include "Globals.h"
#include "Time.h"
#include "IMU.h"
#include "Environment.h"
#include "FlashlightPage.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include <math.h>
#include <string.h>

//======================================================
// Colors
//======================================================

constexpr uint16_t COLOR_DARK_GREY = 0x4208;
constexpr uint16_t COLOR_MED_GREY = 0x8410;
constexpr uint16_t COLOR_WATERMARK = 0x1082;
constexpr uint16_t COLOR_ORANGE = 0xFD20;

//======================================================
// Pages
//======================================================

enum Page
{
    PAGE_CLOCK,
    PAGE_COMPASS,
    PAGE_ATMOSPHERIC,
    PAGE_FLASHLIGHT,
    PAGE_CALIBRATION,
    PAGE_SYSTEM,
    PAGE_COUNT
};

static Page currentPage = PAGE_CLOCK;

//======================================================
// Compass display modes
//======================================================

enum CompassDisplayMode
{
    COMPASS_MODE_DIAL,
    COMPASS_MODE_CARDINAL_TAPE,
    COMPASS_MODE_DEGREE_TAPE,
    COMPASS_MODE_COUNT
};

static CompassDisplayMode compassDisplayMode =
    COMPASS_MODE_DIAL;

//======================================================
// Dynamic drawing state
//======================================================

static char previousTimeText[12] = "";
static char previousEnvironmentText[48] = "";

static char previousAtmosphericTemp[24] = "";
static char previousAtmosphericHumidity[16] = "";
static char previousAtmosphericDewPoint[24] = "";

static float previousCompassHeading = -1000.0f;
static int previousTapeHeading = -1000;

static float previousTiltRoll = -1000.0f;
static float previousTiltPitch = -1000.0f;

static int previousCalibrationProgress = -1;
static int previousCalibrationState = -1;

//======================================================
// Shared UI Helpers
//======================================================

static void drawWakariHeaderMark()
{
    constexpr int centerX = 240;
    constexpr int centerY = 30;
    constexpr int radius = 18;
    constexpr int dotCount = 10;

    for (int i = 0; i < dotCount; i++)
    {
        float angle =
            -HALF_PI +
            (
                TWO_PI *
                i /
                dotCount
            );

        int x =
            centerX +
            (int)(
                cosf(angle) *
                radius
            );

        int y =
            centerY +
            (int)(
                sinf(angle) *
                radius
            );

        int dotRadius =
            (i == 0)
                ? 3
                : 2;

        uint16_t dotColor =
            (i == 0)
                ? COLOR_MED_GREY
                : COLOR_WATERMARK;

        gfx->fillCircle(
            x,
            y,
            dotRadius,
            dotColor
        );
    }
}

void drawHeader(const char* pageName)
{
    gfx->setFont((const GFXfont *)nullptr);
    gfx->setTextSize(2);

    gfx->setTextColor(COLOR_ORANGE);
    gfx->setCursor(40, 22);
    gfx->print("STR-GZR");

    gfx->setTextColor(COLOR_ORANGE);

    // Custom delta symbol.
    constexpr int deltaX = 137;
    constexpr int deltaY = 22;
    constexpr int deltaWidth = 14;
    constexpr int deltaHeight = 13;

    gfx->drawTriangle(
        deltaX + deltaWidth / 2,
        deltaY,

        deltaX,
        deltaY + deltaHeight,

        deltaX + deltaWidth,
        deltaY + deltaHeight,

        COLOR_ORANGE
    );

    // Small bottom opening so it reads as Δ,
    // rather than a solid warning triangle.
    gfx->drawFastHLine(
        deltaX + 9,
        deltaY + deltaHeight,
        deltaWidth - 13,
        COLOR_ORANGE
    );

    // Draw OS immediately after it.
    gfx->setCursor(157, 22);
    gfx->print("OS");

    gfx->setTextSize(2);

    String versionText =
        String(OS_BUILD) +
        " v" +
        OS_VERSION;

    int16_t boundsX;
    int16_t boundsY;
    uint16_t textWidth;
    uint16_t textHeight;

    gfx->getTextBounds(
        versionText,
        0,
        0,
        &boundsX,
        &boundsY,
        &textWidth,
        &textHeight
    );

    int versionX =
        SCREEN_WIDTH -
        textWidth -
        18;

    gfx->setTextColor(COLOR_ORANGE);
    gfx->setCursor(versionX, 22);
    gfx->print(versionText);
    
    drawWakariHeaderMark();

    gfx->drawFastHLine(
        0,
        58,
        SCREEN_WIDTH,
        RGB565_WHITE
    );

    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setTextSize(1);
    gfx->setCursor(20, 68);
    gfx->print(pageName);
}

void drawFooter()
{
    gfx->drawFastHLine(
        0,
        278,
        SCREEN_WIDTH,
        RGB565_WHITE
    );

    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(18, 295);
    gfx->print("< PREV");

    // Japanese manufacturer wordmark.
    gfx->setFont(
        u8g2_font_unifont_t_japanese1
    );

    gfx->setTextColor(COLOR_MED_GREY);

    const char* japaneseBrand =
        "理解研究所";

    int16_t japaneseX;
    int16_t japaneseY;
    uint16_t japaneseWidth;
    uint16_t japaneseHeight;

    gfx->getTextBounds(
        japaneseBrand,
        0,
        0,
        &japaneseX,
        &japaneseY,
        &japaneseWidth,
        &japaneseHeight
    );

    gfx->setCursor(
        (SCREEN_WIDTH - japaneseWidth) / 2,
        298
    );

    gfx->print(japaneseBrand);

    // Small Roman manufacturer caption.
    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_DARK_GREY);

    const char* romanBrand =
        "W A K A R I   L A B S";

    int16_t romanX;
    int16_t romanY;
    uint16_t romanWidth;
    uint16_t romanHeight;

    gfx->getTextBounds(
        romanBrand,
        0,
        0,
        &romanX,
        &romanY,
        &romanWidth,
        &romanHeight
    );

    gfx->setCursor(
        (SCREEN_WIDTH - romanWidth) / 2,
        305
    );

    gfx->print(romanBrand);

    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(424, 295);
    gfx->print("NEXT >");
}

void drawStatusRow(
    int y,
    const char* label,
    const char* status,
    uint16_t statusColor
)
{
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(42, y);
    gfx->print(label);

    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setCursor(205, y);
    gfx->print("..............");

    gfx->setTextColor(statusColor);
    gfx->setCursor(390, y);
    gfx->print(status);
}

void drawAtmosphericRow(
    int y,
    const char* label,
    const char* value,
    uint16_t valueColor
)
{
    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(2);

    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(42, y);
    gfx->print(label);

    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setCursor(180, y);
    gfx->print(".............");

    gfx->setTextColor(
        valueColor,
        RGB565_BLACK
    );

    gfx->setCursor(350, y);
    gfx->print(value);
}

//======================================================
// Helper Functions
//======================================================

static uint16_t compassTapeColor(
    int x,
    int leftEdge,
    int rightEdge
)
{
    int distanceFromLeft =
        x - leftEdge;

    int distanceFromRight =
        rightEdge - x;

    int edgeDistance =
        min(
            distanceFromLeft,
            distanceFromRight
        );

    if (edgeDistance < 30)
    {
        return COLOR_DARK_GREY;
    }

    if (edgeDistance < 65)
    {
        return COLOR_MED_GREY;
    }

    return RGB565_WHITE;
}

static int wrapHeading(int degrees)
{
    while (degrees < 0)
    {
        degrees += 360;
    }

    while (degrees >= 360)
    {
        degrees -= 360;
    }

    return degrees;
}

const char* cardinalForDegrees(int degrees)
{
    degrees = wrapHeading(degrees);

    if (degrees == 0)   return "N";
    if (degrees == 45)  return "NE";
    if (degrees == 90)  return "E";
    if (degrees == 135) return "SE";
    if (degrees == 180) return "S";
    if (degrees == 225) return "SW";
    if (degrees == 270) return "W";
    if (degrees == 315) return "NW";

    return nullptr;
}

//======================================================
// Decimal Clock
//======================================================

void formatDecimalTime(
    uint32_t value,
    char* buffer,
    size_t bufferSize
)
{
    value %= 1000000UL;

    uint32_t decimalHour =
        value / 100000UL;

    uint32_t decimalRemainder =
        value % 100000UL;

    snprintf(
        buffer,
        bufferSize,
        "%lu.%05lu",
        decimalHour,
        decimalRemainder
    );
}

void drawClockPageStatic()
{
    memset(
        previousTimeText,
        0,
        sizeof(previousTimeText)
    );

    memset(
        previousEnvironmentText,
        0,
        sizeof(previousEnvironmentText)
    );

    displayClear();

    gfx->drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        RGB565_WHITE
    );

    drawHeader("D E C I M A L   C H R O N O M E T E R");

    gfx->drawFastHLine(
        55,
        191,
        SCREEN_WIDTH - 110,
        COLOR_DARK_GREY
    );

    const char* tickLabel =
        "1 DAY = 10 HOURS = 1,000,000 TICKS";

    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setTextSize(1);

    int16_t labelBoundsX;
    int16_t labelBoundsY;
    uint16_t labelWidth;
    uint16_t labelHeight;

    gfx->getTextBounds(
        tickLabel,
        0,
        0,
        &labelBoundsX,
        &labelBoundsY,
        &labelWidth,
        &labelHeight
    );

    gfx->setCursor(
        (SCREEN_WIDTH - labelWidth) / 2,
        211
    );

    gfx->print(tickLabel);

    const char* modelLabel =
        "MODEL: D-10";

    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setTextSize(1);

    int16_t modelBoundsX;
    int16_t modelBoundsY;
    uint16_t modelWidth;
    uint16_t modelHeight;

    gfx->getTextBounds(
        modelLabel,
        0,
        0,
        &modelBoundsX,
        &modelBoundsY,
        &modelWidth,
        &modelHeight
    );

    gfx->setCursor(
        (SCREEN_WIDTH - modelWidth) / 2,
        237
    );

    gfx->print(modelLabel);

    drawFooter();
}

void updateClockValue()
{
    uint32_t decimalTime =
        getDecimalTime();

    char timeText[12];

    formatDecimalTime(
        decimalTime,
        timeText,
        sizeof(timeText)
    );

    constexpr int TEXT_SIZE = 8;
    constexpr int CHARACTER_ADVANCE =
        6 * TEXT_SIZE;
    constexpr int CHARACTER_COUNT = 7;

    const int totalWidth =
        CHARACTER_ADVANCE *
        CHARACTER_COUNT;

    const int startX =
        (SCREEN_WIDTH - totalWidth) / 2;

    const int baselineY = 111;

    gfx->setFont((const GFXfont *)nullptr);
    gfx->setTextSize(TEXT_SIZE);

    gfx->setTextColor(
        RGB565_WHITE,
        RGB565_BLACK
    );

    for (
        int i = 0;
        i < CHARACTER_COUNT;
        i++
    )
    {
        if (
            timeText[i] !=
            previousTimeText[i]
        )
        {
            gfx->setCursor(
                startX +
                (i * CHARACTER_ADVANCE),
                baselineY
            );

            gfx->print(timeText[i]);

            previousTimeText[i] =
                timeText[i];
        }
    }

    previousTimeText[
        CHARACTER_COUNT
    ] = '\0';
}

void updateClockEnvironmentWidget()
{
    constexpr int widgetY = 260;
    constexpr int widgetHeight = 10;
    constexpr int rightMargin = 18;

    gfx->setFont((const GFXfont*)nullptr);
    gfx->setTextSize(1);

    char environmentText[48];

    if (
        Environment::isPresent() &&
        Environment::hasValidReading()
    )
    {
        snprintf(
            environmentText,
            sizeof(environmentText),
            "TEMP: %.0fF | %.0fC | RH: %.0f%%",
            Environment::temperatureF(),
            Environment::temperatureC(),
            Environment::humidityRH()
        );
    }
    else
    {
        snprintf(
            environmentText,
            sizeof(environmentText),
            "TEMP: -- F | -- C | RH: --%%"
        );
    }

    if (
        strcmp(
            environmentText,
            previousEnvironmentText
        ) == 0
    )
    {
        return;
    }

    int16_t boundsX;
    int16_t boundsY;
    uint16_t textWidth;
    uint16_t textHeight;

    gfx->getTextBounds(
        environmentText,
        0,
        0,
        &boundsX,
        &boundsY,
        &textWidth,
        &textHeight
    );

    constexpr int clearX = 170;
    constexpr int clearWidth =
        SCREEN_WIDTH -
        clearX -
        rightMargin;

    gfx->fillRect(
        clearX,
        widgetY - 1,
        clearWidth,
        widgetHeight,
        RGB565_BLACK
    );

    int textX =
        SCREEN_WIDTH -
        textWidth -
        rightMargin;

    gfx->setTextColor(
        COLOR_MED_GREY,
        RGB565_BLACK
    );

    gfx->setCursor(
        textX,
        widgetY
    );

    gfx->print(environmentText);

    strncpy(
        previousEnvironmentText,
        environmentText,
        sizeof(previousEnvironmentText) - 1
    );

    previousEnvironmentText[
        sizeof(previousEnvironmentText) - 1
    ] = '\0';
}

void drawClockPage()
{
    drawClockPageStatic();
    updateClockValue();
    updateClockEnvironmentWidget();
}

//======================================================
// Compass Page
//======================================================

constexpr int COMPASS_CX = 240;
constexpr int COMPASS_CY = 170;

constexpr int COMPASS_OUTER_RADIUS = 78;
constexpr int COMPASS_INNER_RADIUS = 57;
constexpr int COMPASS_NEEDLE_LENGTH = 50;

static void drawCardinalTapeStatic()
{
    constexpr int tapeX = 40;
    constexpr int tapeY = 125;
    constexpr int tapeWidth = 400;
    constexpr int tapeHeight = 90;

    constexpr int centerX =
        tapeX + tapeWidth / 2;

    // Tape border
    gfx->drawRect(
        tapeX,
        tapeY,
        tapeWidth,
        tapeHeight,
        RGB565_WHITE
    );

    // Horizontal guide line
    gfx->drawFastHLine(
        tapeX + 12,
        tapeY + 58,
        tapeWidth - 24,
        COLOR_DARK_GREY
    );

    // Fixed center pointer
    gfx->fillTriangle(
        centerX,
        tapeY + 76,

        centerX - 7,
        tapeY + 87,

        centerX + 7,
        tapeY + 87,

        COLOR_ORANGE
    );
}

static void drawDegreeTapeStatic()
{
    constexpr int tapeX = 40;
    constexpr int tapeY = 105;
    constexpr int tapeWidth = 400;
    constexpr int tapeHeight = 110;

    constexpr int centerX =
        tapeX + tapeWidth / 2;

    // Instrument border
    gfx->drawRect(
        tapeX,
        tapeY,
        tapeWidth,
        tapeHeight,
        COLOR_DARK_GREY
    );

    // Tick baseline
    gfx->drawFastHLine(
        tapeX + 12,
        tapeY + 72,
        tapeWidth - 24,
        COLOR_DARK_GREY
    );

    // Fixed center cursor
    gfx->drawFastVLine(
        centerX,
        tapeY + 54,
        30,
        COLOR_ORANGE
    );

    gfx->fillTriangle(
        centerX,
        tapeY + 84,

        centerX - 7,
        tapeY + 95,

        centerX + 7,
        tapeY + 95,

        COLOR_ORANGE
    );
}

static void updateCardinalTape()
{
    constexpr int tapeX = 41;
    constexpr int tapeY = 126;
    constexpr int tapeWidth = 398;
    constexpr int tapeHeight = 72;

    constexpr int centerX = 240;
    constexpr float pixelsPerDegree = 3.0f;

    constexpr int leftEdge =
        tapeX;

    constexpr int rightEdge =
        tapeX + tapeWidth - 1;

    float heading =
        imuHeadingDegrees();

    int displayedHeading =
        (int)roundf(heading);

    if (displayedHeading == previousTapeHeading)
    {
        return;
    }

    previousTapeHeading =
        displayedHeading;

    // Clear the moving interior.
    gfx->fillRect(
        tapeX,
        tapeY,
        tapeWidth,
        tapeHeight,
        RGB565_BLACK
    );

    // Restore the horizontal guide.
    gfx->drawFastHLine(
        tapeX + 11,
        tapeY + 57,
        tapeWidth - 22,
        COLOR_DARK_GREY
    );

    for (
        int bearing = 0;
        bearing < 360;
        bearing += 45
    )
    {
        int difference =
            bearing -
            displayedHeading;

        while (difference > 180)
        {
            difference -= 360;
        }

        while (difference < -180)
        {
            difference += 360;
        }

        int x =
            centerX -
            (int)(
                difference *
                pixelsPerDegree
            );

        const char* label =
            cardinalForDegrees(bearing);

        if (label == nullptr)
        {
            continue;
        }

        gfx->setFont(
            (const GFXfont*)nullptr
        );

        gfx->setTextSize(2);

        int16_t boundsX;
        int16_t boundsY;
        uint16_t textWidth;
        uint16_t textHeight;

        gfx->getTextBounds(
            label,
            0,
            0,
            &boundsX,
            &boundsY,
            &textWidth,
            &textHeight
        );

        int textLeft =
            x -
            textWidth / 2;

        int textRight =
            textLeft +
            textWidth -
            1;

        // Do not draw any part of a label outside
        // the tape's moving interior.
        if (
            textLeft < leftEdge + 2 ||
            textRight > rightEdge - 2
        )
        {
            continue;
        }

        uint16_t fadeColor =
            compassTapeColor(
                x,
                leftEdge,
                rightEdge
            );

        gfx->setTextColor(fadeColor);

        gfx->setCursor(
            textLeft,
            tapeY + 29
        );

        gfx->print(label);

        gfx->drawFastVLine(
            x,
            tapeY + 44,
            14,
            fadeColor
        );
    }

    // Redraw the fixed orange pointer.
    gfx->fillTriangle(
        centerX,
        tapeY + 75,

        centerX - 7,
        tapeY + 86,

        centerX + 7,
        tapeY + 86,

        COLOR_ORANGE
    );
}

static void updateDegreeTape()
{
    constexpr int tapeX = 41;
    constexpr int tapeY = 106;
    constexpr int tapeWidth = 398;
    constexpr int tapeHeight = 92;

    constexpr int centerX = 240;

    constexpr float pixelsPerDegree = 4.0f;
    constexpr int visibleDegrees = 48;

    constexpr int leftEdge =
        tapeX;

    constexpr int rightEdge =
        tapeX + tapeWidth - 1;

    float heading =
        imuHeadingDegrees();

    int displayedHeading =
        (int)roundf(heading);

    if (displayedHeading == previousTapeHeading)
    {
        return;
    }

    previousTapeHeading =
        displayedHeading;

    // Clear the complete moving interior.
    gfx->fillRect(
        tapeX,
        tapeY,
        tapeWidth,
        tapeHeight,
        RGB565_BLACK
    );

    // Restore the baseline.
    gfx->drawFastHLine(
        tapeX + 11,
        tapeY + 71,
        tapeWidth - 22,
        COLOR_DARK_GREY
    );

    int firstBearing =
        displayedHeading -
        visibleDegrees;

    // Round downward to the nearest 5-degree tick.
    firstBearing -=
        wrapHeading(firstBearing) % 5;

    for (
        int bearing = firstBearing;
        bearing <= displayedHeading + visibleDegrees;
        bearing += 5
    )
    {
        int wrappedBearing =
            wrapHeading(bearing);

        float difference =
            (float)bearing -
            heading;

        int x =
            centerX -
            (int)(
                difference *
                pixelsPerDegree
            );

        // Do not draw ticks outside the tape interior.
        if (
            x < leftEdge + 2 ||
            x > rightEdge - 2
        )
        {
            continue;
        }

        bool cardinalTick =
            (wrappedBearing % 45) == 0;

        bool majorTick =
            (wrappedBearing % 15) == 0;

        uint16_t fadeColor =
            compassTapeColor(
                x,
                leftEdge,
                rightEdge
            );

        int tickHeight = 8;

        if (majorTick)
        {
            tickHeight = 15;
        }

        if (cardinalTick)
        {
            tickHeight = 22;
        }

        // Minor ticks remain visually subordinate,
        // but still fade toward the edges.
        uint16_t tickColor;

        if (majorTick || cardinalTick)
        {
            tickColor = fadeColor;
        }
        else if (fadeColor == RGB565_WHITE)
        {
            tickColor = COLOR_MED_GREY;
        }
        else
        {
            tickColor = COLOR_DARK_GREY;
        }

        gfx->drawFastVLine(
            x,
            tapeY + 72 - tickHeight,
            tickHeight,
            tickColor
        );

        if (cardinalTick)
        {
            const char* cardinal =
                cardinalForDegrees(
                    wrappedBearing
                );

            if (cardinal == nullptr)
            {
                continue;
            }

            gfx->setFont(
                (const GFXfont*)nullptr
            );

            gfx->setTextSize(2);

            int16_t boundsX;
            int16_t boundsY;
            uint16_t textWidth;
            uint16_t textHeight;

            gfx->getTextBounds(
                cardinal,
                0,
                0,
                &boundsX,
                &boundsY,
                &textWidth,
                &textHeight
            );

            int textLeft =
                x -
                textWidth / 2;

            int textRight =
                textLeft +
                textWidth -
                1;

            // Only draw when the complete glyph fits.
            if (
                textLeft < leftEdge + 2 ||
                textRight > rightEdge - 2
            )
            {
                continue;
            }

            gfx->setTextColor(fadeColor);

            gfx->setCursor(
                textLeft,
                tapeY + 25
            );

            gfx->print(cardinal);
        }
        else if (majorTick)
        {
            char degreeText[5];

            snprintf(
                degreeText,
                sizeof(degreeText),
                "%03d",
                wrappedBearing
            );

            gfx->setFont(
                (const GFXfont*)nullptr
            );

            gfx->setTextSize(1);

            int16_t boundsX;
            int16_t boundsY;
            uint16_t textWidth;
            uint16_t textHeight;

            gfx->getTextBounds(
                degreeText,
                0,
                0,
                &boundsX,
                &boundsY,
                &textWidth,
                &textHeight
            );

            int textLeft =
                x -
                textWidth / 2;

            int textRight =
                textLeft +
                textWidth -
                1;

            // Prevent any degree-number pixels from
            // escaping beyond the instrument frame.
            if (
                textLeft < leftEdge + 2 ||
                textRight > rightEdge - 2
            )
            {
                continue;
            }

            gfx->setTextColor(fadeColor);

            gfx->setCursor(
                textLeft,
                tapeY + 38
            );

            gfx->print(degreeText);
        }
    }

    // Fixed orange cursor.
    gfx->drawFastVLine(
        centerX,
        tapeY + 53,
        31,
        COLOR_ORANGE
    );

    gfx->fillTriangle(
        centerX,
        tapeY + 84,

        centerX - 7,
        tapeY + 95,

        centerX + 7,
        tapeY + 95,

        COLOR_ORANGE
    );
}

static void updateDegreeHorizon()
{
    float roll =
        -imuRollDegrees();

    float pitch =
        -imuPitchDegrees();

    // Do not redraw unless the visible attitude changed.
    if (
        fabsf(roll - previousTiltRoll) < 0.5f &&
        fabsf(pitch - previousTiltPitch) < 0.5f
    )
    {
        return;
    }

    previousTiltRoll = roll;
    previousTiltPitch = pitch;

    //==================================================
    // Horizon instrument region
    //==================================================

    constexpr int boxX = 80;
    constexpr int boxY = 210;
    constexpr int boxWidth = 320;
    constexpr int boxHeight = 56;

    constexpr int centerX =
        boxX + boxWidth / 2;

    constexpr int centerY =
        boxY + boxHeight / 2;

    // Clear only the horizon region.
    gfx->fillRect(
        boxX,
        boxY,
        boxWidth,
        boxHeight,
        RGB565_BLACK
    );

    // Subtle outer frame.
    gfx->drawRect(
        boxX,
        boxY,
        boxWidth,
        boxHeight,
        COLOR_DARK_GREY
    );

    //==================================================
    // Limit displayed movement
    //==================================================

    float displayRoll =
        constrain(
            roll,
            -18.0f,
            18.0f
        );

    float displayPitch =
        constrain(
            pitch,
            -12.0f,
            12.0f
        );

    // Positive pitch moves upward.
    // Negative pitch moves downward.
    int pitchOffset =
        (int)(
            -displayPitch *
            0.8f
        );

    //==================================================
    // Select horizon color
    //==================================================

    bool level =
        fabsf(displayPitch) < 1.0f &&
        fabsf(displayRoll) < 1.0f;

    uint16_t movingColor;

    if (level)
    {
        movingColor =
            RGB565_WHITE;
    }
    else if (displayPitch > 0.0f)
    {
        // Forward tilt
        movingColor =
            RGB565_GREEN;
    }
    else
    {
        // Back tilt
        movingColor =
            COLOR_ORANGE;
    }

    //==================================================
    // Fixed reference line
    //==================================================

    constexpr int referenceHalfLength = 82;

    gfx->drawFastHLine(
        centerX - referenceHalfLength,
        centerY,
        referenceHalfLength * 2,
        level
            ? RGB565_WHITE
            : COLOR_DARK_GREY
    );

    // Fixed center reticle.
    gfx->drawFastVLine(
        centerX,
        centerY - 4,
        9,
        COLOR_ORANGE
    );

    gfx->fillCircle(
        centerX,
        centerY,
        2,
        COLOR_ORANGE
    );

    //==================================================
    // Moving roll/pitch line
    //==================================================

    float angle =
        -displayRoll *
        PI /
        180.0f;

    constexpr int movingHalfLength = 50;

    int dx =
        (int)(
            cosf(angle) *
            movingHalfLength
        );

    int dy =
        (int)(
            sinf(angle) *
            movingHalfLength
        );

    int movingCenterY =
        centerY +
        pitchOffset;

    gfx->drawLine(
        centerX - dx,
        movingCenterY - dy,
        centerX + dx,
        movingCenterY + dy,
        movingColor
    );

    // Slightly thicken the moving line.
    gfx->drawLine(
        centerX - dx,
        movingCenterY - dy + 1,
        centerX + dx,
        movingCenterY + dy + 1,
        movingColor
    );
}

void drawCompassTick(
    int degrees,
    int innerRadius,
    int outerRadius,
    uint16_t color
)
{
    float angle =
        (degrees - 90.0f) *
        PI /
        180.0f;

    int x1 =
        COMPASS_CX +
        cos(angle) *
        innerRadius;

    int y1 =
        COMPASS_CY +
        sin(angle) *
        innerRadius;

    int x2 =
        COMPASS_CX +
        cos(angle) *
        outerRadius;

    int y2 =
        COMPASS_CY +
        sin(angle) *
        outerRadius;

    gfx->drawLine(
        x1,
        y1,
        x2,
        y2,
        color
    );
}

void drawCompassPageStatic()
{
    previousCompassHeading = -1000.0f;

    displayClear();

    gfx->drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        RGB565_WHITE
    );

    drawHeader("M A G N E T I C   B E A R I N G");

    // Outer compass ring
    gfx->drawCircle(
        COMPASS_CX,
        COMPASS_CY,
        COMPASS_OUTER_RADIUS,
        COLOR_DARK_GREY
    );

    gfx->drawCircle(
        COMPASS_CX,
        COMPASS_CY,
        COMPASS_OUTER_RADIUS - 1,
        COLOR_DARK_GREY
    );

    // Tick marks
    for (
        int degrees = 0;
        degrees < 360;
        degrees += 15
    )
    {
        bool major =
            (degrees % 45) == 0;

        drawCompassTick(
            degrees,
            major
                ? COMPASS_OUTER_RADIUS - 12
                : COMPASS_OUTER_RADIUS - 7,
            COMPASS_OUTER_RADIUS,
            major
                ? RGB565_WHITE
                : COLOR_DARK_GREY
        );
    }

    // Fixed forward pointer
    gfx->fillTriangle(
        COMPASS_CX,
        COMPASS_CY -
            COMPASS_OUTER_RADIUS +
            5,

        COMPASS_CX - 6,
        COMPASS_CY -
            COMPASS_OUTER_RADIUS +
            17,

        COMPASS_CX + 6,
        COMPASS_CY -
            COMPASS_OUTER_RADIUS +
            17,

        RGB565_WHITE
    );

    // Instrument labels
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_DARK_GREY);

    gfx->setCursor(42, 122);
    gfx->print("HEADING");

    gfx->setCursor(42, 164);
    gfx->print("CARDINAL");

    gfx->setCursor(370, 122);
    gfx->print("ROLL");

    gfx->setCursor(370, 164);
    gfx->print("PITCH");

    gfx->setCursor(196, 259);
    gfx->print("MAGNETIC NORTH");

    drawFooter();
}

void erasePreviousCompassNeedle()
{
    if (
        previousCompassHeading <
        -360.0f
    )
    {
        return;
    }

    float previousAngle =
        (-previousCompassHeading - 90.0f) *
        PI /
        180.0f;

    int previousX =
        COMPASS_CX +
        cos(previousAngle) *
        COMPASS_NEEDLE_LENGTH;

    int previousY =
        COMPASS_CY +
        sin(previousAngle) *
        COMPASS_NEEDLE_LENGTH;

    gfx->drawLine(
        COMPASS_CX,
        COMPASS_CY,
        previousX,
        previousY,
        RGB565_BLACK
    );

    // Slightly thicker erase
    gfx->drawLine(
        COMPASS_CX + 1,
        COMPASS_CY,
        previousX + 1,
        previousY,
        RGB565_BLACK
    );
}

void drawCompassNeedle(float heading)
{
    float angle =
        (-heading - 90.0f) *
        PI /
        180.0f;

    int needleX =
        COMPASS_CX +
        cos(angle) *
        COMPASS_NEEDLE_LENGTH;

    int needleY =
        COMPASS_CY +
        sin(angle) *
        COMPASS_NEEDLE_LENGTH;

    gfx->drawLine(
        COMPASS_CX,
        COMPASS_CY,
        needleX,
        needleY,
        RGB565_WHITE
    );

    gfx->drawLine(
        COMPASS_CX + 1,
        COMPASS_CY,
        needleX + 1,
        needleY,
        RGB565_WHITE
    );

    gfx->fillCircle(
        COMPASS_CX,
        COMPASS_CY,
        4,
        RGB565_WHITE
    );
}

void updateTiltIndicator()
{
    float roll = imuRollDegrees();
    float pitch = imuPitchDegrees();

    // Avoid repainting if nothing visibly changed.
    if (
        fabsf(roll - previousTiltRoll) < 0.5f &&
        fabsf(pitch - previousTiltPitch) < 0.5f
    )
    {
        return;
    }

    previousTiltRoll = roll;
    previousTiltPitch = pitch;

    constexpr int boxX = 350;
    constexpr int boxY = 211;
    constexpr int boxWidth = 105;
    constexpr int boxHeight = 49;

    constexpr int centerX =
        boxX + boxWidth / 2;

    constexpr int centerY =
        boxY + boxHeight / 2;

    // Clear only the small attitude-indicator region.
    gfx->fillRect(
        boxX,
        boxY,
        boxWidth,
        boxHeight,
        RGB565_BLACK
    );

    gfx->drawRect(
        boxX,
        boxY,
        boxWidth,
        boxHeight,
        COLOR_DARK_GREY
    );

    // Limit the display range so the horizon line remains
    // inside the small instrument window.
    float displayRoll =
        constrain(roll, -45.0f, 45.0f);

    float displayPitch =
        constrain(pitch, -20.0f, 20.0f);

    float angle =
        -displayRoll *
        PI /
        180.0f;

    int pitchOffset =
        (int)(displayPitch * 0.45f);

    int horizonCenterY =
        centerY + pitchOffset;

    constexpr int halfLength = 25;

    int dx =
        (int)(
            cosf(angle) *
            halfLength
        );

    int dy =
        (int)(
            sinf(angle) *
            halfLength
        );

    // Moving horizon line.
    gfx->drawLine(
        centerX - dx,
        horizonCenterY - dy,
        centerX + dx,
        horizonCenterY + dy,
        RGB565_WHITE
    );

    // Fixed aircraft/device reticle.
    gfx->drawFastHLine(
        centerX - 9,
        centerY,
        19,
        COLOR_ORANGE
    );

    gfx->drawFastVLine(
        centerX,
        centerY - 4,
        9,
        COLOR_ORANGE
    );

    gfx->fillCircle(
        centerX,
        centerY,
        2,
        COLOR_ORANGE
    );
}

void updateCompassPage()
{
    if (
        compassDisplayMode ==
        COMPASS_MODE_CARDINAL_TAPE
    )
    {
        updateCardinalTape();
        return;
    }

    if (
        compassDisplayMode ==
        COMPASS_MODE_DEGREE_TAPE
    )
    {
        updateDegreeTape();
        updateDegreeHorizon();
        return;
    }
    
    if (!imuPresent())
    {
        gfx->setTextColor(
            RGB565_RED,
            RGB565_BLACK
        );

        gfx->setTextSize(2);
        gfx->setCursor(170, 160);
        gfx->print("IMU NOT FOUND");

        return;
    }

    float heading =
        imuHeadingDegrees();

    float roll =
        imuRollDegrees();

    float pitch =
        imuPitchDegrees();

    const char* cardinal =
        imuCardinalDirection();

    // Erase only previous needle.
    erasePreviousCompassNeedle();

    drawCompassNeedle(heading);

    previousCompassHeading = heading;

    char headingText[8];

    snprintf(
        headingText,
        sizeof(headingText),
        "%03d",
        ((int)(heading + 0.5f)) % 360
    );

    char cardinalText[4];

    snprintf(
        cardinalText,
        sizeof(cardinalText),
        "%-2s",
        cardinal
    );

    char rollText[10];

    snprintf(
        rollText,
        sizeof(rollText),
        "%+04d",
        (int)roll
    );

    char pitchText[10];

    snprintf(
        pitchText,
        sizeof(pitchText),
        "%+04d",
        (int)pitch
    );

    gfx->setFont((const GFXfont *)nullptr);

    // Heading value
    gfx->setTextColor(
        RGB565_WHITE,
        RGB565_BLACK
    );

    gfx->setTextSize(3);
    gfx->setCursor(42, 136);
    gfx->print(headingText);

    // Degree mark approximation
    gfx->setTextSize(1);
    gfx->setCursor(98, 136);
    gfx->print("o");

    // Cardinal direction
    gfx->setTextSize(3);
    gfx->setCursor(42, 178);
    gfx->print(cardinalText);

    // Roll
    gfx->setTextSize(2);
    gfx->setCursor(370, 136);
    gfx->print(rollText);

    // Pitch
    gfx->setCursor(370, 178);
    gfx->print(pitchText);
}

void drawCompassPage()
{
    if (compassDisplayMode == COMPASS_MODE_DIAL)
    {
        drawCompassPageStatic();
        updateCompassPage();
        return;
    }

    displayClear();

    gfx->drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        RGB565_WHITE
    );

    drawHeader("M A G N E T I C   B E A R I N G");

    gfx->setFont((const GFXfont *)nullptr);

    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(3);

    if (
        compassDisplayMode ==
        COMPASS_MODE_CARDINAL_TAPE
    )
    { 
        drawCardinalTapeStatic();
        previousTapeHeading = -1000;
        updateCardinalTape();
    }
    if (
        compassDisplayMode ==
        COMPASS_MODE_DEGREE_TAPE
    )
    {
        drawDegreeTapeStatic();

        previousTapeHeading = -1000;
        previousTiltRoll = -1000.0f;
        previousTiltPitch = -1000.0f;
        
        updateDegreeTape();
        updateDegreeHorizon();
    }
    
    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setTextSize(1);
    gfx->setCursor(180, 235);
    gfx->print("SELECT: CHANGE MODE");

    drawFooter();
}

//======================================================
// IMU Calibration Page
//======================================================

void drawCalibrationPageStatic()
{
    previousCalibrationProgress = -1;
    previousCalibrationState = -1;

    displayClear();

    gfx->drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        RGB565_WHITE
    );

    drawHeader("I M U   C A L I B R A T I O N");

    gfx->setFont((const GFXfont *)nullptr);

    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(116, 101);
    gfx->print("ROTATE DEVICE THROUGH");

    gfx->setCursor(143, 126);
    gfx->print("ALL ORIENTATIONS");

    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setTextSize(1);
    gfx->setCursor(139, 154);
    gfx->print("TURN FLAT - ROLL - PITCH - INVERT");

    // Progress-bar outline
    gfx->drawRect(
        89,
        184,
        302,
        22,
        COLOR_DARK_GREY
    );

    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setCursor(173, 220);
    gfx->print("SELECT: START / FINISH");

    drawFooter();
}

void updateCalibrationPage()
{
    gfx->setFont((const GFXfont *)nullptr);

    uint8_t progress =
        imuCalibrationProgress();

    int calibrationState;

    if (imuCalibrationActive())
    {
        calibrationState = 1;
    }
    else if (imuCalibrationValid())
    {
        calibrationState = 2;
    }
    else
    {
        calibrationState = 0;
    }

    if (
        progress == previousCalibrationProgress &&
        calibrationState == previousCalibrationState
    )
    {
        return;
    }

    previousCalibrationProgress = progress;
    previousCalibrationState = calibrationState;

    // Clear inside of progress bar.
    gfx->fillRect(
        92,
        187,
        296,
        16,
        RGB565_BLACK
    );

    int fillWidth =
        map(
            progress,
            0,
            100,
            0,
            296
        );

    if (fillWidth > 0)
    {
        gfx->fillRect(
            92,
            187,
            fillWidth,
            16,
            RGB565_WHITE
        );
    }

    // Clear changing status area.
    gfx->fillRect(
        100,
        242,
        280,
        18,
        RGB565_BLACK
    );

    gfx->setTextSize(1);

    if (imuCalibrationActive())
    {
        gfx->setTextColor(
            RGB565_YELLOW,
            RGB565_BLACK
        );

        gfx->setCursor(176, 246);
        gfx->print("CALIBRATING ");

        if (progress < 100)
        {
            gfx->print(progress);
            gfx->print("%");
        }
        else
        {
            gfx->print("READY");
        }
    }
    else if (imuCalibrationValid())
    {
        gfx->setTextColor(
            RGB565_GREEN,
            RGB565_BLACK
        );

        gfx->setCursor(187, 246);
        gfx->print("CALIBRATION VALID");
    }
    else
    {
        gfx->setTextColor(
            COLOR_DARK_GREY,
            RGB565_BLACK
        );

        gfx->setCursor(157, 246);
        gfx->print("PRESS SELECT TO BEGIN");
    }
}

void drawCalibrationPage()
{
    drawCalibrationPageStatic();
    updateCalibrationPage();
}

//======================================================
// Atmospheric Conditions
//======================================================
void drawAtmosphericPageStatic()
{
    previousAtmosphericTemp[0] = '\0';
    previousAtmosphericHumidity[0] = '\0';
    previousAtmosphericDewPoint[0] = '\0';

    displayClear();

    gfx->drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        RGB565_WHITE
    );

    drawHeader(
        "A T M O S P H E R I C   C O N D I T I O N S"
    );

    gfx->drawFastHLine(
        22,
        94,
        SCREEN_WIDTH - 44,
        COLOR_DARK_GREY
    );

    gfx->setFont((const GFXfont*)nullptr);

    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setTextSize(1);
    gfx->setCursor(42, 228);
    gfx->print("SAMPLER");

    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(390, 228);
    gfx->print("IDLE");

    gfx->setTextColor(COLOR_DARK_GREY);
    gfx->setCursor(159, 255);
    gfx->print("SELECT: COLLECT AIR SAMPLE");

    drawFooter();
}

void updateAtmosphericPage()
{
    char temperatureText[24];
    char humidityText[16];
    char dewPointText[24];

    if (
        Environment::isPresent() &&
        Environment::hasValidReading()
    )
    {
        snprintf(
            temperatureText,
            sizeof(temperatureText),
            "%.0fF | %.0fC",
            Environment::temperatureF(),
            Environment::temperatureC()
        );

        snprintf(
            humidityText,
            sizeof(humidityText),
            "%.0f%%",
            Environment::humidityRH()
        );

        snprintf(
            dewPointText,
            sizeof(dewPointText),
            "%.0fF | %.0fC",
            Environment::dewPointF(),
            Environment::dewPointC()
        );
    }
    else
    {
        snprintf(
            temperatureText,
            sizeof(temperatureText),
            "--F | --C"
        );

        snprintf(
            humidityText,
            sizeof(humidityText),
            "--%%"
        );

        snprintf(
            dewPointText,
            sizeof(dewPointText),
            "--F | --C"
        );
    }

    if (
        strcmp(
            temperatureText,
            previousAtmosphericTemp
        ) != 0
    )
    {
        gfx->fillRect(
            330,
            106,
            130,
            18,
            RGB565_BLACK
        );

        drawAtmosphericRow(
            120,
            "TEMP",
            temperatureText,
            RGB565_WHITE
        );

        strncpy(
            previousAtmosphericTemp,
            temperatureText,
            sizeof(previousAtmosphericTemp) - 1
        );

        previousAtmosphericTemp[
            sizeof(previousAtmosphericTemp) - 1
        ] = '\0';
    }

    if (
        strcmp(
            humidityText,
            previousAtmosphericHumidity
        ) != 0
    )
    {
        gfx->fillRect(
            330,
            141,
            130,
            18,
            RGB565_BLACK
        );

        drawAtmosphericRow(
            155,
            "HUMIDITY",
            humidityText,
            RGB565_WHITE
        );

        strncpy(
            previousAtmosphericHumidity,
            humidityText,
            sizeof(previousAtmosphericHumidity) - 1
        );

        previousAtmosphericHumidity[
            sizeof(previousAtmosphericHumidity) - 1
        ] = '\0';
    }

    if (
        strcmp(
            dewPointText,
            previousAtmosphericDewPoint
        ) != 0
    )
    {
        gfx->fillRect(
            330,
            176,
            130,
            18,
            RGB565_BLACK
        );

        drawAtmosphericRow(
            190,
            "DEW POINT",
            dewPointText,
            COLOR_MED_GREY
        );

        strncpy(
            previousAtmosphericDewPoint,
            dewPointText,
            sizeof(previousAtmosphericDewPoint) - 1
        );

        previousAtmosphericDewPoint[
            sizeof(previousAtmosphericDewPoint) - 1
        ] = '\0';
    }
}

void drawAtmosphericPage()
{
    drawAtmosphericPageStatic();
    updateAtmosphericPage();
}

//======================================================
// System Diagnostics
//======================================================

void drawSystemPage()
{
    displayClear();

    gfx->drawRect(
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        RGB565_WHITE
    );

    drawHeader("SYSTEM DIAGNOSTICS");

    gfx->drawFastHLine(
        22,
        99,
        SCREEN_WIDTH - 44,
        COLOR_DARK_GREY
    );

    drawStatusRow(
        120,
        "DISPLAY",
        HAS_DISPLAY ? "OK" : "ERR",
        HAS_DISPLAY
            ? RGB565_GREEN
            : RGB565_RED
    );

    drawStatusRow(
        150,
        "TOUCH",
        HAS_TOUCH ? "OK" : "WAIT",
        HAS_TOUCH
            ? RGB565_GREEN
            : RGB565_YELLOW
    );

    drawStatusRow(
        180,
        "RTC",
        rtcPresent() ? "OK" : "WAIT",
        rtcPresent()
            ? RGB565_GREEN
            : RGB565_YELLOW
    );

    drawStatusRow(
        210,
        "IMU",
        imuPresent() ? "OK" : "WAIT",
        imuPresent()
            ? RGB565_GREEN
            : RGB565_YELLOW
    );

    drawStatusRow(
        240,
        "ENV",
        Environment::isPresent() ? "OK" : "WAIT",
        Environment::isPresent()
            ? RGB565_GREEN
            : RGB565_YELLOW
    );

    drawFooter();
}

//======================================================
// Public ScreenManager Functions
//======================================================

void runInstrumentStartupAnimation()
{
    // Rebuilt later.
}

void drawInstrumentPanel()
{
    drawClockPage();
}

void drawCurrentPage()
{
    switch (currentPage)
    {
        case PAGE_CLOCK:
            drawClockPage();
            break;

        case PAGE_COMPASS:
            drawCompassPage();
            break;
            
        case PAGE_ATMOSPHERIC:
            drawAtmosphericPage();
            break;

        case PAGE_FLASHLIGHT:
            displayClear();

            gfx->drawRect(
                0,
                0,
                SCREEN_WIDTH,
                SCREEN_HEIGHT,
                RGB565_WHITE
            );

            drawHeader(
                "F L A S H L I G H T"
            );

            flashlightPageDraw();

            drawFooter();
            break;

        case PAGE_CALIBRATION:
            drawCalibrationPage();
            break;

        case PAGE_SYSTEM:
            drawSystemPage();
            break;

        default:
            drawClockPage();
            break;
    }
}

void updateCurrentPage()
{
    switch (currentPage)
    {
        case PAGE_CLOCK:
            updateClockValue();
            updateClockEnvironmentWidget();
            break;

        case PAGE_COMPASS:
            updateCompassPage();
            break;

        case PAGE_ATMOSPHERIC:
            updateAtmosphericPage();
            break;

        case PAGE_FLASHLIGHT:
            flashlightPageUpdate();
            break;

        case PAGE_CALIBRATION:
            updateCalibrationPage();
            break;

        case PAGE_SYSTEM:
        default:
            break;
    }
}

void nextPage()
{
    currentPage =
        static_cast<Page>(
            (currentPage + 1) %
            PAGE_COUNT
        );
}

void previousPage()
{
    currentPage =
        static_cast<Page>(
            (
                currentPage +
                PAGE_COUNT -
                1
            ) %
            PAGE_COUNT
        );
}

void selectCurrentPage()
{
    if (currentPage == PAGE_FLASHLIGHT)
    {
        flashlightPageSelect();
        drawCurrentPage();
        return;
    }

    if (currentPage == PAGE_COMPASS)
    {
        compassDisplayMode =
            static_cast<CompassDisplayMode>(
                (compassDisplayMode + 1)
                % COMPASS_MODE_COUNT
            );

        drawCurrentPage();

        return;
    }

    if (currentPage != PAGE_CALIBRATION)
    {
        return;
    }

    if (imuCalibrationActive())
    {
        imuFinishCalibration();

        Serial.println(
            imuCalibrationValid()
                ? "IMU calibration completed."
                : "IMU calibration failed: insufficient movement."
        );
    }
    else
    {
        imuStartCalibration();

        Serial.println("IMU calibration started.");
    }

    drawCurrentPage();
}
