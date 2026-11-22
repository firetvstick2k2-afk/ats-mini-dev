#pragma once

#include <Arduino.h>
#include "NavtexCcIr476.h"

// SITOR-B phasing acquisition at the 7-bit codeword level.
//
// This stage assumes bit timing and 7-bit character framing have already
// produced one codeword per slot. It identifies which alternating slot
// parity carries the ALPHA phasing signal. The caller remains responsible
// for bit/character synchronization and for feeding the selected DX/RX
// parity to the FEC combiner.
class NavtexSitorBPhasing {
public:
  static constexpr uint8_t kWindow = 24;
  static constexpr uint8_t kMinAlpha = 4;
  static constexpr uint8_t kMinRep = 4;
  static constexpr uint8_t kMinMargin = 3;

  struct Status {
    bool locked = false;
    bool dxOnEvenSlots = true;
    uint8_t evenAlpha = 0;
    uint8_t oddAlpha = 0;
    uint8_t evenRep = 0;
    uint8_t oddRep = 0;
    uint8_t confidence = 0;
  };

  void reset() {
    for (uint8_t i = 0; i < kWindow; ++i) {
      codes_[i] = 0;
      valid_[i] = false;
    }
    writeIndex_ = 0;
    received_ = 0;
    status_ = Status{};
  }

  // Feed one completed 7-bit codeword. Invalid 4-of-7 words are retained
  // as invalid samples so they cannot accidentally contribute to phasing.
  // A lock is reported only when ALPHA is consistently on one parity and
  // REP is consistently on the opposite parity.
  const Status& push(uint8_t code) {
    code &= 0x7F;
    codes_[writeIndex_] = code;
    valid_[writeIndex_] = NavtexCcIr476::isValid(code);
    writeIndex_ = static_cast<uint8_t>((writeIndex_ + 1) % kWindow);
    if (received_ < kWindow) ++received_;

    status_ = evaluate();
    return status_;
  }

  const Status& status() const { return status_; }

private:
  Status evaluate() const {
    Status out{};
    if (received_ < kWindow) return out;

    // Evaluate parity in absolute stream order, not ring-buffer order.
    uint8_t evenAlpha = 0, oddAlpha = 0;
    uint8_t evenRep = 0, oddRep = 0;
    uint8_t evenValid = 0, oddValid = 0;

    const uint8_t oldest = writeIndex_;
    for (uint8_t i = 0; i < kWindow; ++i) {
      const uint8_t index = static_cast<uint8_t>((oldest + i) % kWindow);
      if (!valid_[index]) continue;
      const uint8_t code = codes_[index];
      if ((i & 1U) == 0) {
        ++evenValid;
        if (code == NavtexCcIr476::kAlpha) ++evenAlpha;
        if (code == NavtexCcIr476::kRep) ++evenRep;
      } else {
        ++oddValid;
        if (code == NavtexCcIr476::kAlpha) ++oddAlpha;
        if (code == NavtexCcIr476::kRep) ++oddRep;
      }
    }

    out.evenAlpha = evenAlpha;
    out.oddAlpha = oddAlpha;
    out.evenRep = evenRep;
    out.oddRep = oddRep;

    const int evenScore = static_cast<int>(evenAlpha) + oddRep;
    const int oddScore = static_cast<int>(oddAlpha) + evenRep;
    const bool evenCandidate = evenAlpha >= kMinAlpha && oddRep >= kMinRep;
    const bool oddCandidate = oddAlpha >= kMinAlpha && evenRep >= kMinRep;
    const int margin = evenScore > oddScore ? evenScore - oddScore : oddScore - evenScore;

    if (evenCandidate && evenScore >= oddScore + kMinMargin) {
      out.locked = true;
      out.dxOnEvenSlots = true;
      out.confidence = static_cast<uint8_t>(evenScore > 255 ? 255 : evenScore);
    } else if (oddCandidate && oddScore >= evenScore + kMinMargin) {
      out.locked = true;
      out.dxOnEvenSlots = false;
      out.confidence = static_cast<uint8_t>(oddScore > 255 ? 255 : oddScore);
    } else {
      (void)margin;
    }
    return out;
  }

  uint8_t codes_[kWindow]{};
  bool valid_[kWindow]{};
  uint8_t writeIndex_ = 0;
  uint8_t received_ = 0;
  Status status_{};
};
