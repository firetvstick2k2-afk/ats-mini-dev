#include "../ats-mini/src/js8/Js8NormalSync.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

static std::vector<int16_t> makeFrame(size_t prefix, double carrierOffsetHz,
                                      bool corruptCostas) {
  std::vector<int16_t> samples(prefix + Js8NormalSync::kFrameSamples + 10, 2048);
  const int costas[7] = {4, 2, 5, 6, 1, 3, 0};
  double phase = 0.0;

  for (uint32_t symbol = 0; symbol < Js8NormalSync::kSymbolCount; ++symbol) {
    int tone;
    if (symbol < 7) tone = costas[symbol];
    else if (symbol >= 36 && symbol < 43) tone = costas[symbol - 36];
    else if (symbol >= 72) tone = costas[symbol - 72];
    else tone = static_cast<int>((symbol * 3 + 1) & 7);

    if (corruptCostas && (symbol < 7 ||
        (symbol >= 36 && symbol < 43) || symbol >= 72)) {
      tone = (tone + 1) & 7;
    }

    const double frequency = 1000.0 + carrierOffsetHz + tone * 6.25;
    for (uint32_t i = 0; i < Js8NormalSync::kSymbolSamples; ++i) {
      phase += 2.0 * M_PI * frequency / Js8NormalSync::kSampleRate;
      if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
      samples[prefix + symbol * Js8NormalSync::kSymbolSamples + i] =
          static_cast<int16_t>(2048.0 + 1200.0 * sin(phase));
    }
  }
  return samples;
}

int main() {
  Js8NormalSync::Result result{};

  // Correct frame with non-zero frame offset and common carrier offset.
  const auto shifted = makeFrame(17, 1.25, false);
  assert(Js8NormalSync::evaluate(shifted.data(), shifted.size(), 17,
                                1000.0f, 1.25f, result));
  assert(result.evaluated);
  assert(result.matchedPilots == 21);
  assert(result.meanExpectedToBest > 0.99f);

  // A deliberately altered Costas sequence must not pass this metric.
  const auto wrong = makeFrame(0, 0.0, true);
  assert(!Js8NormalSync::evaluate(wrong.data(), wrong.size(), 0,
                                  1000.0f, 0.0f, result));

  // Reject an incomplete frame window.
  assert(!Js8NormalSync::evaluate(
      shifted.data(), 17 + Js8NormalSync::kFrameSamples - 1, 17,
      1000.0f, 1.25f, result));

  puts("PASS: NORMAL Costas metric offset, pilot count, wrong sequence, bounds");
  return 0;
}
