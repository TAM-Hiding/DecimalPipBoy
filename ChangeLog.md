# Decimal PipBoy
# Change Log

------------------------------------------------------------
v0.2.0 "Polaris"
------------------------------------------------------------

Major firmware architecture overhaul.

SYSTEM
------
- Reorganized firmware into modular source files.
- Added ScreenManager page system.
- Added Button input manager.
- Added partial screen redraw architecture.
- Significantly reduced display flicker.
- Added persistent instrument panel layout.
- Added STR-GZR OS branding.
- Added Polaris firmware version display.
- Added WAKARI LABS footer branding.
- Updated UI color palette with orange system branding.

TIME
----
- Upgraded RTC from DS1307 to DS3231.
- Added automatic RTC detection.
- Decimal time now sourced directly from RTC.
- Improved decimal time precision.
- Clock display now updates individual digits instead of full screen redraws.

COMPASS
-------
- Added BerryIMU v3 support.
- Added LIS3MDL magnetometer driver.
- Added LSM6DSL accelerometer driver.
- Added heading calculation.
- Added roll and pitch calculation.
- Added heading smoothing filter.
- Added heading offset support.
- Added magnetometer calibration.
- Added calibration validity checking.
- Added experimental tilt compensation.
- Added compass visualization.
- Added roll/pitch live readouts.

CALIBRATION
-----------
- Added dedicated IMU calibration page.
- Added calibration progress bar.
- Added calibration status messages.
- Added one-button calibration workflow.
- Calibration now applies offsets and scale correction.

SYSTEM PAGE
-----------
- Added hardware diagnostics page.
- Added RTC status indicator.
- Added IMU status indicator.
- Added sensor detection reporting.
- Added visual status bars.

DISPLAY
-------
- Added optimized partial redraw system.
- Eliminated full-screen refreshes during normal operation.
- Improved UI responsiveness.
- Added custom color theme.
- Improved text alignment throughout interface.

INPUT
-----
- Added Left / Select / Right page navigation.
- Added page-specific Select button actions.
- Added calibration controls.

KNOWN ISSUES
------------
- Tilt compensation still requires refinement.
- Calibration currently stored in RAM only.
- Touchscreen support not yet implemented.
- Battery monitoring pending.
- Environmental sensor pages pending.
- Radio integration pending.

------------------------------------------------------------
Development Statistics
------------------------------------------------------------

Firmware Status:
    Stable

Display:
    Fully operational

RTC:
    DS3231 integrated

IMU:
    Operational
    Calibration functional

Navigation:
    Complete

Estimated Code Size:
    ~3,000+ lines

Current Hardware:
    Teensy 4.1
    BerryIMU v3
    DS3231 RTC
    480x320 TFT
    
    Developer Notes
---------------

Today's accomplishments:

- Replaced DS1307 with DS3231.
- Finally defeated display flicker.
- Compass now approximately knows where north is.
- Accidentally built an operating system.
- Determined that arguing about fonts for an hour was worthwhile.
