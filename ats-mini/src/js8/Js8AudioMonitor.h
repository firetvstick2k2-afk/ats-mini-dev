#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "../audio/AudioCaptureService.h"

// JS8-owned diagnostics subscriber. It never samples GPIO11 itself and never
// modifies the shared capture stream or another decoder's state.
class Js8AudioMonitor {
public:
  static constexpr size_t kSnapshotSamples = 50560; // one 79-symbol NORMAL frame at 4 kHz

  struct Metrics {
    bool valid = false;
    uint32_t callbackBlocks = 0;
    uint32_t processedBlocks = 0;
    uint32_t queueDrops = 0;
    uint32_t discontinuities = 0;
    uint32_t estimatedMissingBlocks = 0;
    uint32_t samples = 0;
    uint32_t clippedSamples = 0;
    int16_t adcMin = 0;
    int16_t adcMax = 0;
    float adcMean = 0.0f;
    float adcRmsAc = 0.0f;
    float callbackRateHz = 0.0f;
    float meanBlockIntervalUs = 0.0f;
    uint32_t minBlockIntervalUs = 0;
    uint32_t maxBlockIntervalUs = 0;
    uint32_t windowDurationUs = 0;
    bool testFrameComplete = false;
    bool testSyncPass = false;
    uint8_t testMatchedPilots = 0;
    bool testFrameComplete = false;
    bool testSyncPass = false;
    uint8_t testMatchedPilots = 0;
  };

  bool begin(AudioCaptureService& capture);
  void stop();
  bool active() const { return active_; }
  bool testPlaybackActive() const { return testPlayback_; }
  bool startTestPlayback();
  void stopTestPlayback();
  bool copyMetrics(Metrics& out) const;
  // Copies the newest samples in chronological order; returns copied count.
  size_t copyRecentSamples(int16_t* out, size_t capacity) const;

private:
  struct AudioBlock {
    int16_t samples[AudioCaptureService::kBlockSamples];
    int64_t callbackTimeUs = 0; // monotonic time at subscriber entry
    uint8_t count = 0;
    uint32_t sourceGeneration = 0;
  };

  static void onAudioSamples(void* context, const int16_t* samples,
                             size_t count, int64_t utcStartMicros);
  static void taskEntry(void* arg);
  void workerTask();
  void processBlock(const AudioBlock& block);
  void publishWindow(int64_t endTimeUs);
  void resetMeasurementState();
  void makeTestBlock(AudioBlock& block);
  uint32_t currentSourceGeneration() const;
  uint32_t advanceSourceGeneration();

  static constexpr size_t kQueueDepth = 96;
  static constexpr uint32_t kWindowUs = 1000000;
  static constexpr uint32_t kClipLow = 8;
  static constexpr uint32_t kClipHigh = 4087;
  static constexpr uint32_t kTestSymbolSamples = 640; // 160 ms at 4 kHz
  static constexpr uint32_t kTestSymbolCount = 79;

  AudioCaptureService* capture_ = nullptr;
  QueueHandle_t queue_ = nullptr;
  SemaphoreHandle_t sampleMutex_ = nullptr;
  TaskHandle_t task_ = nullptr;
  mutable portMUX_TYPE metricsMux_ = portMUX_INITIALIZER_UNLOCKED;
  Metrics published_{};
  volatile bool active_ = false;
  volatile bool resetRequested_ = false;
  volatile bool testPlayback_ = false;
  uint32_t sourceGeneration_ = 0;
  bool initialized_ = false;

  uint32_t testSampleIndex_ = 0;
  bool testFrameComplete_ = false;
  bool testSyncPass_ = false;
  uint8_t testMatchedPilots_ = 0;
  bool testFrameComplete_ = false;
  bool testSyncPass_ = false;
  uint8_t testMatchedPilots_ = 0;
  float testPhase_ = 0.0f;
  int16_t* sampleRing_ = nullptr;
  size_t sampleWrite_ = 0;
  size_t sampleCount_ = 0;

  uint32_t callbackBlocks_ = 0;
  uint32_t queueDrops_ = 0;
  uint32_t processedBlocks_ = 0;
  uint32_t discontinuities_ = 0;
  uint32_t estimatedMissingBlocks_ = 0;

  int64_t windowStartUs_ = 0;
  int64_t firstCallbackUs_ = 0;
  int64_t lastCallbackUs_ = 0;
  uint64_t intervalSumUs_ = 0;
  uint64_t intervalSamples_ = 0;
  uint32_t intervalCount_ = 0;
  uint32_t minIntervalUs_ = UINT32_MAX;
  uint32_t maxIntervalUs_ = 0;
  uint8_t previousBlockCount_ = 0;
  uint32_t windowSamples_ = 0;
  uint32_t clippedSamples_ = 0;
  int16_t adcMin_ = INT16_MAX;
  int16_t adcMax_ = INT16_MIN;
  int64_t adcSum_ = 0;
  uint64_t adcSquareSum_ = 0;
};

// One monitor instance is owned by the JS8 subsystem; it subscribes to the
// shared capture service. A synthetic 8-FSK source can temporarily replace
// only this subscriber for repeatable DSP-pipeline testing.
