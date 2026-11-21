#include "RttyUi.h"
#include "RttyService.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

extern int16_t currentBFO;
extern bool updateBFO(int newBFO, bool wrap);

namespace {
RttyService service;
bool pageActive=false;
uint8_t restoreNetworkMode=NET_OFF;
uint32_t lastRefresh=0;
uint32_t lastAutoBfoAdjust=0;
uint8_t baudIndex=0;
const float baudChoices[3]={45.45f,50.0f,75.0f};
uint8_t volumeBefore=0;
bool volumeBoost=false;
constexpr uint8_t kPlaybackVolume=50;
constexpr int kBfoLimit=250;
constexpr int kManualBfoStep=5;
constexpr float kTargetToneCenterHz=2210.0f;
constexpr float kAutoDeadbandHz=18.0f;
enum RttyControlMode : uint8_t { RTTY_CONTROL_BAUD=0, RTTY_CONTROL_BFO_AUTO=1, RTTY_CONTROL_BFO_MANUAL=2 };
RttyControlMode controlMode=RTTY_CONTROL_BAUD;
int16_t bfoAtEntry=0;

void applyBfo(int value) {
  int low=bfoAtEntry-kBfoLimit, high=bfoAtEntry+kBfoLimit;
  if(value<low) value=low;
  if(value>high) value=high;
  updateBFO(value,true);
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
  controlMode=RTTY_CONTROL_BAUD;
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
  service.exit();
  if(currentBFO!=bfoAtEntry) updateBFO(bfoAtEntry,true);
  if(volumeBoost) { volume=volumeBefore; rx.setVolume(volume); volumeBoost=false; }
  netInit(restoreNetworkMode);
  currentCmd=CMD_NONE;
}

void rttyUiRotate(int16_t delta) {
  if(!pageActive || !delta) return;
  if(controlMode==RTTY_CONTROL_BAUD) {
    int next=static_cast<int>(baudIndex)+delta;
    while(next<0) next+=3;
    baudIndex=static_cast<uint8_t>(next%3);
    service.setBaud(baudChoices[baudIndex]);
    return;
  }
  if(controlMode==RTTY_CONTROL_BFO_AUTO) controlMode=RTTY_CONTROL_BFO_MANUAL;
  applyBfo(static_cast<int>(currentBFO)+static_cast<int>(delta)*kManualBfoStep);
}

void rttyUiClick() {
  if(!pageActive) return;
  controlMode=static_cast<RttyControlMode>((static_cast<uint8_t>(controlMode)+1)%3);
  lastAutoBfoAdjust=0;
}

bool rttyUiTick(uint32_t now) {
  if(!pageActive || static_cast<uint32_t>(now-lastRefresh)<100) return false;
  lastRefresh=now;
  service.update();
  if(controlMode==RTTY_CONTROL_BFO_AUTO && service.signal() &&
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
  const char* mode=controlMode==RTTY_CONTROL_BAUD?"BAUD":controlMode==RTTY_CONTROL_BFO_AUTO?"BFO AUTO":"BFO MANUAL";
  snprintf(footer,sizeof(footer),"ROTATE: %s  BFO %+d Hz  CLICK: MODE  LONG: EXIT",mode,static_cast<int>(currentBFO));
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(0xFFFF,navy);
  spr.setFont(FONT_SMALL);
  spr.drawString(footer,160,163);
}
