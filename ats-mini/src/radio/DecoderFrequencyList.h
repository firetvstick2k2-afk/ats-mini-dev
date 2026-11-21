#pragma once
#include "Common.h"

// Fixed receiver presets. SI4735 AM/SSB tuning is expressed in whole kHz;
// decimal-kHz carriers are rounded to the nearest receiver kHz.
enum class DecoderKind : uint8_t { None, CW, FT8, FT4, RTTY };

struct DecoderFrequencyPreset {
  const char* label;
  uint32_t frequencyKHz;
  uint8_t modulation;
  uint8_t bandwidthChoice;
  DecoderKind decoder = DecoderKind::None;
  float recommendedBaud = 45.45f;
};

static constexpr DecoderFrequencyPreset kDecoderFrequencyPresets[] = {
  // CW activity
  {"CW 160m QRP", 1836, USB, 0, DecoderKind::CW},
  {"CW 80m QRP", 3560, USB, 0, DecoderKind::CW},
  {"CW 40m QRP", 7030, USB, 0, DecoderKind::CW},
  {"CW 30m QRP", 10116, USB, 0, DecoderKind::CW},
  {"CW 20m QRP", 14060, USB, 0, DecoderKind::CW},
  {"CW 17m", 18096, USB, 0, DecoderKind::CW},
  {"CW 15m", 21060, USB, 0, DecoderKind::CW},
  {"CW 12m", 24906, USB, 0, DecoderKind::CW},
  {"CW 10m", 28060, USB, 0, DecoderKind::CW},

  // FT8
  {"FT8 160m", 1840, USB, 4, DecoderKind::FT8},
  {"FT8 80m", 3573, USB, 4, DecoderKind::FT8},
  {"FT8 60m", 5357, USB, 4, DecoderKind::FT8},
  {"FT8 40m", 7074, USB, 4, DecoderKind::FT8},
  {"FT8 30m", 10136, USB, 4, DecoderKind::FT8},
  {"FT8 20m", 14074, USB, 4, DecoderKind::FT8},
  {"FT8 17m", 18100, USB, 4, DecoderKind::FT8},
  {"FT8 15m", 21074, USB, 4, DecoderKind::FT8},
  {"FT8 12m", 24915, USB, 4, DecoderKind::FT8},
  {"FT8 10m", 28074, USB, 4, DecoderKind::FT8},

  // FT4
  {"FT4 80m", 3575, USB, 4, DecoderKind::FT4},
  {"FT4 40m", 7048, USB, 4, DecoderKind::FT4},
  {"FT4 30m", 10140, USB, 4, DecoderKind::FT4},
  {"FT4 20m", 14080, USB, 4, DecoderKind::FT4},
  {"FT4 17m", 18104, USB, 4, DecoderKind::FT4},
  {"FT4 15m", 21140, USB, 4, DecoderKind::FT4},
  {"FT4 12m", 24919, USB, 4, DecoderKind::FT4},
  {"FT4 10m", 28180, USB, 4, DecoderKind::FT4},

  // RTTY
  {"RTTY 80m", 3580, USB, 4, DecoderKind::RTTY, 45.45f},
  {"RTTY 40m", 7045, USB, 4, DecoderKind::RTTY, 45.45f},
  {"RTTY 20m", 14080, USB, 4, DecoderKind::RTTY, 45.45f},
  {"RTTY 15m", 21080, USB, 4, DecoderKind::RTTY, 45.45f},
  {"RTTY 10m", 28080, USB, 4, DecoderKind::RTTY, 45.45f},

  // DWD Pinneberg: RTTY/weather data and weather fax
  {"PINNEBERG DDH 47", 147, AM, 4}, // Receiver-only preset; below supported RTTY tuning range.
  {"PINNEBERG DDK 2", 4583, USB, 4, DecoderKind::RTTY, 50.0f},
  {"PINNEBERG DDH 7", 7646, USB, 4, DecoderKind::RTTY, 50.0f},
  {"PINNEBERG DDK 9", 10101, USB, 4, DecoderKind::RTTY, 50.0f},
  {"PINNEBERG DDH 9", 11039, USB, 4, DecoderKind::RTTY, 50.0f},
  {"PINNEBERG DDH 8", 14467, USB, 4, DecoderKind::RTTY, 50.0f},
  {"DWD FAX 7.880", 7880, USB, 4},
  {"DWD FAX 13.882", 13883, USB, 4},
  {"DWD FAX 3.855", 3855, USB, 4},

  // NCDXF / IARU international propagation beacons
  {"NCDXF BEACON 20m", 14100, USB, 4},
  {"NCDXF BEACON 17m", 18110, USB, 4},
  {"NCDXF BEACON 15m", 21150, USB, 4},
  {"NCDXF BEACON 12m", 24930, USB, 4},
  {"NCDXF BEACON 10m", 28200, USB, 4},

  // Time and frequency standards
  {"WWV / WWVH 2.5 MHz", 2500, AM, 4},
  {"WWV / WWVH 5 MHz", 5000, AM, 4},
  {"WWV / WWVH 10 MHz", 10000, AM, 4},
  {"WWV / WWVH 15 MHz", 15000, AM, 4},
  {"WWV 20 MHz", 20000, AM, 4},
  {"WWV TEST 25 MHz", 25000, AM, 4},

  // Shortwave broadcast / utility listening points
  {"KURZWELLE 3.950", 3950, AM, 4},
  {"60m BC / UTILITY", 5000, AM, 4},
  {"49m BROADCAST", 6000, AM, 4},
  {"41m BROADCAST", 7300, AM, 4},
  {"31m BROADCAST", 9500, AM, 4},
  {"25m BROADCAST", 11800, AM, 4},
  {"19m BROADCAST", 15100, AM, 4},
  {"16m BROADCAST", 17500, AM, 4},

  // Amateur SSB voice activity
  {"80m SSB QSO", 3700, LSB, 4},
  {"40m SSB QSO", 7100, LSB, 4},
  {"20m SSB QSO", 14200, USB, 4},
  {"17m SSB QSO", 18130, USB, 4},
  {"15m SSB QSO", 21285, USB, 4},
  {"12m SSB QSO", 24950, USB, 4},
  {"10m SSB QSO", 28400, USB, 4},

  // Well-known voice / marker stations and signal-hunting spots
  {"UVB-76 / BUZZER", 4625, USB, 4},
  {"V32 NUMBERS", 7910, USB, 4},
  {"V32 ALTERNATE", 7842, USB, 4},
  {"HF DATA HUNT 10.150", 10150, USB, 4},
  {"HF DATA HUNT 14.070", 14070, USB, 4},
  {"HF DATA HUNT 21.070", 21070, USB, 4},

  // CB
  {"CB KANAL 24", 27235, AM, 4},
  {"CB KANAL 25", 27245, AM, 4},
};

