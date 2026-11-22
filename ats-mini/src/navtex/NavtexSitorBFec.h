#pragma once

#include <Arduino.h>
#include "NavtexCcIr476.h"

// SITOR-B collective FEC-B time-diversity combiner.
//
// Once phased, RX (REP) occupies even stream slots and DX (ALPHA) odd
// slots. The RX copy is transmitted first; its matching DX copy follows
// five interleaved character slots later. Therefore recovery is performed
// when the DX slot arrives, using the RX codeword from slot (DX - 5).
class NavtexSitorBFec {
public:
  static constexpr uint8_t kDiversitySlots = 5;
  static constexpr uint8_t kRingSize = kDiversitySlots + 1;

  enum class Result : uint8_t { Pending, Recovered, Lost };

  void reset() {
    for (uint8_t i = 0; i < kRingSize; ++i) {
      codes_[i] = 0;
      slotTags_[i] = kInvalidSlot;
    }
    nextSlot_ = 0;
  }

  // Feed one complete 7-bit codeword in wire order.
  // On DX slots, pair the current code with its earlier RX copy.
  // RX slots are buffered and never emitted as independent characters.
  Result push(uint8_t code, bool isDxSlot, uint8_t& recoveredCode,
              NavtexCcIr476::CopySource& source) {
    const uint32_t slot = nextSlot_++;
    const uint8_t ringIndex = static_cast<uint8_t>(slot % kRingSize);
    code &= 0x7F;
    codes_[ringIndex] = code;
    slotTags_[ringIndex] = slot;
    recoveredCode = 0;
    source = NavtexCcIr476::CopySource::None;

    if (!isDxSlot || slot < kDiversitySlots) return Result::Pending;

    const uint32_t rxSlot = slot - kDiversitySlots;
    const uint8_t rxIndex = static_cast<uint8_t>(rxSlot % kRingSize);
    if (slotTags_[rxIndex] != rxSlot) return Result::Lost;

    source = NavtexCcIr476::recover(code, codes_[rxIndex], recoveredCode);
    return source == NavtexCcIr476::CopySource::None
      ? Result::Lost : Result::Recovered;
  }

  uint32_t slotsProcessed() const { return nextSlot_; }

private:
  static constexpr uint32_t kInvalidSlot = 0xFFFFFFFFUL;
  uint8_t codes_[kRingSize]{};
  uint32_t slotTags_[kRingSize] = {
    kInvalidSlot, kInvalidSlot, kInvalidSlot,
    kInvalidSlot, kInvalidSlot, kInvalidSlot
  };
  uint32_t nextSlot_ = 0;
};
