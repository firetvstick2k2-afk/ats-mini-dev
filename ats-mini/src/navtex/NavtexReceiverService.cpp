#include "NavtexReceiverService.h"

#include <string.h>
#include <math.h>
#include <stdint.h>

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
  Serial.printf("[NAVTEX] ENTER begin wifiOff=%d initialized=%d heap=%u stack=%u\n",
                wifiOff ? 1 : 0, initialized_ ? 1 : 0,
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  exit();
  Serial.println("[NAVTEX] ENTER after exit");
  if (!wifiOff) {
    Serial.println("[NAVTEX] ENTER abort: wifiOff flag false");
    return false;
  }
  Serial.println("[NAVTEX] AUDIO begin");
  const bool audioReady = capture.begin();
  Serial.printf("[NAVTEX] AUDIO done result=%d heap=%u stack=%u\n",
                audioReady ? 1 : 0, static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  if (!audioReady) return false;

  if (!initialized_) {
    Serial.println("[NAVTEX] QUEUE create");
    queue_ = xQueueCreate(kQueueDepth, sizeof(AudioChunk));
    Serial.printf("[NAVTEX] QUEUE result=%p heap=%u\n",
                  static_cast<void*>(queue_), static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.println("[NAVTEX] MUTEX create engine");
    engineMutex_ = xSemaphoreCreateMutex();
    Serial.printf("[NAVTEX] MUTEX engine=%p\n", static_cast<void*>(engineMutex_));
    Serial.println("[NAVTEX] MUTEX create result");
    resultMutex_ = xSemaphoreCreateMutex();
    Serial.printf("[NAVTEX] MUTEX result=%p\n", static_cast<void*>(resultMutex_));
    if (!queue_ || !engineMutex_ || !resultMutex_) {
      Serial.println("[NAVTEX] TASK CREATE skipped: allocation failed");
      if (queue_) vQueueDelete(queue_);
      if (engineMutex_) vSemaphoreDelete(engineMutex_);
      if (resultMutex_) vSemaphoreDelete(resultMutex_);
      queue_ = nullptr;
      engineMutex_ = resultMutex_ = nullptr;
      task_ = nullptr;
      return false;
    }
    Serial.printf("[NAVTEX] WORKER CREATE stack=%u priority=%u core=%d\n",
                  static_cast<unsigned>(kWorkerStackBytes),
                  static_cast<unsigned>(kWorkerPriority), static_cast<int>(kWorkerCore));
    const BaseType_t taskResult = xTaskCreatePinnedToCore(
        taskEntry, "navtex-worker", kWorkerStackBytes,
        this, kWorkerPriority, &task_, kWorkerCore);
    Serial.printf("[NAVTEX] WORKER CREATE result=%ld handle=%p heap=%u\n",
                  static_cast<long>(taskResult), static_cast<void*>(task_),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    if (taskResult != pdPASS) {
      vQueueDelete(queue_);
      vSemaphoreDelete(engineMutex_);
      vSemaphoreDelete(resultMutex_);
      queue_ = nullptr;
      engineMutex_ = resultMutex_ = nullptr;
      task_ = nullptr;
      return false;
    }
    initialized_ = true;
    Serial.println("[NAVTEX] WORKER CREATE done");
  }

  Serial.println("[NAVTEX] ENGINE LOCK begin");
  xSemaphoreTake(engineMutex_, portMAX_DELAY);
  Serial.println("[NAVTEX] ENGINE LOCK acquired");
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
  Serial.println("[NAVTEX] AUDIO SUBSCRIBE begin");
  if (!capture_->subscribe(this, onAudioSamples)) {
    active_ = false;
    Serial.println("[NAVTEX] AUDIO SUBSCRIBE failed");
    return false;
  }
  Serial.printf("[NAVTEX] AUDIO SUBSCRIBE done heap=%u stack=%u\n",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
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
  if (!self || !self->active_ || self->selfTestQueued_ || !self->queue_ || !samples ||
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
    if (active_) {
      if (chunk.selfTest) {
        runSelfTest();
        selfTestQueued_ = false;
      } else processChunk(chunk);
    }
    xSemaphoreGive(engineMutex_);
  }
}

void NavtexReceiverService::processSample(int16_t sample) {
  bool bit = false;
  if (!demod_.pushSample(sample, bit)) return;
  ++bitsDecoded_;
  uint8_t codeword = 0;
  if (!sync_.pushBit(bit, codeword)) return;
  ++codewordsDecoded_;
  processCodeword(codeword);
}

void NavtexReceiverService::processChunk(const AudioChunk& chunk) {
  for (uint8_t i = 0; i < chunk.count; ++i) processSample(chunk.samples[i]);
}

bool NavtexReceiverService::requestSelfTest() {
  if (!active_ || !queue_) return false;
  if (selfTestQueued_) return false;
  selfTestQueued_ = true;
  AudioChunk request{};
  request.selfTest = true;
  if (xQueueSend(queue_, &request, 0) == pdTRUE) return true;
  selfTestQueued_ = false;
  return false;
}

void NavtexReceiverService::runSelfTest() {
  // Complete, known NAVTEX frame. The generated samples travel through the
  // same 4 kHz demodulator, phasing detector, FEC combiner and parser as RF audio.
  static const char frame[] = "ZCZC CA23\r\nNAVAREA WARNING\r\nNNNN";
  uint8_t codes[128]{};
  size_t codeCount = 0;
  bool figures = false;
  for (size_t i = 0; frame[i] && codeCount < sizeof(codes) - 2; ++i) {
    const char ch = frame[i];
    uint8_t code = 0;
    if (NavtexCcIr476::encode(ch, figures, code)) {
      codes[codeCount++] = code;
      continue;
    }
    if (!figures && NavtexCcIr476::encode(ch, true, code)) {
      codes[codeCount++] = NavtexCcIr476::kFigures;
      codes[codeCount++] = code;
      figures = true;
      continue;
    }
    if (figures && NavtexCcIr476::encode(ch, false, code)) {
      codes[codeCount++] = NavtexCcIr476::kLetters;
      codes[codeCount++] = code;
      figures = false;
      continue;
    }
    // Unknown glyphs are skipped rather than injecting an invalid codeword.
  }

  demod_.reset(); sync_.reset(); fec_.reset(); ccir_.reset();
  const size_t slotCount = codeCount * 2U + NavtexSitorBFec::kDiversitySlots;
  uint8_t slots[256]{};
  if (slotCount > sizeof(slots)) return;
  for (size_t slot = 0; slot < slotCount; ++slot)
    slots[slot] = (slot & 1U) ? NavtexCcIr476::kAlpha : NavtexCcIr476::kRep;
  for (size_t i = 0; i < codeCount; ++i) {
    slots[2U * i] = codes[i];
    slots[2U * i + NavtexSitorBFec::kDiversitySlots] = codes[i];
  }

  // Twelve alternating phasing characters precede the data slots.
  float phase = 0.0f;
  constexpr float kPi2 = 6.28318530718f;
  constexpr float kAmplitude = 520.0f;
  for (uint8_t word = 0; word < NavtexSitorBSync::kPhasingWords; ++word) {
    const uint8_t phasing = (word & 1U) ? NavtexCcIr476::kAlpha : NavtexCcIr476::kRep;
    for (uint8_t bitIndex = 0; bitIndex < 7; ++bitIndex) {
      const bool mark = ((phasing >> bitIndex) & 1U) != 0;
      const float step = kPi2 * (mark ? 1615.0f : 1785.0f) / 4000.0f;
      for (uint8_t sample = 0; sample < 40; ++sample) {
        const int16_t audio = static_cast<int16_t>(2048.0f + kAmplitude * sinf(phase));
        processSample(audio);
        phase += step;
        if (phase >= kPi2) phase -= kPi2;
      }
    }
  }
  for (size_t slot = 0; slot < slotCount; ++slot) {
    const uint8_t code = slots[slot];
    for (uint8_t bitIndex = 0; bitIndex < 7; ++bitIndex) {
      const bool mark = ((code >> bitIndex) & 1U) != 0;
      const float step = kPi2 * (mark ? 1615.0f : 1785.0f) / 4000.0f;
      for (uint8_t sample = 0; sample < 40; ++sample) {
        const int16_t audio = static_cast<int16_t>(2048.0f + kAmplitude * sinf(phase));
        processSample(audio);
        phase += step;
        if (phase >= kPi2) phase -= kPi2;
      }
    }
  }

  // One trailing bit flushes the final integrate-and-dump decision because
  // the streaming demodulator emits decisions on the next bit-clock edge.
  const float flushStep = kPi2 * 1615.0f / 4000.0f;
  for (uint8_t sample = 0; sample < 40; ++sample) {
    const int16_t audio = static_cast<int16_t>(2048.0f + kAmplitude * sinf(phase));
    processSample(audio);
    phase += flushStep;
    if (phase >= kPi2) phase -= kPi2;
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
