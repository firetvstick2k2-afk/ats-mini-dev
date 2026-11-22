#pragma once

#include <Arduino.h>
#include "NavtexCcIr476.h"

// SITOR-B collective FEC time-diversity combiner.
//
// The transmitted stream alternates DX and RX positions. An RX copy follows
// its original DX copy after four intervening character positions:
//  DX A, RX -, DX B, RX -, DX C, RX A ...
// Therefore the matching copies are five 7-bit character slots apart
// (5 * 7 * 10 ms = 350 ms between slot starts; the nominal bit-time
// diversity interval is 280 ms from the end/start convention used by the
// protocol description). The caller must acquire phasing and tell us which
// incoming positions are DX; this class does not guess stream phase.
class NavtexSitorBFec {
public:
  static constexpr uint8_t kDiversitySlots = 5;
  static constexpr uint8_t kRingSize = kDiversitySlots + 1;

  enum class Result : uint8_t { Pending, Recovered, Lost };

  void reset() {
    for (uint8_t i = 0; i < kRingSize; ++i) {
      dxCodes_[i] = 0;
      slotTags_[i] = kInvalidSlot;
    }
    nextSlot_ = 0;
  }

  // Feed exactly one decoded 7-bit CCIR-476 codeword in wire order.
  // isDxSlot must reflect the DX/RX parity established by phasing.
  // Returns Pending for DX slots and the first five positions; returns
  // Recovered or Lost when an RX slot reaches its corresponding DX copy.
  Result push(uint8_t code, bool isDxSlot, uint8_t& recoveredCode,
              NavtexCcIr476::CopySource& source) {
    const uint32_t slot = nextSlot_++;
    const uint8_t ringIndex = static_cast<uint8_t>(slot % kRingSize);
    code &= 0x7F;
    recoveredCode = 0;
    source = NavtexCcIr476::CopySource::None;

    if (isDxSlot) {
      dxCodes_[ringIndex] = code;
      slotTags_[ringIndex] = slot;
      return Result::Pending;
    }

    if (slot < kDiversitySlots) return Result::Pending;

    const uint32_t dxSlot = slot - kDiversitySlots;
    const uint8_t dxIndex = static_cast<uint8_t>(dxSlot % kRingSize);
    if (slotTags_[dxIndex] != dxSlot) return Result::Lost;

    source = NavtexCcIr476::recover(dxCodes_[dxIndex], code, recoveredCode);
    return source == NavtexCcIr476::CopySource::None
      ? Result::Lost : Result::Recovered;
  }

  uint32_t slotsProcessed() const { return nextSlot_; }

private:
  static constexpr uint32_t kInvalidSlot = 0xFFFFFFFFUL;
  uint8_t dxCodes_[kRingSize]{};
  uint32_t slotTags_[kRingSize] = {
    kInvalidSlot, kInvalidSlot, kInvalidSlot,
    kInvalidSlot, kInvalidSlot, kInvalidSlot
  };
  uint32_t nextSlot_ = 0;
};
