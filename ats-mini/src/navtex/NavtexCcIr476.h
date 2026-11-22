#pragma once

#include <Arduino.h>

// CCIR-476 character decoder used by NAVTEX/SITOR-B.
// The seven received bits are packed LSB-first into a 7-bit word.
// Every valid word contains exactly four mark bits.
class NavtexCcIr476 {
public:
  static constexpr uint8_t kLetters = 0x5A;
  static constexpr uint8_t kFigures = 0x36;
  static constexpr uint8_t kAlpha = 0x0F;
  static constexpr uint8_t kBeta = 0x33;
  static constexpr uint8_t kRep = 0x66;

  enum class CopySource : uint8_t { None, Dx, Rx };

  void reset() { figures_ = false; }

  static bool isValid(uint8_t code) {
    code &= 0x7F;
    uint8_t bits = 0;
    for (uint8_t i = 0; i < 7; ++i) bits += (code >> i) & 1U;
    return bits == 4;
  }

  // Prefer a valid direct DX copy; use the earlier RX repeat only if DX
  // is invalid. Both inputs must already represent the same character.
  static CopySource recover(uint8_t dx, uint8_t rx, uint8_t& recovered) {
    if (isValid(dx)) {
      recovered = dx & 0x7F;
      return CopySource::Dx;
    }
    if (isValid(rx)) {
      recovered = rx & 0x7F;
      return CopySource::Rx;
    }
    recovered = 0;
    return CopySource::None;
  }

  bool decode(uint8_t rawCode, char& output) {
    const uint8_t code = rawCode & 0x7F;
    if (!isValid(code)) return false;

    if (code == kLetters) { figures_ = false; return false; }
    if (code == kFigures) { figures_ = true; return false; }
    if (code == kAlpha || code == kBeta || code == kRep || code == 0x6A) return false;

    const char ch = figures_ ? figureFor(code) : letterFor(code);
    if (!ch) return false;
    output = ch;
    return true;
  }

  bool figuresMode() const { return figures_; }

  // Test-signal encoder: find the canonical CCIR-476 word for a glyph.
  static bool encode(char ch, bool figures, uint8_t& code) {
    for (uint8_t candidate = 0; candidate < 128; ++candidate) {
      const char glyph = figures ? figureFor(candidate) : letterFor(candidate);
      if (glyph == ch) { code = candidate; return true; }
    }
    return false;
  }

private:
  static char letterFor(uint8_t code) {
    switch (code) {
      case 0x47: return 'A'; case 0x72: return 'B';
      case 0x1D: return 'C'; case 0x53: return 'D';
      case 0x56: return 'E'; case 0x1B: return 'F';
      case 0x35: return 'G'; case 0x69: return 'H';
      case 0x4D: return 'I'; case 0x17: return 'J';
      case 0x1E: return 'K'; case 0x65: return 'L';
      case 0x39: return 'M'; case 0x59: return 'N';
      case 0x71: return 'O'; case 0x2D: return 'P';
      case 0x2E: return 'Q'; case 0x55: return 'R';
      case 0x4B: return 'S'; case 0x74: return 'T';
      case 0x4E: return 'U'; case 0x3C: return 'V';
      case 0x27: return 'W'; case 0x3A: return 'X';
      case 0x2B: return 'Y'; case 0x63: return 'Z';
      case 0x5C: return ' ';
      case 0x78: return '\r'; case 0x6C: return '\n';
      default: return 0;
    }
  }

  static char figureFor(uint8_t code) {
    switch (code) {
      case 0x2D: return '0'; case 0x2E: return '1';
      case 0x27: return '2'; case 0x56: return '3';
      case 0x55: return '4'; case 0x74: return '5';
      case 0x2B: return '6'; case 0x4E: return '7';
      case 0x4D: return '8'; case 0x71: return '9';
      case 0x17: return '\''; case 0x1B: return '!';
      case 0x1D: return ':'; case 0x1E: return '(';
      case 0x35: return '&'; case 0x39: return '.';
      case 0x3A: return '/'; case 0x3C: return '=';
      case 0x47: return '-'; case 0x53: return '$';
      case 0x59: return ','; case 0x63: return '+';
      case 0x65: return ')'; case 0x69: return '#';
      case 0x72: return '?'; case 0x5C: return ' ';
      case 0x78: return '\r'; case 0x6C: return '\n';
      default: return 0;
    }
  }

  bool figures_ = false;
};
