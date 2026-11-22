#include "CwUi.h"
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
  if(frequencyMenuOpen) { int count=cwPresetCount()+1; int next=static_cast<int>(frequencySelection)+(delta>0?1:-1); if(next<0)next=count-1; if(next>=count)next=0; frequencySelection=static_cast<uint8_t>(next); if(frequencySelection<frequencyScroll)frequencyScroll=frequencySelection; if(frequencySelection>=frequencyScroll+6)frequencyScroll=frequencySelection-5; return; }
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
  constexpr uint16_t bg = 0x0000, panel = 0x0010, panelAlt = 0x0020;
  constexpr uint16_t border = 0x07FF, green = 0x07E0, cyan = 0x07FF, white = 0xFFFF, grey = 0x8410;
  spr.fillSprite(bg);
  spr.drawRect(2, 2, 316, 166, cyan);

  spr.fillRect(8, 5, 304, 29, panel);
  spr.drawRect(8, 5, 304, 29, border);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(green, panel);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  char header[48];
  snprintf(header, sizeof(header), "%lu kHz  %s",
           static_cast<unsigned long>(currentFrequency),
           currentMode == FM ? "FM" : currentMode == USB ? "USB" : currentMode == LSB ? "LSB" : "AM");
  spr.drawString(header, 15, 19);

  const bool active = service.signal();
  const bool audioPresent = service.audioPresent();
  char status[40];
  snprintf(status, sizeof(status), "%s %s %d WPM",
           audioPresent ? "AUDIO" : "NO AUDIO", active ? "CW" : "SEARCH", service.wpm());
  spr.setTextDatum(TR_DATUM);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.setTextColor(audioPresent ? green : grey, panel);
  spr.drawString(status, 304, 19);

  spr.fillRect(8, 39, 304, 22, panelAlt);
  spr.drawRect(8, 39, 304, 22, border);
  spr.setTextDatum(ML_DATUM);
  spr.setFont(&lgfx::fonts::FreeSans9pt7b);
  char info[48];
  snprintf(info, sizeof(info), "AUTO TONE  %.0f Hz", service.tone());
  spr.setTextColor(cyan, panelAlt);
  spr.drawString(info, 15, 50);
  spr.setTextDatum(TR_DATUM);
  snprintf(info, sizeof(info), "LEVEL %u", static_cast<unsigned>(service.audioLevel()));
  spr.drawString(info, 304, 50);

  // A compact level bar gives immediate feedback from the shared audio input.
  const uint16_t level = service.audioLevel() > 500 ? 500 : service.audioLevel();
  spr.drawRect(8, 64, 304, 5, border);
  if (level) spr.fillRect(9, 65, (302 * level) / 500, 3, service.audioPresent() ? green : cyan);

  const char* decoded = service.text();
  const size_t len = strlen(decoded);
  constexpr size_t charsPerLine = 31;
  const int rowY[4] = {73, 94, 115, 136};
  const size_t visibleChars = charsPerLine * 4;
  const size_t first = len > visibleChars ? len - visibleChars : 0;
  for (int row = 0; row < 4; ++row) {
    const uint16_t rowPanel = (row & 1) ? panelAlt : panel;
    spr.fillRect(8, rowY[row], 304, 18, rowPanel);
    spr.drawRect(8, rowY[row], 304, 18, border);
    const size_t offset = first + static_cast<size_t>(row) * charsPerLine;
    if (offset < len) {
      size_t count = len - offset;
      if (count > charsPerLine) count = charsPerLine;
      char line[charsPerLine + 1];
      memcpy(line, decoded + offset, count);
      line[count] = 0;
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(green, rowPanel);
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.drawString(line, 13, rowY[row] + 9);
    } else if (row == 0 && !active) {
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(grey, rowPanel);
      spr.setFont(&lgfx::fonts::FreeSans9pt7b);
      spr.drawString("WAITING FOR CW SIGNAL...", 13, rowY[row] + 9);
    }
  }

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(cyan, bg);
  spr.setFont(FONT_SMALL);
  spr.drawString("CLICK: ACTIONS   LONG PRESS: EXIT", 160, 162);

  if (menuOpen) {
    constexpr uint16_t menuPanel=0x0010, highlight=0x07FF;
    if(frequencyMenuOpen) {
      const int menuX=20,menuY=8,menuW=280,menuH=154;
      spr.fillRect(menuX,menuY,menuW,menuH,menuPanel);
      spr.drawRect(menuX,menuY,menuW,menuH,cyan);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(cyan);
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.drawString("CW PRESETS",menuX+12,menuY+7);
      const uint8_t count=cwPresetCount()+1;
      for(uint8_t row=0;row<5;++row) {
        const uint8_t index=frequencyScroll+row;
        if(index>=count) break;
        char label[40];
        if(index==0) snprintf(label,sizeof(label),"CURRENT  %2lu.%03lu USB",
          static_cast<unsigned long>(currentFrequency/1000),static_cast<unsigned long>(currentFrequency%1000));
        else {
          const DecoderFrequencyPreset* p=cwPresetAt(index-1);
          if(!p) continue;
          const char* name=p->label;
          if(!strncmp(name,"CW ",3)) name+=3;
          snprintf(label,sizeof(label),"%-8s %2lu.%03lu %s",name,
            static_cast<unsigned long>(p->frequencyKHz/1000),static_cast<unsigned long>(p->frequencyKHz%1000),
            p->modulation==USB?"USB":p->modulation==LSB?"LSB":"AM");
        }
        const int y=menuY+29+row*19;
        if(index==frequencySelection) spr.fillRect(menuX+7,y-3,menuW-14,19,highlight);
        spr.setTextDatum(TL_DATUM);
        spr.setTextColor(index==frequencySelection?0x0000:white);
        spr.setFont(FONT_SMALL);
        spr.drawString(label,menuX+13,y);
      }
      char page[12];
      snprintf(page,sizeof(page),"%02u/%02u",static_cast<unsigned>(frequencySelection+1),static_cast<unsigned>(count));
      spr.setTextDatum(TR_DATUM); spr.setTextColor(white); spr.setFont(FONT_SMALL);
      spr.drawString(page,menuX+menuW-10,menuY+7);
      spr.setTextDatum(MC_DATUM); spr.setTextColor(green);
      spr.drawString(frequencySelection==0?"ABGESTIMMT":"AUSWAHL",160,menuY+124);
      spr.setTextDatum(ML_DATUM); spr.setTextColor(white);
      spr.drawString("DREHEN: WÄHLEN",menuX+12,menuY+143);
      spr.setTextDatum(MR_DATUM);
      spr.drawString("HALTEN: ZURÜCK",menuX+menuW-12,menuY+143);
    } else {
      spr.fillRect(28,31,264,126,menuPanel);
      spr.drawRect(28,31,264,126,cyan);
      spr.setTextDatum(MC_DATUM);
      spr.setTextColor(cyan,menuPanel);
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.drawString("CW DECODER ACTIONS",160,41);
      const char* labels[kMenuCount]={"FREQUENCIES","CLEAR TEXT","RESET DECODER","BACK","EXIT CW"};
      for(uint8_t i=0;i<kMenuCount;++i) {
        const int y=58+i*18;
        const uint16_t rowBg=i==menuIndex?0x03E0:menuPanel;
        spr.fillRect(36,y-7,248,16,rowBg);
        spr.setTextDatum(ML_DATUM);
        spr.setTextColor(i==menuIndex?0x0000:white,rowBg);
        spr.setFont(FONT_SMALL);
        spr.drawString(labels[i],43,y+1);
      }
    }
  }
}
