// SPDX-License-Identifier: GPL-3.0-only
// JS8 NORMAL 8-FSK soft-symbol demodulator for ATS Mini.

#include "Js8NormalDemod.h"

#include <math.h>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr uint8_t kDataStarts[2] = {7, 43};

float goertzelEnergy(const int16_t* samples, size_t count, float frequencyHz) {
  const float omega = (2.0f * kPi * frequencyHz) /
                      static_cast<float>(Js8NormalDemod::kSampleRate);
  const float coefficient = 2.0f * cosf(omega);
  float mean = 0.0f;
  for (size_t i = 0; i < count; ++i) mean += samples[i];
  mean /= static_cast<float>(count);

  float q1 = 0.0f;
  float q2 = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    const float value = static_cast<float>(samples[i]) - mean;
    const float q0 = value + coefficient * q1 - q2;
    q2 = q1;
    q1 = q0;
  }
  const float energy = q1 * q1 + q2 * q2 - coefficient * q1 * q2;
  return energy > 0.0f ? energy : 0.0f;
}
}

bool Js8NormalDemod::demodulate(const int16_t* samples, size_t sampleCount,
                                size_t frameStart, float firstToneHz,
                                float frequencyOffsetHz, float llrOut[174],
                                Result& result) {
  result = Result{};
  if (!samples || !llrOut || frameStart > sampleCount ||
      sampleCount - frameStart < kFrameSamples ||
      !isfinite(firstToneHz) || !isfinite(frequencyOffsetHz)) return false;

  const float lowestHz = firstToneHz + frequencyOffsetHz;
  const float highestHz = lowestHz + 7.0f * kToneSpacingHz;
  if (lowestHz <= 0.0f || highestHz >= (kSampleRate * 0.5f)) return false;

  float absLlrSum = 0.0f;
  float minAbsLlr = 1.0e30f;
  float totalEnergy = 0.0f;

  // JS8 NORMAL sends 29 parity 3-bit words, then 29 message 3-bit words.
  for (uint8_t block = 0; block < 2; ++block) {
    for (uint8_t symbolInBlock = 0; symbolInBlock < kDataSymbolsPerBlock;
         ++symbolInBlock) {
      const uint8_t symbol = static_cast<uint8_t>(
          kDataStarts[block] + symbolInBlock);
      const size_t offset = frameStart +
          static_cast<size_t>(symbol) * kSymbolSamples;
      const int16_t* symbolSamples = samples + offset;

      float energies[8]{};
      for (uint8_t tone = 0; tone < 8; ++tone) {
        const float hz = lowestHz + static_cast<float>(tone) * kToneSpacingHz;
        energies[tone] = goertzelEnergy(symbolSamples, kSymbolSamples, hz);
        totalEnergy += energies[tone];
      }

      const size_t outputBase =
          static_cast<size_t>(block) * 87u +
          static_cast<size_t>(symbolInBlock) * 3u;
      for (uint8_t bit = 0; bit < 3; ++bit) {
        const uint8_t mask = static_cast<uint8_t>(1u << (2u - bit));
        float maxZero = 0.0f;
        float maxOne = 0.0f;
        for (uint8_t tone = 0; tone < 8; ++tone) {
          if (tone & mask) {
            if (energies[tone] > maxOne) maxOne = energies[tone];
          } else {
            if (energies[tone] > maxZero) maxZero = energies[tone];
          }
        }

        // Max-log likelihood ratio. The +1 floor avoids log(0) on silence.
        const float llr = logf(maxOne + 1.0f) - logf(maxZero + 1.0f);
        llrOut[outputBase + bit] = llr;
        const float magnitude = fabsf(llr);
        absLlrSum += magnitude;
        if (magnitude < minAbsLlr) minAbsLlr = magnitude;
      }
    }
  }

  result.evaluated = true;
  result.dataSymbols = kDataSymbolCount;
  result.meanAbsLlr = absLlrSum / 174.0f;
  result.minimumAbsLlr = minAbsLlr;
  result.totalToneEnergy = totalEnergy;
  return true;
}
