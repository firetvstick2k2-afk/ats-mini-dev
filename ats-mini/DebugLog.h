#pragma once

#include <Arduino.h>

// Central USB-CDC diagnostic logger. Producers only enqueue bounded records;
// the main loop drains them when USB has room.
void atsDebugBegin();
void atsDebugSettingsReady(bool enabled);
void atsDebugTick();
void atsDebugFlushTick();
void atsDebugLogf(const char* format, ...);
