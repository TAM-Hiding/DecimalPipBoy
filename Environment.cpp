#include "Environment.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

namespace Environment {

// -----------------------------------------------------------------------------
// SHT40 configuration
// -----------------------------------------------------------------------------

static constexpr uint8_t SHT40_ADDRESS = 0x44;

// SHT40 high-precision measurement command
static constexpr uint8_t SHT40_MEASURE_HIGH_PRECISION = 0xFD;

// High-precision measurement typically finishes in about 8.3 ms.
// Giving it 10 ms provides a little margin.
static constexpr uint32_t MEASUREMENT_WAIT_MS = 10;

// How often to begin a new environmental measurement.
static constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;

// -----------------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------------

enum class ReadState : uint8_t {
  IDLE,
  WAITING_FOR_MEASUREMENT
};

static ReadState readState = ReadState::IDLE;

static bool sensorPresent = false;
static bool readingValid = false;

static uint32_t lastSampleStartedMs = 0;
static uint32_t measurementStartedMs = 0;

static float cachedTemperatureC = 0.0f;
static float cachedHumidityRH = 0.0f;

// -----------------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------------

static uint8_t calculateCRC(const uint8_t* data, size_t length) {
  uint8_t crc = 0xFF;

  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];

    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x80) {
        crc = static_cast<uint8_t>((crc << 1) ^ 0x31);
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

static bool beginMeasurement() {
  Wire.beginTransmission(SHT40_ADDRESS);
  Wire.write(SHT40_MEASURE_HIGH_PRECISION);

  const uint8_t result = Wire.endTransmission();

  if (result != 0) {
    sensorPresent = false;
    readingValid = false;
    return false;
  }

  sensorPresent = true;
  measurementStartedMs = millis();
  readState = ReadState::WAITING_FOR_MEASUREMENT;

  return true;
}

static bool collectMeasurement() {
  constexpr uint8_t expectedBytes = 6;

  const uint8_t received =
      Wire.requestFrom(SHT40_ADDRESS, expectedBytes);

  if (received != expectedBytes) {
    while (Wire.available()) {
      Wire.read();
    }

    readingValid = false;
    return false;
  }

  uint8_t data[expectedBytes];

  for (uint8_t i = 0; i < expectedBytes; ++i) {
    data[i] = Wire.read();
  }

  const bool temperatureCRCValid =
      calculateCRC(&data[0], 2) == data[2];

  const bool humidityCRCValid =
      calculateCRC(&data[3], 2) == data[5];

  if (!temperatureCRCValid || !humidityCRCValid) {
    readingValid = false;
    return false;
  }

  const uint16_t rawTemperature =
      (static_cast<uint16_t>(data[0]) << 8) |
      static_cast<uint16_t>(data[1]);

  const uint16_t rawHumidity =
      (static_cast<uint16_t>(data[3]) << 8) |
      static_cast<uint16_t>(data[4]);

  cachedTemperatureC =
      -45.0f +
      175.0f *
      static_cast<float>(rawTemperature) /
      65535.0f;

  cachedHumidityRH =
      -6.0f +
      125.0f *
      static_cast<float>(rawHumidity) /
      65535.0f;

  cachedHumidityRH =
      constrain(cachedHumidityRH, 0.0f, 100.0f);

  sensorPresent = true;
  readingValid = true;

  return true;
}

// -----------------------------------------------------------------------------
// Public functions
// -----------------------------------------------------------------------------

void begin() {
  sensorPresent = false;
  readingValid = false;
  readState = ReadState::IDLE;

  lastSampleStartedMs = 0;
  measurementStartedMs = 0;

  // Wire.begin() should already be called by the main firmware because the
  // RTC and IMU share this I2C bus. We therefore do not restart Wire here.

  // Probe the sensor immediately.
  Wire.beginTransmission(SHT40_ADDRESS);
  sensorPresent = Wire.endTransmission() == 0;

  if (sensorPresent) {
    beginMeasurement();
    lastSampleStartedMs = millis();
  }
}

void update() {
  const uint32_t now = millis();

  switch (readState) {
    case ReadState::IDLE:
      if (now - lastSampleStartedMs >= SAMPLE_INTERVAL_MS) {
        lastSampleStartedMs = now;
        beginMeasurement();
      }
      break;

    case ReadState::WAITING_FOR_MEASUREMENT:
      if (now - measurementStartedMs >= MEASUREMENT_WAIT_MS) {
        collectMeasurement();
        readState = ReadState::IDLE;
      }
      break;
  }
}

bool isPresent() {
  return sensorPresent;
}

bool hasValidReading() {
  return readingValid;
}

float temperatureC() {
  return cachedTemperatureC;
}

float temperatureF() {
  return cachedTemperatureC * 9.0f / 5.0f + 32.0f;
}

float humidityRH() {
  return cachedHumidityRH;
}

float dewPointC() {
  if (!readingValid) {
    return 0.0f;
  }

  // Magnus approximation for dew point.
  constexpr float a = 17.62f;
  constexpr float b = 243.12f;

  const float safeHumidity =
      constrain(cachedHumidityRH, 0.1f, 100.0f);

  const float gamma =
      logf(safeHumidity / 100.0f) +
      (a * cachedTemperatureC) /
      (b + cachedTemperatureC);

  return (b * gamma) / (a - gamma);
}

float dewPointF() {
  return dewPointC() * 9.0f / 5.0f + 32.0f;
}

}  // namespace Environment
