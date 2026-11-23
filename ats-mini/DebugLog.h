#pragma once

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward ESP-IDF log output to the same USB CDC Serial stream used by the
// firmware. Arduino Serial.printf()/print() output remains unchanged.
static int atsDebugVprintf(const char *format, va_list args)
{
  char line[512];
  const int length = vsnprintf(line, sizeof(line), format, args);
  if(length <= 0) return length;

  const size_t count = (size_t)length < sizeof(line) ? (size_t)length : sizeof(line) - 1;
  Serial.write((const uint8_t *)line, count);
  return length;
}

static const char *atsResetReasonName(esp_reset_reason_t reason)
{
  switch(reason)
  {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXTERNAL";
    case ESP_RST_SW:        return "SOFTWARE";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT:  return "TASK_WATCHDOG";
    case ESP_RST_WDT:       return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "OTHER";
  }
}

static void atsDebugBegin()
{
  // Enable the most detailed ESP-IDF log level at runtime. The build currently
  // uses USB CDC on boot (ESP32-S3 HWCDC), so this is visible in the USB terminal.
  esp_log_set_vprintf(atsDebugVprintf);
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  const esp_reset_reason_t reason = esp_reset_reason();
  Serial.println();
  Serial.println("========== ATS MINI FULL DEBUG ==========");
  Serial.printf("[ATS][BOOT] reset_reason=%s (%d)\n",
                atsResetReasonName(reason), (int)reason);
  Serial.printf("[ATS][BOOT] chip=%s rev=%u cpu_mhz=%u uptime_ms=%lu\n",
                ESP.getChipModel(), (unsigned)ESP.getChipRevision(),
                (unsigned)ESP.getCpuFreqMHz(), (unsigned long)millis());
  Serial.printf("[ATS][BOOT] heap_free=%u heap_min=%u heap_max_block=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());
  Serial.printf("[ATS][BOOT] psram_total=%u psram_free=%u\n",
                (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());
  Serial.printf("[ATS][BOOT] setup_stack_high_water=%u tasks=%u\n",
                (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                (unsigned)uxTaskGetNumberOfTasks());
  Serial.println("[ATS][BOOT] ESP-IDF verbose logging enabled");
  Serial.println("==========================================");
}

// Low-rate system heartbeat. Service-specific worker stack/heap diagnostics
// are emitted by WeatherService, NewsService and BandOpeningService.
static void atsDebugTick()
{
  static uint32_t lastReportMs = 0;
  const uint32_t now = millis();
  if((uint32_t)(now - lastReportMs) < 60000UL) return;
  lastReportMs = now;

  Serial.printf("[ATS][SYS] uptime_ms=%lu heap_free=%u heap_min=%u heap_max_block=%u "
                "psram_free=%u tasks=%u loop_stack_high_water=%u\n",
                (unsigned long)now,
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMinFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap(),
                (unsigned)ESP.getFreePsram(),
                (unsigned)uxTaskGetNumberOfTasks(),
                (unsigned)uxTaskGetStackHighWaterMark(nullptr));
}
