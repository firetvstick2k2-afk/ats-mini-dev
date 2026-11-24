#include "Js8NormalAcquire.h"

#include <math.h>

bool Js8NormalAcquire::search(const int16_t* samples, size_t sampleCount,
                              const Search& cfg, Result& result) {
  result = Result{};
  if (!samples || !cfg.startStep || !cfg.maxCandidates ||
      !isfinite(cfg.firstToneHz) || !isfinite(cfg.minFrequencyOffsetHz) ||
      !isfinite(cfg.maxFrequencyOffsetHz) || !isfinite(cfg.frequencyStepHz) ||
      cfg.frequencyStepHz <= 0.0f ||
      cfg.maxFrequencyOffsetHz < cfg.minFrequencyOffsetHz ||
      cfg.lastStart < cfg.firstStart ||
      cfg.minimumMatchedPilots > Js8NormalSync::kSyncSymbols ||
      cfg.minimumMeanExpectedToBest < 0.0f ||
      cfg.minimumMeanExpectedToBest > 1.0f) return false;

  float bestScore = -1.0f;
  for (size_t start = cfg.firstStart; start <= cfg.lastStart;) {
    for (float offset = cfg.minFrequencyOffsetHz;
         offset <= cfg.maxFrequencyOffsetHz + cfg.frequencyStepHz * 0.001f;
         offset += cfg.frequencyStepHz) {
      if (result.candidatesEvaluated >= cfg.maxCandidates) break;
      ++result.candidatesEvaluated;

      if (start > sampleCount ||
          sampleCount - start < Js8NormalSync::kFrameSamples) continue;
      const float lowestHz = cfg.firstToneHz + offset;
      const float highestHz = lowestHz + 7.0f * Js8NormalSync::kToneSpacingHz;
      if (lowestHz <= 0.0f ||
          highestHz >= Js8NormalSync::kSampleRate * 0.5f) continue;

      Js8NormalSync::Result metric{};
      if (!Js8NormalSync::evaluate(samples, sampleCount, start,
                                   cfg.firstToneHz, offset, metric)) continue;

      // Prefer more matched pilots, then the expected-tone share of total
      // measured energy. This is candidate ranking, not frame validation.
      const float score = static_cast<float>(metric.matchedPilots) +
                          metric.meanExpectedToTotal * 0.5f +
                          metric.meanExpectedToBest * 0.001f;
      if (score > bestScore) {
        bestScore = score;
        result.candidateFound = true;
        result.frameStart = start;
        result.frequencyOffsetHz = offset;
        result.sync = metric;
      }
    }
    if (result.candidatesEvaluated >= cfg.maxCandidates ||
        cfg.lastStart - start < cfg.startStep) break;
    start += cfg.startStep;
  }

  result.syncCandidate = result.candidateFound &&
      result.sync.matchedPilots >= cfg.minimumMatchedPilots &&
      result.sync.meanExpectedToBest >= cfg.minimumMeanExpectedToBest;
  return result.candidateFound;
}
