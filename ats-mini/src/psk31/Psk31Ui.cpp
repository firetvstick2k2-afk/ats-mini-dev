#include "Psk31Ui.h"
#include "Psk31Service.h"
#include "../audio/AudioCaptureService.h"
#include "../DecoderMenuUi.h"
#include "../../Common.h"
#include "../../Menu.h"
#include "../../Utils.h"
#include <stdio.h>
#include <string.h>

namespace {
struct Psk31Preset {
  const char* label;
  uint32_t frequencyKHz;
  uint8_t modulation;
  uint8_t bandwidthChoice;
};

static constexpr Psk31Preset kPsk31Presets[] = {
  {"160m  PSK31", 1838, USB, 4},
  {"80m   PSK31", 3580, USB, 4},
  {"40m   PSK31", 7070, USB, 4},
  {"30m   PSK31", 10140, USB, 4},
  {"20m   PSK31", 14070, USB, 4},
  {"17m   PSK31", 18100, USB, 4},
  {"15m   PSK31", 21070, USB, 4},
  {"10m   PSK31", 28120, USB, 4},
};
constexpr uint8_t kPsk31PresetCount =
  sizeof(kPsk31Presets) / sizeof(kPsk31Presets[0]);
constexpr uint8_t kActionCount = 6;
constexpr uint32_t kPreviewDelayMs = 2000;

Psk31Service service;
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
uint8_t restoreNetworkMode = NET_OFF;
uint8_t volumeBefore = 0;
bool volumeBoost = false;
uint32_t lastRefresh = 0;

bool tunePreset(const Psk31Preset& preset) {
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
  target.bandMode = preset.modulation;
  target.currentFreq = static_cast<uint16_t>(preset.frequencyKHz);
  target.bandwidthIdx = preset.bandwidthChoice;
  currentBFO = 0;
  selectBand(static_cast<uint8_t>(bestBand), false);
  currentMode = preset.modulation;
  currentFrequency = rx.getFrequency();
  return currentFrequency == preset.frequencyKHz;
}

void restorePreview(bool clearState = true) {
  if (originalValid && previewActive) {
    if (targetBandSnapshotValid && targetBandIndex >= 0 &&
        targetBandIndex < getTotalBands())
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

void commitPreview() {
  originalValid = false;
  targetBandIndex = -1;
  targetBandSnapshotValid = false;
  previewPending = false;
  previewActive = false;
}
}

bool psk31UiOpen() {
  actionsOpen = presetsOpen = false;
  actionSelection = presetSelection = 0;
  previewPending = previewActive = originalValid = false;
  targetBandIndex = -1;
  targetBandSnapshotValid = false;
  restoreNetworkMode = wifiModeIdx;
  netStop();
  if (getWiFiStatus() != 0 || !service.enter(sharedAudioCapture(), true)) {
    service.exit();
    netInit(restoreNetworkMode);
    statusShow("PSK31 start failed", "Check Wi-Fi and audio input");
    return false;
  }
  volumeBefore = volume;
  volumeBoost = volume < 50;
  if (volumeBoost) { volume = 50; rx.setVolume(volume); }
  currentFrequency = rx.getFrequency();
  bands[bandIdx].currentFreq = currentFrequency;
  pageActive = true;
  lastRefresh = 0;
  return true;
}

void psk31UiExit() {
  if (!pageActive) return;
  restorePreview();
  pageActive = false;
  actionsOpen = presetsOpen = false;
  service.exit();
  if (volumeBoost) { volume = volumeBefore; rx.setVolume(volume); volumeBoost = false; }
  netInit(restoreNetworkMode);
  currentCmd = CMD_NONE;
}

void psk31UiRotate(int16_t delta) {
  if (!pageActive || !actionsOpen || !delta) return;
  if (presetsOpen) {
    int next = static_cast<int>(presetSelection) + (delta > 0 ? 1 : -1);
    if (next < 0) next = kPsk31PresetCount - 1;
    if (next >= kPsk31PresetCount) next = 0;
    if (previewActive) restorePreview();
    presetSelection = static_cast<uint8_t>(next);
    schedulePreview(millis());
    return;
  }
  int next = static_cast<int>(actionSelection) + (delta > 0 ? 1 : -1);
  if (next < 0) next = kActionCount - 1;
  if (next >= kActionCount) next = 0;
  actionSelection = static_cast<uint8_t>(next);
}

void psk31UiClick() {
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
      if (!tunePreset(kPsk31Presets[presetSelection])) {
        restorePreview();
        statusShow("PSK31 preset unavailable", "No matching receiver band");
        return;
      }
    }
    if (previewActive) commitPreview();
    else restorePreview();
    presetsOpen = actionsOpen = false;
    return;
  }
  switch (actionSelection) {
    case 0:
      if (!service.setRunning(!service.active()))
        statusShow("PSK31 receiver", "Could not change RX state");
      break;
    case 1:
      presetsOpen = true;
      presetSelection = 0;
      schedulePreview(millis());
      break;
    case 2: service.clearText(); break;
    case 3: service.resetDecoder(); break;
    case 4: actionsOpen = false; break;
    default: psk31UiExit(); break;
  }
}

