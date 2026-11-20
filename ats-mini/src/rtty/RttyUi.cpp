#include "RttyUi.h"
#include "RttyService.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include <string.h>
#include <stdio.h>

namespace {
RttyService service;
bool pageActive=false;
uint8_t restoreNetworkMode=NET_OFF;
uint32_t lastRefresh=0;
uint8_t baudIndex=0;
const float baudChoices[3]={45.45f,50.0f,75.0f};
uint8_t volumeBefore=0;
bool volumeBoost=false;
constexpr uint8_t kPlaybackVolume=50;
}

bool rttyUiOpen() {
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
  pageActive=true;
  lastRefresh=0;
  baudIndex=0;
  return true;
}

void rttyUiExit() {
  if(!pageActive) return;
  pageActive=false;
  service.exit();
  if(volumeBoost) { volume=volumeBefore; rx.setVolume(volume); volumeBoost=false; }
  netInit(restoreNetworkMode);
  currentCmd=CMD_NONE;
}

void rttyUiRotate(int16_t delta) {
  if(!pageActive || !delta) return;
  int next=static_cast<int>(baudIndex)+delta;
  while(next<0) next+=3;
  baudIndex=static_cast<uint8_t>(next%3);
  service.setBaud(baudChoices[baudIndex]);
}

bool rttyUiTick(uint32_t now) {
  if(!pageActive || static_cast<uint32_t>(now-lastRefresh)<100) return false;
  lastRefresh=now;
  service.update();
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
  spr.drawString(freq,16,13);
  const bool active=service.signal();
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(active?green:red,blueA);
  spr.setFont(FONT_SMALL);
  char status[32];
  snprintf(status,sizeof(status),"%s %.2fBd",active?"RX":"NO RX",service.baud());
  spr.drawString(status,302,16);

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
      spr.drawString(line,14,panelY[row]+5);
    } else if(row==0 && !active) {
      spr.setTextDatum(ML_DATUM); spr.setTextColor(0xBDF7,panel);
      spr.setFont(&lgfx::fonts::FreeSans9pt7b);
      spr.drawString("WAITING FOR RTTY SIGNAL...",14,panelY[row]+5);
    }
  }
  const uint16_t led=active?green:red;
  if((millis()/350)&1) spr.fillCircle(296,20,5,led); else spr.drawCircle(296,20,5,led);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(0xBDF7,navy);
  spr.setFont(FONT_SMALL);
  spr.drawString("ROTATE: BAUD  |  CLICK/LONG: EXIT",160,163);
}
