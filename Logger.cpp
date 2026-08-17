#include "Logger.h"

#include "Globals.h"
#include "Time.h"

#include <SD.h>

//======================================================
// Internal state
//======================================================

static bool sdReady = false;

static const char* LOG_FILE = "/STRGZR_LOG.txt";

//======================================================
// Public functions
//======================================================

bool loggerInit()
{
#if HAS_SD
    sdReady = SD.begin(BUILTIN_SDCARD);

    if (!sdReady)
    {
        Serial.println("SD logger initialization failed.");
        return false;
    }

    Serial.println("SD logger initialized.");

    loggerLog("BOOT");

    return true;
#else
    sdReady = false;
    return false;
#endif
}

void loggerLog(const String& message)
{
#if HAS_SD
    if (!sdReady)
    {
        return;
    }

    File file = SD.open(LOG_FILE, FILE_WRITE);

    if (!file)
    {
        Serial.println("Logger failed to open log file.");
        return;
    }

    char timestamp[24];

    if (timeGetTimestamp(timestamp, sizeof(timestamp)))
    {
        file.print(timestamp);
    }
    else
    {
        file.print("NO_RTC");
    }

    file.print(",");
    file.println(message);

    file.close();
#endif
}

bool loggerReady()
{
    return sdReady;
}

void loggerDumpToSerial()
{
#if HAS_SD
    if (!sdReady)
    {
        Serial.println("Logger unavailable.");
        return;
    }

    File file = SD.open(LOG_FILE, FILE_READ);

    if (!file)
    {
        Serial.println("Could not open log file.");
        return;
    }

    Serial.println();
    Serial.println("==============================");
    Serial.println("STR-GZR LOG");
    Serial.println("==============================");

    while (file.available())
    {
        Serial.write(file.read());
    }

    file.close();

    Serial.println("==============================");
    Serial.println("END LOG");
    Serial.println("==============================");
    Serial.println();
#endif
}
