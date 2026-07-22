#include "IMU.h"

#include <Wire.h>
#include <math.h>
#include <limits.h>
#include <EEPROM.h>

//======================================================
// BerryIMU v3 — LIS3MDL magnetometer
//======================================================

constexpr uint8_t LIS3MDL_ADDR = 0x1C;

constexpr uint8_t LIS3MDL_WHO_AM_I = 0x0F;
constexpr uint8_t LIS3MDL_CTRL_REG1 = 0x20;
constexpr uint8_t LIS3MDL_CTRL_REG2 = 0x21;
constexpr uint8_t LIS3MDL_CTRL_REG3 = 0x22;
constexpr uint8_t LIS3MDL_CTRL_REG4 = 0x23;
constexpr uint8_t LIS3MDL_OUT_X_L   = 0x28;

//======================================================
// BerryIMU v3 — LSM6DSL accelerometer / gyro
//======================================================

constexpr uint8_t LSM6DSL_ADDR = 0x6A;

constexpr uint8_t LSM6DSL_WHO_AM_I = 0x0F;
constexpr uint8_t LSM6DSL_CTRL1_XL = 0x10;
constexpr uint8_t LSM6DSL_CTRL2_G  = 0x11;
constexpr uint8_t LSM6DSL_OUTX_L_XL = 0x28;

//======================================================
// Axis mapping
//
// These are the first values to change if the compass
// direction is mirrored or rotated relative to the case.
//
// Keep the raw sensor accessors unchanged; this mapping
// only affects calculated orientation.
//======================================================

constexpr float MAG_X_SIGN =  1.0f;
constexpr float MAG_Y_SIGN =  1.0f;
constexpr float MAG_Z_SIGN =  1.0f;

constexpr float ACCEL_X_SIGN =  1.0f;
constexpr float ACCEL_Y_SIGN =  1.0f;
constexpr float ACCEL_Z_SIGN =  1.0f;

//======================================================
// Heading behavior
//======================================================

// Applied after calibration and tilt compensation.
static float headingOffsetDeg = 0.0f;

// 0.0 = no movement, 1.0 = no smoothing.
// Around 0.15–0.30 works well for a handheld compass.
constexpr float HEADING_SMOOTHING = 0.20f;

//======================================================
// Sensor presence
//======================================================

static bool magPresent = false;
static bool accelPresent = false;

//======================================================
// Raw readings
//======================================================

static int16_t magX = 0;
static int16_t magY = 0;
static int16_t magZ = 0;

static int16_t accelX = 0;
static int16_t accelY = 0;
static int16_t accelZ = 0;

//======================================================
// Calculated orientation
//======================================================

static float headingDeg = 0.0f;
static float rollDeg = 0.0f;
static float pitchDeg = 0.0f;

static bool headingFilterInitialized = false;
static float filteredHeadingX = 1.0f;
static float filteredHeadingY = 0.0f;

//======================================================
// Magnetometer calibration
//======================================================

static bool calibrationActive = false;
static bool calibrationValid = false;

static int16_t calibrationMinX = INT16_MAX;
static int16_t calibrationMinY = INT16_MAX;
static int16_t calibrationMinZ = INT16_MAX;

static int16_t calibrationMaxX = INT16_MIN;
static int16_t calibrationMaxY = INT16_MIN;
static int16_t calibrationMaxZ = INT16_MIN;

static float magOffsetX = 0.0f;
static float magOffsetY = 0.0f;
static float magOffsetZ = 0.0f;

static float magScaleX = 1.0f;
static float magScaleY = 1.0f;
static float magScaleZ = 1.0f;

//======================================================
// Persistent calibration storage
//======================================================

constexpr uint32_t CALIBRATION_MAGIC =
    0x53545247; // "STRG"

constexpr uint16_t CALIBRATION_VERSION =
    1;

constexpr int CALIBRATION_EEPROM_ADDRESS =
    0;

struct StoredCalibration
{
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;

    float offsetX;
    float offsetY;
    float offsetZ;

    float scaleX;
    float scaleY;
    float scaleZ;
};

// Raw-span target used only for the approximate progress
// indicator. It does not affect final calibration math.
constexpr float CALIBRATION_TARGET_SPAN = 7200.0f;

//======================================================
// Utility functions
//======================================================

static float degreesToRadians(float degrees)
{
    return degrees * PI / 180.0f;
}

