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
bool menuOpen=false;
bool menuEditing=false;
uint8_t menuIndex=0;
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
constexpr uint8_t kMenuCount=7;

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
  bfoAuto=true;
  polarityIndex=0;
  service.setPolarity(polarityIndex);
  menuOpen=false;
  menuEditing=false;
  menuIndex=0;
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
  menuOpen=false;
  menuEditing=false;
  service.exit();
  if(currentBFO!=bfoAtEntry) updateBFO(bfoAtEntry,true);
  if(volumeBoost) { volume=volumeBefore; rx.setVolume(volume); volumeBoost=false; }
  netInit(restoreNetworkMode);
  currentCmd=CMD_NONE;
}

void rttyUiRotate(int16_t delta) {
  if(!pageActive || !delta) return;
  if(menuOpen) {
    if(menuEditing) {
      if(menuIndex==1) {
        int next=static_cast<int>(baudIndex)+delta;
        while(next<0) next+=3;
        baudIndex=static_cast<uint8_t>(next%3);
        service.setBaud(baudChoices[baudIndex]);
      } else if(menuIndex==2) {
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
  if(menuEditing) {
    menuEditing=false;
    return;
  }
  switch(menuIndex) {
    case 0:
      bfoAuto=!bfoAuto;
      lastAutoBfoAdjust=0;
      break;
    case 1:
    case 2:
      menuEditing=true;
      break;
    case 3:
      service.clearText();
      break;
    case 4:
      service.resetDecoder();
      lastAutoBfoAdjust=0;
      break;
    case 5:
      menuOpen=false;
      break;
    case 6:
      rttyUiExit();
      break;
  }
}

bool rttyUiTick(uint32_t now) {
  if(!pageActive || static_cast<uint32_t>(now-lastRefresh)<100) return false;
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
    constexpr uint16_t panel=0x0008,highlight=0x0359,border=0x07E0;
    spr.fillRect(28,27,264,130,panel);
    spr.drawRect(28,27,264,130,border);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(0xFFFF,panel);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.drawString("RTTY ACTIONS",160,35);
    char toneStatus[48];
    if(service.markHz()>0 && service.spaceHz()>0)
      snprintf(toneStatus,sizeof(toneStatus),"MARK %.0f  SPACE %.0f Hz",service.markHz(),service.spaceHz());
    else snprintf(toneStatus,sizeof(toneStatus),"TONE PAIR: SEARCHING...");
    spr.setFont(FONT_SMALL);
    spr.drawString(toneStatus,160,47);
    const char* labels[kMenuCount]={"BFO MODE","BAUD RATE","POLARITY","CLEAR TEXT","RESET DECODER","BACK","EXIT RTTY"};
    char value[32];
    for(uint8_t i=0;i<kMenuCount;++i) {
      const int y=59+i*12;
      const uint16_t bg=(i==menuIndex)?highlight:panel;
      spr.fillRect(36,y-6,248,13,bg);
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(0xFFFF,bg);
      spr.setFont(FONT_SMALL);
      spr.drawString(labels[i],43,y);
      const char* state="";
      if(i==0) state=bfoAuto?"AUTO":"MANUAL";
      else if(i==1) { snprintf(value,sizeof(value),"%.2f Bd",baudChoices[baudIndex]); state=value; }
      else if(i==2) state=polarityNames[polarityIndex];
      spr.setTextDatum(MR_DATUM);
      spr.drawString(state,277,y);
    }
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(0xFFFF,panel);
    spr.setFont(FONT_SMALL);
    const char* hint=menuEditing?"ROTATE: CHANGE  CLICK: DONE":"ROTATE: SELECT  CLICK: OPEN";
    spr.drawString(hint,160,148);
  }
}
