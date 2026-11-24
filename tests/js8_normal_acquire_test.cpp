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

  Js8NormalAcquire::Search cfg;
  cfg.firstStart = 0;
  cfg.lastStart = 160;
  cfg.startStep = 40;
  cfg.firstToneHz = baseHz;
  cfg.minFrequencyOffsetHz = -2.5f;
  cfg.maxFrequencyOffsetHz = 2.5f;
  cfg.frequencyStepHz = 1.25f;
  cfg.maxCandidates = 64;
  cfg.minimumMatchedPilots = 15;
  cfg.minimumMeanExpectedToBest = 0.55f;

  Js8NormalAcquire::Result result;
  if (!Js8NormalAcquire::search(audio.data(), audio.size(), cfg, result) ||
      !result.syncCandidate || result.candidatesEvaluated == 0 ||
      result.candidatesEvaluated > cfg.maxCandidates ||
      std::abs(static_cast<int>(result.frameStart) -
               static_cast<int>(actualStart)) > 40 ||
      std::abs(result.frequencyOffsetHz - offsetHz) > 1.26f) {
    std::printf("FAIL bounded search: found=%d sync=%d start=%zu offset=%.2f "
                "pilots=%u candidates=%u\n",
                result.candidateFound, result.syncCandidate, result.frameStart,
                result.frequencyOffsetHz, result.sync.matchedPilots,
                result.candidatesEvaluated);
    return 1;
  }

  std::vector<int16_t> silence(frame + 512, 0);
  Js8NormalAcquire::Result silentResult;
  if (Js8NormalAcquire::search(silence.data(), silence.size(), cfg,
                              silentResult) || silentResult.syncCandidate) {
    std::printf("FAIL silence accepted as sync\n");
    return 1;
  }

  std::printf("PASS bounded NORMAL acquisition; start=%zu offset=%.2f "
              "pilots=%u candidates=%u; silence rejected\n",
              result.frameStart, result.frequencyOffsetHz,
              result.sync.matchedPilots, result.candidatesEvaluated);
  return 0;
}
