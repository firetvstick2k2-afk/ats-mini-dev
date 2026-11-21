#include "StationLogoClient.h"
#include "Common.h"
#include "Menu.h"
#include "Draw.h"
#include <ctype.h>

// FM-only DAB-style screen ported from the ATS Mini OS FM layout.
// Sender images are read from the existing LittleFS /Logos directory.
namespace {
bool getLogoDimensions(const uint8_t* data, size_t length, uint16_t& width, uint16_t& height) {
  width = height = 0;
  if (!data || length < 10) return false;

  // PNG stores its dimensions as big-endian integers in the IHDR chunk.
  static const uint8_t pngSignature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (length >= 24 && memcmp(data, pngSignature, sizeof(pngSignature)) == 0) {
    const uint32_t w = (static_cast<uint32_t>(data[16]) << 24) |
                       (static_cast<uint32_t>(data[17]) << 16) |
                       (static_cast<uint32_t>(data[18]) << 8) | data[19];
    const uint32_t h = (static_cast<uint32_t>(data[20]) << 24) |
                       (static_cast<uint32_t>(data[21]) << 16) |
                       (static_cast<uint32_t>(data[22]) << 8) | data[23];
    if (!w || !h || w > 65535 || h > 65535) return false;
    width = static_cast<uint16_t>(w);
    height = static_cast<uint16_t>(h);
    return true;
  }

  // JPEG dimensions are carried by a Start Of Frame marker.
  if (length < 4 || data[0] != 0xFF || data[1] != 0xD8) return false;
  size_t pos = 2;
  while (pos + 4 <= length) {
    if (data[pos] != 0xFF) { ++pos; continue; }
    while (pos < length && data[pos] == 0xFF) ++pos;
    if (pos >= length) break;
    const uint8_t marker = data[pos++];
    if (marker == 0xD9 || marker == 0xDA) break;
    if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;
    if (pos + 2 > length) break;
    const uint16_t segmentLength = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
    if (segmentLength < 2 || pos + segmentLength > length) break;
    const bool isSof = (marker >= 0xC0 && marker <= 0xC3) ||
                       (marker >= 0xC5 && marker <= 0xC7) ||
                       (marker >= 0xC9 && marker <= 0xCB) ||
                       (marker >= 0xCD && marker <= 0xCF);
    if (isSof && segmentLength >= 7) {
      height = (static_cast<uint16_t>(data[pos + 3]) << 8) | data[pos + 4];
      width = (static_cast<uint16_t>(data[pos + 5]) << 8) | data[pos + 6];
      return width > 0 && height > 0;
    }
    pos += segmentLength;
  }
  return false;
}
}

