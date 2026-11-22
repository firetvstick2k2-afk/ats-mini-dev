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
  devPeak_ = 0.0f;
  devCount_ = 0;
  xSemaphoreGive(mutex_);

  audioLevel_ = 0;
  audioPeak_ = 0;
  audioClipped_ = false;
  clippedCount_ = 0;
  audioPresent_ = false;
  toneWindowCount_ = 0;
  autoToneHz_ = 700.0f;
  toneCandidateHz_ = autoToneHz_;
  toneCandidateCount_ = 0;
  toneNoiseFloor_ = 0.0f;
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
      float mean = 0.0f;
      for (size_t n = 0; n < kToneWindowSamples; ++n) mean += toneWindow_[n];
      mean /= static_cast<float>(kToneWindowSamples);

      float energy = 0.0f;
      for (size_t n = 0; n < kToneWindowSamples; ++n) {
        const float centered = static_cast<float>(toneWindow_[n]) - mean;
        energy += centered * centered;
      }
      const float rms = sqrtf(energy / static_cast<float>(kToneWindowSamples));
      float bestPower = 0.0f, secondPower = 0.0f;
      float bestHz = autoToneHz_;
      for (int hz = 400; hz <= 1000; hz += 10) {
        const float coeff = 2.0f * cosf(2.0f * 3.14159265358979323846f * hz / kSampleRate);
        float q1 = 0.0f, q2 = 0.0f;
        for (size_t n = 0; n < kToneWindowSamples; ++n) {
          const float q0 = coeff * q1 - q2 + (static_cast<float>(toneWindow_[n]) - mean);
          q2 = q1; q1 = q0;
        }
        const float power = fmaxf(q1*q1 + q2*q2 - coeff*q1*q2, 0.0f);
        if (power > bestPower) {
          secondPower = bestPower;
          bestPower = power;
          bestHz = static_cast<float>(hz);
        } else if (power > secondPower) {
          secondPower = power;
        }
      }

      // Learn the local noise floor only when no narrow-band tone is dominant.
      const float coherence = energy > 1.0f
        ? bestPower / (energy * static_cast<float>(kToneWindowSamples))
        : 0.0f;
      const bool narrowTone = rms >= 8.0f &&
        coherence > 0.08f && bestPower > secondPower * 1.05f;
      if (!narrowTone) {
        if (toneNoiseFloor_ == 0.0f) toneNoiseFloor_ = rms;
        else toneNoiseFloor_ += (rms - toneNoiseFloor_) * (rms < toneNoiseFloor_ ? 0.08f : 0.01f);
        toneCandidateCount_ = 0;
      } else if (rms > fmaxf(8.0f, toneNoiseFloor_ * 1.8f)) {
        if (fabsf(bestHz - toneCandidateHz_) <= 15.0f) {
          if (toneCandidateCount_ < 5) ++toneCandidateCount_;
        } else {
          toneCandidateHz_ = bestHz;
          toneCandidateCount_ = 1;
        }
        // Three consecutive windows reject brief clicks and adjacent-bin jumps.
        if (toneCandidateCount_ >= 3 && fabsf(bestHz - autoToneHz_) >= 20.0f) {
          autoToneHz_ = bestHz;
          decoder_.setTone(autoToneHz_);
          toneCandidateCount_ = 0;
        }
      } else {
        toneCandidateCount_ = 0;
      }
      toneWindowCount_ = 0;
    }
  }
  for (size_t i = 0; i < count; ++i) {
    const float value = static_cast<float>(samples[i]);
    dcEstimate_ += (value - dcEstimate_) * 0.001f;
    const float deviation = fabsf(value - dcEstimate_);
    devSum_ += deviation;
    if (deviation > devPeak_) devPeak_ = deviation;
    if (samples[i] <= 4 || samples[i] >= 4091) ++clippedCount_;
    if (++devCount_ >= kSampleRate / 2) {
      audioLevel_ = static_cast<uint16_t>(devSum_ / devCount_);
      audioPeak_ = static_cast<uint16_t>(devPeak_ > 65535.0f ? 65535.0f : devPeak_);
      audioClipped_ = clippedCount_ >= 4;
      devSum_ = 0.0f;
      devPeak_ = 0.0f;
      devCount_ = 0;
      clippedCount_ = 0;
    }
  }
  xSemaphoreGive(mutex_);
}
