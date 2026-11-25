#include "DebugLog.h"

extern bool webDiagEnabled;

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if defined(ARDUINO_USB_MODE) && (ARDUINO_USB_MODE == 1) && defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
#include "HWCDC.h"
#define ATS_DEBUG_USE_HWCDC 1
#else
#define ATS_DEBUG_USE_HWCDC 0
#endif

namespace {
constexpr size_t kLogRecordCount = 48;
constexpr size_t kLogLineBytes = 256;
constexpr size_t kFlushChunkBytes = 192;

struct LogRecord {
  uint32_t sequence = 0;
  uint16_t length = 0;
  char data[kLogLineBytes] = {};
};

LogRecord gRecords[kLogRecordCount];
portMUX_TYPE gLogMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t gHead = 0;
uint8_t gTail = 0;
uint8_t gCount = 0;
uint16_t gTailOffset = 0;
uint32_t gDroppedRecords = 0;
uint32_t gReportedDroppedRecords = 0;
uint32_t gNextSequence = 1;
bool gUsbStateKnown = false;
bool gLastUsbPlugged = false;
volatile bool gSettingsReady = false;

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "OTHER";
  }
}

bool usbHostPresent() {
#if ATS_DEBUG_USE_HWCDC
  return HWCDC::isPlugged();
#else
  return Serial.availableForWrite() > 0;
#endif
}

void queueText(const char* text, size_t length) {
  if (!text || !length) return;
  if (length >= kLogLineBytes) length = kLogLineBytes - 1;

  portENTER_CRITICAL(&gLogMux);
  if (gCount >= kLogRecordCount) {
    gTail = static_cast<uint8_t>((gTail + 1U) % kLogRecordCount);
    --gCount;
    gTailOffset = 0;
    ++gDroppedRecords;
  }

  LogRecord& record = gRecords[gHead];
  memcpy(record.data, text, length);
  record.data[length] = '\0';
  record.length = static_cast<uint16_t>(length);
  record.sequence = gNextSequence++;
  gHead = static_cast<uint8_t>((gHead + 1U) % kLogRecordCount);
  ++gCount;
  portEXIT_CRITICAL(&gLogMux);
}

void queueBody(const char* body) {
  if (!body || (gSettingsReady && !webDiagEnabled)) return;
  char line[kLogLineBytes];
  const int written = snprintf(line, sizeof(line), "[%10lu ms] %s\n",
                                static_cast<unsigned long>(millis()), body);
  if (written <= 0) return;
  const size_t length = static_cast<size_t>(written) < sizeof(line)
                          ? static_cast<size_t>(written) : sizeof(line) - 1;
  queueText(line, length);
}

void queueV(const char* format, va_list args) {
  char body[208];
  vsnprintf(body, sizeof(body), format, args);
  size_t length = strnlen(body, sizeof(body));
  while (length && (body[length - 1] == '\n' || body[length - 1] == '\r'))
    body[--length] = '\0';
  queueBody(body);
}

} // namespace

void atsStandardLogf(const char* format, ...) {
  if (!format || !usbHostPresent()) return;

  char body[208];
  va_list args;
  va_start(args, format);
  const int bodyLength = vsnprintf(body, sizeof(body), format, args);
  va_end(args);
  if (bodyLength <= 0) return;

  char line[256];
  const int lineLength = snprintf(line, sizeof(line), "[%10lu ms] %s\n",
                                  static_cast<unsigned long>(millis()), body);
  if (lineLength <= 0 || static_cast<size_t>(lineLength) >= sizeof(line)) return;

  // Standard messages are independent of USB-LOG and never wait for the host.
  const int writable = Serial.availableForWrite();
  if (writable >= lineLength) Serial.write(
      reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(lineLength));
}

void atsDebugLogf(const char* format, ...) {
  if (!format) return;
  va_list args;
  va_start(args, format);
  queueV(format, args);
  va_end(args);
}

