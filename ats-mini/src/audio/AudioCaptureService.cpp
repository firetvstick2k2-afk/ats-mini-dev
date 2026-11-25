#include "AudioCaptureService.h"
#include "../../DebugLog.h"

#include <esp_timer.h>
#include <sys/time.h>
#include <math.h>

bool AudioCaptureService::begin() {
#if defined(LILYGO_SI473X)
  return false; // GPIO11 is the LILYGO SPI MOSI pin, not the ATS Mini audio input.
#else
  if (initialized_) {
    atsDebugLogf("[AUDIO] begin: capture task already initialized");
    return true;
  }
  atsDebugLogf("[AUDIO] begin GPIO=%u rate=%u heap=%u stack=%u\n",
                static_cast<unsigned>(kPin), static_cast<unsigned>(kSampleRate),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  // ATS Mini V4B routes receiver audio to GPIO11 (ADC2_CH0).
  // Keep 12-bit samples; do not force pin attenuation before ADC channel setup.
  analogReadResolution(12);
  pinMode(kPin, INPUT);
  atsDebugLogf("[AUDIO] creating capture task stack=4096 core=0");
  const BaseType_t taskResult = xTaskCreatePinnedToCore(taskEntry, "audio-capture", 4096, this, 3, nullptr, 0);
  atsDebugLogf("[AUDIO] capture task result=%ld heap=%u\n",
                static_cast<long>(taskResult), static_cast<unsigned>(ESP.getFreeHeap()));
  if (taskResult != pdPASS) return false;
  initialized_ = true;
  atsDebugLogf("[AUDIO] begin done");
  return true;
#endif
}

bool AudioCaptureService::subscribe(void* context, Consumer consumer) {
  if (!initialized_ || !context || !consumer) return false;
  portENTER_CRITICAL(&mux_);
  for (size_t i = 0; i < kMaxSubscribers; ++i) {
    if (subscribers_[i].context == context) {
      subscribers_[i].consumer = consumer;
      portEXIT_CRITICAL(&mux_);
      return true;
    }
  }
  for (size_t i = 0; i < kMaxSubscribers; ++i) {
    if (!subscribers_[i].context) {
      subscribers_[i] = {context, consumer};
      portEXIT_CRITICAL(&mux_);
      return true;
    }
  }
  portEXIT_CRITICAL(&mux_);
  return false;
}

void AudioCaptureService::unsubscribe(void* context) {
  if (!context) return;
  portENTER_CRITICAL(&mux_);
  for (size_t i = 0; i < kMaxSubscribers; ++i) {
    if (subscribers_[i].context == context) subscribers_[i] = {};
  }
  portEXIT_CRITICAL(&mux_);
}

size_t AudioCaptureService::snapshotSubscribers(Subscriber* out, size_t capacity) {
  size_t count = 0;
  portENTER_CRITICAL(&mux_);
  for (size_t i = 0; i < kMaxSubscribers && count < capacity; ++i) {
    if (subscribers_[i].context && subscribers_[i].consumer) out[count++] = subscribers_[i];
  }
  portEXIT_CRITICAL(&mux_);
  return count;
}

void AudioCaptureService::taskEntry(void* arg) {
  static_cast<AudioCaptureService*>(arg)->sampleTask();
}

void AudioCaptureService::sampleTask() {
  int16_t samples[kBlockSamples];
  size_t count = 0;
  int64_t base = 0, next = 0;
  uint64_t index = 0;
  const uint32_t taskStartMs = millis();
  uint32_t lastHealthReportMs = taskStartMs;
  uint32_t lastOverrunReportMs = 0;
  size_t lastSubscriberCount = SIZE_MAX;

  for (;;) {
    Subscriber active[kMaxSubscribers]{};
    const size_t activeCount = snapshotSubscribers(active, kMaxSubscribers);
    if (activeCount != lastSubscriberCount) {
      lastSubscriberCount = activeCount;
      atsDebugLogf("[AUDIO][SUBSCRIBERS] active=%u sample_rate=%u block_samples=%u",
                   static_cast<unsigned>(activeCount), static_cast<unsigned>(kSampleRate),
                   static_cast<unsigned>(kBlockSamples));
    }
    const uint32_t healthNow = millis();
    const uint32_t taskElapsedMs = healthNow - taskStartMs;
    const uint32_t healthIntervalMs = taskElapsedMs < 20000UL ? 2000UL : 15000UL;
    if (activeCount && (uint32_t)(healthNow - lastHealthReportMs) >= healthIntervalMs) {
      lastHealthReportMs = healthNow;
      atsDebugLogf("[AUDIO][TASK_HEALTH] elapsed_ms=%lu sample_index=%llu block_fill=%u heap_free=%u heap_min=%u task_stack_high_water=%u subscribers=%u sample_rate=%u",
                   static_cast<unsigned long>(taskElapsedMs),
                   static_cast<unsigned long long>(index), static_cast<unsigned>(count),
                   static_cast<unsigned>(ESP.getFreeHeap()),
                   static_cast<unsigned>(ESP.getMinFreeHeap()),
                   static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                   static_cast<unsigned>(activeCount), static_cast<unsigned>(kSampleRate));
    }
    if (activeCount == 0) {
      count = 0;
      base = next = 0;
      index = 0;
      vTaskDelay(pdMS_TO_TICKS(25));
      continue;
    }

    if (!base) {
      base = next = esp_timer_get_time();
      index = 0;
    }
    while (esp_timer_get_time() < next) taskYIELD();

    ++index;
    next = base + static_cast<int64_t>(index * 1000000ULL / kSampleRate);
    // GPIO11 is ADC2 on the ATS Mini V4B; callers must stop Wi-Fi first.
    int value = analogRead(kPin);
    if (value < 0) value = 2048;
    samples[count++] = static_cast<int16_t>(value);

    if (count >= kBlockSamples) {
      timeval wallTime{};
      gettimeofday(&wallTime, nullptr);
      const int64_t utcEndMicros = static_cast<int64_t>(wallTime.tv_sec) * 1000000LL + wallTime.tv_usec;
      const int64_t utcStartMicros = utcEndMicros -
        static_cast<int64_t>((count - 1) * 1000000ULL / kSampleRate);
      const size_t consumers = snapshotSubscribers(active, kMaxSubscribers);
      for (size_t i = 0; i < consumers; ++i) {
        active[i].consumer(active[i].context, samples, count, utcStartMicros);
      }
      count = 0;
    }

    if (index % (kSampleRate / 2) == 0) {
      vTaskDelay(1);
      base = next = esp_timer_get_time();
      index = 0;
    }
    const int64_t lateMicros = esp_timer_get_time() - next;
    if (lateMicros > 10000) {
      if ((uint32_t)(millis() - lastOverrunReportMs) >= 5000UL) {
        lastOverrunReportMs = millis();
        atsDebugLogf("[AUDIO][SAMPLE_OVERRUN] late_us=%lld sample_index=%llu heap_free=%u",
                     static_cast<long long>(lateMicros),
                     static_cast<unsigned long long>(index),
                     static_cast<unsigned>(ESP.getFreeHeap()));
      }
      base = next = esp_timer_get_time();
      index = 0;
    }
  }
}

AudioCaptureService& sharedAudioCapture() {
  static AudioCaptureService capture;
  return capture;
}