// Category ranges keep the original preset order intact.
struct DecoderFrequencyCategory {
  const char* name;
  const char* nameDe;
  uint8_t firstPreset;
  uint8_t presetCount;
};

static constexpr DecoderFrequencyCategory kDecoderFrequencyCategories[] = {
  {"CW", "CW", 0, 9},
  {"FT8", "FT8", 9, 10},
  {"FT4", "FT4", 19, 8},
  {"RTTY", "RTTY", 27, 5},
  {"DWD / Pinneberg", "DWD / Pinneberg", 32, 9},
  {"NCDXF Beacons", "NCDXF-Baken", 41, 5},
  {"Time Signals", "Zeitzeichen", 46, 6},
  {"Shortwave Broadcast", "Kurzwellen-Rundfunk", 52, 8},
  {"SSB QSO", "SSB-QSO", 60, 7},
  {"Signal / Data", "Signal / Datenfunk", 67, 6},
  {"CB", "CB-Funk", 73, 2},
};
static constexpr uint8_t kDecoderFrequencyCategoryCount =
  sizeof(kDecoderFrequencyCategories) / sizeof(kDecoderFrequencyCategories[0]);

static constexpr uint8_t kDecoderFrequencyPresetCount =
  sizeof(kDecoderFrequencyPresets) / sizeof(kDecoderFrequencyPresets[0]);