static float radiansToDegrees(float radians)
{
    return radians * 180.0f / PI;
}

static float wrapDegrees(float degrees)
{
    while (degrees < 0.0f)
    {
        degrees += 360.0f;
    }

    while (degrees >= 360.0f)
    {
        degrees -= 360.0f;
    }

    return degrees;
}

static void writeRegister(
    uint8_t address,
    uint8_t registerAddress,
    uint8_t value
)
{
    Wire.beginTransmission(address);
    Wire.write(registerAddress);
    Wire.write(value);
    Wire.endTransmission();
}

static uint8_t readRegister(
    uint8_t address,
    uint8_t registerAddress
)
{
    Wire.beginTransmission(address);
    Wire.write(registerAddress);

    if (Wire.endTransmission(false) != 0)
    {
        return 0;
    }

    if (Wire.requestFrom(address, (uint8_t)1) != 1)
    {
        return 0;
    }

    return Wire.read();
}

static int16_t readInt16(
    uint8_t address,
    uint8_t lowRegister,
    bool autoIncrement
)
{
    Wire.beginTransmission(address);

    if (autoIncrement)
    {
        Wire.write(lowRegister | 0x80);
    }
    else
    {
        Wire.write(lowRegister);
    }

    if (Wire.endTransmission(false) != 0)
    {
        return 0;
    }

    if (Wire.requestFrom(address, (uint8_t)2) != 2)
    {
        return 0;
    }

    uint8_t lowByte = Wire.read();
    uint8_t highByte = Wire.read();

    return (int16_t)(
        ((uint16_t)highByte << 8) |
        lowByte
    );
}

//======================================================
// Calibration helpers
//======================================================

static void resetCalibrationExtremes()
{
    calibrationMinX = INT16_MAX;
    calibrationMinY = INT16_MAX;
    calibrationMinZ = INT16_MAX;

    calibrationMaxX = INT16_MIN;
    calibrationMaxY = INT16_MIN;
    calibrationMaxZ = INT16_MIN;
}

static void collectCalibrationSample()
{
    if (!calibrationActive || !magPresent)
    {
        return;
    }

    if (magX < calibrationMinX) calibrationMinX = magX;
    if (magY < calibrationMinY) calibrationMinY = magY;
    if (magZ < calibrationMinZ) calibrationMinZ = magZ;

    if (magX > calibrationMaxX) calibrationMaxX = magX;
    if (magY > calibrationMaxY) calibrationMaxY = magY;
    if (magZ > calibrationMaxZ) calibrationMaxZ = magZ;
}

static bool calculateCalibration()
{
    float spanX =
        (float)calibrationMaxX -
        (float)calibrationMinX;

    float spanY =
        (float)calibrationMaxY -
        (float)calibrationMinY;

    float spanZ =
        (float)calibrationMaxZ -
        (float)calibrationMinZ;

    // Reject obviously incomplete calibration data.
    if (
        spanX < 1000.0f ||
        spanY < 1000.0f ||
        spanZ < 1000.0f
    )
    {
        return false;
    }

    magOffsetX =
        (
            (float)calibrationMaxX +
            (float)calibrationMinX
        ) * 0.5f;

    magOffsetY =
        (
            (float)calibrationMaxY +
            (float)calibrationMinY
        ) * 0.5f;

    magOffsetZ =
        (
            (float)calibrationMaxZ +
            (float)calibrationMinZ
        ) * 0.5f;

    float radiusX = spanX * 0.5f;
    float radiusY = spanY * 0.5f;
    float radiusZ = spanZ * 0.5f;

    float averageRadius =
        (radiusX + radiusY + radiusZ) /
        3.0f;

    magScaleX = averageRadius / radiusX;
    magScaleY = averageRadius / radiusY;
    magScaleZ = averageRadius / radiusZ;

    return true;
}

//======================================================
// Orientation calculation
//======================================================

