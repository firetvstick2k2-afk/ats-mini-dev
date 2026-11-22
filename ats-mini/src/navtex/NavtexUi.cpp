#include "NavtexUi.h"
#include "NavtexPresetList.h"
#include "../DecoderMenuUi.h"
#include "../../Common.h"
#include "../../Menu.h"
#include <string.h>
#include <stdio.h>

namespace {
bool pageActive = false;
bool actionsOpen = false;
bool presetsOpen = false;
uint8_t actionSelection = 0;
uint8_t presetSelection = 0;
uint32_t previewStartedAt = 0;
bool previewPending = false;
bool previewActive = false;
VfoState originalVfo{};
int16_t originalBfo = 0;
bool originalValid = false;
int targetBandIndex = -1;
Band targetBandOriginal{};
bool targetBandSnapshotValid = false;
constexpr uint32_t kPreviewDelayMs = 2000;

bool tunePreset(const NavtexPreset& preset) {
  int bestBand = -1;
  uint32_t bestWidth = UINT32_MAX;
  for (int i = 0; i < getTotalBands(); ++i) {
    const Band& candidate = bands[i];
    if (candidate.bandType == FM_BAND_TYPE ||
        preset.frequencyKHz < candidate.minimumFreq ||
        preset.frequencyKHz > candidate.maximumFreq) continue;
    const uint32_t width = candidate.maximumFreq - candidate.minimumFreq;
    if (width < bestWidth) { bestWidth = width; bestBand = i; }
  }
  if (bestBand < 0) return false;
  targetBandIndex = bestBand;
  targetBandOriginal = bands[bestBand];
  targetBandSnapshotValid = true;
  Band& target = bands[bestBand];
  target.bandMode = AM;
  target.currentFreq = preset.frequencyKHz;
  // Retain the target band's valid AM bandwidth and tuning step.
  currentBFO = 0;
  selectBand(static_cast<uint8_t>(bestBand), false);
  clearStationInfo();
  identifyFrequency(currentFrequency);
  return currentFrequency == preset.frequencyKHz;
}

void restorePreview(bool clearState = true) {
  if (originalValid && previewActive) {
    if (targetBandSnapshotValid && targetBandIndex >= 0 && targetBandIndex < getTotalBands())
      bands[targetBandIndex] = targetBandOriginal;
    Band& saved = bands[originalVfo.band];
    saved.currentFreq = originalVfo.freq;
    saved.bandMode = originalVfo.mode;
    saved.currentStepIdx = originalVfo.stepIdx;
    saved.bandwidthIdx = originalVfo.bandwidthIdx;
    selectBand(originalVfo.band, false);
    if (currentBFO != originalBfo) updateBFO(originalBfo, true);
  }
  previewPending = false;
  previewActive = false;
  if (clearState) {
    originalValid = false;
    targetBandIndex = -1;
    targetBandSnapshotValid = false;
  }
}

void schedulePreview(uint32_t now) {
  if (!originalValid) {
    originalVfo = captureVfo();
    originalBfo = currentBFO;
    originalValid = true;
  }
  previewPending = true;
  previewActive = false;
  previewStartedAt = now;
}
}

bool navtexUiOpen() {
  actionsOpen = presetsOpen = false;
  actionSelection = presetSelection = 0;
  previewPending = previewActive = originalValid = false;
  pageActive = true;
  return true;
}

void navtexUiExit() {
  if (!pageActive) return;
  restorePreview();
  pageActive = false;
  actionsOpen = presetsOpen = false;
  currentCmd = CMD_NONE;
}

void navtexUiRotate(int delta) {
  if (!pageActive || !actionsOpen || !delta) return;
  if (presetsOpen) {
    const int next = (static_cast<int>(presetSelection) + (delta > 0 ? 1 : -1) +
                      kNavtexPresetCount) % kNavtexPresetCount;
    if (previewActive) restorePreview();
    presetSelection = static_cast<uint8_t>(next);
    schedulePreview(millis());
  } else {
    actionSelection = static_cast<uint8_t>((actionSelection + (delta > 0 ? 1 : 2)) % 3);
  }
}

void navtexUiClick() {
  if (!pageActive) return;
  if (!actionsOpen) {
    actionsOpen = true;
    actionSelection = 0;
    return;
  }
  if (presetsOpen) {
    if (previewPending && !previewActive) {
      previewPending = false;
      previewActive = true;
      if (!tunePreset(kNavtexPresets[presetSelection])) {
        restorePreview();
        statusShow("NAVTEX preset unavailable", "Receiver band does not cover frequency");
        return;
      }
    }
    if (previewActive) {
      // Keep the selected frequency and discard the rollback snapshot.
      originalValid = false;
      targetBandIndex = -1;
      targetBandSnapshotValid = false;
    } else {
      restorePreview();
    }
    presetsOpen = actionsOpen = false;
    return;
  }
  switch (actionSelection) {
    case 0:
      presetsOpen = true;
      presetSelection = 0;
      schedulePreview(millis());
      break;
    case 1:
      actionsOpen = false;
      break;
    default:
      navtexUiExit();
      break;
  }
}

bool navtexUiTick(uint32_t now) {
  if (!pageActive || !presetsOpen || !previewPending ||
      static_cast<uint32_t>(now - previewStartedAt) < kPreviewDelayMs) return false;
  previewPending = false;
  previewActive = true; // Provisional before attempting the tune, so failures roll back.
  if (!tunePreset(kNavtexPresets[presetSelection])) {
    restorePreview(false);
    statusShow("NAVTEX preset unavailable", "Receiver band does not cover frequency");
  }
  return true;
}

void drawNavtexPage() {
  spr.fillSprite(0x0000);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(0x07FF);
  spr.setFont(FONT_SMALL);
  spr.drawString("NAVTEX RECEIVER", 160, 10);
  spr.setTextColor(0xFFFF);
  spr.setFont(FONT_DEFAULT);
  spr.drawString("SITOR-B / 518 kHz", 160, 42);
  spr.setTextColor(0x7BEF);
  spr.drawString("Receiver integration in progress", 160, 78);
  spr.drawString("CLICK: ACTIONS", 160, 130);

  if (!actionsOpen) return;
  if (presetsOpen) {
    char labels[kNavtexPresetCount][40] = {};
    const char* entries[kNavtexPresetCount] = {};
    for (uint8_t i = 0; i < kNavtexPresetCount; ++i) {
      snprintf(labels[i], sizeof(labels[i]), "%s  %u kHz",
               kNavtexPresets[i].label, static_cast<unsigned>(kNavtexPresets[i].frequencyKHz));
      entries[i] = labels[i];
    }
    const char* state = previewActive ? "PROBE TUNED" : previewPending ? "2 SEC PROBE..." : "SELECT";
    DecoderMenuUi::draw("NAVTEX PRESETS", entries, nullptr, kNavtexPresetCount,
                        presetSelection, "TURN: SELECT", "CLICK: APPLY", state);
  } else {
    const char* labels[] = {"NAVTEX PRESETS", "BACK", "EXIT NAVTEX"};
    DecoderMenuUi::draw("NAVTEX ACTIONS", labels, nullptr, 3, actionSelection,
                        "TURN: SELECT", "CLICK: OPEN");
  }
}
