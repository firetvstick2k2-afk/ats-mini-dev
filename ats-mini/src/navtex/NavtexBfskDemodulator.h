#pragma once

#include <Arduino.h>
#include <math.h>
#include <string.h>

// NAVTEX/SITOR-B 2-FSK audio demodulator.
//
// Input: raw ATS Mini ADC samples (0..4095) from AudioCaptureService at 4 kHz.
// Output: one hard bit decision at the recovered 100-baud bit clock.
// NAVTEX audio defaults: MARK=1615 Hz (bit 1), SPACE=1785 Hz (bit 0).
//
// A sliding quadrature correlator measures both tones over 20 ms. A simple
// transition-assisted timing loop tracks the 10 ms bit period. The output
// bit stream is intended for NavtexSitorBSync::pushBit().
class NavtexBfskDemodulator {
public:
  static constexpr uint32_t kSampleRate = 4000;
  static constexpr float kBaud = 100.0f;
  static constexpr uint8_t kWindowSamples = 40; // 10 ms (one complete NAVTEX bit)
  static constexpr float kDefaultMarkHz = 1615.0f;
  static constexpr float kDefaultSpaceHz = 1785.0f;

  struct Metrics {
    float markEnergy = 0.0f;
    float spaceEnergy = 0.0f;
    float confidence = 0.0f;
    float timingErrorSamples = 0.0f;
    uint32_t bitsProduced = 0;
  };

  NavtexBfskDemodulator() { configure(kDefaultMarkHz, kDefaultSpaceHz); }

  void configure(float markHz, float spaceHz) {
    if (markHz < 300.0f || markHz > 1900.0f ||
        spaceHz < 300.0f || spaceHz > 1900.0f ||
        fabsf(markHz - spaceHz) < 40.0f) return;

    markHz_ = markHz;
    spaceHz_ = spaceHz;
    const float markStep = 2.0f * static_cast<float>(M_PI) * markHz_ / kSampleRate;
    const float spaceStep = 2.0f * static_cast<float>(M_PI) * spaceHz_ / kSampleRate;
    markCosStep_ = cosf(markStep);
    markSinStep_ = sinf(markStep);
    spaceCosStep_ = cosf(spaceStep);
    spaceSinStep_ = sinf(spaceStep);
    reset();
  }

  void reset() {
    memset(markRingI_, 0, sizeof(markRingI_));
    memset(markRingQ_, 0, sizeof(markRingQ_));
    memset(spaceRingI_, 0, sizeof(spaceRingI_));
    memset(spaceRingQ_, 0, sizeof(spaceRingQ_));
    markSumI_ = markSumQ_ = spaceSumI_ = spaceSumQ_ = 0.0f;
    markOscI_ = spaceOscI_ = 1.0f;
    markOscQ_ = spaceOscQ_ = 0.0f;
    dc_ = 2048.0f;
    ringPos_ = 0;
    samplesSeen_ = 0;
    clockPhase_ = 0.0f;
    clockStarted_ = false;
    lastSign_ = 0;
    haveSign_ = false;
    timingError_ = 0.0f;
    metrics_ = Metrics{};
  }

