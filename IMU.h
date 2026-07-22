#pragma once

#include <Arduino.h>

//======================================================
// Initialization and updates
//======================================================

void imuInit();
void imuUpdate();

bool imuPresent();

//======================================================
// Raw sensor readings
//======================================================

int16_t imuMagX();
int16_t imuMagY();
int16_t imuMagZ();

int16_t imuAccelX();
int16_t imuAccelY();
int16_t imuAccelZ();

//======================================================
// Orientation
//======================================================

float imuHeadingDegrees();
float imuYawDegrees();
float imuRollDegrees();
float imuPitchDegrees();

const char* imuCardinalDirection();

//======================================================
// Magnetometer calibration
//======================================================

// Begin collecting minimum and maximum magnetometer
// readings while the device is rotated through all axes.
void imuStartCalibration();

// Continue collecting calibration data.
// imuUpdate() calls this automatically while active.
void imuUpdateCalibration();

// Finish calibration and calculate offsets/scales.
void imuFinishCalibration();

bool imuCalibrationActive();
bool imuCalibrationValid();

// Current calibration progress, from 0 to 100.
// This is an approximate movement/range indicator.
uint8_t imuCalibrationProgress();

// Reset calibration values to their defaults.
void imuResetCalibration();

//======================================================
// Heading adjustment
//======================================================

// Fine adjustment applied after calibration and axis mapping.
void imuSetHeadingOffset(float degrees);
float imuHeadingOffset();
