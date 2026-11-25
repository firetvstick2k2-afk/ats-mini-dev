#include "RttyUi.h"
#include "../DecoderMenuUi.h"
#include "../DecoderPageLayout.h"
#include "RttyService.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include "../radio/DecoderFrequencyList.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <esp32-hal-cpu.h>

extern int16_t currentBFO;
extern bool updateBFO(int newBFO, bool wrap);

namespace {
RttyService service;
bool pageActive=false;
bool spectrumMode=false;
uint32_t savedCpuMHz=80;
bool spectrumCpuActive=false;
void setRttySpectrumMode(bool enabled) { if(enabled==spectrumMode) return; spectrumMode=enabled; if(enabled) { savedCpuMHz=getCpuFrequencyMhz(); if(!setCpuFrequencyMhz(160)) { spectrumMode=false; return; } spectrumCpuActive=true; } else if(spectrumCpuActive) { setCpuFrequencyMhz(savedCpuMHz); spectrumCpuActive=false; } }
bool menuOpen=false;
bool menuEditing=false;
bool presetMenuOpen=false;
uint8_t menuIndex=0;
uint8_t presetSelection=0;
uint8_t presetScroll=0;
constexpr uint32_t kRttyPresetPreviewDelayMs=2000;
VfoState presetOriginalState{};
int16_t presetOriginalBfo=0;
bool presetOriginalValid=false;
Band presetTargetBandOriginal{};
int presetTargetBandIndex=-1;
bool presetTargetBandSnapshotValid=false;
bool presetPreviewPending=false;
bool presetPreviewActive=false;
uint32_t presetPreviewStartedAt=0;
bool presetPreviewSpeakerWasEnabled=true;
bool presetPreviewSpeakerForced=false;
bool presetPreviewAmpWasEnabled=true;
bool presetPreviewAmpForced=false;
bool presetPreviewSelectionMuted=false;
uint8_t restoreNetworkMode=NET_OFF;
uint32_t lastRefresh=0;
uint32_t lastAutoBfoAdjust=0;
uint8_t baudIndex=0;
const float baudChoices[3]={45.45f,50.0f,75.0f};
const char* const polarityNames[3]={"AUTO","NORMAL","REVERSE"};
uint8_t polarityIndex=0;
bool bfoAuto=true;
uint8_t volumeBefore=0;
bool volumeBoost=false;
int16_t bfoAtEntry=0;
constexpr uint8_t kPlaybackVolume=50;
constexpr int kBfoLimit=250;
constexpr int kManualBfoStep=5;
constexpr float kTargetToneCenterHz=2210.0f;
constexpr float kAutoDeadbandHz=18.0f;
constexpr uint8_t kMenuCount=9;
void applyBfo(int value) {
  int low=bfoAtEntry-kBfoLimit, high=bfoAtEntry+kBfoLimit;
  if(value<low) value=low;
  if(value>high) value=high;
  updateBFO(value,true);
}

uint8_t rttyPresetCount() {
  uint8_t count=0;
  for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i)
    if(kDecoderFrequencyPresets[i].decoder==DecoderKind::RTTY) ++count;
  return count;
}

const DecoderFrequencyPreset* rttyPresetAt(uint8_t wanted) {
  uint8_t index=0;
  for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i) {
    if(kDecoderFrequencyPresets[i].decoder!=DecoderKind::RTTY) continue;
    if(index++==wanted) return &kDecoderFrequencyPresets[i];
  }
  return nullptr;
}

void restoreRttyPresetPreviewSpeaker() {
  if(presetPreviewSpeakerForced || presetPreviewSelectionMuted) {
    digitalWrite(AUDIO_MUTE,presetPreviewSpeakerWasEnabled?LOW:HIGH);
    presetPreviewSpeakerForced=false;
    presetPreviewSelectionMuted=false;
  }
  if(presetPreviewAmpForced) {
    if(PIN_AMP_EN>=0) digitalWrite(PIN_AMP_EN,presetPreviewAmpWasEnabled?HIGH:LOW);
    presetPreviewAmpForced=false;
  }
}

void clearRttyPresetPreviewState() {
  presetOriginalValid=false;
  presetTargetBandIndex=-1;
  presetTargetBandSnapshotValid=false;
  presetPreviewPending=false;
  presetPreviewActive=false;
  presetPreviewStartedAt=0;
  restoreRttyPresetPreviewSpeaker();
}

