#include "CwDecoderService.h"
#include <math.h>
#include <string.h>

bool CwDecoderService::begin(AudioCaptureService& capture) {
  if (initialized_) return capture_ == &capture;
  mutex_ = xSemaphoreCreateMutex();
  if (!mutex_) return false;
  decoder_.begin(kSampleRate, 700.0f);
  capture_ = &capture;
  initialized_ = true;
  return true;
}

bool CwDecoderService::enter(AudioCaptureService& capture, bool wifiOff) {
  exit();
#if defined(LILYGO_SI473X)
  (void)wifiOff;
  wifiBlocked_ = true;
  active_ = false;
  return false;
#else
  wifiBlocked_ = !wifiOff;
  if (wifiBlocked_) { active_ = false; return false; }
  if (!capture.begin() || !begin(capture)) return false;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  decoder_.reset();
  shownText_[0] = 0;
  wpm_ = 0;
  dcEstimate_ = 2048.0f;
  devSum_ = 0.0f;
  devCount_ = 0;
  xSemaphoreGive(mutex_);

  audioLevel_ = 0;
  audioPresent_ = false;
  toneWindowCount_ = 0;
  autoToneHz_ = 700.0f;
  signal_ = false;
  lastToneMs_ = 0;
  lastUiMs_ = millis();
  active_ = true;
  if (!capture_->subscribe(this, onAudioSamples)) {
    active_ = false;
    return false;
  }
  return true;
#endif
}

void CwDecoderService::exit() {
  active_ = false;
  if (capture_) capture_->unsubscribe(this);
  wifiBlocked_ = false;
}

void CwDecoderService::setTone(float toneHz) {
  if (!mutex_ || !initialized_) return;
  xSemaphoreTake(mutex_, portMAX_DELAY);
  decoder_.setTone(toneHz);
  autoToneHz_ = toneHz;
  xSemaphoreGive(mutex_);
}

void CwDecoderService::resetDecoder() {
  if (!mutex_ || !initialized_) return;
  xSemaphoreTake(mutex_, portMAX_DELAY);
  decoder_.reset();
  shownText_[0] = 0;
  wpm_ = 0;
  xSemaphoreGive(mutex_);
}

void CwDecoderService::clearText() {
  if (!mutex_) return;
  xSemaphoreTake(mutex_, portMAX_DELAY);
  decoder_.clearText();
  shownText_[0] = 0;
  xSemaphoreGive(mutex_);
}

void CwDecoderService::update() {
  if (!active_ || millis() - lastUiMs_ < 100) return;
  lastUiMs_ = millis();
  xSemaphoreTake(mutex_, portMAX_DELAY);
  strncpy(shownText_, decoder_.getText(), sizeof(shownText_) - 1);
  shownText_[sizeof(shownText_) - 1] = 0;
  wpm_ = decoder_.getWpm();
  const bool key = decoder_.isKeyDown();
  const uint8_t level = decoder_.getLevel();
  xSemaphoreGive(mutex_);
  audioPresent_ = audioLevel_ >= 12;
  if (key && level > 20) lastToneMs_ = millis();
  signal_ = lastToneMs_ && millis() - lastToneMs_ < 1200;
}

void CwDecoderService::onAudioSamples(void* context, const int16_t* samples, size_t count, int64_t utcStartMicros) {
  (void)utcStartMicros;
  if (!context) return;
  static_cast<CwDecoderService*>(context)->consumeSamples(samples, count);
}

void CwDecoderService::consumeSamples(const int16_t* samples, size_t count) {
  if (!active_ || !mutex_ || !samples) return;
  if (xSemaphoreTake(mutex_, 0) != pdTRUE) return;

  decoder_.push(samples, static_cast<int>(count));
  for (size_t i = 0; i < count; ++i) {
    toneWindow_[toneWindowCount_++] = samples[i];
    if (toneWindowCount_ >= kToneWindowSamples) {
      float bestHz = autoToneHz_;
      float bestPower = 0.0f;
      float mean = 0.0f;
      for (size_t n = 0; n < kToneWindowSamples; ++n) mean += toneWindow_[n];
      mean /= kToneWindowSamples;
      for (int hz = 400; hz <= 1000; hz += 25) {
        const float coeff = 2.0f * cosf(2.0f * 3.14159265358979323846f * hz / kSampleRate);
        float q1 = 0.0f, q2 = 0.0f;
        for (size_t n = 0; n < kToneWindowSamples; ++n) {
          const float q0 = coeff * q1 - q2 + (static_cast<float>(toneWindow_[n]) - mean);
          q2 = q1; q1 = q0;
        }
        const float power = q1*q1 + q2*q2 - coeff*q1*q2;
        if (power > bestPower) { bestPower = power; bestHz = static_cast<float>(hz); }
      }
      // Only retune when a clear audio tone is present; retain the last estimate in silence.
      if (bestPower > 180000.0f && fabsf(bestHz - autoToneHz_) >= 20.0f) {
        autoToneHz_ = bestHz;
        decoder_.setTone(autoToneHz_);
      }
      toneWindowCount_ = 0;
    }
  }
  for (size_t i = 0; i < count; ++i) {
    const float value = static_cast<float>(samples[i]);
    dcEstimate_ += (value - dcEstimate_) * 0.001f;
    devSum_ += fabsf(value - dcEstimate_);
    if (++devCount_ >= kSampleRate / 2) {
      audioLevel_ = static_cast<uint16_t>(devSum_ / devCount_);
      devSum_ = 0.0f;
      devCount_ = 0;
    }
  }
  xSemaphoreGive(mutex_);
}
