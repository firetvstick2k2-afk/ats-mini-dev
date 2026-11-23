#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "../audio/AudioCaptureService.h"
#include "Psk31Decoder.h"

class Psk31Service {
public:
  bool enter(AudioCaptureService& capture, bool wifiOff);
  void exit();
  bool setRunning(bool running);
  bool active() const { return active_; }
  bool signal() const { return signal_; }
  uint16_t audioLevel() const { return audioLevel_; }
  uint32_t droppedBlocks() const { return droppedBlocks_; }
  void clearText();
  void resetDecoder();
  size_t copyText(char* out, size_t capacity) const;
private:
  struct AudioChunk { int16_t samples[AudioCaptureService::kBlockSamples]{}; uint8_t count = 0; };
  static void onAudioSamples(void* context, const int16_t* samples, size_t count, int64_t utcStartMicros);
  static void taskEntry(void* arg);
  void workerTask();
  void appendText(char value);
  AudioCaptureService* capture_ = nullptr;
  QueueHandle_t queue_ = nullptr;
  SemaphoreHandle_t mutex_ = nullptr;
  TaskHandle_t task_ = nullptr;
  Psk31Decoder decoder_;
  volatile bool active_ = false;
  bool initialized_ = false;
  volatile bool signal_ = false;
  volatile uint16_t audioLevel_ = 0;
  volatile uint32_t droppedBlocks_ = 0;
  char text_[256]{};
  size_t textLength_ = 0;
};
