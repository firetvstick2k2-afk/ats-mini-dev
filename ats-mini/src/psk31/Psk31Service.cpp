#include "Psk31Service.h"
#include <math.h>
#include <string.h>

namespace {
constexpr UBaseType_t kQueueDepth = 48;
constexpr uint32_t kWorkerStack = 4096;
constexpr UBaseType_t kWorkerPriority = 2;
constexpr BaseType_t kWorkerCore = 1;
}

bool Psk31Service::enter(AudioCaptureService& capture, bool wifiOff) {
  exit();
#if defined(LILYGO_SI473X)
  (void)capture; (void)wifiOff;
  return false;
#else
  if (!wifiOff || !capture.begin()) return false;
  if (!initialized_) {
    queue_ = xQueueCreate(kQueueDepth, sizeof(AudioChunk));
    mutex_ = xSemaphoreCreateMutex();
    if (!queue_ || !mutex_) {
      if (queue_) vQueueDelete(queue_);
      if (mutex_) vSemaphoreDelete(mutex_);
      queue_ = nullptr; mutex_ = nullptr;
      return false;
    }
    if (xTaskCreatePinnedToCore(taskEntry, "psk31-worker", kWorkerStack, this,
                                kWorkerPriority, &task_, kWorkerCore) != pdPASS) {
      vQueueDelete(queue_); vSemaphoreDelete(mutex_);
      queue_ = nullptr; mutex_ = nullptr; task_ = nullptr;
      return false;
    }
    initialized_ = true;
  }
  xQueueReset(queue_);
  xSemaphoreTake(mutex_, portMAX_DELAY);
  decoder_.reset();
  text_[0] = 0; textLength_ = 0;
  xSemaphoreGive(mutex_);
  audioLevel_ = 0; signal_ = false; droppedBlocks_ = 0;
  capture_ = &capture;
  active_ = true;
  if (!capture_->subscribe(this, onAudioSamples)) {
    active_ = false; capture_ = nullptr;
    return false;
  }
  return true;
#endif
}

void Psk31Service::exit() {
  active_ = false;
  if (capture_) capture_->unsubscribe(this);
  capture_ = nullptr;
  if (queue_) xQueueReset(queue_);
  signal_ = false;
}

bool Psk31Service::setRunning(bool running) {
  if (!capture_ || !queue_ || !mutex_) return false;
  if (running == active_) return true;
  if (!running) {
    active_ = false;
    capture_->unsubscribe(this);
    if (queue_) xQueueReset(queue_);
    signal_ = false;
    return true;
  }
  xQueueReset(queue_);
  xSemaphoreTake(mutex_, portMAX_DELAY);
  decoder_.reset();
  xSemaphoreGive(mutex_);
  if (!capture_->subscribe(this, onAudioSamples)) return false;
  active_ = true;
  return true;
}

void Psk31Service::onAudioSamples(void* context, const int16_t* samples,
                                  size_t count, int64_t utcStartMicros) {
  (void)utcStartMicros;
  auto* self = static_cast<Psk31Service*>(context);
  if (!self || !self->active_ || !self->queue_ || !samples || count == 0 ||
      count > AudioCaptureService::kBlockSamples) return;
  AudioChunk chunk{};
  memcpy(chunk.samples, samples, count * sizeof(int16_t));
  chunk.count = static_cast<uint8_t>(count);
  int64_t sum = 0;
  for (size_t i = 0; i < count; ++i) {
    const int32_t centered = static_cast<int32_t>(samples[i]) - 2048;
    sum += static_cast<int64_t>(centered) * centered;
  }
  self->audioLevel_ = static_cast<uint16_t>(sqrtf(static_cast<float>(sum / count)));
  if (xQueueSend(self->queue_, &chunk, 0) != pdTRUE) ++self->droppedBlocks_;
}

void Psk31Service::taskEntry(void* arg) { static_cast<Psk31Service*>(arg)->workerTask(); }

void Psk31Service::workerTask() {
  AudioChunk chunk{};
  for (;;) {
    if (xQueueReceive(queue_, &chunk, portMAX_DELAY) != pdTRUE) continue;
    if (!active_ || !mutex_) continue;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (active_) {
      for (uint8_t i = 0; i < chunk.count; ++i) {
        char decoded = 0;
        if (decoder_.pushSample(chunk.samples[i], decoded)) appendText(decoded);
      }
      signal_ = decoder_.signal();
    }
    xSemaphoreGive(mutex_);
  }
}

void Psk31Service::appendText(char value) {
  if (value == '\r') return;
  if (textLength_ >= sizeof(text_) - 1) {
    const size_t keep = sizeof(text_) / 2;
    memmove(text_, text_ + textLength_ - keep, keep);
    textLength_ = keep;
  }
  text_[textLength_++] = value;
  text_[textLength_] = 0;
}

void Psk31Service::clearText() {
  if (!mutex_) return;
  xSemaphoreTake(mutex_, portMAX_DELAY);
  text_[0] = 0; textLength_ = 0;
  xSemaphoreGive(mutex_);
}

void Psk31Service::resetDecoder() {
  if (!mutex_) return;
  xSemaphoreTake(mutex_, portMAX_DELAY);
  decoder_.reset();
  xSemaphoreGive(mutex_);
}

size_t Psk31Service::copyText(char* out, size_t capacity) const {
  if (!out || capacity == 0 || !mutex_) return 0;
  xSemaphoreTake(mutex_, portMAX_DELAY);
  size_t count = textLength_ < capacity - 1 ? textLength_ : capacity - 1;
  memcpy(out, text_, count);
  out[count] = 0;
  xSemaphoreGive(mutex_);
  return count;
}