void restoreRttyPresetOriginal(bool clearSnapshot=true) {
  if(presetOriginalValid && presetPreviewActive) {
    if(presetTargetBandSnapshotValid && presetTargetBandIndex>=0 &&
       presetTargetBandIndex<getTotalBands())
      bands[presetTargetBandIndex]=presetTargetBandOriginal;
    Band& savedBand=bands[presetOriginalState.band];
    savedBand.currentFreq=presetOriginalState.freq;
    savedBand.bandMode=presetOriginalState.mode;
    savedBand.currentStepIdx=presetOriginalState.stepIdx;
    savedBand.bandwidthIdx=presetOriginalState.bandwidthIdx;
    selectBand(presetOriginalState.band,false);
    if(currentBFO!=presetOriginalBfo) updateBFO(presetOriginalBfo,true);
  }
  if(clearSnapshot) clearRttyPresetPreviewState();
  else {
    presetPreviewPending=false;
    presetPreviewActive=false;
    presetPreviewStartedAt=0;
    restoreRttyPresetPreviewSpeaker();
  }
}

void scheduleRttyPresetPreview(uint32_t now) {
  if(!presetOriginalValid) {
    presetOriginalState=captureVfo();
    presetOriginalBfo=currentBFO;
    presetOriginalValid=true;
  }
  presetPreviewPending=true;
  presetPreviewStartedAt=now;
}

bool applyRttyPreset(const DecoderFrequencyPreset& preset) {
  int bestBand=-1;
  uint16_t bestSpan=UINT16_MAX;
  for(int i=0;i<getTotalBands();++i) {
    if(bands[i].bandType==FM_BAND_TYPE ||
       preset.frequencyKHz<bands[i].minimumFreq ||
       preset.frequencyKHz>bands[i].maximumFreq) continue;
    const uint16_t span=bands[i].maximumFreq-bands[i].minimumFreq;
    if(span<bestSpan) { bestSpan=span; bestBand=i; }
  }
  if(bestBand<0) return false;

  presetTargetBandIndex=bestBand;
  presetTargetBandOriginal=bands[bestBand];
  presetTargetBandSnapshotValid=true;
  bands[bestBand].currentFreq=static_cast<uint16_t>(preset.frequencyKHz);
  bands[bestBand].bandMode=preset.modulation;
  bands[bestBand].bandwidthIdx=preset.bandwidthChoice;
  currentBFO=0;

  selectBand(static_cast<uint8_t>(bestBand),false);

  currentMode=preset.modulation;
  currentFrequency=rx.getFrequency();
  return currentFrequency==preset.frequencyKHz;
}

void startRttyPresetPreview() {
  if(presetTargetBandSnapshotValid && presetTargetBandIndex>=0 && presetTargetBandIndex<getTotalBands()) {
    bands[presetTargetBandIndex]=presetTargetBandOriginal;
    presetTargetBandIndex=-1;
    presetTargetBandSnapshotValid=false;
  }
  const DecoderFrequencyPreset* preset=rttyPresetAt(presetSelection);
  if(!preset || !presetOriginalValid) {
    presetPreviewPending=false;
    return;
  }
  presetPreviewActive=true;
  if(!applyRttyPreset(*preset)) {
    restoreRttyPresetOriginal(false);
    return;
  }
  presetPreviewPending=false;
  if(PIN_AMP_EN>=0 && digitalRead(PIN_AMP_EN)!=HIGH) {
    presetPreviewAmpWasEnabled=false;
    presetPreviewAmpForced=true;
    digitalWrite(PIN_AMP_EN,HIGH);
  }
  if(digitalRead(AUDIO_MUTE)!=LOW && !presetPreviewSelectionMuted && !presetPreviewSpeakerForced) {
    presetPreviewSpeakerWasEnabled=false;
    presetPreviewSpeakerForced=true;
  }
  presetPreviewSelectionMuted=false;
  digitalWrite(AUDIO_MUTE,LOW);
}

void commitRttyPresetPreview() {
  restoreRttyPresetPreviewSpeaker();
  presetOriginalValid=false;
  presetTargetBandIndex=-1;
  presetTargetBandSnapshotValid=false;
  presetPreviewPending=false;
  presetPreviewActive=false;
  presetPreviewStartedAt=0;
}

void acceptRttyPreset() {
  const DecoderFrequencyPreset* preset=rttyPresetAt(presetSelection);
  bool applied=presetPreviewActive && !presetPreviewPending;
  if(!applied && preset) {
    if(presetTargetBandSnapshotValid && presetTargetBandIndex>=0 && presetTargetBandIndex<getTotalBands()) {
      bands[presetTargetBandIndex]=presetTargetBandOriginal;
      presetTargetBandIndex=-1;
      presetTargetBandSnapshotValid=false;
    }
    presetPreviewActive=true;
    applied=applyRttyPreset(*preset);
  }
  if(applied && preset) {
    commitRttyPresetPreview();
    float baud=preset->recommendedBaud;
    if(baud!=45.45f && baud!=50.0f && baud!=75.0f) baud=45.45f;
    baudIndex=0;
    for(uint8_t i=0;i<3;++i)
      if(fabsf(baudChoices[i]-baud)<0.01f) baudIndex=i;
    service.setBaud(baudChoices[baudIndex]);
    polarityIndex=0;
    service.setPolarity(polarityIndex);
    service.resetDecoder();
    lastAutoBfoAdjust=0;
    bfoAtEntry=currentBFO;
  } else {
    restoreRttyPresetOriginal();
    statusShow("RTTY preset tune failed","No matching receiver band");
  }
  presetMenuOpen=false;
  menuOpen=false;
}
}

