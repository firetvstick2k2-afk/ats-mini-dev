#include "FtxUi.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "FtxDecoderService.h"
#include "../audio/AudioCaptureService.h"
#include "Common.h"
#include "Menu.h"
#include "Themes.h"
#include "Utils.h"

namespace {
AudioCaptureService audioCapture;
FtxDecoderService decoderService;
bool pageActive = false;
uint8_t restoreNetworkMode = NET_OFF;
uint32_t lastPageRefresh = 0;
FtxMode pageMode = FtxMode::FT8;

FtxDecodedMessage recentMessages[3]{};
uint32_t decodedMessageCount = 0;
uint32_t ldpcErrorCount = 0;
time_t lastProcessedSlot = 0;
bool haveProcessedSlot = false;

// Temporarily raise receiver audio while the digital decoder is active.
// The original user volume is restored when leaving the decoder page.
constexpr uint8_t kFtxPlaybackVolume = 50;
uint8_t volumeBeforeFtx = 0;
bool ftxVolumeBoostActive = false;
// Toggle only the external speaker amplifier; keep SI473x audio and GPIO11 ADC capture alive.
bool ftxSpeakerAmpBefore = true;
bool ftxSpeakerEnabled = true;

const char* countryForCall(const char* call)
{
  if(!call || !*call) return "";
  char prefix[4] = {};
  size_t n = 0;
  while(call[n] && !isdigit(static_cast<unsigned char>(call[n])) && n < 3) {
    prefix[n] = static_cast<char>(toupper(static_cast<unsigned char>(call[n])));
    ++n;
  }
  if(!n && isdigit(static_cast<unsigned char>(call[0]))) {
    prefix[0] = call[0];
    prefix[1] = call[1] ? static_cast<char>(toupper(static_cast<unsigned char>(call[1]))) : 0;
  }
  // Common amateur-radio prefixes. Unknown/ambiguous calls deliberately have no flag.
  if(!strcmp(prefix,"DL") || !strcmp(prefix,"DA") || !strcmp(prefix,"DB") ||
     !strcmp(prefix,"DC") || !strcmp(prefix,"DD") || !strcmp(prefix,"DF") ||
     !strcmp(prefix,"DG") || !strcmp(prefix,"DH") || !strcmp(prefix,"DJ") ||
     !strcmp(prefix,"DK") || !strcmp(prefix,"DM") || !strcmp(prefix,"DO") ||
     !strcmp(prefix,"DP") || !strcmp(prefix,"DQ") || !strcmp(prefix,"DR")) return "DE";
  if(!strcmp(prefix,"G") || !strcmp(prefix,"M") || !strcmp(prefix,"2E") ||
     !strcmp(prefix,"2I") || !strcmp(prefix,"2M") || !strcmp(prefix,"2W")) return "GB";
  if(!strcmp(prefix,"F") || !strcmp(prefix,"TM") || !strcmp(prefix,"TO")) return "FR";
  if(!strcmp(prefix,"I") || !strcmp(prefix,"IK") || !strcmp(prefix,"IZ")) return "IT";
  if(!strcmp(prefix,"EA") || !strcmp(prefix,"EB") || !strcmp(prefix,"EC") ||
     !strcmp(prefix,"ED") || !strcmp(prefix,"EE") || !strcmp(prefix,"EF") ||
     !strcmp(prefix,"EG") || !strcmp(prefix,"EH")) return "ES";
  if(!strcmp(prefix,"CT") || !strcmp(prefix,"CS")) return "PT";
  if(!strcmp(prefix,"ON")) return "BE";
  if(!strcmp(prefix,"PA") || !strcmp(prefix,"PB") || !strcmp(prefix,"PC") ||
     !strcmp(prefix,"PD") || !strcmp(prefix,"PE") || !strcmp(prefix,"PF") ||
     !strcmp(prefix,"PG") || !strcmp(prefix,"PH") || !strcmp(prefix,"PI")) return "NL";
  if(!strcmp(prefix,"HB") || !strcmp(prefix,"HE")) return "CH";
  if(!strcmp(prefix,"OE")) return "AT";
  if(!strcmp(prefix,"OK") || !strcmp(prefix,"OL")) return "CZ";
  if(!strcmp(prefix,"OM")) return "SK";
  if(!strcmp(prefix,"SP") || !strcmp(prefix,"SQ") || !strcmp(prefix,"SR")) return "PL";
  if(!strcmp(prefix,"SM") || !strcmp(prefix,"SA") || !strcmp(prefix,"SB") ||
     !strcmp(prefix,"SC") || !strcmp(prefix,"SD") || !strcmp(prefix,"SE") ||
     !strcmp(prefix,"SF") || !strcmp(prefix,"SG") || !strcmp(prefix,"SH") ||
     !strcmp(prefix,"SI") || !strcmp(prefix,"SJ") || !strcmp(prefix,"SK") ||
     !strcmp(prefix,"SL") || !strcmp(prefix,"7S") || !strcmp(prefix,"8S")) return "SE";
  if(!strcmp(prefix,"OZ")) return "DK";
  if(!strcmp(prefix,"LA") || !strcmp(prefix,"LB") || !strcmp(prefix,"LC") ||
     !strcmp(prefix,"LD") || !strcmp(prefix,"LE") || !strcmp(prefix,"LF") ||
     !strcmp(prefix,"LG") || !strcmp(prefix,"LH") || !strcmp(prefix,"LI") ||
     !strcmp(prefix,"LJ") || !strcmp(prefix,"LK") || !strcmp(prefix,"LL") ||
     !strcmp(prefix,"LM") || !strcmp(prefix,"LN")) return "NO";
  if(!strcmp(prefix,"OH") || !strcmp(prefix,"OF") || !strcmp(prefix,"OG")) return "FI";
  if(!strcmp(prefix,"K") || !strcmp(prefix,"W") || !strcmp(prefix,"N") ||
     (prefix[0]=='A' && prefix[1]>='A' && prefix[1]<='L')) return "US";
  if(!strcmp(prefix,"VE") || !strcmp(prefix,"VA") || !strcmp(prefix,"VO") ||
     !strcmp(prefix,"VY")) return "CA";
  if(!strcmp(prefix,"VK")) return "AU";
  if(!strcmp(prefix,"JA") || !strcmp(prefix,"JE") || !strcmp(prefix,"JF") ||
     !strcmp(prefix,"JG") || !strcmp(prefix,"JH") || !strcmp(prefix,"JI") ||
     !strcmp(prefix,"JJ") || !strcmp(prefix,"JK") || !strcmp(prefix,"JL") ||
     !strcmp(prefix,"JM") || !strcmp(prefix,"JN") || !strcmp(prefix,"JO") ||
     !strcmp(prefix,"JP") || !strcmp(prefix,"JQ") || !strcmp(prefix,"JR")) return "JP";
  if(!strcmp(prefix,"ZL")) return "NZ";
  if(!strcmp(prefix,"PY") || !strcmp(prefix,"PP") || !strcmp(prefix,"PQ") ||
     !strcmp(prefix,"PR") || !strcmp(prefix,"PS") || !strcmp(prefix,"PT") ||
     !strcmp(prefix,"PU") || !strcmp(prefix,"PV") || !strcmp(prefix,"PW") ||
     !strcmp(prefix,"PX")) return "BR";
  if(!strcmp(prefix,"LU") || !strcmp(prefix,"LO") || !strcmp(prefix,"LP") ||
     !strcmp(prefix,"LQ") || !strcmp(prefix,"LR") || !strcmp(prefix,"LS") ||
     !strcmp(prefix,"LT") || !strcmp(prefix,"LW")) return "AR";
  if(!strcmp(prefix,"XE") || !strcmp(prefix,"XF") || !strcmp(prefix,"4A") ||
     !strcmp(prefix,"4B") || !strcmp(prefix,"4C")) return "MX";
  if(!strcmp(prefix,"HL") || !strcmp(prefix,"DS") || !strcmp(prefix,"DT")) return "KR";
  if(!strcmp(prefix,"BY") || !strcmp(prefix,"BA") || !strcmp(prefix,"BD") ||
     !strcmp(prefix,"BG") || !strcmp(prefix,"BH") || !strcmp(prefix,"BI") ||
     !strcmp(prefix,"BJ") || !strcmp(prefix,"BL") || !strcmp(prefix,"BT") ||
     !strcmp(prefix,"BU") || !strcmp(prefix,"BV") || !strcmp(prefix,"BW") ||
     !strcmp(prefix,"BX") || !strcmp(prefix,"BZ")) return "CN";
  return "";
}

bool isLocator(const char* token)
{
  const size_t len = strlen(token);
  if(len != 4 && len != 6) return false;
  if(token[0]<'A' || token[0]>'R' || token[1]<'A' || token[1]>'R' ||
     !isdigit(static_cast<unsigned char>(token[2])) ||
     !isdigit(static_cast<unsigned char>(token[3]))) return false;
  if(len == 6 && (token[4]<'A' || token[4]>'X' || token[5]<'A' || token[5]>'X')) return false;
  return true;
}

bool locatorCenter(const char* locator, double& lat, double& lon)
{
  if(!isLocator(locator)) return false;
  lon = -180.0 + (toupper(locator[0])-'A')*20.0 +
        (locator[2]-'0')*2.0 + 1.0;
  lat = -90.0 + (toupper(locator[1])-'A')*10.0 +
        (locator[3]-'0')*1.0 + 0.5;
  if(strlen(locator) == 6) {
    lon += (toupper(locator[4])-'A')*(2.0/24.0) + (1.0/24.0);
    lat += (toupper(locator[5])-'A')*(1.0/24.0) + (1.0/48.0);
  }
  return true;
}

bool distanceFromQthKm(const char* locator, int& km)
{
  double lat2, lon2;
  if(!locatorCenter("JO31TV", lat2, lon2)) return false;
  double lat1, lon1;
  if(!locatorCenter(locator, lat1, lon1)) return false;
  const double rad = 0.017453292519943295;
  const double dLat = (lat1-lat2)*rad;
  const double dLon = (lon1-lon2)*rad;
  const double a = sin(dLat/2)*sin(dLat/2) +
    cos(lat2*rad)*cos(lat1*rad)*sin(dLon/2)*sin(dLon/2);
  const double c = 2.0*atan2(sqrt(a), sqrt(1.0-a));
  km = static_cast<int>(6371.0*c + 0.5);
  return true;
}

void extractMetadata(const char* message, char* country, size_t countrySize,
                     char* locator, size_t locatorSize, int& distanceKm)
{
  country[0] = locator[0] = 0;
  distanceKm = -1;
  char copy[FTX_MAX_MESSAGE_LENGTH + 1] = {};
  strncpy(copy, message, sizeof(copy)-1);
  char* save = nullptr;
  for(char* token = strtok_r(copy, " ", &save); token; token = strtok_r(nullptr, " ", &save)) {
    if(!locator[0] && isLocator(token)) {
      strncpy(locator, token, locatorSize-1);
      distanceFromQthKm(locator, distanceKm);
    }
    if(!country[0]) {
      bool hasDigit = false;
      for(const char* p=token; *p; ++p) if(isdigit(static_cast<unsigned char>(*p))) hasDigit = true;
      if(hasDigit) {
        const char* found = countryForCall(token);
        if(*found) strncpy(country, found, countrySize-1);
      }
    }
  }
}

void drawFlag(const char* country, int x, int y)
{
  const uint16_t white=0xFFFF, black=0x0000, red=0xF800, gold=0xFEA0;
  const uint16_t blue=0x001F, navy=0x0010, green=0x07E0;
  spr.drawRect(x, y, 22, 14, 0x7BEF);
  if(!strcmp(country,"DE")) {
    spr.fillRect(x+1,y+1,20,4,black); spr.fillRect(x+1,y+5,20,4,red); spr.fillRect(x+1,y+9,20,4,gold);
  } else if(!strcmp(country,"FR")) {
    spr.fillRect(x+1,y+1,7,12,blue); spr.fillRect(x+8,y+1,6,12,white); spr.fillRect(x+14,y+1,7,12,red);
  } else if(!strcmp(country,"IT")) {
    spr.fillRect(x+1,y+1,7,12,green); spr.fillRect(x+8,y+1,6,12,white); spr.fillRect(x+14,y+1,7,12,red);
  } else if(!strcmp(country,"NL")) {
    spr.fillRect(x+1,y+1,20,4,red); spr.fillRect(x+1,y+5,20,4,white); spr.fillRect(x+1,y+9,20,4,blue);
  } else if(!strcmp(country,"BE")) {
    spr.fillRect(x+1,y+1,7,12,black); spr.fillRect(x+8,y+1,6,12,gold); spr.fillRect(x+14,y+1,7,12,red);
  } else if(!strcmp(country,"JP")) {
    spr.fillRect(x+1,y+1,20,12,white); spr.fillCircle(x+11,y+7,3,red);
  } else if(!strcmp(country,"US")) {
    for(int i=0;i<6;++i) spr.fillRect(x+1,y+1+i*2,20,1,(i&1)?white:red);
    spr.fillRect(x+1,y+1,9,6,navy);
  } else if(!strcmp(country,"GB")) {
    spr.fillRect(x+1,y+1,20,12,blue);
    spr.drawLine(x+2,y+2,x+20,y+12,white); spr.drawLine(x+20,y+2,x+2,y+12,white);
    spr.fillRect(x+9,y+1,4,12,white); spr.fillRect(x+1,y+5,20,4,white);
    spr.fillRect(x+10,y+1,2,12,red); spr.fillRect(x+1,y+6,20,2,red);
  } else if(!strcmp(country,"ES")) {
    spr.fillRect(x+1,y+1,20,3,red); spr.fillRect(x+1,y+4,20,6,gold); spr.fillRect(x+1,y+10,20,3,red);
  } else if(!strcmp(country,"CA")) {
    spr.fillRect(x+1,y+1,5,12,red); spr.fillRect(x+6,y+1,10,12,white); spr.fillRect(x+16,y+1,5,12,red);
    spr.fillTriangle(x+11,y+3,x+9,y+7,x+13,y+7,red); spr.fillTriangle(x+11,y+6,x+9,y+10,x+13,y+10,red);
  } else if(!strcmp(country,"CH")) {
    spr.fillRect(x+1,y+1,20,12,red); spr.fillRect(x+9,y+3,4,8,white); spr.fillRect(x+7,y+5,8,4,white);
  } else {
    // Unknown/unmapped flag: show the country code as a deliberate cyan badge.
    spr.fillRect(x+1,y+1,20,12,0x1036);
    spr.drawRect(x,y,22,14,0x07FF);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(0x07FF);
    spr.drawString(country,x+11,y+7);
  }
}

void addRecentMessage(const FtxDecodedMessage& message)
{
  if(!message.text[0]) return;
  for(int i=2;i>0;--i) recentMessages[i]=recentMessages[i-1];
  recentMessages[0]=message;
  ++decodedMessageCount;
  if(message.ldpcErrors>0) ldpcErrorCount += static_cast<uint32_t>(message.ldpcErrors);
}

void refreshRecentMessages()
{
  FtxDecodedMessage messages[FtxDecoderService::kResultCapacity]{};
  const size_t count=decoderService.copyResults(messages,FtxDecoderService::kResultCapacity);
  if(!count) return;
  time_t newestSlot=lastProcessedSlot;
  for(size_t i=0;i<count;++i)
    if(messages[i].slotUtc>newestSlot) newestSlot=messages[i].slotUtc;
  if(!haveProcessedSlot || newestSlot>lastProcessedSlot) {
    for(size_t i=0;i<count;++i)
      if(messages[i].slotUtc==newestSlot) addRecentMessage(messages[i]);
    lastProcessedSlot=newestSlot;
    haveProcessedSlot=true;
  }
}

void drawClippedText(const char* value, int x, int y, int maxWidth, uint16_t color)
{
  char clipped[FTX_MAX_MESSAGE_LENGTH+1]={};
  size_t len=strlen(value);
  if(len>=sizeof(clipped)) len=sizeof(clipped)-1;
  memcpy(clipped,value,len);
  clipped[len]=0;
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  while(len>0 && spr.textWidth(clipped)>maxWidth) clipped[--len]=0;
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(color);
  spr.drawString(clipped,x,y);
}
}