void atsDebugBegin() {
  // Never let a closed Android terminal stall the radio loop on USB CDC writes.
  // All diagnostic records use the bounded RAM queue below.
  Serial.setTxTimeoutMs(0);
  // Keep normal ESP-IDF warnings/errors on the standard USB stream.
  // The USB-LOG switch controls structured firmware diagnostics; it must not
  // turn on verbose ESP-IDF driver output.
  esp_log_level_set("*", ESP_LOG_WARN);
  atsStandardLogf("[USB] Standard USB logging ready; extra USB-LOG is OFF until Settings enables it.");

  const esp_reset_reason_t reason = esp_reset_reason();
  atsDebugLogf("========== ATS MINI USB DIAGNOSTICS ==========");
  atsStandardLogf("[BOOT] reset_reason=%s (%d) chip=%s rev=%u cpu_mhz=%u",
               resetReasonName(reason), static_cast<int>(reason), ESP.getChipModel(),
               static_cast<unsigned>(ESP.getChipRevision()),
               static_cast<unsigned>(ESP.getCpuFreqMHz()));
  atsDebugLogf("[BOOT][MEM] heap_free=%u heap_min=%u heap_max_block=%u psram_total=%u psram_free=%u",
               static_cast<unsigned>(ESP.getFreeHeap()),
               static_cast<unsigned>(ESP.getMinFreeHeap()),
               static_cast<unsigned>(ESP.getMaxAllocHeap()),
               static_cast<unsigned>(ESP.getPsramSize()),
               static_cast<unsigned>(ESP.getFreePsram()));
  atsDebugLogf("[BOOT][TASK] setup_stack_high_water=%u task_count=%u",
               static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
               static_cast<unsigned>(uxTaskGetNumberOfTasks()));
  atsDebugLogf("[BOOT] ESP-IDF verbose logging deferred until USB-LOG is enabled; USB TX timeout=0 ms; RAM log queue=%u records",
               static_cast<unsigned>(kLogRecordCount));
}

void atsDebugSettingsReady(bool enabled) {
  portENTER_CRITICAL(&gLogMux);
  gSettingsReady = true;
  if (!enabled) {
    gHead = 0;
    gTail = 0;
    gCount = 0;
    gTailOffset = 0;
    gDroppedRecords = 0;
    gReportedDroppedRecords = 0;
  }
  portEXIT_CRITICAL(&gLogMux);

  // The switch controls only the additional diagnostic stream.
  // Standard USB messages continue independently when this is disabled.
  // USB-LOG enables the firmware's structured atsDebugLogf() diagnostics.
  // Keep ESP-IDF itself at WARN in both modes: INFO/DEBUG/VERBOSE driver chatter
  // (especially I2C transactions) obscures useful application diagnostics.
  // Warnings and errors from ESP-IDF remain visible.
  esp_log_level_set("*", ESP_LOG_WARN);
  if (enabled) atsDebugLogf("[LOG] Extra USB diagnostics enabled; ESP-IDF limited to WARN+");
  else atsStandardLogf("[USB] Extra USB-LOG disabled; standard logging remains active.");
}

