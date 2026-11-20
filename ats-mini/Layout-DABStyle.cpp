#include "Common.h"
#include "Draw.h"
#include <LittleFS.h>
#include <ctype.h>

// FM-only DAB-style screen ported from the ATS Mini OS FM layout.
// Sender images are read from the existing LittleFS /Logos directory.
namespace {
static uint16_t cachedPi = 0xFFFF;
static char cachedLogoPath[96] = {};

static bool isPngName(const String &name)
{
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".png");
}

static void findStationLogo(uint16_t pi)
{
  if(pi == cachedPi) return;
  cachedPi = pi;
  cachedLogoPath[0] = 0;
  if(!pi || !LittleFS.exists("/Logos")) return;

  char piText[5];
  snprintf(piText, sizeof(piText), "%04X", static_cast<unsigned>(pi));
  String needle(piText);
  File dir = LittleFS.open("/Logos", "r");
  if(!dir || !dir.isDirectory()) { if(dir) dir.close(); return; }

  File entry = dir.openNextFile();
  while(entry)
  {
    if(!entry.isDirectory())
    {
      String path = entry.name();
      String upper = path;
      upper.toUpperCase();
      if(isPngName(path) && upper.indexOf(needle) >= 0)
      {
        if(path[0] != '/') path = String("/Logos/") + path;
        path.toCharArray(cachedLogoPath, sizeof(cachedLogoPath));
        entry.close();
        break;
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
}
}

void drawLayoutDABStyle()
{
  constexpr uint16_t BG=0x0000, WHITE=0xFFFF, GREEN=0x07E0;
  constexpr uint16_t CYAN=0x07FF, MUTED=0x7BEF, GRID=0x4208;
  const int w=spr.width();
  spr.fillSprite(BG);

  // Header: band, RDS clock, Wi-Fi and battery indicators.
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(GREEN);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawString(getCurrentBand()->bandName, 8, 3);
  if(rdsTimeAvailable())
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(WHITE);
    spr.setFont(&lgfx::fonts::Font2);
    spr.drawString(getRdsTime(), w/2, 4);
  }
  bool hasVoltage=drawBattery(w-43, 3);
  drawWiFiIndicator(hasVoltage ? w-73 : w-43, 3);
  spr.drawFastHLine(8,25,w-16,GRID);

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

  // Local PI-matched PNG logo. The image is scaled into the square viewport.
  constexpr int logoX=8,logoY=29,logoW=108,logoH=108;
  spr.fillRoundRect(logoX,logoY,logoW,logoH,5,WHITE);
  findStationLogo(getRdsPiCode());
  bool logoDrawn=false;
  if(cachedLogoPath[0])
    logoDrawn=spr.drawPngFile(LittleFS,cachedLogoPath,logoX+4,logoY+4,logoW-8,logoH-8);
  if(!logoDrawn)
  {
    spr.fillRoundRect(logoX,logoY,logoW,logoH,5,BG);
    spr.drawRoundRect(logoX,logoY,logoW,logoH,5,MUTED);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(WHITE);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.drawString(cachedLogoPath[0] ? "LOGO ERROR" : "NO LOGO",logoX+logoW/2,logoY+logoH/2);
  }

  // FM frequency, large digital font.
  char frequency[20];
  snprintf(frequency,sizeof(frequency),"%u.%02u",
    static_cast<unsigned>(currentFrequency/100),
    static_cast<unsigned>(currentFrequency%100));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(GREEN);
  spr.setFont(&lgfx::fonts::Font7);
  spr.setTextSize(0.78f);
  spr.drawString(frequency,124,56);
  spr.setTextSize(1.0f);
  spr.setTextColor(CYAN);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawRightString("MHz",w-8,85);

  // Stereo/mono and programme type.
  constexpr int badgeY=104;
  spr.drawRoundRect(124,badgeY,78,20,4,GREEN);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(GREEN);
  spr.setFont(&lgfx::fonts::Font0);
  spr.drawString(rx.getCurrentStereo() ? "STEREO" : "MONO",163,badgeY+6);
  spr.drawRoundRect(210,badgeY,w-218,20,4,WHITE);
  const char *pty=getProgramInfo();
  spr.setTextColor(WHITE);
  spr.drawString((pty && *pty) ? pty : "FM",210+(w-218)/2,badgeY+6);

  // RDS RadioText in a dedicated green ticker frame.
  constexpr int rtX=8,rtY=143,rtW=304,rtH=24;
  spr.drawRoundRect(rtX,rtY,rtW,rtH,6,GREEN);
  const char *rt=getRadioText();
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(WHITE);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.setClipRect(rtX+8,rtY+2,rtW-16,rtH-4);
  if(rt && *rt) spr.drawString(rt,rtX+12,rtY+4);
  else spr.drawString("NO RADIO TEXT",rtX+12,rtY+4);
  spr.clearClipRect();

  if(currentCmd!=CMD_NONE && currentCmd!=CMD_SCAN && currentCmd!=CMD_FREQ)
    drawSideBar(currentCmd,MENU_OFFSET_X,MENU_OFFSET_Y,MENU_DELTA_X);
}
