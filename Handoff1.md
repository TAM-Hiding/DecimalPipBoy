# STR-GZR Δ-10
## Firmware Handoff
### STR-GZR OS
### Release: Polaris v0.2.0

---

# Current Status

The firmware architecture is complete and functioning.

Current modules:

- Boot
- Display
- Globals
- Logo
- ScreenManager
- Time
- Display Driver

The codebase is modular. Each subsystem has its own files rather than a monolithic Arduino sketch.

---

# Current Hardware

Controller
- Teensy 4.1

Display
- 2.42" SSD1309 OLED
- I2C

Current Time Source
- Compile time + millis()
- Monotonic
- No backwards jumps

RTC Available
- DS1307 module (Elegoo kit)
- Not yet integrated

---

# Current UI

Boot Diagnostics

↓

Starburst Animation

↓

Δ-10 title

↓

Logo assembly

↓

Divider animation

↓

Instrument Panel

Current Instrument Panel:

- STR-GZR logo
- Narrow divider
- Fixed-position decimal clock
- Raised separator dot
- Fur20 font
- No digit jitter

UI is considered feature-complete for Polaris.
Avoid aesthetic changes unless required for new functionality.

---

# Decimal Time Format

Day

0.00000

↓

9.99999

One Earth day

=

1,000,000 STR-GZR ticks

Current implementation is integer-only.

No floating point.

---

# Architecture

Display

↓

ScreenManager

↓

Time

↓

Hardware Drivers

Display never computes time.

Time never draws graphics.

ScreenManager only composes UI.

---

# Naming

Hardware

STR-GZR Δ-10

Firmware

STR-GZR OS

Current Release

Polaris v0.2.0

---

# Roadmap

## Phase 1

□ Integrate DS1307 RTC

Replace compile-time source.

Time module only.

No UI changes.

---

## Phase 2

Buttons

- Next page
- Previous page
- Select (optional)

Create page navigation framework.

---

## Phase 3

Battery

- LiPo
- Charger
- Regulation
- Battery monitoring

Run untethered.

---

## Phase 4

IMU

BerryIMU v3

Implement:

Raise wrist

↓

Wake display

Lower wrist

↓

Sleep display

No shake-to-wake.

Use orientation.

---

## Phase 5

Sensor Pages

Compass

Environment

Geiger Counter

Battery

Diagnostics

IR Remote

Robot Control

---

# Design Philosophy

Minimal words.

Monochrome.

Industrial.

Instrument first.

Decorative animation only during startup.

Every pixel should have a purpose.

No gradients.

No anti-aliasing.

No glossy UI.

Think:

Late-70s HP
Tektronix
Fluke
Laboratory instrumentation

Not:

Smartwatch
Phone
Cyberpunk HUD

---

# Future Ideas

Magnetic charging dock

Expansion connector

MicroSD logging

USB serial console

RTC auto-sync

GPS sync

Radio time sync

Custom bitmap font

Custom boot sound

Vibration motor

---

# Notes

The raised middle dot should remain.

Fixed-position digits should remain.

Logo geometry is considered finalized.

Divider geometry is considered finalized.

The boot animation is intentionally understated.

Future polish should prioritize functionality over visual effects.
