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

  struct Refinement {
    size_t startRadiusSamples = 40;
    size_t startStepSamples = 2;
    float frequencyRadiusHz = 1.25f;
    float frequencyStepHz = 0.625f;
    uint32_t maxCandidates = 256;
    uint8_t minimumMatchedPilots = 18;
    float minimumMeanExpectedToBest = 0.65f;
  };

  // Refines a coarse candidate in a bounded local time/frequency window.
  // Returns a candidate only; the caller must still require LDPC convergence
  // and JS8 CRC success before declaring lock or publishing decoded text.
  static bool refine(const int16_t* samples, size_t sampleCount,
                     size_t coarseFrameStart, float coarseFrequencyOffsetHz,
                     float firstToneHz, const Refinement& cfg, Result& result);

  // Searches no more than maxCandidates. The caller chooses a narrow enough
  // range and step sizes to fit its worker CPU budget. No allocation is done.
  static bool search(const int16_t* samples, size_t sampleCount,
                     const Search& search, Result& result);
};