  // Returns true exactly when a new 100-baud bit is available.
  bool pushSample(int16_t rawAdc, bool& bit) {
    dc_ += (static_cast<float>(rawAdc) - dc_) * 0.0005f;
    const float sample = static_cast<float>(rawAdc) - dc_;

    const float markI = sample * markOscI_;
    const float markQ = sample * markOscQ_;
    const float spaceI = sample * spaceOscI_;
    const float spaceQ = sample * spaceOscQ_;

    markSumI_ += markI - markRingI_[ringPos_];
    markSumQ_ += markQ - markRingQ_[ringPos_];
    spaceSumI_ += spaceI - spaceRingI_[ringPos_];
    spaceSumQ_ += spaceQ - spaceRingQ_[ringPos_];

    markRingI_[ringPos_] = markI;
    markRingQ_[ringPos_] = markQ;
    spaceRingI_[ringPos_] = spaceI;
    spaceRingQ_[ringPos_] = spaceQ;
    if (++ringPos_ >= kWindowSamples) ringPos_ = 0;

    advanceOscillators();
    ++samplesSeen_;
    if (samplesSeen_ < kWindowSamples) return false;

    const float markEnergy = markSumI_ * markSumI_ + markSumQ_ * markSumQ_;
    const float spaceEnergy = spaceSumI_ * spaceSumI_ + spaceSumQ_ * spaceSumQ_;
    const float total = markEnergy + spaceEnergy + 1.0f;
    const float discriminator = (markEnergy - spaceEnergy) / total;
    const int8_t sign = discriminator >= 0.0f ? 1 : -1;

    metrics_.markEnergy = markEnergy;
    metrics_.spaceEnergy = spaceEnergy;
    metrics_.confidence = fabsf(discriminator);

    bit = discriminator >= 0.0f; // MARK=1, SPACE=0
    if (!clockStarted_) {
      clockStarted_ = true;
      clockPhase_ = 0.0f;
      lastSign_ = sign;
      haveSign_ = true;
      ++metrics_.bitsProduced;
      return true;
    }

    // A tone transition belongs halfway between adjacent bit-centre samples.
    // Correct the 40-sample clock gently toward that half-bit point.
    if (haveSign_ && sign != lastSign_) {
      float error = clockPhase_ - 20.0f;
      if (error > 20.0f) error -= 40.0f;
      if (error < -20.0f) error += 40.0f;
      if (error > 8.0f) error = 8.0f;
      if (error < -8.0f) error = -8.0f;
      timingError_ = error;
      clockPhase_ -= error * 0.08f;
    }
    lastSign_ = sign;
    haveSign_ = true;

    clockPhase_ += 1.0f;
    if (clockPhase_ >= 40.0f) {
      clockPhase_ -= 40.0f;
      ++metrics_.bitsProduced;
      return true;
    }
    return false;
  }

  const Metrics& metrics() const { return metrics_; }
  float markHz() const { return markHz_; }
  float spaceHz() const { return spaceHz_; }

private:
  void advanceOscillators() {
    const float nextMarkI = markOscI_ * markCosStep_ - markOscQ_ * markSinStep_;
    const float nextMarkQ = markOscI_ * markSinStep_ + markOscQ_ * markCosStep_;
    markOscI_ = nextMarkI;
    markOscQ_ = nextMarkQ;

    const float nextSpaceI = spaceOscI_ * spaceCosStep_ - spaceOscQ_ * spaceSinStep_;
    const float nextSpaceQ = spaceOscI_ * spaceCosStep_ + spaceOscQ_ * spaceSinStep_;
    spaceOscI_ = nextSpaceI;
    spaceOscQ_ = nextSpaceQ;

    // Keep recurrence error bounded during long unattended NAVTEX sessions.
    if ((samplesSeen_ & 1023U) == 1023U) {
      const float markNorm = 1.0f / sqrtf(markOscI_ * markOscI_ + markOscQ_ * markOscQ_);
      const float spaceNorm = 1.0f / sqrtf(spaceOscI_ * spaceOscI_ + spaceOscQ_ * spaceOscQ_);
      markOscI_ *= markNorm; markOscQ_ *= markNorm;
      spaceOscI_ *= spaceNorm; spaceOscQ_ *= spaceNorm;
    }
  }

  float markHz_ = kDefaultMarkHz;
  float spaceHz_ = kDefaultSpaceHz;
  float markCosStep_ = 1.0f, markSinStep_ = 0.0f;
  float spaceCosStep_ = 1.0f, spaceSinStep_ = 0.0f;
  float markOscI_ = 1.0f, markOscQ_ = 0.0f;
  float spaceOscI_ = 1.0f, spaceOscQ_ = 0.0f;
  float markRingI_[kWindowSamples]{}, markRingQ_[kWindowSamples]{};
  float spaceRingI_[kWindowSamples]{}, spaceRingQ_[kWindowSamples]{};
  float markSumI_ = 0.0f, markSumQ_ = 0.0f;
  float spaceSumI_ = 0.0f, spaceSumQ_ = 0.0f;
  float dc_ = 2048.0f;
  float clockPhase_ = 20.0f;
  float timingError_ = 0.0f;
  uint8_t ringPos_ = 0;
  uint32_t samplesSeen_ = 0;
  int8_t lastSign_ = 0;
  bool haveSign_ = false;
  bool clockStarted_ = false;
  Metrics metrics_{};
};
