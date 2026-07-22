#pragma once

namespace Environment {

// Initializes the SHT40 subsystem.
// Assumes Wire.begin() has already been called elsewhere.
void begin();

// Call continuously from loop().
// Handles non-blocking measurement timing and cached readings.
void update();

// Sensor/status information
bool isPresent();
bool hasValidReading();

// Cached environmental readings
float temperatureC();
float temperatureF();
float humidityRH();

// Calculated from temperature and relative humidity
float dewPointC();
float dewPointF();

}  // namespace Environment
