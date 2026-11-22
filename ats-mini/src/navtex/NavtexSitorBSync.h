#pragma once

#include <Arduino.h>

// NAVTEX/SITOR-B bit-to-codeword synchronizer.
//
// Input is a hard-sliced 100-baud bit stream (one call per recovered bit).
// This class searches the alternating CCIR-476 phasing words to establish
// the 7-bit character boundary, stream polarity and first DX/RX parity.
// It does not recover the 100-baud clock from audio samples; the upstream
// FSK demodulator must provide correctly timed bit decisions.
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

  // Feed one timing-recovered bit. Returns true when a complete 7-bit
  // codeword is available. Codewords are assembled MSB first.
  bool pushBit(bool bit, uint8_t& codeword) {
    codeword = 0;
    if (!status_.locked) {
      bits_[writeIndex_] = bit ? 1U : 0U;
      writeIndex_ = static_cast<uint8_t>((writeIndex_ + 1U) % kPatternBits);
      if (received_ < kPatternBits) ++received_;
      if (received_ == kPatternBits) acquire();
      return false;
    }

    const bool decodedBit = bit ^ status_.inverted;\n    codeValue_ = static_cast<uint8_t>((codeValue_ << 1U) | (decodedBit ? 1U : 0U));
    if (++codeBits_ < 7) return false;

    codeword = codeValue_ & 0x7F;
    codeValue_ = 0;
    codeBits_ = 0;
    ++codeSlot_;
    return true;
  }

  const Status& status() const { return status_; }

  // Slot zero is the first complete codeword after the detected phasing
  // sequence. The caller uses this parity when feeding the SITOR-B FEC.
  bool isDxSlot(uint32_t slotIndex) const {
    return ((slotIndex & 1U) == 0U) == status_.dxOnEvenSlots;
  }

private:
  static uint8_t patternBit(uint8_t position, bool startsWithAlpha) {
    const uint8_t word = static_cast<uint8_t>(position / 7U);
    const uint8_t bit = static_cast<uint8_t>(position % 7U);
    const bool alphaWord = ((word & 1U) == 0U) == startsWithAlpha;
    const uint8_t code = alphaWord ? 0x0F : 0x66; // ALPHA / REP
    return static_cast<uint8_t>((code >> (6U - bit)) & 1U);
  }

  void acquire() {
    uint8_t bestErrors = 0xFF;
    bool bestInvert = false;
    bool bestStartsAlpha = true;

    // The ring now contains exactly the most recent phasing-length window.
    // Test both possible DX/RX word orders and both signal polarities.
    for (uint8_t order = 0; order < 2; ++order) {
      for (uint8_t invert = 0; invert < 2; ++invert) {
        uint8_t errors = 0;
        for (uint8_t i = 0; i < kPatternBits; ++i) {
          const uint8_t index = static_cast<uint8_t>((writeIndex_ + i) % kPatternBits);
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

    // In collective SITOR-B, ALPHA marks the DX channel and REP the RX
    // channel. The detected first phasing word therefore establishes parity.
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
