// SPDX-License-Identifier: GPL-3.0-only
// Host end-to-end JS8 NORMAL frame test: encoder -> 8-FSK -> soft LLR -> LDPC/CRC.

#include "../ats-mini/src/js8/Js8Ldpc.h"
#include "../ats-mini/src/js8/Js8NormalDemod.h"
#include "../ats-mini/src/js8/Js8NormalSync.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main() {
  uint8_t messageBits[87]{};
  uint8_t codeword[174]{};
  assert(Js8Ldpc::encodePayload("ATSMINI-TEST", 5, messageBits, codeword));

  uint8_t tones[79]{};
  const uint8_t costas[7] = {4, 2, 5, 6, 1, 3, 0};
  for (int i = 0; i < 7; ++i) {
    tones[i] = costas[i];
    tones[36 + i] = costas[i];
    tones[72 + i] = costas[i];
  }
  for (int symbol = 0; symbol < 29; ++symbol) {
    const int bit = symbol * 3;
    tones[7 + symbol] = static_cast<uint8_t>(
        (codeword[bit] << 2) | (codeword[bit + 1] << 1) | codeword[bit + 2]);
    const int messageBit = 87 + bit;
    tones[43 + symbol] = static_cast<uint8_t>(
        (codeword[messageBit] << 2) |
        (codeword[messageBit + 1] << 1) |
         codeword[messageBit + 2]);
  }

  static int16_t samples[Js8NormalDemod::kFrameSamples];
  float phase = 0.0f;
  for (int symbol = 0; symbol < 79; ++symbol) {
    const float frequency = 1000.0f + tones[symbol] * 6.25f;
    for (int n = 0; n < 640; ++n) {
      phase += 2.0f * 3.14159265358979323846f * frequency / 4000.0f;
      if (phase >= 2.0f * 3.14159265358979323846f)
        phase -= 2.0f * 3.14159265358979323846f;
      samples[symbol * 640 + n] =
          static_cast<int16_t>(2048.0f + 1200.0f * sinf(phase));
    }
  }

  Js8NormalSync::Result sync{};
  assert(Js8NormalSync::evaluate(samples, Js8NormalDemod::kFrameSamples,
                                 0, 1000.0f, 0.0f, sync));
  assert(sync.matchedPilots == 21);

  float llr[174]{};
  Js8NormalDemod::Result demod{};
  assert(Js8NormalDemod::demodulate(samples, Js8NormalDemod::kFrameSamples,
                                    0, 1000.0f, 0.0f, llr, demod));
  assert(demod.evaluated && demod.dataSymbols == 58);

  Js8Ldpc::Workspace workspace{};
  Js8Ldpc::Result decoded{};
  assert(Js8Ldpc::decode(llr, workspace, decoded, 30));
  char text[13]{};
  uint8_t frameType = 0;
  assert(Js8Ldpc::unpackMessage(decoded.decodedBits, text, frameType));
  assert(strcmp(text, "ATSMINI-TEST") == 0);
  assert(frameType == 5);

  puts("PASS: JS8 NORMAL 79-symbol 8-FSK to soft LLR, LDPC, CRC and payload");
  return 0;
}
