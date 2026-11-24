// SPDX-License-Identifier: GPL-3.0-only
// Bounded local timing/frequency refinement test for JS8 NORMAL acquisition.

#include "../ats-mini/src/js8/Js8NormalAcquire.h"

#include <cmath>
#include <cstdio>
#include <vector>

int main() {
  constexpr float pi = 3.14159265358979323846f;
  constexpr size_t frame = Js8NormalSync::kFrameSamples;
  constexpr size_t actualStart = 17;
  constexpr float baseHz = 1000.0f;
  constexpr float offsetHz = 1.25f;
  const uint8_t costas[7] = {4, 2, 5, 6, 1, 3, 0};
  std::vector<int16_t> audio(frame + 512, 0);
  float phase = 0.0f;

  for (size_t symbol = 0; symbol < 79; ++symbol) {
    uint8_t tone = 0;
    if (symbol < 7) tone = costas[symbol];
    else if (symbol >= 36 && symbol < 43) tone = costas[symbol - 36];
    else if (symbol >= 72) tone = costas[symbol - 72];
    else tone = static_cast<uint8_t>((symbol * 5 + 3) & 7);

    const float hz = baseHz + offsetHz + tone * 6.25f;
    for (size_t n = 0; n < 640; ++n) {
      audio[actualStart + symbol * 640 + n] =
          static_cast<int16_t>(1200.0f * std::sin(phase));
      phase += 2.0f * pi * hz / 4000.0f;
      if (phase > 2.0f * pi) phase -= 2.0f * pi;
    }
  }

  Js8NormalAcquire::Refinement cfg;
  cfg.startRadiusSamples = 40;
  cfg.startStepSamples = 2;
  cfg.frequencyRadiusHz = 1.25f;
  cfg.frequencyStepHz = 0.625f;
  cfg.maxCandidates = 256;
  cfg.minimumMatchedPilots = 18;
  cfg.minimumMeanExpectedToBest = 0.65f;

  Js8NormalAcquire::Result result;
  if (!Js8NormalAcquire::refine(audio.data(), audio.size(), 0, 0.0f,
                               baseHz, cfg, result) ||
      !result.syncCandidate || result.candidatesEvaluated == 0 ||
      result.candidatesEvaluated > cfg.maxCandidates ||
      std::abs(static_cast<int>(result.frameStart) -
               static_cast<int>(actualStart)) > 1 ||
      std::abs(result.frequencyOffsetHz - offsetHz) > 0.001f) {
    std::printf("FAIL refinement: found=%d sync=%d start=%zu offset=%.3f "
                "pilots=%u candidates=%u\n",
                result.candidateFound, result.syncCandidate, result.frameStart,
                result.frequencyOffsetHz, result.sync.matchedPilots,
                result.candidatesEvaluated);
    return 1;
  }

  std::vector<int16_t> silence(frame + 512, 0);
  Js8NormalAcquire::Result silent;
  if (Js8NormalAcquire::refine(silence.data(), silence.size(), 0, 0.0f,
                               baseHz, cfg, silent) || silent.syncCandidate) {
    std::puts("FAIL: silence accepted by refinement");
    return 1;
  }

  std::printf("PASS NORMAL local refinement; start=%zu offset=%.2f Hz "
              "pilots=%u candidates=%u\n",
              result.frameStart, result.frequencyOffsetHz,
              result.sync.matchedPilots, result.candidatesEvaluated);
  return 0;
}
