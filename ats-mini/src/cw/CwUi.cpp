#include "CwUi.h"
#include "../DecoderMenuUi.h"
#include "../DecoderPageLayout.h"
#include "CwDecoderService.h"
#include "../audio/AudioCaptureService.h"
#include "Common.h"
#include <esp32-hal-cpu.h>
#include "Menu.h"
#include "Utils.h"
#include "../radio/DecoderFrequencyList.h"
#include <stdio.h>
#include <string.h>

namespace {
CwDecoderService service;
bool pageActive = false;
bool spectrumMode = false;
uint32_t savedCpuMHz = 80;
bool spectrumCpuActive = false;
void setCwSpectrumMode(bool enabled) { if(enabled==spectrumMode) return; spectrumMode=enabled; if(enabled) { savedCpuMHz=getCpuFrequencyMhz(); if(!setCpuFrequencyMhz(160)) { spectrumMode=false; return; } spectrumCpuActive=true; } else if(spectrumCpuActive) { setCpuFrequencyMhz(savedCpuMHz); spectrumCpuActive=false; } }
bool menuOpen = false;
bool menuEditing = false;
bool frequencyMenuOpen = false;
uint8_t frequencySelection = 0;
uint8_t frequencyScroll = 0;
uint8_t menuIndex = 0;
uint8_t restoreNetworkMode = NET_OFF;
uint8_t volumeBeforeCw = 0;
bool volumeBoostActive = false;
uint32_t lastRefresh = 0;
bool previewPending = false, previewActive = false, previewOriginalValid = false;
uint32_t previewStartedAt = 0;
VfoState previewOriginalVfo{};
int16_t previewOriginalBfo = 0;
int previewTargetBandIndex = -1;
Band previewTargetBandOriginal{};
bool previewTargetBandSnapshotValid = false;
bool previewSpeakerWasEnabled = true, previewSpeakerForced = false;
bool previewSelectionMuted = false;
bool previewAmpWasEnabled = true, previewAmpForced = false;
constexpr uint32_t kCwPresetPreviewDelayMs = 2000;
constexpr uint8_t kMenuCount = 6;
uint8_t cwPresetCount() { uint8_t n=0; for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i) if(kDecoderFrequencyPresets[i].decoder==DecoderKind::CW) ++n; return n; }
const DecoderFrequencyPreset* cwPresetAt(uint8_t wanted) { uint8_t n=0; for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i) if(kDecoderFrequencyPresets[i].decoder==DecoderKind::CW && n++==wanted) return &kDecoderFrequencyPresets[i]; return nullptr; }
float cwToneForCurrentFilter() {
  if (currentMode == USB || currentMode == LSB) {
    const uint8_t filter = getCurrentBandwidth()->idx;
    if (filter == 4) return 500.0f;
    if (filter == 5) return 1000.0f;
  }
  return 700.0f;
}
bool applyCwPreset(const DecoderFrequencyPreset& preset) { int best=-1; uint16_t span=UINT16_MAX; for(int i=0;i<getTotalBands();++i) if(bands[i].bandType!=FM_BAND_TYPE && preset.frequencyKHz>=bands[i].minimumFreq && preset.frequencyKHz<=bands[i].maximumFreq) { uint16_t width=bands[i].maximumFreq-bands[i].minimumFreq; if(width<span){span=width;best=i;} } if(best<0)return false; bands[best].currentFreq=static_cast<uint16_t>(preset.frequencyKHz); bands[best].bandMode=preset.modulation; bands[best].bandwidthIdx=preset.bandwidthChoice; selectBand(static_cast<uint8_t>(best),false); currentMode=preset.modulation; currentFrequency=rx.getFrequency(); service.setTone(cwToneForCurrentFilter()); service.resetDecoder(); return true; }
void restoreCwPreviewAudio() {
  if(previewSpeakerForced || previewSelectionMuted) {
    digitalWrite(AUDIO_MUTE,previewSpeakerWasEnabled?LOW:HIGH);
    previewSpeakerForced=false;
    previewSelectionMuted=false;
  }
  if(previewAmpForced) { if(PIN_AMP_EN>=0) digitalWrite(PIN_AMP_EN,previewAmpWasEnabled?HIGH:LOW); previewAmpForced=false; }
}
void clearCwPreview() {
  previewPending=false; previewActive=false; previewOriginalValid=false;
  previewStartedAt=0; previewTargetBandIndex=-1; previewTargetBandSnapshotValid=false;
  restoreCwPreviewAudio();
}
void restoreCwPreview(bool clearState=true) {
  if(previewOriginalValid && previewActive) {
    if(previewTargetBandSnapshotValid && previewTargetBandIndex>=0 && previewTargetBandIndex<getTotalBands())
      bands[previewTargetBandIndex]=previewTargetBandOriginal;
    Band& saved=bands[previewOriginalVfo.band];
    saved.currentFreq=previewOriginalVfo.freq;
    saved.bandMode=previewOriginalVfo.mode;
    saved.currentStepIdx=previewOriginalVfo.stepIdx;
    saved.bandwidthIdx=previewOriginalVfo.bandwidthIdx;
    selectBand(previewOriginalVfo.band,false);
    if(currentBFO!=previewOriginalBfo) updateBFO(previewOriginalBfo,true);
    service.setTone(cwToneForCurrentFilter());
  }
  if(clearState) clearCwPreview();
  else { previewPending=false; previewActive=false; previewStartedAt=0; restoreCwPreviewAudio(); }
}
void scheduleCwPreview(uint32_t now) {
  if(!previewOriginalValid) {
    previewOriginalVfo=captureVfo(); previewOriginalBfo=currentBFO; previewOriginalValid=true;
  }
  previewPending=true; previewStartedAt=now;
}
void startCwPreview() {
  if(previewTargetBandSnapshotValid && previewTargetBandIndex>=0 && previewTargetBandIndex<getTotalBands()) {
    bands[previewTargetBandIndex]=previewTargetBandOriginal;
    previewTargetBandIndex=-1;
    previewTargetBandSnapshotValid=false;
  }
  const DecoderFrequencyPreset* preset=cwPresetAt(frequencySelection-1);
  if(!preset || !previewOriginalValid) { previewPending=false; return; }
  int best=-1; uint16_t span=UINT16_MAX;
  for(int i=0;i<getTotalBands();++i) if(bands[i].bandType!=FM_BAND_TYPE &&
      preset->frequencyKHz>=bands[i].minimumFreq && preset->frequencyKHz<=bands[i].maximumFreq) {
    uint16_t width=bands[i].maximumFreq-bands[i].minimumFreq;
    if(width<span) { span=width; best=i; }
  }
  if(best<0) { previewPending=false; return; }
  previewTargetBandIndex=best; previewTargetBandOriginal=bands[best]; previewTargetBandSnapshotValid=true;
  previewActive=true;
  if(!applyCwPreset(*preset)) { restoreCwPreview(false); return; }
  previewPending=false;
  if(PIN_AMP_EN>=0 && digitalRead(PIN_AMP_EN)!=HIGH) {
    previewAmpWasEnabled=false; previewAmpForced=true; digitalWrite(PIN_AMP_EN,HIGH);
  }
  if(digitalRead(AUDIO_MUTE)!=LOW && !previewSelectionMuted && !previewSpeakerForced) {
    previewSpeakerWasEnabled=false; previewSpeakerForced=true;
  }
  previewSelectionMuted=false;
  digitalWrite(AUDIO_MUTE,LOW);
}
void commitCwPreview() {
  restoreCwPreviewAudio(); previewOriginalValid=false; previewTargetBandIndex=-1;
  previewTargetBandSnapshotValid=false; previewPending=false; previewActive=false; previewStartedAt=0;
}

}