bool rttyUiOpen(float initialBaud) {
  restoreNetworkMode=wifiModeIdx;
  netStop();
  if(getWiFiStatus()!=0 || !service.enter()) {
    service.exit();
    netInit(restoreNetworkMode);
    statusShow("RTTY decoder start failed","Check Wi-Fi and audio input");
    return false;
  }
  volumeBefore=volume;
  if(volume<kPlaybackVolume) { volume=kPlaybackVolume; rx.setVolume(volume); }
  volumeBoost=true;
  bfoAtEntry=currentBFO;
  bfoAuto=true;
  polarityIndex=0;
  service.setPolarity(polarityIndex);
  menuOpen=false;
  menuEditing=false;
  presetMenuOpen=false;
  menuIndex=0;
  presetSelection=presetScroll=0;
  clearRttyPresetPreviewState();
  lastAutoBfoAdjust=0;
  pageActive=true;
  lastRefresh=0;
  baudIndex=0;
  float bestDifference=fabsf(baudChoices[0]-initialBaud);
  for(uint8_t i=1;i<3;++i) {
    const float difference=fabsf(baudChoices[i]-initialBaud);
    if(difference<bestDifference) { bestDifference=difference; baudIndex=i; }
  }
  service.setBaud(baudChoices[baudIndex]);
  return true;
}

void rttyUiExit() {
  if(!pageActive) return;
  pageActive=false;
  restoreRttyPresetOriginal();
  menuOpen=false;
  menuEditing=false;
  presetMenuOpen=false;
  service.exit();
  if(currentBFO!=bfoAtEntry) updateBFO(bfoAtEntry,true);
  if(volumeBoost) { volume=volumeBefore; rx.setVolume(volume); volumeBoost=false; }
  netInit(restoreNetworkMode);
  currentCmd=CMD_NONE;
}

void rttyUiRotate(int16_t delta) {
  if(!pageActive || !delta) return;
  if(menuOpen) {
    if(presetMenuOpen) {
      const int count=rttyPresetCount();
      if(presetPreviewActive) { digitalWrite(AUDIO_MUTE,HIGH); presetPreviewSelectionMuted=true; }
      int next=static_cast<int>(presetSelection)+(delta>0?1:-1);
      if(next<0) next=count-1;
      if(next>=count) next=0;
      presetSelection=static_cast<uint8_t>(next);
      if(presetSelection<presetScroll) presetScroll=presetSelection;
      presetScroll=DecoderMenuUi::firstVisible(presetSelection,static_cast<uint8_t>(count));
      scheduleRttyPresetPreview(millis());
      return;
    }
    if(menuEditing) {
      if(menuIndex==2) {
        int next=static_cast<int>(baudIndex)+delta;
        while(next<0) next+=3;
        baudIndex=static_cast<uint8_t>(next%3);
        service.setBaud(baudChoices[baudIndex]);
      } else if(menuIndex==3) {
        int next=static_cast<int>(polarityIndex)+delta;
        while(next<0) next+=3;
        polarityIndex=static_cast<uint8_t>(next%3);
        service.setPolarity(polarityIndex);
      }
      return;
    }
    int next=static_cast<int>(menuIndex)+delta;
    while(next<0) next+=kMenuCount;
    menuIndex=static_cast<uint8_t>(next%kMenuCount);
    return;
  }
  if(!bfoAuto) {
    applyBfo(static_cast<int>(currentBFO)+static_cast<int>(delta)*kManualBfoStep);
  }
}

void rttyUiClick() {
  if(!pageActive) return;
  if(!menuOpen) {
    menuOpen=true;
    menuEditing=false;
    menuIndex=0;
    return;
  }
  if(presetMenuOpen) {
    acceptRttyPreset();
    return;
  }
  if(menuEditing) {
    menuEditing=false;
    return;
  }
  switch(menuIndex) {
    case 0:
      presetMenuOpen=true;
      presetSelection=presetScroll=0;
      scheduleRttyPresetPreview(millis());
      break;
    case 1:
      bfoAuto=!bfoAuto;
      lastAutoBfoAdjust=0;
      break;
    case 2:
    case 3:
      menuEditing=true;
      break;
    case 4:
      service.clearText();
      break;
    case 5:
      service.resetDecoder();
      lastAutoBfoAdjust=0;
      break;
    case 6:
      menuOpen=false;
      break;
    case 7:
      rttyUiExit();
      break;
  }
}

