#pragma once

#include <Arduino.h>
#include <stdarg.h>

// ATS Mini Dev adapter for Mini OS FTX diagnostics.
// Keep decoder diagnostics independent from Mini OS storage/LittleFS services.
namespace BootDiagnostics {
inline void write(const char* message) {
  if (message) Serial.println(message);
}
inline void printf(const char* format, ...) {
  if (!format) return;
  char buffer[512];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  Serial.println(buffer);
}
} // namespace BootDiagnostics