bool cwUiOpen() {
  restoreNetworkMode = wifiModeIdx;
  netStop();
  volumeBeforeCw = volume;
  volumeBoostActive = volume < 50;
  if(volumeBoostActive) { volume = 50; rx.setVolume(volume); }
  currentFrequency = rx.getFrequency();
  bands[bandIdx].currentFreq = currentFrequency;
  if (getWiFiStatus() != 0 || !service.enter(sharedAudioCapture(), true)) {
    service.exit();
    if(volumeBoostActive) { volume = volumeBeforeCw; rx.setVolume(volume); volumeBoostActive = false; }
    netInit(restoreNetworkMode);
    statusShow("CW decoder start failed", "Check Wi-Fi and audio input");
    return false;
  }
  service.setTone(cwToneForCurrentFilter());
  menuOpen = false;
  menuEditing = false;
  frequencyMenuOpen = false;
  clearCwPreview();
  frequencySelection = frequencyScroll = 0;
  menuIndex = 0;
  pageActive = true;
  lastRefresh = 0;
  return true;
}

void cwUiExit() {
  if (!pageActive) return;
  pageActive = false;
  menuOpen = false;
  menuEditing = false;
  restoreCwPreview();
  frequencyMenuOpen = false;
  service.exit();
  if(volumeBoostActive) { volume = volumeBeforeCw; rx.setVolume(volume); volumeBoostActive = false; }
  // Publish the final receiver frequency after a preset preview was committed or restored.
  currentFrequency = rx.getFrequency();
  bands[bandIdx].currentFreq = currentFrequency;
  netInit(restoreNetworkMode);
  currentCmd = CMD_NONE;
}

void cwUiRotate(int16_t delta) {
  if (!pageActive || !delta) return;
  if (!menuOpen) {
    // In the normal CW decoder view, the encoder performs RF fine tuning.
    const int step = static_cast<int>(getCurrentStep()->step);
    if (step > 0 && updateFrequency(static_cast<int>(currentFrequency) + delta * step, false)) {
      clearStationInfo();
      identifyFrequency(currentFrequency + currentBFO / 1000);
    }
    return;
  }
  if(frequencyMenuOpen) { if(previewActive) { digitalWrite(AUDIO_MUTE,HIGH); previewSelectionMuted=true; } int count=cwPresetCount()+1; int next=static_cast<int>(frequencySelection)+(delta>0?1:-1); if(next<0)next=count-1; if(next>=count)next=0; frequencySelection=static_cast<uint8_t>(next); if(frequencySelection<frequencyScroll)frequencyScroll=frequencySelection; frequencyScroll=DecoderMenuUi::firstVisible(frequencySelection,static_cast<uint8_t>(count)); if(frequencySelection) scheduleCwPreview(millis()); return; }
  int next = static_cast<int>(menuIndex) + delta;
  while (next < 0) next += kMenuCount;
  menuIndex = static_cast<uint8_t>(next % kMenuCount);
}

