// SPDX-License-Identifier: GPL-3.0-only
// JS8 NORMAL 8-FSK soft-symbol demodulator.

#pragma once

#include <stddef.h>
#include <stdint.h>

class Js8NormalDemod {
public:
  static constexpr uint32_t kSampleRate = 4000;
  static constexpr uint32_t kSymbolSamples = 640;
  static constexpr uint32_t kSymbolCount = 79;
  static constexpr uint32_t kFrameSamples = kSymbolSamples * kSymbolCount;
  static constexpr float kToneSpacingHz = 6.25f;
  static constexpr uint8_t kDataSymbolsPerBlock = 29;
  static constexpr uint8_t kDataSymbolCount = 58;

  struct Result {
    bool evaluated = false;
    uint8_t dataSymbols = 0;
    float meanAbsLlr = 0.0f;
    float minimumAbsLlr = 0.0f;
    float totalToneEnergy = 0.0f;
  };

  // Produces 174 LLRs in JS8Call order: 87 parity bits, then 87 message bits.
  // Positive LLR favors bit 1; negative LLR favors bit 0.
  // This evaluates a caller-proposed frame start and frequency only; it does
  // not search timing or frequency and does not validate the frame.
  static bool demodulate(const int16_t* samples, size_t sampleCount,
                         size_t frameStart, float firstToneHz,
                         float frequencyOffsetHz, float llrOut[174],
                         Result& result);
};
