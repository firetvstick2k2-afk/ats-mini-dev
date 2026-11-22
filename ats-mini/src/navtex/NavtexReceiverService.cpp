#include "NavtexReceiverService.h"

#include <string.h>
#include <math.h>

namespace {
constexpr size_t kQueueDepth = 64;
constexpr uint32_t kWorkerStackBytes = 6144;
constexpr UBaseType_t kWorkerPriority = 2;
constexpr BaseType_t kWorkerCore = 1;
}

bool NavtexReceiverService::enter(bool wifiOff, AudioCaptureService& capture) {
#if defined(LILYGO_SI473X)
  (void)wifiOff; (void)capture;
  return false;
#else
  exit();
  if (!wifiOff || !capture.begin()) return false;

  if (!initialized_) {
    queue_ = xQueueCreate(kQueueDepth, sizeof(AudioChunk));
    engineMutex_ = xSemaphoreCreateMutex();
    resultMutex_ = xSemaphoreCreateMutex();
    if (!queue_ || !engineMutex_ || !resultMutex_ ||
        xTaskCreatePinnedToCore(taskEntry, "navtex-worker", kWorkerStackBytes,
                                this, kWorkerPriority, &task_, kWorkerCore) != pdPASS) {
      if (queue_) vQueueDelete(queue_);
      if (engineMutex_) vSemaphoreDelete(engineMutex_);
      if (resultMutex_) vSemaphoreDelete(resultMutex_);
      queue_ = nullptr;
      engineMutex_ = resultMutex_ = nullptr;
      task_ = nullptr;
      return false;
    }
    initialized_ = true;
  }

  xSemaphoreTake(engineMutex_, portMAX_DELAY);
  demod_.reset();
  sync_.reset();
  fec_.reset();
  ccir_.reset();
  xSemaphoreTake(resultMutex_, portMAX_DELAY);
  parser_.reset();
  xSemaphoreGive(resultMutex_);
  bitsDecoded_ = codewordsDecoded_ = messagesCompleted_ = droppedBlocks_ = 0;
  audioLevel_ = 0;
  xSemaphoreGive(engineMutex_);
  xQueueReset(queue_);

  capture_ = &capture;
  active_ = true;
  if (!capture_->subscribe(this, onAudioSamples)) {
    active_ = false;
    return false;
  }
  return true;
#endif
}

void NavtexReceiverService::exit() {
  active_ = false;
  if (capture_) capture_->unsubscribe(this);
  if (queue_) xQueueReset(queue_);
}

void NavtexReceiverService::onAudioSamples(void* context, const int16_t* samples,
                                           size_t count, int64_t utcStartMicros) {
  (void)utcStartMicros;
  auto* self = static_cast<NavtexReceiverService*>(context);
  if (!self || !self->active_ || !self->queue_ || !samples ||
      count == 0 || count > AudioCaptureService::kBlockSamples) return;

  AudioChunk chunk{};
  memcpy(chunk.samples, samples, count * sizeof(int16_t));
  chunk.count = static_cast<uint8_t>(count);

  int64_t sum = 0;
  for (size_t i = 0; i < count; ++i) sum += samples[i];
  const int32_t mean = static_cast<int32_t>(sum / static_cast<int64_t>(count));
  uint64_t squareSum = 0;
  for (size_t i = 0; i < count; ++i) {
    const int32_t centered = static_cast<int32_t>(samples[i]) - mean;
    squareSum += static_cast<uint64_t>(static_cast<int64_t>(centered) * centered);
  }
  self->audioLevel_ = static_cast<uint16_t>(sqrtf(static_cast<float>(squareSum / count)));
  if (xQueueSend(self->queue_, &chunk, 0) != pdTRUE) ++self->droppedBlocks_;
}

void NavtexReceiverService::taskEntry(void* arg) {
  static_cast<NavtexReceiverService*>(arg)->workerTask();
}

void NavtexReceiverService::workerTask() {
  AudioChunk chunk{};
  for (;;) {
    if (xQueueReceive(queue_, &chunk, portMAX_DELAY) != pdTRUE) continue;
    if (!active_) continue;
    xSemaphoreTake(engineMutex_, portMAX_DELAY);
    if (active_) processChunk(chunk);
    xSemaphoreGive(engineMutex_);
  }
}

void NavtexReceiverService::processChunk(const AudioChunk& chunk) {
  for (uint8_t i = 0; i < chunk.count; ++i) {
    bool bit = false;
    if (!demod_.pushSample(chunk.samples[i], bit)) continue;
    ++bitsDecoded_;

    uint8_t codeword = 0;
    if (!sync_.pushBit(bit, codeword)) continue;
    ++codewordsDecoded_;
    processCodeword(codeword);
  }
}

void NavtexReceiverService::processCodeword(uint8_t code) {
  if (!sync_.status().locked) return;

  const uint32_t slot = fec_.slotsProcessed();
  const bool isDx = sync_.isDxSlot(slot);
  uint8_t recovered = 0;
  NavtexCcIr476::CopySource source = NavtexCcIr476::CopySource::None;
  const NavtexSitorBFec::Result result = fec_.push(code, isDx, recovered, source);
  if (result != NavtexSitorBFec::Result::Recovered) return;

  char output = 0;
  if (!ccir_.decode(recovered, output)) return;
  xSemaphoreTake(resultMutex_, portMAX_DELAY);
  parser_.push(output);
  messagesCompleted_ = parser_.completedCount();
  xSemaphoreGive(resultMutex_);
}

uint8_t NavtexReceiverService::copyMessages(NavtexMessageParser::Message* out,
                                             uint8_t capacity) const {
  if (!out || capacity == 0 || !resultMutex_) return 0;
  xSemaphoreTake(resultMutex_, portMAX_DELAY);
  uint8_t count = capacity < NavtexMessageParser::kMaxMessages
    ? capacity : NavtexMessageParser::kMaxMessages;
  uint8_t available = parser_.count();
  if (count > available) count = available;
  for (uint8_t i = 0; i < count; ++i) parser_.get(i, out[i]);
  xSemaphoreGive(resultMutex_);
  return count;
}
