#pragma once

#include <Arduino.h>

// NAVTEX/SITOR-B bit-to-codeword synchronizer.
//
// Input is a hard-sliced 100-baud bit stream. CCIR-476 words are packed
// LSB-first. This stage searches the alternating ALPHA (DX) / REP (RX)
// phasing sequence, establishes the 7-bit boundary and detects polarity.
// It does not recover the 100-baud clock; the audio demodulator must supply
// correctly timed bit decisions.
class NavtexSitorBSync {
public:
  static constexpr uint8_t kPhasingWords = 12;
  static constexpr uint8_t kPatternBits = kPhasingWords * 7;
  static constexpr uint8_t kMaxPhasingErrors = 12;

  struct Status {
    bool locked = false;
    bool inverted = false;
    bool dxOnEvenSlots = true;
    uint8_t phasingErrors = 0;
  };

  void reset() {
    for (uint8_t i = 0; i < kPatternBits; ++i) bits_[i] = 0;
    writeIndex_ = 0;
    received_ = 0;
    codeBits_ = 0;
    codeValue_ = 0;
    codeSlot_ = 0;
    status_ = Status{};
  }

  // Feed one timing-recovered bit. Returns true for each complete word.
  // The first codeword after the detected 12-word phasing window is slot 0.
  bool pushBit(bool bit, uint8_t& codeword) {
    codeword = 0;
    if (!status_.locked) {
      bits_[writeIndex_] = bit ? 1U : 0U;
      writeIndex_ = static_cast<uint8_t>((writeIndex_ + 1U) % kPatternBits);
      if (received_ < kPatternBits) ++received_;
      // Keep testing the rolling window: acquisition may begin after the
      // receiver starts listening, not necessarily at stream bit zero.
      if (received_ == kPatternBits) acquire();
      return false;
    }

    const bool decodedBit = bit ^ status_.inverted;
    codeValue_ = static_cast<uint8_t>(
      codeValue_ | (static_cast<uint8_t>(decodedBit ? 1U : 0U) << codeBits_));
    if (++codeBits_ < 7) return false;

    codeword = codeValue_ & 0x7F;
    codeValue_ = 0;
    codeBits_ = 0;
    ++codeSlot_;
    return true;
  }

  const Status& status() const { return status_; }

  bool isDxSlot(uint32_t slotIndex) const {
    return ((slotIndex & 1U) == 0U) == status_.dxOnEvenSlots;
  }

private:
  static uint8_t patternBit(uint8_t position, bool startsWithAlpha) {
    const uint8_t word = static_cast<uint8_t>(position / 7U);
    const uint8_t bit = static_cast<uint8_t>(position % 7U);
    const bool alphaWord = ((word & 1U) == 0U) == startsWithAlpha;
    const uint8_t code = alphaWord ? 0x0F : 0x66;
    return static_cast<uint8_t>((code >> bit) & 1U);
  }

  void acquire() {
    uint8_t bestErrors = 0xFF;
    bool bestInvert = false;
    bool bestStartsAlpha = true;

    // writeIndex_ points at the oldest bit in the full rolling window.
    for (uint8_t order = 0; order < 2; ++order) {
      for (uint8_t invert = 0; invert < 2; ++invert) {
        uint8_t errors = 0;
        for (uint8_t i = 0; i < kPatternBits; ++i) {
          const uint8_t index = static_cast<uint8_t>(
            (writeIndex_ + i) % kPatternBits);
          const uint8_t expected = static_cast<uint8_t>(
            patternBit(i, order == 0) ^ (invert != 0));
          if (bits_[index] != expected) ++errors;
        }
        if (errors < bestErrors) {
          bestErrors = errors;
          bestInvert = invert != 0;
          bestStartsAlpha = order == 0;
        }
      }
    }

    if (bestErrors > kMaxPhasingErrors) return;

    status_.locked = true;
    status_.inverted = bestInvert;
    status_.phasingErrors = bestErrors;
    // Twelve phasing words is an even count, so data slot zero has the
    // same DX/RX parity as the first phasing word.
    status_.dxOnEvenSlots = bestStartsAlpha;
    codeBits_ = 0;
    codeValue_ = 0;
    codeSlot_ = 0;
  }

  uint8_t bits_[kPatternBits]{};
  uint8_t writeIndex_ = 0;
  uint8_t received_ = 0;
  uint8_t codeBits_ = 0;
  uint8_t codeValue_ = 0;
  uint32_t codeSlot_ = 0;
  Status status_{};
};
