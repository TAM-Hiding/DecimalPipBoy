#include "Boot.h"
#include "Display.h"
#include "Globals.h"

#include <Arduino.h>
#include <U8g2lib.h>

// Display object lives in Display.cpp
extern U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2;

void drawSnakeFrame(int progress);

const char* spinnerFrames[] = {
  "|",
  "/",
  "—",
  "\\"
};

void drawBootFrame(int frame, const char* status)
{
  u8g2.clearBuffer();

  u8g2.drawFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(34, 12, OS_NAME);

  u8g2.drawStr(44, 24, "v" OS_VERSION);

  u8g2.drawStr(19, 36, "Build: " OS_BUILD);

  u8g2.drawStr(8, 49, status);

  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawUTF8(111, 51, spinnerFrames[frame % 4]);

  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(4, 62, "WAKARI LABS");

  u8g2.sendBuffer();
}

void drawSnakeFrame(int progress)
{
    progress = constrain(progress, 0, 380);

    int remaining = progress;

    // Top edge: left → right, length 127
    int top = min(remaining, 127);
    if (top > 0) u8g2.drawLine(0, 0, top, 0);
    remaining -= top;

    // Right edge: top → bottom, length 63
    int right = min(remaining, 63);
    if (right > 0) u8g2.drawLine(127, 0, 127, right);
    remaining -= right;

    // Bottom edge: right → left, length 127
    int bottom = min(remaining, 127);
    if (bottom > 0) u8g2.drawLine(127, 63, 127 - bottom, 63);
    remaining -= bottom;

    // Left edge: bottom → top, length 63
    int left = min(remaining, 63);
    if (left > 0) u8g2.drawLine(0, 63, 0, 63 - left);
}

void runBootSequence()
{
  const char* bootSteps[] = {
    "Display.........OK",
    "Memory..........OK",
    "I2C Bus.........OK",
    "Storage......WAIT",
    "RTC..........WAIT",
    "IMU..........WAIT",
    "System.......READY"
  };

  int frame = 0;

  for (int i = 0; i < 7; i++)
  {
    for (int j = 0; j < 5; j++)
    {
      drawBootFrame(frame, bootSteps[i]);
      frame++;
      delay(90);
    }
  }

  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

  u8g2.setFont(u8g2_font_9x18B_tf);
  u8g2.drawStr(15, 24, "BOOT READY");

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(25, 43, OS_NAME);
  u8g2.drawStr(31, 56, "ONLINE");

  u8g2.sendBuffer();
  delay(1200);
}
