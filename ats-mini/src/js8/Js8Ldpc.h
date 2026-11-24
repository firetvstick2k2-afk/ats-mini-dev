// SPDX-License-Identifier: GPL-3.0-only
// JS8Call-compatible LDPC(174,87) and NORMAL payload helpers.
// Adapted from the JS8Call implementation; see NOTICE.md and COPYING.

#pragma once

#include <stdint.h>

namespace Js8Ldpc {

static constexpr uint16_t kCodewordBits = 174;
static constexpr uint16_t kMessageBits = 87;

struct Workspace {
  float tov[174][3];
  float toc[87][7];
  float tanhtoc[87][7];
  float zn[174];
  uint8_t hard[174];
};

struct Result {
  bool converged = false;
  uint8_t iterations = 0;
  uint8_t unsatisfiedChecks = 0;
  uint16_t hardErrors = 0;
  uint8_t decodedBits[87]{};
};

// text must contain exactly 12 characters from the JS8 64-character alphabet.
bool encodePayload(const char* text12, uint8_t frameType,
                   uint8_t messageBits[87], uint8_t codeword[174]);
bool encodeCodeword(const uint8_t messageBits[87], uint8_t codeword[174]);

// LLR sign convention: positive favors bit 1; negative favors bit 0.
// Workspace is caller-owned (about 8 KiB); keep it off small task stacks.
bool decode(const float llr[174], Workspace& workspace, Result& result,
            uint8_t maxIterations = 30);

bool checkPayloadCrc(const uint8_t messageBits[87]);
bool unpackMessage(const uint8_t messageBits[87], char textOut[13],
                   uint8_t& frameType);

} // namespace Js8Ldpc