bool ftxUiOpen(FtxMode mode)
{
  if(!clockAvailable())
  {
    statusShow("UTC time not available", "Wait for NTP or RDS CT");
    return false;
  }

  pageMode = mode;
  restoreNetworkMode = wifiModeIdx;
  netStop();

  if(getWiFiStatus() != 0 || !clockAvailable() ||
     !decoderService.enter(mode, true, clockAvailable(), audioCapture))
  {
    decoderService.exit();
    netInit(restoreNetworkMode);
    statusShow("Decoder start failed", "Check UTC and audio input");
    return false;
  }

  // Raise the SI473x output level for audible monitoring and a stronger
  // signal at the existing audio tap. The decoder samples themselves are
  // untouched; only the receiver's output volume register is changed.
  volumeBeforeFtx = volume;
  if(volume < kFtxPlaybackVolume) {
    volume = kFtxPlaybackVolume;
    rx.setVolume(volume);
  }
  ftxVolumeBoostActive = true;

  // Preserve the amplifier state and default to audible monitoring on entry.
  ftxSpeakerAmpBefore = PIN_AMP_EN < 0 || digitalRead(PIN_AMP_EN) == HIGH;
  ftxSpeakerEnabled = ftxSpeakerAmpBefore;

  memset(recentMessages,0,sizeof(recentMessages));
  decodedMessageCount=ldpcErrorCount=0;
  lastProcessedSlot=0;
  haveProcessedSlot=false;
  pageActive = true;
  lastPageRefresh = 0;
  return true;
}

