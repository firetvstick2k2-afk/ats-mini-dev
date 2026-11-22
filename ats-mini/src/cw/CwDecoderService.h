#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "CwDecoder.h"
#include "../audio/AudioCaptureService.h"

class CwDecoderService {
public:
  bool begin(AudioCaptureService& capture);
  bool enter(AudioCaptureService& capture, bool wifiOff);
  void exit();
  bool active() const { return active_; }
  bool wifiBlocked() const { return wifiBlocked_; }
  void clearText();
  void setTone(float toneHz);
  void resetDecoder();
  void update();
  const char* text() const { return shownText_; }
  bool signal() const { return signal_; }
  bool audioPresent() const { return audioPresent_; }
  float tone() const { return autoToneHz_; }
  uint16_t audioLevel() const { return audioLevel_; }
  uint16_t audioPeak() const { return audioPeak_; }
  bool audioClipped() const { return audioClipped_; }
  int wpm() const { return wpm_; }

private:
  static void onAudioSamples(void* context, const int16_t* samples, size_t count, int64_t utcStartMicros);
  void consumeSamples(const int16_t* samples, size_t count);

  static constexpr uint32_t kSampleRate = AudioCaptureService::kSampleRate;
  CwDecoder decoder_;
  AudioCaptureService* capture_ = nullptr;
  SemaphoreHandle_t mutex_ = nullptr;
  volatile bool active_ = false;
  bool initialized_ = false, wifiBlocked_ = false, signal_ = false, audioPresent_ = false;
  volatile uint16_t audioLevel_ = 0, audioPeak_ = 0;
  volatile bool audioClipped_ = false;
  float dcEstimate_ = 2048.0f, devSum_ = 0.0f;
  uint32_t devCount_ = 0, clippedCount_ = 0;
  float toneNoiseFloor_ = 0.0f;
  float toneCandidateHz_ = 700.0f;
  uint8_t toneCandidateCount_ = 0;
  char shownText_[CwDecoder::TEXT_MAX + 1] = {};
  int wpm_ = 0;
  static constexpr size_t kToneWindowSamples = 256;
  int16_t toneWindow_[kToneWindowSamples] = {};
  size_t toneWindowCount_ = 0;
  float autoToneHz_ = 700.0f;
  uint32_t lastUiMs_ = 0, lastToneMs_ = 0;
};
