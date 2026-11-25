#pragma once

#include <stddef.h>
#include <stdint.h>

// Independent JS8 NORMAL synchronization metric.
// This evaluates one already-proposed frame start and one proposed lowest-tone
// frequency. It does not search the full waterfall, decode data bits, perform
// LDPC/CRC, or claim that a frame is a valid JS8 message.
class Js8NormalSync {
public:
  static constexpr uint32_t kSampleRate = 4000;
  static constexpr uint32_t kSymbolSamples = 640;
  static constexpr uint32_t kSymbolCount = 79;
  static constexpr uint32_t kFrameSamples = kSymbolSamples * kSymbolCount;
  static constexpr float kToneSpacingHz = 6.25f;
  static constexpr size_t kSyncSymbols = 21;

  struct Result {
    bool evaluated = false;
    float meanExpectedToBest = 0.0f;
    float meanExpectedToTotal = 0.0f;
    float minimumExpectedToBest = 0.0f;
    float meanPilotEnergy = 0.0f;
    uint8_t matchedPilots = 0;
  };

  // samples are raw ADC-domain values (e.g. 0..4095); each symbol has its
  // own DC component removed before tone-energy measurement.
  // firstToneHz is the nominal lowest of the eight tones. frequencyOffsetHz
  // is a caller-proposed common offset, not an automatically searched value.
  static bool evaluate(const int16_t* samples, size_t sampleCount,
                       size_t frameStart, float firstToneHz,
                       float frequencyOffsetHz, Result& result);
};