bool rttyUiTick(uint32_t now) {
  if(!pageActive) return false;
  bool changed=false;
  if(presetMenuOpen && presetPreviewPending &&
     static_cast<uint32_t>(now-presetPreviewStartedAt)>=kRttyPresetPreviewDelayMs) {
    startRttyPresetPreview();
    changed=true;
  }
  if(static_cast<uint32_t>(now-lastRefresh)<100) return changed;
  lastRefresh=now;
  service.update();
  if(bfoAuto && service.signal() &&
     static_cast<uint32_t>(now-lastAutoBfoAdjust)>=500) {
    const float error=kTargetToneCenterHz-service.toneCenterHz();
    if(fabsf(error)>kAutoDeadbandHz) {
      int correction=static_cast<int>(lroundf(error));
      if(correction>8) correction=8;
      if(correction< -8) correction=-8;
      applyBfo(static_cast<int>(currentBFO)+correction);
      lastAutoBfoAdjust=now;
    }
  }
  return true;
}

void drawRttyDecoderPage() {
  const bool active=service.signal();
  char status[32];
  snprintf(status,sizeof(status),"%s  %.2f Bd",active?"RTTY":"SEARCH",service.baud());
  DecoderPageLayout::draw(currentFrequency,status,service.text(),
                           "WAITING FOR RTTY SIGNAL...",active?500:0,500,spectrumMode?service.spectrumBins():nullptr);

  char footer[80];
  if(bfoAuto) snprintf(footer,sizeof(footer),"CLICK: ACTIONS  LONG PRESS: EXIT");
  else snprintf(footer,sizeof(footer),"ROTATE: BFO  CLICK: ACTIONS  LONG: EXIT");
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(0x07FF);
  spr.setFont(FONT_SMALL);
  spr.drawString(footer,160,162);

  if(menuOpen) {
    if(presetMenuOpen) {
      const uint8_t count = rttyPresetCount();
      char storage[kDecoderFrequencyPresetCount][40] = {};
      const char* labels[kDecoderFrequencyPresetCount] = {};
      for(uint8_t i = 0; i < count; ++i) {
        const DecoderFrequencyPreset* preset = rttyPresetAt(i);
        if(!preset) continue;
        const char* name = preset->label;
        if(!strncmp(name, "RTTY ", 5)) name += 5;
        else if(!strncmp(name, "PINNEBERG ", 10)) name += 10;
        snprintf(storage[i], sizeof(storage[i]), "%s  %u.%03u %s", name,
                 static_cast<unsigned>(preset->frequencyKHz / 1000),
                 static_cast<unsigned>(preset->frequencyKHz % 1000),
                 preset->modulation == USB ? "USB" : preset->modulation == LSB ? "LSB" : "AM");
        labels[i] = storage[i];
      }
      const char* previewStatus = presetPreviewPending ? "TUNING..." : presetPreviewActive ? "TUNED" : "READY";
      DecoderMenuUi::draw("RTTY FREQUENCIES", labels, nullptr, count, presetSelection,
                          "TURN: SELECT", "CLICK: APPLY", previewStatus);
    } else {
      const char* labels[kMenuCount] = {
        "RTTY FREQUENCIES", "BFO MODE", "BAUD RATE", "POLARITY",
        "CLEAR TEXT", "RESET DECODER", "SPECTRUM", "BACK", "EXIT RTTY"
      };
      const char* values[kMenuCount] = {
        nullptr, bfoAuto ? "AUTO" : "MANUAL", nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr
      };
      char baudValue[16], polarityValue[16];
      snprintf(baudValue, sizeof(baudValue), "%.2f Bd", baudChoices[baudIndex]);
      snprintf(polarityValue, sizeof(polarityValue), "%s", polarityNames[polarityIndex]);
      values[2] = baudValue;
      values[3] = polarityValue;
      char toneStatus[32];
      if(service.markHz() > 0 && service.spaceHz() > 0)
        snprintf(toneStatus, sizeof(toneStatus), "M %.0f S %.0f Hz", service.markHz(), service.spaceHz());
      else snprintf(toneStatus, sizeof(toneStatus), "TONE SEARCH");
      DecoderMenuUi::draw("RTTY ACTIONS", labels, values, kMenuCount, menuIndex,
                          menuEditing ? "TURN: CHANGE" : "TURN: SELECT",
                          menuEditing ? "CLICK: DONE" : "CLICK: OPEN", toneStatus, true);
    }
  }
}