static void calculateOrientation()
{
    //==================================================
    // Accelerometer: device coordinate frame
    //   +X = right
    //   +Y = forward
    //   +Z = up
    //==================================================

    // Read the accelerometer in its native sensor axes.
    float sensorAx =
        ((float)accelX / 16384.0f) *
        ACCEL_X_SIGN;

    float sensorAy =
        ((float)accelY / 16384.0f) *
        ACCEL_Y_SIGN;

    float sensorAz =
        ((float)accelZ / 16384.0f) *
        ACCEL_Z_SIGN;

    // Rotate the software-defined device front
    // 90 degrees counterclockwise.
    //
    // New right   (+X) = old forward (+Y)
    // New forward (+Y) = old left    (-X)
    float ax = sensorAy;
    float ay = -sensorAx;
    float az = sensorAz;

    if (accelPresent)
    {
        // Roll: rotate forearm as though checking a watch.
        rollDeg =
            radiansToDegrees(
                atan2f(ax, az)
            );

        // Pitch: raise or lower the hand.
        pitchDeg =
            radiansToDegrees(
                atan2f(
                    -ay,
                    sqrtf(
                        ax * ax +
                        az * az
                    )
                )
            );
    }

    if (!magPresent)
    {
        return;
    }

    //==================================================
    // Apply magnetometer calibration
    //==================================================

    float sensorMagX =
        (
            (float)magX -
            magOffsetX
        ) *
        magScaleX *
        MAG_X_SIGN;

    float sensorMagY =
        (
            (float)magY -
           magOffsetY
        ) *
        magScaleY *
        MAG_Y_SIGN;

    float sensorMagZ =
        (
            (float)magZ -
            magOffsetZ
        ) *
        magScaleZ *
        MAG_Z_SIGN;

    // Match the accelerometer's 90-degree
    // counterclockwise software-axis rotation.
    float calibratedX = sensorMagY;
    float calibratedY = -sensorMagX;
    float calibratedZ = sensorMagZ;

    float rawHeading = 0.0f;

    //==================================================
    // Tilt-compensated heading
    //==================================================

    float accelLength =
        sqrtf(
            ax * ax +
            ay * ay +
            az * az
        );

    if (
        accelPresent &&
        accelLength > 0.01f
    )
    {
        float upX = ax / accelLength;
        float upY = ay / accelLength;
        float upZ = az / accelLength;

        // Device-forward vector is (0, 1, 0).
        // Remove its vertical component.
        float forwardDotUp = upY;

        float forwardX =
            -forwardDotUp * upX;

        float forwardY =
            1.0f -
            forwardDotUp * upY;

        float forwardZ =
            -forwardDotUp * upZ;

        float forwardLength =
            sqrtf(
                forwardX * forwardX +
                forwardY * forwardY +
                forwardZ * forwardZ
            );

        if (forwardLength > 0.01f)
        {
            forwardX /= forwardLength;
            forwardY /= forwardLength;
            forwardZ /= forwardLength;

            // Horizontal right = forward × up.
            float rightX =
                forwardY * upZ -
                forwardZ * upY;

            float rightY =
                forwardZ * upX -
                forwardX * upZ;

            float rightZ =
                forwardX * upY -
                forwardY * upX;

            float magneticForward =
                calibratedX * forwardX +
                calibratedY * forwardY +
                calibratedZ * forwardZ;

            float magneticRight =
                calibratedX * rightX +
                calibratedY * rightY +
                calibratedZ * rightZ;

            // Matches the experimentally confirmed
            // flat-board atan2(X, Y) orientation.
            rawHeading =
                radiansToDegrees(
                    atan2f(
                        magneticRight,
                        magneticForward
                    )
                );
        }
        else
        {
            rawHeading =
                radiansToDegrees(
                    atan2f(
                        calibratedX,
                        calibratedY
                    )
                );
        }
    }
    else
    {
        rawHeading =
            radiansToDegrees(
                atan2f(
                    calibratedX,
                    calibratedY
                )
            );
    }

    //==================================================
    // Offset, wrapping, and circular smoothing
    //==================================================

    rawHeading =
        wrapDegrees(
            rawHeading +
            headingOffsetDeg
        );

    float headingRadians =
        degreesToRadians(rawHeading);

    float sampleX =
        cosf(headingRadians);

    float sampleY =
        sinf(headingRadians);

    if (!headingFilterInitialized)
    {
        filteredHeadingX = sampleX;
        filteredHeadingY = sampleY;

        headingFilterInitialized = true;
    }
    else
    {
        filteredHeadingX =
            (
                1.0f -
                HEADING_SMOOTHING
            ) *
            filteredHeadingX +
            HEADING_SMOOTHING *
            sampleX;

        filteredHeadingY =
            (
                1.0f -
                HEADING_SMOOTHING
            ) *
            filteredHeadingY +
            HEADING_SMOOTHING *
            sampleY;
    }

    headingDeg =
        wrapDegrees(
            radiansToDegrees(
                atan2f(
                    filteredHeadingY,
                    filteredHeadingX
                )
            )
        );
}

