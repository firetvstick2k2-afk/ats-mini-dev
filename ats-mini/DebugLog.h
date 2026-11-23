#pragma once

#include <Arduino.h>

// Additional USB-CDC diagnostic logger. Producers enqueue bounded records;
// the main loop drains them when USB has room.
void atsDebugBegin();
void atsDebugSettingsReady(bool enabled);
void atsDebugTick();
void atsDebugFlushTick();
void atsDebugLogf(const char* format, ...);

// Standard USB log: always independent of the USB-LOG Settings switch.
// Use this for deliberately selected, low-volume messages that should remain visible.
void atsStandardLogf(const char* format, ...);
