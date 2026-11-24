#pragma once

#include "Js8NormalSync.h"

#include <stddef.h>
#include <stdint.h>

// Bounded coarse NORMAL acquisition over caller-selected time/frequency bounds.
// This ranks pilot candidates only; LDPC+CRC must still validate a JS8 frame.
class Js8NormalAcquire {
public:
  struct Search {
    size_t firstStart = 0;
    size_t lastStart = 0;
    size_t startStep = 640;
    float firstToneHz = 1000.0f;
    float minFrequencyOffsetHz = -2.5f;
    float maxFrequencyOffsetHz = 2.5f;
    float frequencyStepHz = 1.25f;
    uint32_t maxCandidates = 256;
    uint8_t minimumMatchedPilots = 15;
    float minimumMeanExpectedToBest = 0.55f;
  };

  struct Result {
    bool candidateFound = false;
    bool syncCandidate = false;
    size_t frameStart = 0;
    float frequencyOffsetHz = 0.0f;
    uint32_t candidatesEvaluated = 0;
    Js8NormalSync::Result sync{};
  };

  // Searches no more than maxCandidates. The caller chooses a narrow enough
  // range and step sizes to fit its worker CPU budget. No allocation is done.
  static bool search(const int16_t* samples, size_t sampleCount,
                     const Search& search, Result& result);
};
