#include "Display.h"
#include "Globals.h"
#include "Input.h"
#include "ScreenManager.h"
#include "Time.h"
#include "IMU.h"
#include "Environment.h"
#include "Flashlight.h"
#include "FMRadio.h"
#include "FMPage.h"
#include "Logger.h"
#include "Haptics.h"

#include <Arduino.h>

constexpr uint32_t SCREEN_REFRESH_MS = 100;

uint32_t lastScreenRefresh = 0;

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("STR-GZR OS starting...");

    if (!displayInit())
    {
        Serial.println("ST7796S initialization failed.");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println("ST7796S initialized.");

    timeInit();
    loggerInit();
    
    imuInit();
    Environment::begin();
    inputInit();
    hapticsInit();
    flashlightInit();
    fmPageInit();
    fmRadioInit();

    #if HAS_TOUCH
        Serial.println("DEBUG: HAS_TOUCH is enabled.");
    #else
        Serial.println("DEBUG: HAS_TOUCH is disabled or undefined.");
    #endif

    Serial.println("Subsystems initialized.");

    drawCurrentPage();

    lastScreenRefresh = millis();
    
}

void loop()
{
    inputUpdate();
    Environment::update();
    hapticsUpdate();

    //--------------------------------------------------
    // Navigation
    //--------------------------------------------------

    if (inputLeftPressed())
    {
        previousPage();
        drawCurrentPage();
    }

    if (inputRightPressed())
    {
        nextPage();
        drawCurrentPage();
    }

    if (inputSelectPressed())
    {
        selectCurrentPage();
        drawCurrentPage();
    }

    //--------------------------------------------------
    // Dynamic updates
    //--------------------------------------------------

    uint32_t now = millis();

    static uint32_t lastEnvironmentDebug = 0;

    if (now - lastEnvironmentDebug >= 1000)
    {
        lastEnvironmentDebug = now;

        if (Environment::hasValidReading())
        {
            Serial.print("TEMP:");
            Serial.print(Environment::temperatureF(), 1);

            Serial.print(" F  RH:");
            Serial.print(Environment::humidityRH(), 1);

            Serial.print("%  DEW:");
            Serial.print(Environment::dewPointF(), 1);

            Serial.println(" F");
        }
        else
        {
            Serial.println("SHT40 waiting for valid reading...");
        }
    }

    if (now - lastScreenRefresh >= SCREEN_REFRESH_MS)
    {
        lastScreenRefresh = now;

        imuUpdate();
        updateCurrentPage();

        static uint32_t lastImuDebug = 0;

        if (now - lastImuDebug >= 500)
        {
            lastImuDebug = now;

            Serial.print("HEAD:");
            Serial.print(imuHeadingDegrees(), 1);

            Serial.print("  ROLL:");
            Serial.print(imuRollDegrees(), 1);

            Serial.print("  PITCH:");
            Serial.print(imuPitchDegrees(), 1);

            Serial.print("  | MAG X:");
            Serial.print(imuMagX());

            Serial.print(" Y:");
            Serial.print(imuMagY());

            Serial.print(" Z:");
            Serial.print(imuMagZ());

            Serial.print("  | ACC X:");
            Serial.print(imuAccelX());

            Serial.print(" Y:");
            Serial.print(imuAccelY());

            Serial.print(" Z:");
            Serial.println(imuAccelZ());
        }
    }
}
