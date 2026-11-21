#include "RttyUi.h"
#include "RttyService.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include "../radio/DecoderFrequencyList.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

extern int16_t currentBFO;
extern bool updateBFO(int newBFO, bool wrap);

namespace {
RttyService service;
bool pageActive=false;
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
constexpr uint8_t kMenuCount=8;
// Extra RTTY-only preset: keep this out of the shared top-level Decoder Frequencies list.
static constexpr DecoderFrequencyPreset kRtty147UsbPreset={
  "PINNEBERG DDH 47 USB",147,USB,4,DecoderKind::RTTY,50.0f
};

void applyBfo(int value) {
  int low=bfoAtEntry-kBfoLimit, high=bfoAtEntry+kBfoLimit;
  if(value<low) value=low;
  if(value>high) value=high;
  updateBFO(value,true);
}

uint8_t rttyPresetCount() {
  uint8_t count=1; // Additional 147 kHz USB preset, local to the RTTY submenu.
  for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i)
    if(kDecoderFrequencyPresets[i].decoder==DecoderKind::RTTY) ++count;
  return count;
}

const DecoderFrequencyPreset* rttyPresetAt(uint8_t wanted) {
  uint8_t index=0;
  for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i) {
    if(kDecoderFrequencyPresets[i].decoder!=DecoderKind::RTTY) continue;
    if(index++==wanted) return &kDecoderFrequencyPresets[i];
    if(kDecoderFrequencyPresets[i].frequencyKHz==147 &&
       kDecoderFrequencyPresets[i].modulation==AM) {
      if(index++==wanted) return &kRtty147UsbPreset;
    }
  }
  return nullptr;
}

