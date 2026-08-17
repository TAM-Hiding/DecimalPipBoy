#include "Time.h"
#include "Globals.h"

#include <Arduino.h>
#include <Wire.h>

//======================================================
// DS3231 registers
//======================================================

constexpr uint8_t DS3231_ADDRESS = 0x68;

constexpr uint8_t REG_SECONDS = 0x00;
constexpr uint8_t REG_STATUS  = 0x0F;

constexpr uint8_t STATUS_OSF = 0x80;

//======================================================
// Time state
//======================================================

static bool rtcDetected = false;

static uint64_t baseMillisecondsToday = 0;
static uint32_t baseMillis = 0;

//======================================================
// BCD conversion
//======================================================

static uint8_t bcdToDecimal(uint8_t value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

static uint8_t decimalToBcd(uint8_t value)
{
    return ((value / 10) << 4) | (value % 10);
}

//======================================================
// Compile-time parsing
//======================================================

static uint8_t compileHour()
{
    return ((__TIME__[0] - '0') * 10)
         +  (__TIME__[1] - '0');
}

static uint8_t compileMinute()
{
    return ((__TIME__[3] - '0') * 10)
         +  (__TIME__[4] - '0');
}

static uint8_t compileSecond()
{
    return ((__TIME__[6] - '0') * 10)
         +  (__TIME__[7] - '0');
}

static uint8_t compileMonth()
{
    const char* months =
        "JanFebMarAprMayJunJulAugSepOctNovDec";

    for (uint8_t month = 0; month < 12; month++)
    {
        if (
            __DATE__[0] == months[month * 3] &&
            __DATE__[1] == months[month * 3 + 1] &&
            __DATE__[2] == months[month * 3 + 2]
        )
        {
            return month + 1;
        }
    }

    return 1;
}

static uint8_t compileDay()
{
    uint8_t tens =
        (__DATE__[4] == ' ')
        ? 0
        : (__DATE__[4] - '0');

    uint8_t ones = __DATE__[5] - '0';

    return (tens * 10) + ones;
}

static uint16_t compileYear()
{
    return
        ((__DATE__[7]  - '0') * 1000) +
        ((__DATE__[8]  - '0') * 100)  +
        ((__DATE__[9]  - '0') * 10)   +
        (__DATE__[10] - '0');
}

//======================================================
// Low-level DS3231 functions
//======================================================

static bool rtcResponding()
{
    Wire.beginTransmission(DS3231_ADDRESS);

    return Wire.endTransmission() == 0;
}

static bool readRegister(
    uint8_t registerAddress,
    uint8_t& value
)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(registerAddress);

    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    if (Wire.requestFrom(DS3231_ADDRESS, (uint8_t)1) != 1)
    {
        return false;
    }

    value = Wire.read();

    return true;
}

static bool writeRegister(
    uint8_t registerAddress,
    uint8_t value
)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(registerAddress);
    Wire.write(value);

    return Wire.endTransmission() == 0;
}

static bool rtcOscillatorStopped()
{
    uint8_t status = 0;

    if (!readRegister(REG_STATUS, status))
    {
        return true;
    }

    return (status & STATUS_OSF) != 0;
}

static void clearOscillatorStopFlag()
{
    uint8_t status = 0;

    if (!readRegister(REG_STATUS, status))
    {
        return;
    }

    status &= ~STATUS_OSF;

    writeRegister(REG_STATUS, status);
}

static bool readRtcTime(
    uint8_t& hour,
    uint8_t& minute,
    uint8_t& second
)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(REG_SECONDS);

    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    if (Wire.requestFrom(DS3231_ADDRESS, (uint8_t)3) != 3)
    {
        return false;
    }

    uint8_t rawSecond = Wire.read();
    uint8_t rawMinute = Wire.read();
    uint8_t rawHour   = Wire.read();

    second = bcdToDecimal(rawSecond & 0x7F);
    minute = bcdToDecimal(rawMinute & 0x7F);

    // Handle either 24-hour or 12-hour mode.
    if (rawHour & 0x40)
    {
        bool isPm = rawHour & 0x20;

        hour = bcdToDecimal(rawHour & 0x1F);

        if (hour == 12)
        {
            hour = 0;
        }

        if (isPm)
        {
            hour += 12;
        }
    }
    else
    {
        hour = bcdToDecimal(rawHour & 0x3F);
    }

    if (
        hour > 23 ||
        minute > 59 ||
        second > 59
    )
    {
        return false;
    }

    return true;
}