void atsDebugTick() {
  static uint32_t lastLoopStartMs = 0;
  static uint32_t lastHealthReportMs = 0;
  static uint32_t maxLoopGapMs = 0;
  static uint32_t slowLoopCount = 0;

  const uint32_t now = millis();
  if (lastLoopStartMs) {
    const uint32_t gap = now - lastLoopStartMs;
    if (gap > maxLoopGapMs) maxLoopGapMs = gap;
    // Count slow loop intervals for the periodic health summary instead of
    // printing one line for every transient delay.
    if (gap > 500U) ++slowLoopCount;
  }
  lastLoopStartMs = now;

  const bool plugged = usbHostPresent();
  if (!gUsbStateKnown) {
    gUsbStateKnown = true;
    gLastUsbPlugged = plugged;
    atsDebugLogf("[USB][LINK] host_present=%u", plugged ? 1U : 0U);
  } else if (plugged != gLastUsbPlugged) {
    gLastUsbPlugged = plugged;
    atsDebugLogf("[USB][LINK] host_present=%u", plugged ? 1U : 0U);
  }

  if ((uint32_t)(now - lastHealthReportMs) >= 60000UL) {
    lastHealthReportMs = now;
    uint8_t queuedRecords = 0;
    uint32_t droppedRecords = 0;
    portENTER_CRITICAL(&gLogMux);
    queuedRecords = gCount;
    droppedRecords = gDroppedRecords;
    portEXIT_CRITICAL(&gLogMux);
    atsDebugLogf("[SYS][HEALTH] uptime_ms=%lu heap_free=%u heap_min=%u heap_max_block=%u psram_free=%u tasks=%u loop_stack_high_water=%u loop_gap_max_ms=%lu slow_loops=%lu usb_host=%u usb_tx_free=%d log_queue=%u log_dropped=%lu",
                 static_cast<unsigned long>(now),
                 static_cast<unsigned>(ESP.getFreeHeap()),
                 static_cast<unsigned>(ESP.getMinFreeHeap()),
                 static_cast<unsigned>(ESP.getMaxAllocHeap()),
                 static_cast<unsigned>(ESP.getFreePsram()),
                 static_cast<unsigned>(uxTaskGetNumberOfTasks()),
                 static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                 static_cast<unsigned long>(maxLoopGapMs),
                 static_cast<unsigned long>(slowLoopCount),
                 usbHostPresent() ? 1U : 0U, Serial.availableForWrite(),
                 static_cast<unsigned>(queuedRecords),
                 static_cast<unsigned long>(droppedRecords));
    maxLoopGapMs = 0;
    slowLoopCount = 0;
  }
}

void atsDebugFlushTick() {
  if (!gSettingsReady || !webDiagEnabled || !usbHostPresent()) return;

  int writable = Serial.availableForWrite();
  if (writable <= 0) return;

  uint8_t output[kFlushChunkBytes];
  size_t amount = 0;
  uint32_t recordSequence = 0;

  portENTER_CRITICAL(&gLogMux);
  if (gCount) {
    const LogRecord& record = gRecords[gTail];
    recordSequence = record.sequence;
    const size_t remaining = record.length > gTailOffset
                               ? static_cast<size_t>(record.length - gTailOffset) : 0;
    if (remaining) {
      amount = remaining;
      if (amount > sizeof(output)) amount = sizeof(output);
      if (amount > static_cast<size_t>(writable)) amount = static_cast<size_t>(writable);
      memcpy(output, record.data + gTailOffset, amount);
    } else {
      gTail = static_cast<uint8_t>((gTail + 1U) % kLogRecordCount);
      --gCount;
      gTailOffset = 0;
    }
  }
  portEXIT_CRITICAL(&gLogMux);

  if (amount) {
    // TX timeout is zero, and the write is limited to currently available space.
    const size_t sent = Serial.write(output, amount);
    if (sent) {
      portENTER_CRITICAL(&gLogMux);
      if (gCount && gRecords[gTail].sequence == recordSequence) {
        gTailOffset = static_cast<uint16_t>(gTailOffset + sent);
        if (gTailOffset >= gRecords[gTail].length) {
          gTail = static_cast<uint8_t>((gTail + 1U) % kLogRecordCount);
          --gCount;
          gTailOffset = 0;
        }
      }
      portEXIT_CRITICAL(&gLogMux);
    }
  }

  uint32_t droppedToReport = 0;
  portENTER_CRITICAL(&gLogMux);
  if (gDroppedRecords != gReportedDroppedRecords && gCount < kLogRecordCount) {
    droppedToReport = gDroppedRecords - gReportedDroppedRecords;
    gReportedDroppedRecords = gDroppedRecords;
  }
  portEXIT_CRITICAL(&gLogMux);

  if (droppedToReport) {
    atsDebugLogf("[LOG][OVERFLOW] oldest_records_overwritten=%lu; latest diagnostics retained",
                 static_cast<unsigned long>(droppedToReport));
  }
}