bool psk31UiTick(uint32_t now) {
  if (!pageActive) return false;
  bool changed = false;
  if (presetsOpen && previewPending &&
      static_cast<uint32_t>(now - previewStartedAt) >= kPreviewDelayMs) {
    previewPending = false;
    previewActive = true;
    if (!tunePreset(kPsk31Presets[presetSelection])) {
      restorePreview(false);
      statusShow("PSK31 preset unavailable", "No matching receiver band");
    }
    changed = true;
  }
  if (static_cast<uint32_t>(now - lastRefresh) >= 100) {
    lastRefresh = now;
    changed = true;
  }
  return changed;
}

void drawPsk31DecoderPage() {
  constexpr uint16_t bg = 0x0000, cyan = 0x07FF, green = 0x07E0, white = 0xFFFF, grey = 0x7BEF;
  spr.fillSprite(bg);
  char frequency[24] = {};
  if (currentFrequency >= 1000) {
    snprintf(frequency, sizeof(frequency), "%lu.%03lu MHz",
             static_cast<unsigned long>(currentFrequency / 1000),
             static_cast<unsigned long>(currentFrequency % 1000));
  } else {
    snprintf(frequency, sizeof(frequency), "%u kHz", static_cast<unsigned>(currentFrequency));
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(white);
  spr.drawString(frequency, 8, 5, &lgfx::fonts::Orbitron_Light_24);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(cyan);
  spr.drawString("BPSK31  31.25 Bd", 312, 12, FONT_SMALL);

  char status[48] = {};
  snprintf(status, sizeof(status), "%s  %s  LVL %u",
           service.active() ? "RUNNING" : "PAUSED",
           service.signal() ? "SIGNAL" : "SEARCH",
           static_cast<unsigned>(service.audioLevel()));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(service.signal() ? green : grey);
  spr.drawString(status, 9, 35, FONT_SMALL);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(grey);
  spr.drawString("TONE 1000 Hz", 311, 35, FONT_SMALL);
  const uint16_t level = service.audioLevel() > 500 ? 500 : service.audioLevel();
  spr.drawRect(8, 50, 304, 9, cyan);
  spr.fillRect(10, 52, 300, 5, bg);
  if (level) spr.fillRect(10, 52, (300 * level) / 500, 5, service.signal() ? green : cyan);

  static char text[256] = {};
  const size_t length = service.copyText(text, sizeof(text));
  constexpr size_t charsPerLine = 32;
  const size_t visible = charsPerLine * 4;
  const size_t first = length > visible ? length - visible : 0;
  const int rowY[4] = {68, 91, 114, 137};
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(green);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  for (uint8_t row = 0; row < 4; ++row) {
    const size_t offset = first + static_cast<size_t>(row) * charsPerLine;
    if (offset >= length) break;
    size_t count = length - offset;
    if (count > charsPerLine) count = charsPerLine;
    char line[charsPerLine + 1] = {};
    memcpy(line, text + offset, count);
    for (size_t i = 0; i < count; ++i) if (line[i] == '\n' || line[i] == '\r') line[i] = ' ';
    spr.drawString(line, 10, rowY[row]);
  }
  if (!length) {
    spr.setTextColor(grey);
    spr.setTextDatum(TL_DATUM);
    spr.setFont(FONT_SMALL);
    spr.drawString(service.active() ? "WAITING FOR PSK31 TEXT..." : "RECEIVER PAUSED", 10, 78);
  }
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(cyan);
  spr.setFont(FONT_SMALL);
  spr.drawString("CLICK: ACTIONS   LONG PRESS: EXIT", 160, 162);

  if (actionsOpen) {
    if (presetsOpen) {
      char storage[kPsk31PresetCount][40] = {};
      const char* labels[kPsk31PresetCount] = {};
      for (uint8_t i = 0; i < kPsk31PresetCount; ++i) {
        snprintf(storage[i], sizeof(storage[i]), "%s  %u.%03u USB",
                 kPsk31Presets[i].label,
                 static_cast<unsigned>(kPsk31Presets[i].frequencyKHz / 1000),
                 static_cast<unsigned>(kPsk31Presets[i].frequencyKHz % 1000));
        labels[i] = storage[i];
      }
      const char* state = previewActive ? "PROBE TUNED" : previewPending ? "2 SEC PROBE..." : "SELECT";
      DecoderMenuUi::draw("PSK31 FREQUENCIES", labels, nullptr, kPsk31PresetCount,
                          presetSelection, "TURN: SELECT", "CLICK: APPLY", state);
    } else {
      const char* labels[kActionCount] = {
        "STOP RX / START RX", "PSK31 FREQUENCIES", "CLEAR TEXT",
        "RESET DECODER", "BACK", "EXIT PSK31"
      };
      const char* values[kActionCount] = {
        service.active() ? "RUNNING" : "PAUSED", nullptr, nullptr, nullptr, nullptr, nullptr
      };
      DecoderMenuUi::draw("PSK31 ACTIONS", labels, values, kActionCount, actionSelection,
                          "TURN: SELECT", "CLICK: OPEN", nullptr, true);
    }
  }
}