static bool readRtcDateTime(
    uint16_t& year,
    uint8_t& month,
    uint8_t& day,
    uint8_t& hour,
    uint8_t& minute,
    uint8_t& second
)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(REG_SECONDS);

    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    // Read:
    // seconds, minutes, hours, day-of-week,
    // date, month, year
    if (Wire.requestFrom(DS3231_ADDRESS, (uint8_t)7) != 7)
    {
        return false;
    }

    uint8_t rawSecond = Wire.read();
    uint8_t rawMinute = Wire.read();
    uint8_t rawHour   = Wire.read();

    // Day-of-week is currently unused.
    Wire.read();

    uint8_t rawDay   = Wire.read();
    uint8_t rawMonth = Wire.read();
    uint8_t rawYear  = Wire.read();

    second = bcdToDecimal(rawSecond & 0x7F);
    minute = bcdToDecimal(rawMinute & 0x7F);

    // Handle either 24-hour or 12-hour mode.
    if (rawHour & 0x40)
    {
        bool isPm = rawHour & 0x20;

        hour = bcdToDecimal(rawHour & 0x1F);

        if (hour == 12)
        {
            hour = 0;
        }

        if (isPm)
        {
            hour += 12;
        }
    }
    else
    {
        hour = bcdToDecimal(rawHour & 0x3F);
    }

    day = bcdToDecimal(rawDay & 0x3F);

    // Bit 7 is the DS3231 century bit.
    bool century = rawMonth & 0x80;

    month = bcdToDecimal(rawMonth & 0x1F);

    uint8_t yearTwoDigits =
        bcdToDecimal(rawYear);

    year =
        2000 +
        yearTwoDigits +
        (century ? 100 : 0);

    if (
        hour > 23 ||
        minute > 59 ||
        second > 59 ||
        month < 1 ||
        month > 12 ||
        day < 1 ||
        day > 31
    )
    {
        return false;
    }

    return true;
}

static bool writeRtcFromCompileTime()
{
    uint8_t hour   = compileHour();
    uint8_t minute = compileMinute();
    uint8_t second = compileSecond();

    uint8_t day   = compileDay();
    uint8_t month = compileMonth();

    uint16_t fullYear = compileYear();
    uint8_t year = fullYear % 100;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(REG_SECONDS);

    Wire.write(decimalToBcd(second));
    Wire.write(decimalToBcd(minute));
    Wire.write(decimalToBcd(hour));

    // Day of week is not currently used.
    Wire.write(decimalToBcd(1));

    Wire.write(decimalToBcd(day));
    Wire.write(decimalToBcd(month));
    Wire.write(decimalToBcd(year));

    if (Wire.endTransmission() != 0)
    {
        return false;
    }

    clearOscillatorStopFlag();

    return true;
}

//======================================================
// Software clock
//======================================================

static void setSoftwareTime(
    uint8_t hour,
    uint8_t minute,
    uint8_t second
)
{
    uint32_t secondsToday =
        ((uint32_t)hour * 3600UL) +
        ((uint32_t)minute * 60UL) +
        second;

    baseMillisecondsToday =
        (uint64_t)secondsToday * 1000ULL;

    baseMillis = millis();
}

//======================================================
// Public functions
//======================================================

void setTimeFromCompileTime()
{
    setSoftwareTime(
        compileHour(),
        compileMinute(),
        compileSecond()
    );

    if (rtcDetected)
    {
        writeRtcFromCompileTime();
    }
}

void timeInit()
{
    Wire.begin();
    Wire.setClock(I2C_CLOCK);

    rtcDetected = rtcResponding();

    if (!rtcDetected)
    {
        Serial.println("RTC not detected; using compile time.");

        setTimeFromCompileTime();
        return;
    }

    Serial.println("DS3231 detected.");

    bool rtcNeedsSetting =
        rtcOscillatorStopped();

    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;

    bool validTime =
        readRtcTime(hour, minute, second);

    if (rtcNeedsSetting || !validTime)
    {
        Serial.println(
            "RTC time invalid or oscillator stopped."
        );

        Serial.println(
            "Setting RTC from compile time."
        );

        writeRtcFromCompileTime();

        hour   = compileHour();
        minute = compileMinute();
        second = compileSecond();
    }
    else
    {
        Serial.println("RTC time loaded.");
    }

    setSoftwareTime(
        hour,
        minute,
        second
    );

    Serial.print("RTC time: ");

    if (hour < 10)
    {
        Serial.print("0");
    }

    Serial.print(hour);
    Serial.print(":");

    if (minute < 10)
    {
        Serial.print("0");
    }

    Serial.print(minute);
    Serial.print(":");

    if (second < 10)
    {
        Serial.print("0");
    }

    Serial.println(second);
}

bool rtcPresent()
{
    return rtcDetected;
}

uint32_t getSecondsToday()
{
    uint64_t elapsedMilliseconds =
        (uint32_t)(millis() - baseMillis);

    uint64_t millisecondsToday =
        (
            baseMillisecondsToday +
            elapsedMilliseconds
        ) % 86400000ULL;

    return millisecondsToday / 1000ULL;
}

uint32_t getDecimalTime()
{
    uint64_t elapsedMilliseconds =
        (uint32_t)(millis() - baseMillis);

    uint64_t millisecondsToday =
        (
            baseMillisecondsToday +
            elapsedMilliseconds
        ) % 86400000ULL;

    return (uint32_t)(
        (millisecondsToday * 1000000ULL)
        / 86400000ULL
    );
}

bool timeGetTimestamp(
    char* buffer,
    size_t bufferSize
)
{
    if (!rtcDetected || buffer == nullptr || bufferSize < 20)
    {
        return false;
    }

    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;

    if (
        !readRtcDateTime(
            year,
            month,
            day,
            hour,
            minute,
            second
        )
    )
    {
        return false;
    }

    snprintf(
        buffer,
        bufferSize,
        "%04u-%02u-%02u %02u:%02u:%02u",
        year,
        month,
        day,
        hour,
        minute,
        second
    );

    return true;
}
