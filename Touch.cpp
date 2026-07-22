#include "Touch.h"
#include "Globals.h"

#include <Wire.h>

//======================================================
// GT911 configuration
//======================================================

static constexpr uint8_t GT911_ADDRESS = 0x5D;

// Product ID: four ASCII bytes, commonly "911\0"
static constexpr uint16_t GT911_REG_PRODUCT_ID = 0x8140;

// Touch status:
// Bit 7 = new coordinate data ready
// Bits 3:0 = number of touch points
static constexpr uint16_t GT911_REG_STATUS = 0x814E;

// First touch-point record:
// byte 0    = track ID
// bytes 1-2 = X, little endian
// bytes 3-4 = Y, little endian
// bytes 5-6 = touch size, little endian
// byte 7    = reserved
static constexpr uint16_t GT911_REG_POINT_1 = 0x814F;

//======================================================
// Internal state
//======================================================

static bool controllerAvailable = false;

static bool currentlyHeld = false;
static bool pressedEvent = false;
static bool releasedEvent = false;

static uint16_t currentX = 0;
static uint16_t currentY = 0;

static uint8_t currentTouchID = 0;
static uint8_t currentPointCount = 0;

//======================================================
// Low-level I2C functions
//======================================================

static bool gt911WriteRegister(
    uint16_t registerAddress,
    const uint8_t* data,
    size_t length
)
{
    Wire.beginTransmission(GT911_ADDRESS);

    Wire.write(static_cast<uint8_t>(registerAddress >> 8));
    Wire.write(static_cast<uint8_t>(registerAddress & 0xFF));

    for (size_t i = 0; i < length; i++)
    {
        Wire.write(data[i]);
    }

    return Wire.endTransmission(true) == 0;
}

static bool gt911ReadRegister(
    uint16_t registerAddress,
    uint8_t* data,
    size_t length
)
{
    Wire.beginTransmission(GT911_ADDRESS);

    Wire.write(static_cast<uint8_t>(registerAddress >> 8));
    Wire.write(static_cast<uint8_t>(registerAddress & 0xFF));

    const uint8_t transmissionResult = Wire.endTransmission(false);

    if (transmissionResult != 0)
    {
        Serial.print("GT911 I2C error: ");
        Serial.println(transmissionResult);
        return false;
    }

    delayMicroseconds(200);

    const uint8_t received = Wire.requestFrom(
        static_cast<uint8_t>(GT911_ADDRESS),
        static_cast<uint8_t>(length),
        static_cast<uint8_t>(true)
    );

    if (received != length)
    {
        while (Wire.available())
        {
            Wire.read();
        }

        return false;
    }

    for (size_t i = 0; i < length; i++)
    {
        if (!Wire.available())
        {
            return false;
        }

        data[i] = static_cast<uint8_t>(Wire.read());
    }

    return true;
}

static bool gt911ClearStatus()
{
    const uint8_t clearValue = 0x00;

    return gt911WriteRegister(
        GT911_REG_STATUS,
        &clearValue,
        1
    );
}

//======================================================
// Public functions
//======================================================

bool touchInit()
{
    controllerAvailable = false;

    currentlyHeld = false;
    pressedEvent = false;
    releasedEvent = false;

    currentX = 0;
    currentY = 0;
    currentTouchID = 0;
    currentPointCount = 0;

    Serial.println("GT911: touchInit reached.");

    // Reset GT911 and deliberately select I2C address 0x5D.
    // INT low during reset selects 0x5D.
    // INT high during reset selects 0x14.
    pinMode(TOUCH_RST, OUTPUT);
    pinMode(TOUCH_INT, OUTPUT);

    digitalWrite(TOUCH_RST, LOW);
    digitalWrite(TOUCH_INT, LOW);

    delay(20);

    digitalWrite(TOUCH_RST, HIGH);

    delay(10);

    // Release the interrupt line so the GT911 can use it normally.
    pinMode(TOUCH_INT, INPUT);

    delay(100);

    Wire.setClock(100000);

    Serial.println("GT911: attempting product ID read at 0x5D.");
     
    uint8_t productID[4] = {0};

    if (!gt911ReadRegister(
        GT911_REG_PRODUCT_ID,
        productID,
        sizeof(productID)
    ))
    {
        Serial.println("GT911: controller not responding at 0x5D.");
        return false;
    }

    controllerAvailable = true;

    Serial.print("GT911 Product ID: ");

    for (uint8_t i = 0; i < 4; i++)
    {
        if (productID[i] < 0x10)
        {
            Serial.print("0");
        }

        Serial.print(productID[i], HEX);
        Serial.print(" ");
    }

    Serial.println();

    gt911ClearStatus();

    Serial.println("GT911: initialized at address 0x5D.");

    return true;
}

void touchUpdate()
{
    pressedEvent = false;
    releasedEvent = false;

    if (!controllerAvailable)
    {
        return;
    }

    uint8_t status = 0;

    if (!gt911ReadRegister(
        GT911_REG_STATUS,
        &status,
        1
    ))
    {
        return;
    }

    // Bit 7 means fresh coordinate data is ready.
    const bool dataReady = (status & 0x80) != 0;

    if (!dataReady)
    {
        return;
    }

    currentPointCount = status & 0x0F;

    if (currentPointCount == 0)
    {
        // A fresh report containing zero points means release.
        if (currentlyHeld)
        {
            currentlyHeld = false;
            releasedEvent = true;
        }

        gt911ClearStatus();
        return;
    }

    // We only need the first point for now.
    uint8_t pointData[8] = {0};

    if (!gt911ReadRegister(
        GT911_REG_POINT_1,
        pointData,
        sizeof(pointData)
    ))
    {
        gt911ClearStatus();
        return;
    }

    currentTouchID = pointData[0];

    const uint16_t rawX =
        static_cast<uint16_t>(pointData[1]) |
        (static_cast<uint16_t>(pointData[2]) << 8);

    const uint16_t rawY =
        static_cast<uint16_t>(pointData[3]) |
        (static_cast<uint16_t>(pointData[4]) << 8);

    currentX = 479 - rawY;
    currentY = rawX;
    
    if (currentX > 479)
    {
        currentX = 479;
    }

    if (currentY > 319)
    {
        currentY = 319;
    }
        
    static uint32_t lastCoordinatePrint = 0;

    if (millis() - lastCoordinatePrint >= 100)
    {
        lastCoordinatePrint = millis();

        Serial.print("TOUCH screen X=");
        Serial.print(currentX);
        Serial.print(" Y=");
        Serial.println(currentY);
    }

    if (!currentlyHeld)
    {
        currentlyHeld = true;
        pressedEvent = true;
    }

    // The GT911 requires the host to clear the data-ready flag
    // after consuming each coordinate report.
    gt911ClearStatus();
}

bool touchPressed()
{
    return pressedEvent;
}

bool touchReleased()
{
    return releasedEvent;
}

bool touchHeld()
{
    return currentlyHeld;
}

uint16_t touchX()
{
    return currentX;
}

uint16_t touchY()
{
    return currentY;
}

uint8_t touchID()
{
    return currentTouchID;
}

uint8_t touchPointCount()
{
    return currentPointCount;
}

bool touchAvailable()
{
    return controllerAvailable;
}