void cwUiClick() {
  if (!pageActive) return;
  if (!menuOpen) {
    menuOpen = true;
    menuEditing = false;
    menuIndex = 0;
    return;
  }
  if(frequencyMenuOpen) { if(frequencySelection==0) { restoreCwPreview(); service.resetDecoder(); } else { const DecoderFrequencyPreset* preset=cwPresetAt(frequencySelection-1); bool applied=previewActive && !previewPending; if(!applied && preset) { if(previewTargetBandSnapshotValid && previewTargetBandIndex>=0 && previewTargetBandIndex<getTotalBands()) { bands[previewTargetBandIndex]=previewTargetBandOriginal; previewTargetBandIndex=-1; previewTargetBandSnapshotValid=false; } applied=applyCwPreset(*preset); previewActive=true; } if(applied) commitCwPreview(); else { restoreCwPreview(); statusShow("CW preset failed","No matching receiver band"); } } frequencyMenuOpen=false; return; }
  switch (menuIndex) { case 0: frequencyMenuOpen=true; frequencySelection=frequencyScroll=0; break; case 1: service.clearText(); break; case 2: service.resetDecoder(); break; case 3: setCwSpectrumMode(!spectrumMode); break; case 4: menuOpen=false; break; case 5: cwUiExit(); break; }
}

bool cwUiTick(uint32_t now) {
  if (!pageActive) return false;
  bool changed=false;
  if(frequencyMenuOpen && previewPending &&
     static_cast<uint32_t>(now-previewStartedAt)>=kCwPresetPreviewDelayMs) {
    startCwPreview();
    changed=true;
  }
  const uint32_t interval=spectrumMode?33:100;
  if(static_cast<uint32_t>(now-lastRefresh)<interval) return changed;
  lastRefresh = now;
  service.update();
  if(spectrumMode) service.updateSpectrum();
  return true;
}

void drawCwDecoderPage() {
  const bool active=service.signal();
  char status[32];
  snprintf(status,sizeof(status),"%s  %d WPM",active?"CW":"SEARCH",service.wpm());
  const uint16_t level=service.audioLevel()>500?500:service.audioLevel();
  DecoderPageLayout::draw(currentFrequency,status,service.text(),
                           "WAITING FOR CW SIGNAL...",level,500,spectrumMode?service.spectrumBins():nullptr);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(0x07FF);
  spr.setFont(FONT_SMALL);
  spr.drawString("CLICK: ACTIONS   LONG PRESS: EXIT",160,162);

  if(menuOpen) {
    if(frequencyMenuOpen) {
      const uint8_t count = cwPresetCount() + 1;
      char storage[kDecoderFrequencyPresetCount + 1][40] = {};
      const char* labels[kDecoderFrequencyPresetCount + 1] = {};
      for(uint8_t i = 0; i < count; ++i) {
        if(i == 0) {
          snprintf(storage[i], sizeof(storage[i]), "CURRENT  %lu.%03lu USB",
                   static_cast<unsigned long>(currentFrequency / 1000),
                   static_cast<unsigned long>(currentFrequency % 1000));
        } else {
          const DecoderFrequencyPreset* preset = cwPresetAt(i - 1);
          if(!preset) continue;
          const char* name = preset->label;
          if(!strncmp(name, "CW ", 3)) name += 3;
          snprintf(storage[i], sizeof(storage[i]), "%s  %lu.%03lu %s", name,
                   static_cast<unsigned long>(preset->frequencyKHz / 1000),
                   static_cast<unsigned long>(preset->frequencyKHz % 1000),
                   preset->modulation == USB ? "USB" : preset->modulation == LSB ? "LSB" : "AM");
        }
        labels[i] = storage[i];
      }
      DecoderMenuUi::draw("CW FREQUENCIES", labels, nullptr, count, frequencySelection,
                          "TURN: SELECT", "CLICK: TUNE",
                          frequencySelection == 0 ? "CURRENT" : "PRESET");
    } else {
      const char* labels[kMenuCount] = {
        "CW FREQUENCIES", "CLEAR TEXT", "RESET DECODER", "SPECTRUM", "BACK", "EXIT CW"
      };
      const char* values[kMenuCount] = {nullptr,nullptr,nullptr,spectrumMode?"ON":"OFF",nullptr,nullptr};
      DecoderMenuUi::draw("CW ACTIONS", labels, values, kMenuCount, menuIndex,
                          "TURN: SELECT", "CLICK: OPEN", nullptr, true);
    }
  }
}
