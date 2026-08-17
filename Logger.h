#pragma once

#include <Arduino.h>

bool loggerInit();
void loggerLog(const String& message);
bool loggerReady(); 

// Print the current log file to Serial.
void loggerDumpToSerial();
