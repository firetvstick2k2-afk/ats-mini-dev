#include "Js8NormalSync.h"

#include <math.h>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr uint8_t kNormalCostas[7] = {4, 2, 5, 6, 1, 3, 0};
constexpr uint8_t kPilotStarts[3] = {0, 36, 72};

float goertzelEnergy(const int16_t* samples, size_t count, float frequencyHz) {
  const float omega = (2.0f * kPi * frequencyHz) /
                      static_cast<float>(Js8NormalSync::kSampleRate);
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
  float energy = q1 * q1 + q2 * q2 - coefficient * q1 * q2;
  return energy > 0.0f ? energy : 0.0f;
}
}

bool Js8NormalSync::evaluate(const int16_t* samples, size_t sampleCount,
                             size_t frameStart, float firstToneHz,
                             float frequencyOffsetHz, Result& result) {
  result = Result{};
  if (!samples || frameStart > sampleCount ||
      sampleCount - frameStart < kFrameSamples ||
      !isfinite(firstToneHz) || !isfinite(frequencyOffsetHz)) return false;

  float expectedToBestSum = 0.0f;
  float expectedToTotalSum = 0.0f;
  float pilotEnergySum = 0.0f;
  float minimumRatio = 1.0f;
  uint8_t matched = 0;

  for (uint8_t block = 0; block < 3; ++block) {
    for (uint8_t pilot = 0; pilot < 7; ++pilot) {
      const uint8_t symbol = static_cast<uint8_t>(kPilotStarts[block] + pilot);
      const size_t offset = frameStart +
          static_cast<size_t>(symbol) * kSymbolSamples;
      const int16_t* symbolSamples = samples + offset;
      const float expectedHz = firstToneHz + frequencyOffsetHz +
          static_cast<float>(kNormalCostas[pilot]) * kToneSpacingHz;

      float expectedEnergy = 0.0f;
      float bestEnergy = 0.0f;
      float totalEnergy = 0.0f;
      for (uint8_t tone = 0; tone < 8; ++tone) {
        const float hz = firstToneHz + frequencyOffsetHz +
                         static_cast<float>(tone) * kToneSpacingHz;
        const float energy = goertzelEnergy(symbolSamples, kSymbolSamples, hz);
        totalEnergy += energy;
        if (energy > bestEnergy) bestEnergy = energy;
        if (tone == kNormalCostas[pilot]) expectedEnergy = energy;
      }

      if (bestEnergy <= 1.0f || totalEnergy <= 1.0f) continue;
      const float bestRatio = expectedEnergy / bestEnergy;
      const float totalRatio = expectedEnergy / totalEnergy;
      expectedToBestSum += bestRatio;
      expectedToTotalSum += totalRatio;
      pilotEnergySum += expectedEnergy;
      if (bestRatio < minimumRatio) minimumRatio = bestRatio;
      if (bestRatio >= 0.50f) ++matched;
    }
  }

  if (!matched) return false;
  result.evaluated = true;
  result.matchedPilots = matched;
  result.meanExpectedToBest = expectedToBestSum / kSyncSymbols;
  result.meanExpectedToTotal = expectedToTotalSum / kSyncSymbols;
  result.minimumExpectedToBest = minimumRatio;
  result.meanPilotEnergy = pilotEnergySum / kSyncSymbols;
  return true;
}