void drawLayoutDABStyle()
{
  constexpr uint16_t BG=0x0000, WHITE=0xFFFF;
  constexpr uint16_t BLUE=0x001F; // Match the vivid blue used by the main-menu frame.
  constexpr uint16_t MUTED=0x7BEF;
  const int w=spr.width();
  spr.fillSprite(BG);

  // Header: band, RDS clock, Wi-Fi and battery indicators.
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(BLUE);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawString(getCurrentBand()->bandName, 8, 3);
  if(rdsTimeAvailable())
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(WHITE);
    spr.setFont(&lgfx::fonts::Font2);
    spr.drawString(getRdsTime(), w/2, 13);
  }
  bool hasVoltage=drawBattery(w-43, 3);
  drawWiFiIndicator(hasVoltage ? w-100 : w-43, 3);
  spr.drawFastHLine(8,25,w-16,BLUE);

  // Sender name, with a reserved logo column on the left.
  const int stationX=124;
  const char *station=getStationName();
  if(station && *station)
  {
    if(static_cast<uint8_t>(*station)==0xFF) ++station;
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(WHITE);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.setClipRect(stationX,29,w-stationX-8,22);
    spr.drawString(station,stationX,30);
    spr.clearClipRect();
  }

  // Mini_OS StationLogoClient: local /logos lookup first, then online fallback.
  constexpr int logoX=8,logoY=29,logoW=108,logoH=108;
  spr.fillRoundRect(logoX,logoY,logoW,logoH,5,WHITE);

  StationLogoState logoState{};
  logoState.piCode = getRdsPiCode();
  logoState.frequencyKHz = static_cast<uint32_t>(currentFrequency) * 10UL;
  const char *rawStation = getStationName();
  if(rawStation && static_cast<uint8_t>(*rawStation)==0xFF) ++rawStation;
  if(rawStation) snprintf(logoState.stationName,sizeof(logoState.stationName),"%s",rawStation);
  const char *rawRadioText = getRadioText();
  if(rawRadioText) snprintf(logoState.radioText,sizeof(logoState.radioText),"%s",rawRadioText);

  StationLogoClient& logoClient = dabLogoClient();
  logoClient.update(logoState,getWiFiStatus()>0);

  char logoKey[40] = {};
  if(logoState.piCode && logoState.frequencyKHz)
    snprintf(logoKey,sizeof(logoKey),"%04X_%lu",
      static_cast<unsigned>(logoState.piCode),static_cast<unsigned long>(logoState.frequencyKHz));

  const uint8_t *logoData = nullptr;
  size_t logoLength = 0;
  bool logoIsJpg = false;
  const bool logoLocked = logoKey[0] &&
    logoClient.lockLogo(logoKey,logoData,logoLength,logoIsJpg);
  const bool logoSearching = !logoLocked && logoKey[0] &&
    logoClient.isLookupPending(logoKey);

  bool logoDrawn = false;
  if(logoLocked && logoData && logoLength)
  {
    uint16_t sourceW=0,sourceH=0;
    if(getLogoDimensions(logoData,logoLength,sourceW,sourceH))
    {
      const float sx=static_cast<float>(logoW-8)/sourceW;
      const float sy=static_cast<float>(logoH-8)/sourceH;
      const float scale=sx<sy?sx:sy;
      const int drawW=static_cast<int>(sourceW*scale+0.5f);
      const int drawH=static_cast<int>(sourceH*scale+0.5f);
      const int drawX=logoX+(logoW-drawW)/2;
      const int drawY=logoY+(logoH-drawH)/2;
      if(logoIsJpg)
        logoDrawn=spr.drawJpg(logoData,logoLength,drawX,drawY,0,0,0,0,scale,scale);
      else
        logoDrawn=spr.drawPng(logoData,logoLength,drawX,drawY,0,0,0,0,scale,scale);
    }
  }
  if(logoLocked) logoClient.unlockLogo();

  if(!logoDrawn)
  {
    spr.fillRoundRect(logoX,logoY,logoW,logoH,5,BG);
    spr.drawRoundRect(logoX,logoY,logoW,logoH,5,MUTED);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(WHITE);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    if(logoSearching)
    {
      spr.drawString("SEARCHING",logoX+logoW/2,logoY+42);
      spr.drawString("LOGO...",logoX+logoW/2,logoY+62);
    }
    else
      spr.drawString(logoData ? "LOGO ERROR" : "NO LOGO",logoX+logoW/2,logoY+logoH/2);
  }

  // FM frequency, large digital font.
  char frequency[20];
  snprintf(frequency,sizeof(frequency),"%u.%02u",
    static_cast<unsigned>(currentFrequency/100),
    static_cast<unsigned>(currentFrequency%100));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(WHITE);
  spr.setFont(&lgfx::fonts::Font7);
  spr.setTextSize(0.78f);
  spr.drawString(frequency,124,56);
  spr.setTextSize(1.0f);
  spr.setTextColor(BLUE);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawRightString("MHz",w-8,85);

  // Stereo/mono and programme type.
  constexpr int badgeY=104;
  spr.drawRoundRect(124,badgeY,78,20,4,BLUE);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(BLUE);
  spr.setFont(&lgfx::fonts::Font0);
  spr.drawString("FM",163,badgeY+10);
  spr.drawRoundRect(210,badgeY,w-218,20,4,WHITE);
  const char *pty=getProgramInfo();
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(WHITE);
  spr.drawString((pty && *pty) ? pty : "FM",210+(w-218)/2,badgeY+10);

  // RDS RadioText in a dedicated green ticker frame.
  constexpr int rtX=8,rtY=143,rtW=304,rtH=24;
  spr.drawRoundRect(rtX,rtY,rtW,rtH,6,BLUE);
  const char *rt=getRadioText();
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(WHITE);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.setClipRect(rtX+8,rtY+2,rtW-16,rtH-4);
  if(rt && *rt) spr.drawString(rt,rtX+rtW/2,rtY+rtH/2);
  else spr.drawString("NO RADIO TEXT",rtX+rtW/2,rtY+rtH/2);
  spr.clearClipRect();

  if(currentCmd!=CMD_NONE && currentCmd!=CMD_SCAN && currentCmd!=CMD_FREQ)
    drawSideBar(currentCmd,MENU_OFFSET_X,MENU_OFFSET_Y,MENU_DELTA_X);
}
