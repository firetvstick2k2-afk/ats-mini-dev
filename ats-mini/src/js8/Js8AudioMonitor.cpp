#include "Js8AudioMonitor.h"

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
}

bool Js8AudioMonitor::begin(AudioCaptureService& capture) {
#if defined(LILYGO_SI473X)
  (void)capture;
  return false; // GPIO11 is SPI MOSI on this profile; keep the board guard.
#else
  if (initialized_) {
    if (capture_ != &capture) return false;
    if (active_) return true;
  } else {
    queue_ = xQueueCreate(kQueueDepth, sizeof(AudioBlock));
    sampleMutex_ = xSemaphoreCreateMutex();
    sampleRing_ = static_cast<int16_t*>(heap_caps_malloc(
      kSnapshotSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!sampleRing_)
      sampleRing_ = static_cast<int16_t*>(heap_caps_malloc(
        kSnapshotSamples * sizeof(int16_t), MALLOC_CAP_8BIT));
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

  if (!capture.begin()) return false;
  active_ = false;
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
  if (capture_) capture_->unsubscribe(this);
  if (queue_) xQueueReset(queue_);
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
  (void)utcStartMicros; // Wall-clock timestamps are not used for cadence analysis.
  auto* self = static_cast<Js8AudioMonitor*>(context);
  if (!self || !self->active_ || !self->queue_ || !samples ||
      count == 0 || count > AudioCaptureService::kBlockSamples) return;

  AudioBlock block{};
  memcpy(block.samples, samples, count * sizeof(int16_t));
  block.count = static_cast<uint8_t>(count);
  block.callbackTimeUs = esp_timer_get_time();

  portENTER_CRITICAL(&self->metricsMux_);
  ++self->callbackBlocks_;
  portEXIT_CRITICAL(&self->metricsMux_);

  if (xQueueSend(self->queue_, &block, 0) != pdTRUE) {
    portENTER_CRITICAL(&self->metricsMux_);
    ++self->queueDrops_;
    portEXIT_CRITICAL(&self->metricsMux_);
  }
}

void Js8AudioMonitor::taskEntry(void* arg) {
  static_cast<Js8AudioMonitor*>(arg)->workerTask();
}

void Js8AudioMonitor::workerTask() {
  AudioBlock block{};
  for (;;) {
    if (resetRequested_) {
      resetMeasurementState();
      resetRequested_ = false;
    }
    if (xQueueReceive(queue_, &block, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (resetRequested_) {
        resetMeasurementState();
        resetRequested_ = false;
      } else if (active_) {
        processBlock(block);
      }
    }
  }
}

void Js8AudioMonitor::resetMeasurementState() {
  windowStartUs_ = firstCallbackUs_ = lastCallbackUs_ = 0;
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
  for (size_t i = 0; i < block.count; ++i) {
    sampleRing_[sampleWrite_] = block.samples[i];
    sampleWrite_ = (sampleWrite_ + 1) % kSnapshotSamples;
    if (sampleCount_ < kSnapshotSamples) ++sampleCount_;
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
  next.meanBlockIntervalUs = intervalCount_
    ? static_cast<float>(intervalSumUs_) / intervalCount_ : 0.0f;
  next.minBlockIntervalUs = intervalCount_ ? minIntervalUs_ : 0;
  next.maxBlockIntervalUs = maxIntervalUs_;
  const int64_t elapsed = lastCallbackUs_ - firstCallbackUs_;
  next.callbackRateHz = elapsed > 0
    ? static_cast<float>(static_cast<double>(intervalSamples_) * 1000000.0 / elapsed)
    : 0.0f;

  portENTER_CRITICAL(&metricsMux_);
  const uint32_t callbacks = callbackBlocks_;
  const uint32_t drops = queueDrops_;
  const uint32_t discontinuities = discontinuities_;
  const uint32_t missing = estimatedMissingBlocks_;
  next.callbackBlocks = callbacks;
  next.queueDrops = drops;
  next.discontinuities = discontinuities;
  next.estimatedMissingBlocks = missing;
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
