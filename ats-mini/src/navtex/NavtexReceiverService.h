#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "../audio/AudioCaptureService.h"
#include "NavtexBfskDemodulator.h"
#include "NavtexSitorBSync.h"
#include "NavtexSitorBFec.h"
#include "NavtexCcIr476.h"
#include "NavtexMessageParser.h"

class NavtexReceiverService {
public:
  bool enter(bool wifiOff, AudioCaptureService& capture);
  void exit();
  bool active() const { return active_; }
  bool synchronized() const { return sync_.status().locked; }
  uint16_t audioLevel() const { return audioLevel_; }
  uint32_t droppedBlocks() const { return droppedBlocks_; }
  uint32_t bitsDecoded() const { return bitsDecoded_; }
  uint32_t codewordsDecoded() const { return codewordsDecoded_; }
  uint32_t messagesCompleted() const { return messagesCompleted_; }
  uint8_t copyMessages(NavtexMessageParser::Message* out, uint8_t capacity) const;
  bool requestSelfTest();

private:
  struct AudioChunk {
    int16_t samples[AudioCaptureService::kBlockSamples]{};
    uint8_t count = 0;
    bool selfTest = false;
  };

  static void onAudioSamples(void* context, const int16_t* samples,
                             size_t count, int64_t utcStartMicros);
  static void taskEntry(void* arg);
  void workerTask();
  void processChunk(const AudioChunk& chunk);
  void processCodeword(uint8_t code);
  void runSelfTest();
  void processSample(int16_t sample);

  AudioCaptureService* capture_ = nullptr;
  QueueHandle_t queue_ = nullptr;
  SemaphoreHandle_t engineMutex_ = nullptr;
  SemaphoreHandle_t resultMutex_ = nullptr;
  TaskHandle_t task_ = nullptr;
  NavtexBfskDemodulator demod_;
  NavtexSitorBSync sync_;
  NavtexSitorBFec fec_;
  NavtexCcIr476 ccir_;
  NavtexMessageParser parser_;
  volatile bool active_ = false;
  volatile bool selfTestQueued_ = false;
  bool initialized_ = false;
  volatile uint16_t audioLevel_ = 0;
  volatile uint32_t droppedBlocks_ = 0;
  volatile uint32_t bitsDecoded_ = 0;
  volatile uint32_t codewordsDecoded_ = 0;
  volatile uint32_t messagesCompleted_ = 0;
};