void ftxUiExit()
{
  if(!pageActive) return;
  pageActive = false;
  decoderService.exit();
  if(PIN_AMP_EN >= 0) digitalWrite(PIN_AMP_EN, ftxSpeakerAmpBefore ? HIGH : LOW);
  ftxSpeakerEnabled = ftxSpeakerAmpBefore;
  if(ftxVolumeBoostActive) {
    volume = volumeBeforeFtx;
    rx.setVolume(volume);
    ftxVolumeBoostActive = false;
  }
  netInit(restoreNetworkMode);
  currentCmd = CMD_NONE;
}

void ftxUiClick()
{
  if(!pageActive || PIN_AMP_EN < 0) return;
  ftxSpeakerEnabled = !ftxSpeakerEnabled;
  // Do not call muteOn()/rx.setAudioMute(): GPIO11 is the decoder's live ADC tap.
  digitalWrite(PIN_AMP_EN, ftxSpeakerEnabled ? HIGH : LOW);
}

bool ftxUiTick(uint32_t now)
{
  if(!pageActive || static_cast<uint32_t>(now - lastPageRefresh) < 500) return false;
  lastPageRefresh = now;
  refreshRecentMessages();
  return true;
}

void drawFtxDecoderPage()
{
  const uint16_t bg=0x0000, blueDark=0x1036, blueLight=0x1647, cyan=0x07FF;
  spr.fillSprite(bg);
  spr.drawRect(2,2,316,166,0x001F);

  // Compact top row: tuned frequency on the left, decode/error counters on the right.
  char frequencyText[24];
  snprintf(frequencyText,sizeof(frequencyText),"%u.%03u",
    currentFrequency/1000,currentFrequency%1000);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(0xFFFF);
  spr.drawString(frequencyText,8,5,&lgfx::fonts::Orbitron_Light_24);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(ftxSpeakerEnabled?0x07E0:0xF800);
  spr.drawString(ftxSpeakerEnabled?"SPK ON":"SPK OFF",151,13,FONT_SMALL);
  char counters[32];
  snprintf(counters,sizeof(counters),"DEC %lu ERR %lu",
    static_cast<unsigned long>(decodedMessageCount),
    static_cast<unsigned long>(ldpcErrorCount));
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(cyan);
  spr.drawString(counters,311,10,FONT_SMALL);

  // FT8 slot progress bar: cyan fill advances with collected decoder blocks.
  const uint32_t maxBlocks=decoderService.decoderMaxBlocks();
  const uint32_t blocks=decoderService.decoderBlocks();
  const int innerWidth=296;
  int progress=maxBlocks ? static_cast<int>((blocks>maxBlocks?maxBlocks:blocks)*innerWidth/maxBlocks) : 0;
  spr.drawRect(8,34,304,12,0x0359);
  spr.fillRect(10,36,innerWidth,8,blueDark);
  if(progress>0) spr.fillRect(10,36,progress,8,cyan);
  // Three large, alternating blue message rows with flag and QTH distance.
  for(int i=0;i<3;++i) {
    const int y=52+i*37;
    spr.fillRect(7,y,306,34,(i&1)?blueLight:blueDark);
    spr.drawRect(7,y,306,34,0x0359);
    if(!recentMessages[i].text[0]) {
      if(i==0) {
        spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
        spr.setTextDatum(ML_DATUM);
        spr.setTextColor(cyan);
        spr.drawString("Waiting for FT8/FT4 decodes...",14,y+10);
      }
      continue;
    }

    char country[4]={}, locator[8]={};
    int distanceKm=-1;
    extractMetadata(recentMessages[i].text,country,sizeof(country),locator,sizeof(locator),distanceKm);
    if(country[0]) drawFlag(country,12,y+10);

    const int messageX=country[0]?40:14;
    const int distanceWidth=64;
    const int messageWidth=306-(messageX-7)-distanceWidth-8;
    drawClippedText(recentMessages[i].text,messageX,y+9,messageWidth,0xFFFF);

    if(distanceKm>=0) {
      char distanceText[16];
      if(distanceKm>=10000) snprintf(distanceText,sizeof(distanceText),">9999 km");
      else snprintf(distanceText,sizeof(distanceText),"%d km",distanceKm);
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.setTextDatum(TR_DATUM);
      spr.setTextColor(cyan);
      spr.drawString(distanceText,306,y+9);
    } else {
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.setTextDatum(TR_DATUM);
      spr.setTextColor(0x7BEF);
      spr.drawString("---",306,y+9);
    }
  }
}