//======================================================
// EEPROM calibration
//======================================================

static void saveCalibration()
{
    StoredCalibration data;

    data.magic =
        CALIBRATION_MAGIC;

    data.version =
        CALIBRATION_VERSION;

    data.reserved = 0;

    data.offsetX = magOffsetX;
    data.offsetY = magOffsetY;
    data.offsetZ = magOffsetZ;

    data.scaleX = magScaleX;
    data.scaleY = magScaleY;
    data.scaleZ = magScaleZ;

    EEPROM.put(
        CALIBRATION_EEPROM_ADDRESS,
        data
    );

    Serial.println(
        "Calibration saved."
    );
}

static bool loadCalibration()
{
    StoredCalibration data;

    EEPROM.get(
        CALIBRATION_EEPROM_ADDRESS,
        data
    );

    if (
        data.magic != CALIBRATION_MAGIC ||
        data.version != CALIBRATION_VERSION
    )
    {
        Serial.println(
            "No valid saved calibration."
        );

        return false;
    }

    if (
        !isfinite(data.offsetX) ||
        !isfinite(data.offsetY) ||
        !isfinite(data.offsetZ) ||
        !isfinite(data.scaleX) ||
        !isfinite(data.scaleY) ||
        !isfinite(data.scaleZ)
    )
    {
        Serial.println(
            "Saved calibration contains invalid values."
        );

        return false;
    }

    if (
        data.scaleX <= 0.0f ||
        data.scaleY <= 0.0f ||
        data.scaleZ <= 0.0f ||
        data.scaleX > 10.0f ||
        data.scaleY > 10.0f ||
        data.scaleZ > 10.0f
    )
    {
        Serial.println(
            "Saved calibration scale values rejected."
        );

        return false;
    }

    magOffsetX = data.offsetX;
    magOffsetY = data.offsetY;
    magOffsetZ = data.offsetZ;

    magScaleX = data.scaleX;
    magScaleY = data.scaleY;
    magScaleZ = data.scaleZ;

    calibrationValid = true;

    Serial.println(
        "Saved calibration loaded."
    );

    return true;
}
//======================================================
// Public initialization and updates
//======================================================

void imuInit()
{
    uint8_t magnetometerId =
        readRegister(
            LIS3MDL_ADDR,
            LIS3MDL_WHO_AM_I
        );

    magPresent =
        magnetometerId == 0x3D;

    if (magPresent)
    {
        // 10 Hz output, high-performance XY.
        writeRegister(
            LIS3MDL_ADDR,
            LIS3MDL_CTRL_REG1,
            0b01110000
        );

        // +/- 4 gauss.
        writeRegister(
            LIS3MDL_ADDR,
            LIS3MDL_CTRL_REG2,
            0b00000000
        );

        // Continuous-conversion mode.
        writeRegister(
            LIS3MDL_ADDR,
            LIS3MDL_CTRL_REG3,
            0b00000000
        );

        // High-performance Z.
        writeRegister(
            LIS3MDL_ADDR,
            LIS3MDL_CTRL_REG4,
            0b00001100
        );
    }

    uint8_t accelerometerId =
        readRegister(
            LSM6DSL_ADDR,
            LSM6DSL_WHO_AM_I
        );

    accelPresent =
        accelerometerId == 0x6A;

    if (accelPresent)
    {
        // Accelerometer: 104 Hz, +/- 2 g.
        writeRegister(
            LSM6DSL_ADDR,
            LSM6DSL_CTRL1_XL,
            0b01000000
        );

        // Gyroscope: 104 Hz, 245 dps.
        writeRegister(
            LSM6DSL_ADDR,
            LSM6DSL_CTRL2_G,
            0b01000000
        );
    }

    delay(100);
    
    loadCalibration();

    headingFilterInitialized = false;

    imuUpdate();
}

