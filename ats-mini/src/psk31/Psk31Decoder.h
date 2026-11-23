#pragma once
#include <stdint.h>

class Psk31Decoder {
public:
  static constexpr uint32_t kSampleRate = 4000;
  static constexpr float kBaud = 31.25f;
  static constexpr float kDefaultToneHz = 1000.0f;
  void reset();
  void setTone(float hz);
  bool pushSample(int16_t sample, char& decoded);
  bool signal() const { return signal_; }
private:
  void pushBit(bool bit, char& decoded, bool& produced);
  void appendBit(uint8_t bit);
  bool decodeVaricode(char& decoded) const;
  float toneHz_ = kDefaultToneHz;
  float oscCos_ = 1.0f, oscSin_ = 0.0f;
  float stepCos_ = 0.0f, stepSin_ = 1.0f;
  float dc_ = 2048.0f;
  float iFilter_ = 0.0f, qFilter_ = 0.0f;
  float previousI_ = 0.0f, previousQ_ = 0.0f;
  float timingEnvelope_[16]{};
  float timingClock_ = 0.0f;
  uint8_t timingDecimation_ = 0;
  uint16_t varicode_ = 0;
  uint8_t varicodeBits_ = 0;
  uint8_t zeroRun_ = 0;
  bool havePrevious_ = false;
  bool signal_ = false;
};
