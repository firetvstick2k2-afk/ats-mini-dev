// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) JS8Call contributors; adapted for ATS Mini.
 // Derived from JS8Call's JS8.cpp (GPL-3.0), including its LDPC parity
 // matrix, belief-propagation graph and JS8 payload/CRC conventions.
 // See NOTICE.md and COPYING for provenance and license terms.

#include "Js8Ldpc.h"

#include <array>
#include <math.h>
#include <string.h>

namespace Js8Ldpc {
namespace {
constexpr int N = 174;
constexpr int K = 87;
constexpr int M = 87;
constexpr int kMaxRows = 7;
constexpr int kMaxChecks = 3;
constexpr float kAtanhLimit = 0.999999f;
constexpr char kAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-+";

struct CheckNode {
  int valid_neighbors;
  std::array<int, kMaxRows> neighbors;
};

static constexpr std::array<std::array<int, 3>, 174> kMn =
    {{
        { 0, 24, 68}, { 1,  4, 72}, { 2, 31, 67}, { 3, 50, 60}, { 5, 62, 69}, { 6, 32, 78},
        { 7, 49, 85}, { 8, 36, 42}, { 9, 40, 64}, {10, 13, 63}, {11, 74, 76}, {12, 22, 80},
        {14, 15, 81}, {16, 55, 65}, {17, 52, 59}, {18, 30, 51}, {19, 66, 83}, {20, 28, 71},
        {21, 23, 43}, {25, 34, 75}, {26, 35, 37}, {27, 39, 41}, {29, 53, 54}, {33, 48, 86},
        {38, 56, 57}, {44, 73, 82}, {45, 61, 79}, {46, 47, 84}, {58, 70, 77}, { 0, 49, 52},
        { 1, 46, 83}, { 2, 24, 78}, { 3,  5, 13}, { 4,  6, 79}, { 7, 33, 54}, { 8, 35, 68},
        { 9, 42, 82}, {10, 22, 73}, {11, 16, 43}, {12, 56, 75}, {14, 26, 55}, {15, 27, 28},
        {17, 18, 58}, {19, 39, 62}, {20, 34, 51}, {21, 53, 63}, {23, 61, 77}, {25, 31, 76},
        {29, 71, 84}, {30, 64, 86}, {32, 38, 50}, {36, 47, 74}, {37, 69, 70}, {40, 41, 67},
        {44, 66, 85}, {45, 80, 81}, {48, 65, 72}, {57, 59, 65}, {60, 64, 84}, { 0, 13, 20},
        { 1, 12, 58}, { 2, 66, 81}, { 3, 31, 72}, { 4, 35, 53}, { 5, 42, 45}, { 6, 27, 74},
        { 7, 32, 70}, { 8, 48, 75}, { 9, 57, 63}, {10, 47, 67}, {11, 18, 44}, {14, 49, 60},
        {15, 21, 25}, {16, 71, 79}, {17, 39, 54}, {19, 34, 50}, {22, 24, 33}, {23, 62, 86},
        {26, 38, 73}, {28, 77, 82}, {29, 69, 76}, {30, 68, 83}, {21, 36, 85}, {37, 40, 80},
        {41, 43, 56}, {46, 52, 61}, {51, 55, 78}, {59, 74, 80}, { 0, 38, 76}, { 1, 15, 40},
        { 2, 30, 53}, { 3, 35, 77}, { 4, 44, 64}, { 5, 56, 84}, { 6, 13, 48}, { 7, 20, 45},
        { 8, 14, 71}, { 9, 19, 61}, {10, 16, 70}, {11, 33, 46}, {12, 67, 85}, {17, 22, 42},
        {18, 63, 72}, {23, 47, 78}, {24, 69, 82}, {25, 79, 86}, {26, 31, 39}, {27, 55, 68},
        {28, 62, 65}, {29, 41, 49}, {32, 36, 81}, {34, 59, 73}, {37, 54, 83}, {43, 51, 60},
        {50, 52, 71}, {57, 58, 66}, {46, 55, 75}, { 0, 18, 36}, { 1, 60, 74}, { 2,  7, 65},
        { 3, 59, 83}, { 4, 33, 38}, { 5, 25, 52}, { 6, 31, 56}, { 8, 51, 66}, { 9, 11, 14},
        {10, 50, 68}, {12, 13, 64}, {15, 30, 42}, {16, 19, 35}, {17, 79, 85}, {20, 47, 58},
        {21, 39, 45}, {22, 32, 61}, {23, 29, 73}, {24, 41, 63}, {26, 48, 84}, {27, 37, 72},
        {28, 43, 80}, {34, 67, 69}, {40, 62, 75}, {44, 48, 70}, {49, 57, 86}, {47, 53, 82},
        {12, 54, 78}, {76, 77, 81}, { 0,  1, 23}, { 2,  5, 74}, { 3, 55, 86}, { 4, 43, 52},
        { 6, 49, 82}, { 7,  9, 27}, { 8, 54, 61}, {10, 28, 66}, {11, 32, 39}, {13, 15, 19},
        {14, 34, 72}, {16, 30, 38}, {17, 35, 56}, {18, 45, 75}, {20, 41, 83}, {21, 33, 58},
        {22, 25, 60}, {24, 59, 64}, {26, 63, 79}, {29, 36, 65}, {31, 44, 71}, {37, 50, 85},
        {40, 76, 78}, {42, 55, 67}, {46, 73, 81}, {39, 51, 77}, {53, 60, 70}, {45, 57, 68}
    }};

static constexpr std::array<CheckNode, 87> kNm =
    {{
        {6, { 0, 29, 59,  88, 117, 146,   0}}, {6, { 1, 30, 60,  89, 118, 146,   0}}, {6, { 2, 31, 61,  90, 119, 147, 0}},
        {6, { 3, 32, 62,  91, 120, 148,   0}}, {6, { 1, 33, 63,  92, 121, 149,   0}}, {6, { 4, 32, 64,  93, 122, 147, 0}},
        {6, { 5, 33, 65,  94, 123, 150,   0}}, {6, { 6, 34, 66,  95, 119, 151,   0}}, {6, { 7, 35, 67,  96, 124, 152, 0}},
        {6, { 8, 36, 68,  97, 125, 151,   0}}, {6, { 9, 37, 69,  98, 126, 153,   0}}, {6, {10, 38, 70,  99, 125, 154, 0}},
        {6, {11, 39, 60, 100, 127, 144,   0}}, {6, { 9, 32, 59,  94, 127, 155,   0}}, {6, {12, 40, 71,  96, 125, 156, 0}},
        {6, {12, 41, 72,  89, 128, 155,   0}}, {6, {13, 38, 73,  98, 129, 157,   0}}, {6, {14, 42, 74, 101, 130, 158, 0}},
        {6, {15, 42, 70, 102, 117, 159,   0}}, {6, {16, 43, 75,  97, 129, 155,   0}}, {6, {17, 44, 59,  95, 131, 160, 0}},
        {6, {18, 45, 72,  82, 132, 161,   0}}, {6, {11, 37, 76, 101, 133, 162,   0}}, {6, {18, 46, 77, 103, 134, 146, 0}},
        {6, { 0, 31, 76, 104, 135, 163,   0}}, {6, {19, 47, 72, 105, 122, 162,   0}}, {6, {20, 40, 78, 106, 136, 164, 0}},
        {6, {21, 41, 65, 107, 137, 151,   0}}, {6, {17, 41, 79, 108, 138, 153,   0}}, {6, {22, 48, 80, 109, 134, 165, 0}},
        {6, {15, 49, 81,  90, 128, 157,   0}}, {6,  {2, 47, 62, 106, 123, 166,   0}}, {6, { 5, 50, 66, 110, 133, 154, 0}},
        {6, {23, 34, 76,  99, 121, 161,   0}}, {6, {19, 44, 75, 111, 139, 156,   0}}, {6, {20, 35, 63,  91, 129, 158, 0}},
        {6, { 7, 51, 82, 110, 117, 165,   0}}, {6, {20, 52, 83, 112, 137, 167,   0}}, {6, {24, 50, 78,  88, 121, 157, 0}},
        {7, {21, 43, 74, 106, 132, 154, 171}}, {6, { 8, 53, 83,  89, 140, 168,   0}}, {6, {21, 53, 84, 109, 135, 160, 0}},
        {6, { 7, 36, 64, 101, 128, 169,   0}}, {6, {18, 38, 84, 113, 138, 149,   0}}, {6, {25, 54, 70,  92, 141, 166, 0}},
        {7, {26, 55, 64,  95, 132, 159, 173}}, {6, {27, 30, 85,  99, 116, 170,   0}}, {6, {27, 51, 69, 103, 131, 143, 0}},
        {6, {23, 56, 67,  94, 136, 141,   0}}, {6, {6,  29, 71, 109, 142, 150,   0}}, {6, { 3, 50, 75, 114, 126, 167, 0}},
        {6, {15, 44, 86, 113, 124, 171,   0}}, {6, {14, 29, 85, 114, 122, 149,   0}}, {6, {22, 45, 63,  90, 143, 172, 0}},
        {6, {22, 34, 74, 112, 144, 152,   0}}, {7, {13, 40, 86, 107, 116, 148, 169}}, {6, {24, 39, 84,  93, 123, 158, 0}},
        {6, {24, 57, 68, 115, 142, 173,   0}}, {6, {28, 42, 60, 115, 131, 161,   0}}, {6, {14, 57, 87, 111, 120, 163, 0}},
        {7, { 3, 58, 71, 113, 118, 162, 172}}, {6, {26, 46, 85,  97, 133, 152,   0}}, {5, { 4, 43, 77, 108, 140,   0, 0}},
        {6, { 9, 45, 68, 102, 135, 164,   0}}, {6, { 8, 49, 58,  92, 127, 163,   0}}, {6, {13, 56, 57, 108, 119, 165, 0}},
        {6, {16, 54, 61, 115, 124, 153,   0}}, {6, { 2, 53, 69, 100, 139, 169,   0}}, {6, { 0, 35, 81, 107, 126, 173, 0}},
        {5, { 4, 52, 80, 104, 139,   0,   0}}, {6, {28, 52, 66,  98, 141, 172,   0}}, {6, {17, 48, 73,  96, 114, 166, 0}},
        {6, { 1, 56, 62, 102, 137, 156,   0}}, {6, {25, 37, 78, 111, 134, 170,   0}}, {6, {10, 51, 65,  87, 118, 147, 0}},
        {6, {19, 39, 67, 116, 140, 159,   0}}, {6, {10, 47, 80,  88, 145, 168,   0}}, {6, {28, 46, 79,  91, 145, 171, 0}},
        {6, { 5, 31, 86, 103, 144, 168,   0}}, {6, {26, 33, 73, 105, 130, 164,   0}}, {5, {11, 55, 83,  87, 138,   0, 0}},
        {6, {12, 55, 61, 110, 145, 170,   0}}, {6, {25, 36, 79, 104, 143, 150,   0}}, {6, {16, 30, 81, 112, 120, 160, 0}},
        {5, {27, 48, 58,  93, 136,   0,   0}}, {6, { 6, 54, 82, 100, 130, 167,   0}}, {6, {23, 49, 77, 105, 142, 148, 0}}
    }};

static constexpr const char* kParityHex[87] = {
  "23bba830e23b6b6f50982e",
  "1f8e55da218c5df3309052",
  "ca7b3217cd92bd59a5ae20",
  "56f78313537d0f4382964e",
  "6be396b5e2e819e373340c",
  "293548a138858328af4210",
  "cb6c6afcdc28bb3f7c6e86",
  "3f2a86f5c5bd225c961150",
  "849dd2d63673481860f62c",
  "56cdaec6e7ae14b43feeee",
  "04ef5cfa3766ba778f45a4",
  "c525ae4bd4f627320a3974",
  "41fd9520b2e4abeb2f989c",
  "7fb36c24085a34d8c1dbc4",
  "40fc3e44bb7d2bb2756e44",
  "d38ab0a1d2e52a8ec3bc76",
  "3d0f929ef3949bd84d4734",
  "45d3814f504064f80549ae",
  "f14dbf263825d0bd04b05e",
  "db714f8f64e8ac7af1a76e",
  "8d0274de71e7c1a8055eb0",
  "51f81573dd4049b082de14",
  "d8f937f31822e57c562370",
  "b6537f417e61d1a7085336",
  "ecbd7c73b9cd34c3720c8a",
  "3d188ea477f6fa41317a4e",
  "1ac4672b549cd6dba79bcc",
  "a377253773ea678367c3f6",
  "0dbd816fba1543f721dc72",
  "ca4186dd44c3121565cf5c",
  "29c29dba9c545e267762fe",
  "1616d78018d0b4745ca0f2",
  "fe37802941d66dde02b99c",
  "a9fa8e50bcb032c85e3304",
  "83f640f1a48a8ebc0443ea",
  "3776af54ccfbae916afde6",
  "a8fc906976c35669e79ce0",
  "f08a91fb2e1f78290619a8",
  "cc9da55fe046d0cb3a770c",
  "d36d662a69ae24b74dcbd8",
  "40907b01280f03c0323946",
  "d037db825175d851f3af00",
  "1bf1490607c54032660ede",
  "0af7723161ec223080be86",
  "eca9afa0f6b01d92305edc",
  "7a8dec79a51e8ac5388022",
  "9059dfa2bb20ef7ef73ad4",
  "6abb212d9739dfc02580f2",
  "f6ad4824b87c80ebfce466",
  "d747bfc5fd65ef70fbd9bc",
  "612f63acc025b6ab476f7c",
  "05209a0abb530b9e7e34b0",
  "45b7ab6242b77474d9f11a",
  "6c280d2a0523d9c4bc5946",
  "f1627701a2d692fd9449e6",
  "8d9071b7e7a6a2eed6965e",
  "bf4f56e073271f6ab4bf80",
  "c0fc3ec4fb7d2bb2756644",
  "57da6d13cb96a7689b2790",
  "a9fa2eefa6f8796a355772",
  "164cc861bdd803c547f2ac",
  "cc6de59755420925f90ed2",
  "a0c0033a52ab6299802fd2",
  "b274db8abd3c6f396ea356",
  "97d4169cb33e7435718d90",
  "81cfc6f18c35b1e1f17114",
  "481a2a0df8a23583f82d6c",
  "081c29a10d468ccdbcecb6",
  "2c4142bf42b01e71076acc",
  "a6573f3dc8b16c9d19f746",
  "c87af9a5d5206abca532a8",
  "012dee2198eba82b19a1da",
  "b1ca4ea2e3d173bad4379c",
  "b33ec97be83ce413f9acc8",
  "5b0f7742bca86b8012609a",
  "37d8e0af9258b9e8c5f9b2",
  "35ad3fb0faeb5f1b0c30dc",
  "6114e08483043fd3f38a8a",
  "cd921fdf59e882683763f6",
  "95e45ecd0135aca9d6e6ae",
  "2e547dd7a05f6597aac516",
  "14cd0f642fc0c5fe3a65ca",
  "3a0a1dfd7eee29c2e827e0",
  "c8b5dffc335095dcdcaf2a",
  "3dd01a59d86310743ec752",
  "8abdb889efbe39a510a118",
  "3f231f212055371cf3e2a2"
};

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

uint16_t augmentedCrc12(const uint8_t* data, size_t length) {
  uint16_t remainder = 0;
  for (size_t i = 0; i < length; ++i) {
    for (int bit = 7; bit >= 0; --bit) {
      const bool quotient = (remainder & 0x0800u) != 0;
      remainder = static_cast<uint16_t>((remainder << 1) |
                    ((data[i] >> bit) & 1u));
      if (quotient) remainder ^= 0x0C06u;
      remainder &= 0x0FFFu;
    }
  }
  return remainder;
}

void bitsToBytes(const uint8_t bits[87], uint8_t bytes[11]) {
  memset(bytes, 0, 11);
  for (size_t i = 0; i < 87; ++i) {
    if (bits[i] & 1u) bytes[i / 8] |= static_cast<uint8_t>(1u << (7u - (i % 8u)));
  }
}

} // namespace

bool encodeCodeword(const uint8_t messageBits[87], uint8_t codeword[174]) {
  if (!messageBits || !codeword) return false;
  for (int i = 0; i < K; ++i) {
    int parity = 0;
    for (int j = 0; j < K; ++j) {
      const int nibble = hexValue(kParityHex[i][j / 4]);
      if (nibble < 0) return false;
      const bool matrixBit = (nibble & (0x8 >> (j % 4))) != 0;
      parity ^= (matrixBit && (messageBits[j] & 1u)) ? 1 : 0;
    }
    codeword[i] = static_cast<uint8_t>(parity);
    codeword[K + i] = static_cast<uint8_t>(messageBits[i] & 1u);
  }
  return true;
}

bool encodePayload(const char* text12, uint8_t frameType,
                   uint8_t messageBits[87], uint8_t codeword[174]) {
  if (!text12 || !messageBits || !codeword || frameType > 7) return false;

  memset(messageBits, 0, 87);
  uint8_t bytes[11]{};

  for (int i = 0; i < 12; ++i) {
    int word = -1;
    for (int j = 0; j < 64; ++j) {
      if (kAlphabet[j] == text12[i]) { word = j; break; }
    }
    if (word < 0) return false;
    for (int bit = 0; bit < 6; ++bit) {
      messageBits[i * 6 + bit] = static_cast<uint8_t>((word >> (5 - bit)) & 1);
    }
  }

  messageBits[72] = static_cast<uint8_t>((frameType >> 2) & 1u);
  messageBits[73] = static_cast<uint8_t>((frameType >> 1) & 1u);
  messageBits[74] = static_cast<uint8_t>(frameType & 1u);
  bitsToBytes(messageBits, bytes);

  // JS8Call uses Boost augmented_crc<12, 0xc06> and XORs the result with 42.
  const uint16_t crc = static_cast<uint16_t>(augmentedCrc12(bytes, 11) ^ 42u);
  for (int bit = 0; bit < 12; ++bit) {
    messageBits[75 + bit] = static_cast<uint8_t>((crc >> (11 - bit)) & 1u);
  }

  return encodeCodeword(messageBits, codeword);
}

bool checkPayloadCrc(const uint8_t messageBits[87]) {
  if (!messageBits) return false;
  uint8_t bytes[11];
  bitsToBytes(messageBits, bytes);

  const uint16_t received = static_cast<uint16_t>(
      ((static_cast<uint16_t>(bytes[9] & 0x1Fu)) << 7) |
      (static_cast<uint16_t>(bytes[10]) >> 1));

  bytes[9] &= 0xE0u;
  bytes[10] = 0;
  const uint16_t expected = static_cast<uint16_t>(augmentedCrc12(bytes, 11) ^ 42u);
  return received == expected;
}

bool unpackMessage(const uint8_t messageBits[87], char textOut[13],
                   uint8_t& frameType) {
  if (!messageBits || !textOut || !checkPayloadCrc(messageBits)) return false;

  frameType = static_cast<uint8_t>((messageBits[72] << 2) |
                                   (messageBits[73] << 1) |
                                    messageBits[74]);
  for (int i = 0; i < 12; ++i) {
    uint8_t word = 0;
    for (int bit = 0; bit < 6; ++bit) {
      word = static_cast<uint8_t>((word << 1) | (messageBits[i * 6 + bit] & 1u));
    }
    textOut[i] = kAlphabet[word];
  }
  textOut[12] = '\0';
  return true;
}

bool decode(const float llr[174], Workspace& w, Result& result,
            uint8_t maxIterations) {
  result = Result{};
  if (!llr) return false;
  for (int i = 0; i < N; ++i) if (!isfinite(llr[i])) return false;

  memset(&w, 0, sizeof(w));
  for (int check = 0; check < M; ++check) {
    for (int edge = 0; edge < kNm[check].valid_neighbors; ++edge) {
      w.toc[check][edge] = llr[kNm[check].neighbors[edge]];
    }
  }

  for (int iter = 0; iter <= maxIterations; ++iter) {
    for (int bit = 0; bit < N; ++bit) {
      w.zn[bit] = llr[bit] + w.tov[bit][0] +
                  w.tov[bit][1] + w.tov[bit][2];
      w.hard[bit] = w.zn[bit] > 0.0f ? 1u : 0u;
    }

    int unsatisfied = 0;
    for (int check = 0; check < M; ++check) {
      int parity = 0;
      for (int edge = 0; edge < kNm[check].valid_neighbors; ++edge) {
        parity += w.hard[kNm[check].neighbors[edge]];
      }
      if (parity & 1) ++unsatisfied;
    }

    result.iterations = static_cast<uint8_t>(iter);
    result.unsatisfiedChecks = static_cast<uint8_t>(unsatisfied);
    if (unsatisfied == 0) {
      result.converged = true;
      for (int bit = 0; bit < K; ++bit) result.decodedBits[bit] = w.hard[M + bit];
      uint16_t errors = 0;
      for (int bit = 0; bit < N; ++bit) {
        if ((w.hard[bit] ? 1.0f : -1.0f) * llr[bit] < 0.0f) ++errors;
      }
      result.hardErrors = errors;
      return true;
    }

    if (iter > 0) {
      // The upstream early-stop heuristic is intentionally omitted here:
      // this bounded embedded decoder uses the full configured iteration cap.
    }

    for (int check = 0; check < M; ++check) {
      for (int edge = 0; edge < kNm[check].valid_neighbors; ++edge) {
        const int bit = kNm[check].neighbors[edge];
        float message = w.zn[bit];
        for (int k = 0; k < kMaxChecks; ++k) {
          if (kMn[bit][k] == check) message -= w.tov[bit][k];
        }
        w.toc[check][edge] = message;
      }
      for (int edge = 0; edge < kMaxRows; ++edge) {
        w.tanhtoc[check][edge] = tanhf(-w.toc[check][edge] * 0.5f);
      }
    }

    for (int bit = 0; bit < N; ++bit) {
      for (int edge = 0; edge < kMaxChecks; ++edge) {
        const int check = kMn[bit][edge];
        float product = 1.0f;
        for (int other = 0; other < kNm[check].valid_neighbors; ++other) {
          if (kNm[check].neighbors[other] != bit) {
            product *= w.tanhtoc[check][other];
          }
        }
        if (product > kAtanhLimit) product = kAtanhLimit;
        if (product < -kAtanhLimit) product = -kAtanhLimit;
        w.tov[bit][edge] = 2.0f * atanhf(-product);
      }
    }
  }

  return false;
}

} // namespace Js8Ldpc
