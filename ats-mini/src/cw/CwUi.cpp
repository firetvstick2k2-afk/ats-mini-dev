#include "CwUi.h"
#include "../DecoderMenuUi.h"
#include "CwDecoderService.h"
#include "../audio/AudioCaptureService.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include "../radio/DecoderFrequencyList.h"
#include <stdio.h>
#include <string.h>

namespace {
CwDecoderService service;
bool pageActive = false;
bool menuOpen = false;
bool menuEditing = false;
bool frequencyMenuOpen = false;
uint8_t frequencySelection = 0;
uint8_t frequencyScroll = 0;
uint8_t menuIndex = 0;
uint8_t restoreNetworkMode = NET_OFF;
uint32_t lastRefresh = 0;
constexpr uint8_t kMenuCount = 5;
uint8_t cwPresetCount() { uint8_t n=0; for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i) if(kDecoderFrequencyPresets[i].decoder==DecoderKind::CW) ++n; return n; }
const DecoderFrequencyPreset* cwPresetAt(uint8_t wanted) { uint8_t n=0; for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i) if(kDecoderFrequencyPresets[i].decoder==DecoderKind::CW && n++==wanted) return &kDecoderFrequencyPresets[i]; return nullptr; }
bool applyCwPreset(const DecoderFrequencyPreset& preset) { int best=-1; uint16_t span=UINT16_MAX; for(int i=0;i<getTotalBands();++i) if(bands[i].bandType!=FM_BAND_TYPE && preset.frequencyKHz>=bands[i].minimumFreq && preset.frequencyKHz<=bands[i].maximumFreq) { uint16_t width=bands[i].maximumFreq-bands[i].minimumFreq; if(width<span){span=width;best=i;} } if(best<0)return false; bands[best].currentFreq=static_cast<uint16_t>(preset.frequencyKHz); bands[best].bandMode=preset.modulation; bands[best].bandwidthIdx=preset.bandwidthChoice; selectBand(static_cast<uint8_t>(best),false); currentMode=preset.modulation; currentFrequency=rx.getFrequency(); service.resetDecoder(); return true; }
}

bool cwUiOpen() {
  restoreNetworkMode = wifiModeIdx;
  netStop();
  if (getWiFiStatus() != 0 || !service.enter(sharedAudioCapture(), true)) {
    service.exit();
    netInit(restoreNetworkMode);
    statusShow("CW decoder start failed", "Check Wi-Fi and audio input");
    return false;
  }
  menuOpen = false;
  menuEditing = false;
  frequencyMenuOpen = false;
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
  frequencyMenuOpen = false;
  service.exit();
  netInit(restoreNetworkMode);
  currentCmd = CMD_NONE;
}

void cwUiRotate(int16_t delta) {
  if (!pageActive || !delta || !menuOpen) return;
  if(frequencyMenuOpen) { int count=cwPresetCount()+1; int next=static_cast<int>(frequencySelection)+(delta>0?1:-1); if(next<0)next=count-1; if(next>=count)next=0; frequencySelection=static_cast<uint8_t>(next); if(frequencySelection<frequencyScroll)frequencyScroll=frequencySelection; frequencyScroll=DecoderMenuUi::firstVisible(frequencySelection,static_cast<uint8_t>(count)); return; }
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
  if(frequencyMenuOpen) { if(frequencySelection==0) service.resetDecoder(); else { const DecoderFrequencyPreset* preset=cwPresetAt(frequencySelection-1); if(preset && !applyCwPreset(*preset)) statusShow("CW preset failed","No matching receiver band"); } frequencyMenuOpen=false; return; }
  switch (menuIndex) { case 0: frequencyMenuOpen=true; frequencySelection=frequencyScroll=0; break; case 1: service.clearText(); break; case 2: service.resetDecoder(); break; case 3: menuOpen=false; break; case 4: cwUiExit(); break; }
}

bool cwUiTick(uint32_t now) {
  if (!pageActive || static_cast<uint32_t>(now - lastRefresh) < 100) return false;
  lastRefresh = now;
  service.update();
  return true;
}

void drawCwDecoderPage() {
  constexpr uint16_t bg=0x0000, cyan=0x07FF, green=0x07E0, white=0xFFFF, grey=0x7BEF;
  spr.fillSprite(bg);

  // Match the clean, borderless FT8 page: no decorative boxes around CW data.
  char frequencyText[24];
  snprintf(frequencyText,sizeof(frequencyText),"%lu.%03lu",
           static_cast<unsigned long>(currentFrequency/1000),
           static_cast<unsigned long>(currentFrequency%1000));
  spr.setTextColor(white);
  spr.setTextDatum(TL_DATUM);
  const int frequencyWidth=spr.drawString(frequencyText,8,5,&lgfx::fonts::Orbitron_Light_24);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(cyan);
  spr.drawString("kHz",8+frequencyWidth+7,19,FONT_SMALL);

  const bool active=service.signal();
  const bool audioPresent=service.audioPresent();
  char status[40];
  snprintf(status,sizeof(status),"%s  %s  %d WPM",
           audioPresent?"AUDIO":"NO AUDIO",active?"CW":"SEARCH",service.wpm());
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(audioPresent?green:grey);
  spr.drawString(status,311,15,FONT_SMALL);

  char toneText[32];
  snprintf(toneText,sizeof(toneText),"AUTO TONE  %.0f Hz",service.tone());
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(cyan);
  spr.drawString(toneText,10,34,FONT_SMALL);
  char levelText[20];
  snprintf(levelText,sizeof(levelText),"LEVEL %u",static_cast<unsigned>(service.audioLevel()));
  spr.setTextDatum(TR_DATUM);
  spr.drawString(levelText,310,34,FONT_SMALL);

  const uint16_t level=service.audioLevel()>500?500:service.audioLevel();
  spr.drawRect(8,49,304,10,cyan);
  spr.fillRect(10,51,300,6,bg);
  if(level) spr.fillRect(10,51,(300*level)/500,6,audioPresent?green:cyan);

  const char* decoded=service.text();
  const size_t len=strlen(decoded);
  constexpr size_t charsPerLine=34;
  const int rowY[4]={68,91,114,137};
  const size_t visibleChars=charsPerLine*4;
  const size_t first=len>visibleChars?len-visibleChars:0;
  for(int row=0;row<4;++row) {
    const size_t offset=first+static_cast<size_t>(row)*charsPerLine;
    if(offset<len) {
      size_t count=len-offset;
      if(count>charsPerLine) count=charsPerLine;
      char line[charsPerLine+1];
      memcpy(line,decoded+offset,count);
      line[count]=0;
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(green);
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.drawString(line,12,rowY[row]);
    } else if(row==0&&!active) {
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(grey);
      spr.setFont(FONT_SMALL);
      spr.drawString("WAITING FOR CW SIGNAL...",12,rowY[row]);
    }
  }

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(cyan);
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
        "CW FREQUENCIES", "CLEAR TEXT", "RESET DECODER", "BACK", "EXIT CW"
      };
      DecoderMenuUi::draw("CW ACTIONS", labels, nullptr, kMenuCount, menuIndex,
                          "TURN: SELECT", "CLICK: OPEN");
    }
  }
}
