#pragma once

#include <Arduino.h>

// NAVTEX receiver presets supported by the ATS Mini SI4735 tuning interface.
// The receiver API accepts whole-kHz values; 4209.5 kHz is intentionally
// omitted until half-kHz tuning can be represented without rounding.
struct NavtexPreset {
  const char* label;
  uint16_t frequencyKHz;
  const char* service;
};

static constexpr NavtexPreset kNavtexPresets[] = {
  {"NAVTEX INTERNATIONAL", 518, "518 kHz / International / English"},
  {"NAVTEX NATIONAL", 490, "490 kHz / National"}
};

static constexpr uint8_t kNavtexPresetCount =
  sizeof(kNavtexPresets) / sizeof(kNavtexPresets[0]);
