#pragma once

//======================================================
// STR-GZR OS
//======================================================

#define OS_NAME     "STR-GZR OS"
#define OS_VERSION  "0.3.0"
#define OS_BUILD    "Polaris"

//======================================================
// Display
// Waveshare 3.5-inch LCD (F)
// ST7796S, landscape orientation
//======================================================

#define SCREEN_WIDTH     480
#define SCREEN_HEIGHT    320
#define SCREEN_ROTATION  3

// Teensy 4.1 hardware SPI:
// MOSI = 11
// MISO = 12
// SCLK = 13

#define TFT_MOSI         11
#define TFT_MISO         12
#define TFT_SCLK         13

#define TFT_CS           10
#define TFT_DC           9
#define TFT_RST          8
#define TFT_BL           5

//======================================================
// Capacitive Touch
// GT911 — not integrated yet
//======================================================

#define TOUCH_SDA        18
#define TOUCH_SCL        19
#define TOUCH_INT        7
#define TOUCH_RST        6

//======================================================
// Physical Buttons
//======================================================

#define BUTTON_LEFT      4
#define BUTTON_SELECT    3
#define BUTTON_RIGHT     2

//======================================================
// I2C
// Shared by GT911, BerryIMU, SHT40, and DS3231
//======================================================

#define I2C_SDA          18
#define I2C_SCL          19
#define I2C_CLOCK        400000

//======================================================
// Hardware Availability Flags
//
// Leave these disabled until each device has been
// reconnected and individually verified.
//======================================================

#define HAS_DISPLAY      1
#define HAS_TOUCH        1
#define HAS_RTC          1
#define HAS_IMU          1
#define HAS_ENV_SENSOR   1
#define HAS_GEIGER       0
#define HAS_SD           0
#define HAS_IR           0
#define HAS_BATTERY      0