void imuUpdate()
{
    if (magPresent)
    {
        magX =
            readInt16(
                LIS3MDL_ADDR,
                LIS3MDL_OUT_X_L,
                true
            );

        magY =
            readInt16(
                LIS3MDL_ADDR,
                LIS3MDL_OUT_X_L + 2,
                true
            );

        magZ =
            readInt16(
                LIS3MDL_ADDR,
                LIS3MDL_OUT_X_L + 4,
                true
            );
    }

    if (accelPresent)
    {
        accelX =
            readInt16(
                LSM6DSL_ADDR,
                LSM6DSL_OUTX_L_XL,
                false
            );

        accelY =
            readInt16(
                LSM6DSL_ADDR,
                LSM6DSL_OUTX_L_XL + 2,
                false
            );

        accelZ =
            readInt16(
                LSM6DSL_ADDR,
                LSM6DSL_OUTX_L_XL + 4,
                false
            );
    }

    collectCalibrationSample();
    calculateOrientation();
}

bool imuPresent()
{
    return magPresent || accelPresent;
}

//======================================================
// Raw reading accessors
//======================================================

int16_t imuMagX()
{
    return magX;
}

int16_t imuMagY()
{
    return magY;
}

int16_t imuMagZ()
{
    return magZ;
}

int16_t imuAccelX()
{
    return accelX;
}

int16_t imuAccelY()
{
    return accelY;
}

int16_t imuAccelZ()
{
    return accelZ;
}

//======================================================
// Orientation accessors
//======================================================

float imuHeadingDegrees()
{
    return headingDeg;
}

float imuYawDegrees()
{
    return headingDeg;
}

float imuRollDegrees()
{
    return rollDeg;
}

float imuPitchDegrees()
{
    return pitchDeg;
}

const char* imuCardinalDirection()
{
    float heading = headingDeg;

    if (
        heading >= 337.5f ||
        heading < 22.5f
    )
    {
        return "N";
    }

    if (heading < 67.5f)  return "NE";
    if (heading < 112.5f) return "E";
    if (heading < 157.5f) return "SE";
    if (heading < 202.5f) return "S";
    if (heading < 247.5f) return "SW";
    if (heading < 292.5f) return "W";

    return "NW";
}

//======================================================
// Calibration public interface
//======================================================

void imuStartCalibration()
{
    resetCalibrationExtremes();

    calibrationActive = true;
    calibrationValid = false;
}

void imuUpdateCalibration()
{
    collectCalibrationSample();
}

void imuFinishCalibration()
{
    calibrationActive = false;

    calibrationValid =
        calculateCalibration();

    if (calibrationValid)
    {
        saveCalibration();
    }

    headingFilterInitialized = false;
}

bool imuCalibrationActive()
{
    return calibrationActive;
}

bool imuCalibrationValid()
{
    return calibrationValid;
}

uint8_t imuCalibrationProgress()
{
    if (!calibrationActive)
    {
        return calibrationValid
            ? 100
            : 0;
    }

    if (
        calibrationMinX == INT16_MAX ||
        calibrationMinY == INT16_MAX ||
        calibrationMinZ == INT16_MAX
    )
    {
        return 0;
    }

    float spanX =
        (float)calibrationMaxX -
        (float)calibrationMinX;

    float spanY =
        (float)calibrationMaxY -
        (float)calibrationMinY;

    float spanZ =
        (float)calibrationMaxZ -
        (float)calibrationMinZ;

    float progressX =
        spanX /
        CALIBRATION_TARGET_SPAN;

    float progressY =
        spanY /
        CALIBRATION_TARGET_SPAN;

    float progressZ =
        spanZ /
        CALIBRATION_TARGET_SPAN;

    if (progressX > 1.0f) progressX = 1.0f;
    if (progressY > 1.0f) progressY = 1.0f;
    if (progressZ > 1.0f) progressZ = 1.0f;

    float averageProgress =
        (
            progressX +
            progressY +
            progressZ
        ) /
        3.0f;

    return (uint8_t)(
        averageProgress *
        100.0f
    );
}

void imuResetCalibration()
{
    calibrationActive = false;
    calibrationValid = false;

    resetCalibrationExtremes();

    magOffsetX = 0.0f;
    magOffsetY = 0.0f;
    magOffsetZ = 0.0f;

    magScaleX = 1.0f;
    magScaleY = 1.0f;
    magScaleZ = 1.0f;

    headingFilterInitialized = false;
}

//======================================================
// Heading adjustment
//======================================================

void imuSetHeadingOffset(float degrees)
{
    headingOffsetDeg =
        wrapDegrees(degrees);

    headingFilterInitialized = false;
}

float imuHeadingOffset()
{
    return headingOffsetDeg;
}