void restoreRttyPresetPreviewSpeaker() {
  if(presetPreviewSpeakerForced) {
    digitalWrite(AUDIO_MUTE,presetPreviewSpeakerWasEnabled?LOW:HIGH);
    presetPreviewSpeakerForced=false;
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
  presetPreviewActive=false;
  presetPreviewStartedAt=now;
}

bool applyRttyPreset(const DecoderFrequencyPreset& preset) {
  const bool belowNormalFloor=preset.frequencyKHz==147;
  int bestBand=-1;
  uint16_t bestSpan=UINT16_MAX;
  for(int i=0;i<getTotalBands();++i) {
    if(bands[i].bandType==FM_BAND_TYPE ||
       (preset.frequencyKHz<bands[i].minimumFreq &&
        !(belowNormalFloor && bands[i].minimumFreq==150)) ||
       preset.frequencyKHz>bands[i].maximumFreq) continue;
    const uint16_t span=bands[i].maximumFreq-bands[i].minimumFreq;
    if(span<bestSpan) { bestSpan=span; bestBand=i; }
  }
  if(bestBand<0) return false;

  presetTargetBandIndex=bestBand;
  presetTargetBandOriginal=bands[bestBand];
  presetTargetBandSnapshotValid=true;
  bands[bestBand].currentFreq=static_cast<uint16_t>(preset.frequencyKHz);
  bands[bestBand].bandMode=belowNormalFloor?USB:preset.modulation;
  bands[bestBand].bandwidthIdx=preset.bandwidthChoice;
  currentBFO=0;

  // The SI473x normally rejects frequencies below 150 kHz in the standard
  // band setup. Temporarily lower only this selected band's floor so the
  // ordinary USB/AM initialization path can be used at 147 kHz.
  const uint16_t originalMinimum= bands[bestBand].minimumFreq;
  if(belowNormalFloor) bands[bestBand].minimumFreq=147;
  selectBand(static_cast<uint8_t>(bestBand),false);

  // H.J. Berndt-style workaround: initialize the receiver in USB first,
  // then reinitialize the same 147 kHz carrier in AM for DDH47.
  if(belowNormalFloor && preset.modulation==AM) {
    bands[bestBand].bandMode=AM;
    selectBand(static_cast<uint8_t>(bestBand),false);
  }
  if(belowNormalFloor) bands[bestBand].minimumFreq=originalMinimum;

  currentMode=preset.modulation;
  currentFrequency=rx.getFrequency();
  return currentFrequency==preset.frequencyKHz;
}

void startRttyPresetPreview() {
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
  if(digitalRead(AUDIO_MUTE)!=LOW) {
    presetPreviewSpeakerWasEnabled=false;
    presetPreviewSpeakerForced=true;
    digitalWrite(AUDIO_MUTE,LOW);
  }
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
  bool applied=presetPreviewActive;
  if(!applied && preset) {
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
      if(presetPreviewActive) restoreRttyPresetOriginal();
      int next=static_cast<int>(presetSelection)+(delta>0?1:-1);
      if(next<0) next=count-1;
      if(next>=count) next=0;
      presetSelection=static_cast<uint8_t>(next);
      if(presetSelection<presetScroll) presetScroll=presetSelection;
      if(presetSelection>=presetScroll+6) presetScroll=presetSelection-5;
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
  constexpr uint16_t navy=0x0008,blueA=0x0359,blueB=0x047D,frame=0xF800,green=0x07E0,red=0xF800;
  spr.fillSprite(navy);
  spr.drawRect(2,2,316,166,0x001F);
  spr.fillRect(8,5,304,31,blueA);
  spr.drawRect(8,5,304,31,frame);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(0xFFFF,blueA);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  char freq[40];
  snprintf(freq,sizeof(freq),"%lu kHz  %s",static_cast<unsigned long>(currentFrequency),currentMode==USB?"USB":currentMode==LSB?"LSB":"AM");
  spr.drawString(freq,16,20);
  const bool active=service.signal();
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(0xFFFF,blueA);
  spr.setFont(&lgfx::fonts::FreeSans9pt7b);
  char status[32];
  snprintf(status,sizeof(status),"%s %.2fBd",active?"RX":"NO RX",service.baud());
  spr.drawString(status,286,20);

  const char* decoded=service.text();
  const size_t len=strlen(decoded);
  constexpr size_t charsPerLine=34;
  const int panelY[4]={40,69,98,127};
  for(int row=0;row<4;++row) {
    const uint16_t panel=(row&1)?blueB:blueA;
    spr.fillRect(8,panelY[row],304,25,panel);
    spr.drawRect(8,panelY[row],304,25,0x04BF);
    size_t start=0,visible=0;
    const size_t rowOffset=static_cast<size_t>(row)*charsPerLine;
    if(len>rowOffset) {
      visible=len-rowOffset;
      if(visible>charsPerLine) visible=charsPerLine;
      if(len>=static_cast<size_t>((row+1)*charsPerLine)) start=len-static_cast<size_t>((row+1)*charsPerLine);
    }
    if(visible) {
      char line[charsPerLine+1];
      memcpy(line,decoded+start,visible); line[visible]=0;
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(0xFFFF,panel);
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.drawString(line,14,panelY[row]+12);
    } else if(row==0 && !active) {
      spr.setTextDatum(ML_DATUM); spr.setTextColor(0xFFFF,panel);
      spr.setFont(&lgfx::fonts::FreeSans9pt7b);
      spr.drawString("WAITING FOR RTTY SIGNAL...",14,panelY[row]+12);
    }
  }
  const uint16_t led=active?green:red;
  if((millis()/350)&1) spr.fillCircle(300,10,3,led); else spr.drawCircle(300,10,3,led);
  char footer[80];
  if(bfoAuto) snprintf(footer,sizeof(footer),"BFO AUTO  CLICK: ACTIONS  LONG: EXIT");
  else snprintf(footer,sizeof(footer),"ROTATE: BFO  CLICK: ACTIONS  LONG: EXIT  %+d Hz",static_cast<int>(currentBFO));
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(0xFFFF,navy);
  spr.setFont(FONT_SMALL);
  spr.drawString(footer,160,163);

  if(menuOpen) {
    const int menuX=20,menuY=8,menuW=280,menuH=154;
    constexpr uint16_t panel=0x1036,cyan=0x07FF;
    spr.fillRect(menuX,menuY,menuW,menuH,panel);
    spr.drawRect(menuX,menuY,menuW,menuH,cyan);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(cyan);
    if(presetMenuOpen) {
      spr.drawString("RTTY PRESETS",menuX+12,menuY+7);
      const uint8_t count=rttyPresetCount();
      for(uint8_t row=0;row<6;++row) {
        const uint8_t index=presetScroll+row;
        if(index>=count) break;
        const DecoderFrequencyPreset* preset=rttyPresetAt(index);
        if(!preset) continue;
        const char* name=!strncmp(preset->label,"RTTY ",5)?preset->label+5:preset->label;
        char label[52];
        snprintf(label,sizeof(label),"%-15s %2u.%03u MHz %s",name,
          static_cast<unsigned>(preset->frequencyKHz/1000),
          static_cast<unsigned>(preset->frequencyKHz%1000),
          preset->modulation==USB?"USB":preset->modulation==LSB?"LSB":"AM");
        const int y=menuY+29+row*20;
        if(index==presetSelection) spr.fillRect(menuX+7,y-3,menuW-14,19,0x0359);
        spr.setTextColor(index==presetSelection?0xFFFF:0xBDF7);
        spr.drawString(label,menuX+13,y);
        if(index==presetSelection && (presetPreviewPending || presetPreviewActive)) {
          spr.setTextDatum(TR_DATUM);
          spr.setTextColor(presetPreviewActive?0x07E0:0xFFE0);
          spr.drawString(presetPreviewActive?"LISTEN":"WAIT",menuX+menuW-12,y);
          spr.setTextDatum(TL_DATUM);
        }
      }
      char page[12];
      snprintf(page,sizeof(page),"%02u/%02u",static_cast<unsigned>(presetSelection+1),static_cast<unsigned>(count));
      spr.setTextDatum(TR_DATUM);
      spr.setTextColor(0xFFFF);
      spr.drawString(page,menuX+menuW-10,menuY+7);
    } else {
      const char* labels[kMenuCount]={
        "RTTY PRESETS",
        bfoAuto?"BFO MODE       AUTO":"BFO MODE       MANUAL",
        "BAUD RATE",
        "POLARITY",
        "CLEAR TEXT",
        "RESET DECODER",
        "BACK",
        "EXIT RTTY"
      };
      spr.drawString("RTTY ACTIONS",menuX+12,menuY+7);
      char toneStatus[48];
      if(service.markHz()>0 && service.spaceHz()>0)
        snprintf(toneStatus,sizeof(toneStatus),"M %.0f  S %.0f Hz",service.markHz(),service.spaceHz());
      else snprintf(toneStatus,sizeof(toneStatus),"TONE PAIR: SEARCHING");
      spr.setFont(FONT_SMALL);
      spr.setTextDatum(TR_DATUM);
      spr.setTextColor(cyan);
      spr.drawString(toneStatus,menuX+menuW-12,menuY+10);
      spr.setTextDatum(TL_DATUM);
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      for(uint8_t i=0;i<kMenuCount;++i) {
        const int y=menuY+27+i*17;
        if(i==menuIndex) spr.fillRect(menuX+7,y-2,menuW-14,18,0x0359);
        spr.setTextColor(i==menuIndex?0xFFFF:0xBDF7);
        spr.drawString(labels[i],menuX+14,y);
        char value[24]={};
        if(i==2) snprintf(value,sizeof(value),"%.2f Bd",baudChoices[baudIndex]);
        else if(i==3) snprintf(value,sizeof(value),"%s",polarityNames[polarityIndex]);
        if(value[0]) {
          spr.setTextDatum(TR_DATUM);
          spr.setTextColor(i==menuIndex?0xFFFF:0xBDF7);
          spr.drawString(value,menuX+menuW-14,y);
          spr.setTextDatum(TL_DATUM);
        }
      }
      spr.setTextColor(cyan);
      spr.setFont(FONT_SMALL);
      spr.drawString(menuEditing?"ROTATE: CHANGE  CLICK: DONE":"ROTATE: SELECT  CLICK: OPEN",menuX+12,menuY+137);
    }
  }
}