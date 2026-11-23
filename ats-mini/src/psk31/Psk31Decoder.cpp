#include "Psk31Decoder.h"
#include <math.h>
#include <string.h>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kFilterAlpha = 0.18f;
constexpr float kSignalThreshold = 10.0f;
// PSK31 standard Varicode, ASCII 0..127 (ARRL PSK31 specification).
const char* const kVaricode[128] = {
 "1010101011","1011011011","1011101101","1101110111","1011101011","1101011111","1011101111","1011111101",
 "1011111111","11101111","11101","1101101111","1011011101","11111","1101110101","1110101011",
 "1011110111","1011110101","1110101101","1110101111","1101011011","1101101011","1101101101","1101010111",
 "1101111011","1101111101","1110110111","1101010101","1101011101","1110111011","1011111011","1101111111",
 "1","111111111","101011111","111110101","111011011","1011010101","1010111011","101111111",
 "11111011","11110111","101101111","111011111","1110101","110101","1010111","110101111",
 "10110111","10111101","11101101","11111111","101110111","101011011","101101011","110101101",
 "110101011","110110111","11110101","110111101","111101101","1010101","111010111","1010101111",
 "1010111101","1111101","11101011","10101101","10110101","1110111","11011011","11111101",
 "101010101","1111111","111111101","101111101","11010111","10111011","11011101","10101011",
 "11010101","111011101","10101111","1101111","1101101","101010111","110110101","101011101",
 "101110101","101111011","1010101101","111110111","111101111","111111011","1010111111","101101101",
 "1011011111","1011","1011111","101111","101101","11","111101","1011011",
 "101011","1101","111101011","10111111","11011","111011","1111","111",
 "1111111","110111111","10101","10111","101","110111","1111011","1101011",
 "11011111","1011101","111010101","1010110111","110111011","1010110101","1011010111","1110110101"
};
}

void Psk31Decoder::setTone(float hz) {
  if (hz < 300.0f || hz > 1700.0f) return;
  toneHz_ = hz;
  const float phaseStep = 2.0f * kPi * toneHz_ / static_cast<float>(kSampleRate);
  stepCos_ = cosf(phaseStep);
  stepSin_ = sinf(phaseStep);
  reset();
}

void Psk31Decoder::reset() {
  oscCos_ = 1.0f; oscSin_ = 0.0f;
  dc_ = 2048.0f;
  iFilter_ = qFilter_ = symbolI_ = symbolQ_ = 0.0f;
  previousI_ = previousQ_ = 0.0f;
  samplesInSymbol_ = 0;
  varicode_ = 0; varicodeBits_ = 0; zeroRun_ = 0;
  havePrevious_ = false; signal_ = false;
  const float phaseStep = 2.0f * kPi * toneHz_ / static_cast<float>(kSampleRate);
  stepCos_ = cosf(phaseStep);
  stepSin_ = sinf(phaseStep);
}

void Psk31Decoder::appendBit(uint8_t bit) {
  if (varicodeBits_ >= 12) {
    varicode_ = 0;
    varicodeBits_ = 0;
  }
  varicode_ = static_cast<uint16_t>((varicode_ << 1) | (bit & 1U));
  ++varicodeBits_;
}

bool Psk31Decoder::decodeVaricode(char& decoded) const {
  if (!varicodeBits_) return false;
  for (uint8_t i = 0; i < 128; ++i) {
    const char* code = kVaricode[i];
    if (strlen(code) != varicodeBits_) continue;
    uint16_t value = 0;
    for (uint8_t n = 0; n < varicodeBits_; ++n)
      value = static_cast<uint16_t>((value << 1) | (code[n] == '1' ? 1U : 0U));
    if (value != varicode_) continue;
    if ((i >= 32 && i <= 126) || i == 9 || i == 10 || i == 13) {
      decoded = static_cast<char>(i);
      return true;
    }
    return false;
  }
  return false;
}

void Psk31Decoder::pushBit(bool bit, char& decoded, bool& produced) {
  produced = false;
  if (!bit) {
    if (zeroRun_ < 3) ++zeroRun_;
    return;
  }
  if (zeroRun_ >= 2) {
    produced = decodeVaricode(decoded);
    varicode_ = 0;
    varicodeBits_ = 0;
  } else if (zeroRun_ == 1) {
    appendBit(0);
  }
  zeroRun_ = 0;
  appendBit(1);
}

bool Psk31Decoder::pushSample(int16_t sample, char& decoded) {
  const float raw = static_cast<float>(sample);
  dc_ += (raw - dc_) * 0.001f;
  const float centered = raw - dc_;
  const float mixedI = centered * oscCos_;
  const float mixedQ = -centered * oscSin_;
  iFilter_ += kFilterAlpha * (mixedI - iFilter_);
  qFilter_ += kFilterAlpha * (mixedQ - qFilter_);
  const float nextCos = oscCos_ * stepCos_ - oscSin_ * stepSin_;
  const float nextSin = oscSin_ * stepCos_ + oscCos_ * stepSin_;
  oscCos_ = nextCos; oscSin_ = nextSin;
  symbolI_ += iFilter_;
  symbolQ_ += qFilter_;
  if (++samplesInSymbol_ < static_cast<uint16_t>(kSampleRate / kBaud)) return false;

  const float currentI = symbolI_ / static_cast<float>(samplesInSymbol_);
  const float currentQ = symbolQ_ / static_cast<float>(samplesInSymbol_);
  const float magnitude = sqrtf(currentI * currentI + currentQ * currentQ);
  signal_ = magnitude >= kSignalThreshold;
  symbolI_ = symbolQ_ = 0.0f;
  samplesInSymbol_ = 0;
  if (!signal_) {
    havePrevious_ = false;
    varicode_ = 0; varicodeBits_ = 0; zeroRun_ = 0;
    return false;
  }
  if (!havePrevious_) {
    previousI_ = currentI; previousQ_ = currentQ; havePrevious_ = true;
    return false;
  }
  const float differential = currentI * previousI_ + currentQ * previousQ_;
  previousI_ = currentI; previousQ_ = currentQ;
  char result = 0;
  bool produced = false;
  pushBit(differential >= 0.0f, result, produced);
  if (produced) decoded = result;
  return produced;
}
