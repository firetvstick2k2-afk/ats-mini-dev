// SPDX-License-Identifier: GPL-3.0-only
// Host-side protocol and LDPC regression test for the JS8Call-derived decoder.

#include "../ats-mini/src/js8/Js8Ldpc.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main() {
  uint8_t messageBits[87]{};
  uint8_t codeword[174]{};
  assert(Js8Ldpc::encodePayload("ATSMINI-TEST", 5, messageBits, codeword));
  assert(Js8Ldpc::checkPayloadCrc(messageBits));

  Js8Ldpc::Workspace workspace{};
  Js8Ldpc::Result result{};
  float llr[174];
  for (int i = 0; i < 174; ++i) llr[i] = codeword[i] ? 8.0f : -8.0f;

  // One erased channel bit exercises the iterative BP path while preserving
  // the exact JS8Call parity-check graph and message-bit ordering.
  llr[17] = 0.0f;
  assert(Js8Ldpc::decode(llr, workspace, result, 30));
  assert(result.converged);
  assert(result.unsatisfiedChecks == 0);
  assert(Js8Ldpc::checkPayloadCrc(result.decodedBits));

  char decodedText[13]{};
  uint8_t decodedType = 0;
  assert(Js8Ldpc::unpackMessage(result.decodedBits, decodedText, decodedType));
  assert(strcmp(decodedText, "ATSMINI-TEST") == 0);
  assert(decodedType == 5);

  uint8_t damaged[87];
  memcpy(damaged, result.decodedBits, sizeof(damaged));
  damaged[0] ^= 1u;
  assert(!Js8Ldpc::checkPayloadCrc(damaged));

  puts("PASS: JS8 NORMAL payload CRC, LDPC(174,87), erasure recovery and unpack");
  return 0;
}
