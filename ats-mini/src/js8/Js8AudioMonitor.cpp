#include "Js8AudioMonitor.h"
#include "Js8NormalSync.h"

#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

namespace {
constexpr uint32_t kNominalBlockUs =
  static_cast<uint32_t>(AudioCaptureService::kBlockSamples) * 1000000UL /
  AudioCaptureService::kSampleRate;
constexpr uint32_t kWorkerStackBytes = 4096;
constexpr UBaseType_t kWorkerPriority = 1;
constexpr BaseType_t kWorkerCore = 1;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTestBaseHz = 1000.0f;
constexpr float kTestToneSpacingHz = 6.25f;
constexpr uint8_t kNormalCostas[7] = {4, 2, 5, 6, 1, 3, 0};
}

bool Js8AudioMonitor::begin(AudioCaptureService& capture) {
#if defined(LILYGO_SI473X)
  (void)capture;
  return false; // GPIO11 is SPI MOSI on this profile; keep the board guard.
#else
  if (!capture.begin()) return false;

  if (initialized_) {
    if (capture_ != &capture) return false;
    if (active_) return true;
  } else {
    queue_ = xQueueCreate(kQueueDepth, sizeof(AudioBlock));
    sampleMutex_ = xSemaphoreCreateMutex();
    // A full NORMAL frame is ~101 KB. Require PSRAM; never consume scarce internal heap.
    sampleRing_ = static_cast<int16_t*>(heap_caps_malloc(
      kSnapshotSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!queue_ || !sampleMutex_ || !sampleRing_) {
      if (queue_) vQueueDelete(queue_);
      if (sampleMutex_) vSemaphoreDelete(sampleMutex_);
      if (sampleRing_) heap_caps_free(sampleRing_);
      queue_ = nullptr;
      sampleMutex_ = nullptr;
      sampleRing_ = nullptr;
      return false;
    }
    capture_ = &capture;
    if (xTaskCreatePinnedToCore(taskEntry, "js8-audio-mon", kWorkerStackBytes,
                                this, kWorkerPriority, &task_, kWorkerCore) != pdPASS) {
      vQueueDelete(queue_);
      vSemaphoreDelete(sampleMutex_);
      heap_caps_free(sampleRing_);
      queue_ = nullptr;
      sampleMutex_ = nullptr;
      sampleRing_ = nullptr;
      task_ = nullptr;
      capture_ = nullptr;
      return false;
    }
    initialized_ = true;
  }

  active_ = false;
  testPlayback_ = false;
  xQueueReset(queue_);
  resetRequested_ = true;
  active_ = true;
  if (!capture.subscribe(this, onAudioSamples)) {
    active_ = false;
    return false;
  }
  return true;
#endif
}

void Js8AudioMonitor::stop() {
  active_ = false;
  testPlayback_ = false;
  advanceSourceGeneration();
  if (capture_) capture_->unsubscribe(this);
  if (queue_) xQueueReset(queue_);
}

bool Js8AudioMonitor::startTestPlayback() {
  if (!initialized_ || !active_ || !capture_ || testPlayback_) return false;
  capture_->unsubscribe(this);
  advanceSourceGeneration(); // invalidate callbacks already snapshotted by the capture task
  if (queue_) xQueueReset(queue_);
  resetRequested_ = true;
  testSampleIndex_ = 0;
  testPhase_ = 0.0f;
  testFrameComplete_ = false;
  testSyncPass_ = false;
  testMatchedPilots_ = 0;
  testPlayback_ = true;
  return true;
}

void Js8AudioMonitor::stopTestPlayback() {
  if (!testPlayback_) return;
  testPlayback_ = false;
  advanceSourceGeneration(); // discard any callback that began during synthetic playback
  if (queue_) xQueueReset(queue_);
  resetRequested_ = true;
  if (active_ && capture_) capture_->subscribe(this, onAudioSamples);
}

bool Js8AudioMonitor::copyMetrics(Metrics& out) const {
  if (!initialized_) return false;
  portENTER_CRITICAL(&metricsMux_);
  out = published_;
  out.callbackBlocks = callbackBlocks_;
  out.queueDrops = queueDrops_;
  portEXIT_CRITICAL(&metricsMux_);
  return out.valid;
}

size_t Js8AudioMonitor::copyRecentSamples(int16_t* out, size_t capacity) const {
  if (!out || !capacity || !sampleMutex_) return 0;
  xSemaphoreTake(sampleMutex_, portMAX_DELAY);
  const size_t count = capacity < sampleCount_ ? capacity : sampleCount_;
  const size_t oldest = (sampleWrite_ + kSnapshotSamples - sampleCount_) % kSnapshotSamples;
  const size_t skip = sampleCount_ - count;
  const size_t start = (oldest + skip) % kSnapshotSamples;
  for (size_t i = 0; i < count; ++i)
    out[i] = sampleRing_[(start + i) % kSnapshotSamples];
  xSemaphoreGive(sampleMutex_);
  return count;
}

void Js8AudioMonitor::onAudioSamples(void* context, const int16_t* samples,
                                     size_t count, int64_t utcStartMicros) {
  (void)utcStartMicros;
  auto* self = static_cast<Js8AudioMonitor*>(context);
  if (!self || !self->active_ || self->testPlayback_ || !self->queue_ ||
      !samples || count == 0 || count > AudioCaptureService::kBlockSamples) return;

  const uint32_t sourceGeneration = self->currentSourceGeneration();
  if (self->testPlayback_) return;

  AudioBlock block{};
  memcpy(block.samples, samples, count * sizeof(int16_t));
  block.count = static_cast<uint8_t>(count);
  block.sourceGeneration = sourceGeneration;
  block.callbackTimeUs = esp_timer_get_time();

  portENTER_CRITICAL(&self->metricsMux_);
  ++self->callbackBlocks_;
  portEXIT_CRITICAL(&self->metricsMux_);

  // A source switch may have happened while this callback copied its block.
  if (self->testPlayback_ || sourceGeneration != self->currentSourceGeneration()) return;
  if (xQueueSend(self->queue_, &block, 0) != pdTRUE) {
    portENTER_CRITICAL(&self->metricsMux_);
    ++self->queueDrops_;
    portEXIT_CRITICAL(&self->metricsMux_);
  }
}

void Js8AudioMonitor::taskEntry(void* arg) {
  static_cast<Js8AudioMonitor*>(arg)->workerTask();
}

void Js8AudioMonitor::makeTestBlock(AudioBlock& block) {
  block.count = AudioCaptureService::kBlockSamples;
  block.sourceGeneration = currentSourceGeneration();
  block.callbackTimeUs = esp_timer_get_time();
  for (size_t i = 0; i < block.count; ++i) {
    if (testSampleIndex_ < kTestSymbolCount * kTestSymbolSamples) {
      const uint32_t symbol = testSampleIndex_ / kTestSymbolSamples;
      // NORMAL Costas pilots plus deterministic arbitrary data tones.
      // This synthetic frame is NOT a CRC/LDPC-valid JS8 message.
      uint32_t tone = (symbol * 5U + (symbol * symbol + 3U) % 7U) & 7U;
      if (symbol < 7U) tone = kNormalCostas[symbol];
      else if (symbol >= 36U && symbol < 43U) tone = kNormalCostas[symbol - 36U];
      else if (symbol >= 72U) tone = kNormalCostas[symbol - 72U];
      const float frequency = kTestBaseHz + tone * kTestToneSpacingHz;
      testPhase_ += 2.0f * kPi * frequency / AudioCaptureService::kSampleRate;
      if (testPhase_ >= 2.0f * kPi) testPhase_ -= 2.0f * kPi;
      block.samples[i] = static_cast<int16_t>(2048.0f + sinf(testPhase_) * 1200.0f);
      ++testSampleIndex_;
    } else {
      block.samples[i] = 0;
    }
  }
}

void Js8AudioMonitor::workerTask() {
  AudioBlock block{};
  TickType_t nextPlaybackTick = xTaskGetTickCount();
  for (;;) {
    if (resetRequested_) {
      resetMeasurementState();
      resetRequested_ = false;
      nextPlaybackTick = xTaskGetTickCount();
    }

    if (testPlayback_ && active_) {
      makeTestBlock(block);
      portENTER_CRITICAL(&metricsMux_);
      ++callbackBlocks_;
      portEXIT_CRITICAL(&metricsMux_);
      processBlock(block);
      if (testSampleIndex_ >= kTestSymbolCount * kTestSymbolSamples) {
        testPlayback_ = false;
        advanceSourceGeneration(); // invalidate any late synthetic/live block before resubscribe
        // Preserve the completed local frame and its sync result for the UI.
        if (active_ && capture_) capture_->subscribe(this, onAudioSamples);
      }
      vTaskDelayUntil(&nextPlaybackTick, pdMS_TO_TICKS(6));
      continue;
    }

    if (xQueueReceive(queue_, &block, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (resetRequested_) {
        resetMeasurementState();
        resetRequested_ = false;
      } else if (active_ && !testPlayback_ &&
                 block.sourceGeneration == currentSourceGeneration()) {
        processBlock(block);
      }
    }
  }
}

uint32_t Js8AudioMonitor::currentSourceGeneration() const {
  portENTER_CRITICAL(&metricsMux_);
  const uint32_t generation = sourceGeneration_;
  portEXIT_CRITICAL(&metricsMux_);
  return generation;
}

uint32_t Js8AudioMonitor::advanceSourceGeneration() {
  portENTER_CRITICAL(&metricsMux_);
  const uint32_t generation = ++sourceGeneration_;
  portEXIT_CRITICAL(&metricsMux_);
  return generation;
}

void Js8AudioMonitor::resetMeasurementState() {
  windowStartUs_ = firstCallbackUs_ = lastCallbackUs_ = 0;
  testFrameComplete_ = testSyncPass_ = false;
  testMatchedPilots_ = 0;
  intervalSumUs_ = intervalSamples_ = 0;
  intervalCount_ = 0;
  minIntervalUs_ = UINT32_MAX;
  maxIntervalUs_ = 0;
  previousBlockCount_ = 0;
  windowSamples_ = clippedSamples_ = 0;
  adcMin_ = INT16_MAX;
  adcMax_ = INT16_MIN;
  adcSum_ = 0;
  adcSquareSum_ = 0;
  portENTER_CRITICAL(&metricsMux_);
  callbackBlocks_ = queueDrops_ = processedBlocks_ = 0;
  discontinuities_ = estimatedMissingBlocks_ = 0;
  published_ = Metrics{};
  portEXIT_CRITICAL(&metricsMux_);
  if (sampleMutex_) {
    xSemaphoreTake(sampleMutex_, portMAX_DELAY);
    sampleWrite_ = sampleCount_ = 0;
    xSemaphoreGive(sampleMutex_);
  }
}

void Js8AudioMonitor::processBlock(const AudioBlock& block) {
  if (!active_ || !block.count) return;

  if (!windowStartUs_) {
    windowStartUs_ = block.callbackTimeUs;
    firstCallbackUs_ = block.callbackTimeUs;
  } else if (lastCallbackUs_) {
    const int64_t delta64 = block.callbackTimeUs - lastCallbackUs_;
    if (delta64 > 0 && delta64 <= UINT32_MAX) {
      const uint32_t delta = static_cast<uint32_t>(delta64);
      intervalSumUs_ += delta;
      intervalSamples_ += previousBlockCount_;
      ++intervalCount_;
      if (delta < minIntervalUs_) minIntervalUs_ = delta;
      if (delta > maxIntervalUs_) maxIntervalUs_ = delta;
      if (delta > (kNominalBlockUs * 3U) / 2U) {
        ++discontinuities_;
        const uint32_t missing = (delta + kNominalBlockUs / 2U) / kNominalBlockUs;
        if (missing > 1) estimatedMissingBlocks_ += missing - 1;
      }
    }
  }
  lastCallbackUs_ = block.callbackTimeUs;
  previousBlockCount_ = block.count;
  ++processedBlocks_;

  for (size_t i = 0; i < block.count; ++i) {
    const int16_t sample = block.samples[i];
    if (sample < adcMin_) adcMin_ = sample;
    if (sample > adcMax_) adcMax_ = sample;
    adcSum_ += sample;
    adcSquareSum_ += static_cast<uint64_t>(static_cast<int64_t>(sample) * sample);
    if (sample <= static_cast<int16_t>(kClipLow) ||
        sample >= static_cast<int16_t>(kClipHigh)) ++clippedSamples_;
    ++windowSamples_;
  }

  xSemaphoreTake(sampleMutex_, portMAX_DELAY);
  for (size_t i = 0; i < block.count && sampleCount_ < kSnapshotSamples; ++i) {
    sampleRing_[sampleWrite_] = block.samples[i];
    sampleWrite_ = (sampleWrite_ + 1) % kSnapshotSamples;
    ++sampleCount_;
  }
  if (testPlayback_ && !testFrameComplete_ && sampleCount_ == kSnapshotSamples) {
    Js8NormalSync::Result sync{};
    testSyncPass_ = Js8NormalSync::evaluate(sampleRing_, sampleCount_, 0,
                                            kTestBaseHz, 0.0f, sync);
    testMatchedPilots_ = sync.matchedPilots;
    testFrameComplete_ = true;
  }
  xSemaphoreGive(sampleMutex_);

  if (block.callbackTimeUs - windowStartUs_ >= kWindowUs)
    publishWindow(block.callbackTimeUs);
}

void Js8AudioMonitor::publishWindow(int64_t endTimeUs) {
  if (!windowSamples_) return;
  const double mean = static_cast<double>(adcSum_) / windowSamples_;
  double variance = static_cast<double>(adcSquareSum_) / windowSamples_ - mean * mean;
  if (variance < 0.0) variance = 0.0;

  Metrics next{};
  next.valid = true;
  next.processedBlocks = processedBlocks_;
  next.samples = windowSamples_;
  next.clippedSamples = clippedSamples_;
  next.adcMin = adcMin_;
  next.adcMax = adcMax_;
  next.adcMean = static_cast<float>(mean);
  next.adcRmsAc = static_cast<float>(sqrt(variance));
  next.windowDurationUs = static_cast<uint32_t>(endTimeUs - windowStartUs_);
  next.testFrameComplete = testFrameComplete_;
  next.testSyncPass = testSyncPass_;
  next.testMatchedPilots = testMatchedPilots_;
  next.meanBlockIntervalUs = intervalCount_
    ? static_cast<float>(intervalSumUs_) / intervalCount_ : 0.0f;
  next.minBlockIntervalUs = intervalCount_ ? minIntervalUs_ : 0;
  next.maxBlockIntervalUs = maxIntervalUs_;
  const int64_t elapsed = lastCallbackUs_ - firstCallbackUs_;
  next.callbackRateHz = elapsed > 0
    ? static_cast<float>(static_cast<double>(intervalSamples_) * 1000000.0 / elapsed)
    : 0.0f;

  portENTER_CRITICAL(&metricsMux_);
  next.callbackBlocks = callbackBlocks_;
  next.queueDrops = queueDrops_;
  next.discontinuities = discontinuities_;
  next.estimatedMissingBlocks = estimatedMissingBlocks_;
  published_ = next;
  portEXIT_CRITICAL(&metricsMux_);

  windowStartUs_ = endTimeUs;
  firstCallbackUs_ = lastCallbackUs_;
  intervalSumUs_ = intervalSamples_ = 0;
  intervalCount_ = 0;
  minIntervalUs_ = UINT32_MAX;
  maxIntervalUs_ = 0;
  windowSamples_ = clippedSamples_ = 0;
  adcMin_ = INT16_MAX;
  adcMax_ = INT16_MIN;
  adcSum_ = 0;
  adcSquareSum_ = 0;
}
