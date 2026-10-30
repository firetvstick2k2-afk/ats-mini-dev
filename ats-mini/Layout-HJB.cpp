#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include <string.h>
#include <stdio.h>

// HJB is an independent layout. Keep all drawing local so existing layouts
// and their shared RDS/rendering behavior remain untouched.
static const uint16_t HJB_BG = 0x0000;
static const uint16_t HJB_YELLOW = 0xFFE0;
static const uint16_t HJB_GREEN = 0x07E0;
static const uint16_t HJB_BLUE = 0x07FF;
static const uint16_t HJB_RED = 0xF800;
static const uint16_t HJB_MUTED = 0x7BEF;

static void hjbDrawSignalGraph()
{
  const int x0 = 7, y0 = 67, w = 62, h = 42;
  spr.drawRect(x0, y0, w, h, 0x4208);
  spr.drawFastHLine(x0 + 2, y0 + h/2, w - 4, 0x2104);
  int rssi = constrain((int)rx.getCurrentRSSI(), 0, 100);
  int snr = constrain((int)rx.getCurrentSNR(), 0, 40);
  // Draw actual current levels, not an invented/jittered history trace.
  const int graphLeft = x0 + 3;
  const int graphRight = x0 + w - 3;
  const int rssiY = y0 + h - 3 - map(rssi, 0, 100, 0, h - 7);
  const int snrY = y0 + h - 3 - map(snr, 0, 40, 0, h - 7);
  spr.drawFastHLine(graphLeft, rssiY, graphRight - graphLeft, HJB_RED);
  spr.drawFastHLine(graphLeft, snrY, graphRight - graphLeft, HJB_BLUE);
  spr.drawFastVLine(graphLeft, rssiY - 2, 5, HJB_RED);
  spr.drawFastVLine(graphRight - 1, snrY - 2, 5, HJB_BLUE);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(HJB_RED);
  spr.drawString("RSSI", x0 + 2, y0 - 10, FONT_DEFAULT);
  spr.setTextColor(HJB_BLUE);
  spr.drawString("SNR", x0 + 34, y0 - 10, FONT_DEFAULT);
}

static void hjbDrawSignalBars()
{
  const int x = 251, y = 75;
  const int rssi = constrain((int)rx.getCurrentRSSI(), 0, 100);
  const int snr = constrain((int)rx.getCurrentSNR(), 0, 40);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(HJB_MUTED);
  spr.drawString("RSSI", x, y - 13, FONT_DEFAULT);
  spr.drawString("SNR", x, y + 23, FONT_DEFAULT);
  for(int i=0; i<8; ++i)
  {
    uint16_t c1 = i < map(rssi, 0, 100, 0, 8) ? (i < 5 ? HJB_GREEN : HJB_YELLOW) : 0x2104;
    uint16_t c2 = i < map(snr, 0, 40, 0, 8) ? (i < 5 ? HJB_BLUE : HJB_YELLOW) : 0x2104;
    spr.fillRect(x + i*7, y, 5, 8, c1);
    spr.fillRect(x + i*7, y + 36, 5, 8, c2);
  }
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor((currentMode == FM && fmStereoIdx != FM_STEREO_MONO && rx.getCurrentPilot()) ? HJB_GREEN : HJB_MUTED);
  spr.drawString((currentMode == FM && fmStereoIdx != FM_STEREO_MONO && rx.getCurrentPilot()) ? "STEREO" : "MONO", 313, 58, FONT_SMALL);
}

static void hjbDrawScale()
{
  const Band *band = getCurrentBand();
  if(!band) return;
  const int left = 4, right = 316, y = 146;
  spr.drawFastHLine(left, y, right-left, 0x4208);
  spr.drawFastHLine(left, 168, right-left, 0x4208);
  uint32_t span = (uint32_t)band->maximumFreq - band->minimumFreq;
  if(span == 0) return;
  for(int i=0; i<=40; ++i)
  {
    int x = left + (i * (right - left)) / 40;
    bool major = (i % 4) == 0;
    spr.drawFastVLine(x, major ? y : y+12, major ? 22 : 10, 0x7BEF);
    if(major)
    {
      uint32_t f = band->minimumFreq + (span * i) / 40;
      spr.setTextDatum(MC_DATUM);
      spr.setTextColor(HJB_MUTED);
      if(currentMode == FM) spr.drawFloat(f / 100.0f, 1, x, 139, FONT_DEFAULT);
      else spr.drawNumber(f, x, 139, FONT_DEFAULT);
    }
  }
  float pos = ((float)currentFrequency - (float)band->minimumFreq) / (float)span;
  pos = constrain(pos, 0.0f, 1.0f);
  int markerX = left + (int)(pos * (right-left));
  spr.fillTriangle(markerX-4, y-1, markerX+4, y-1, markerX, y+5, HJB_RED);
  spr.drawFastVLine(markerX, y+5, 17, HJB_RED);
}

void drawLayoutHJB()
{
  spr.fillSprite(HJB_BG);

  // Compact top row: band/PTY on the left, AF and status icons on the right.
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(HJB_GREEN);
  spr.drawString(currentMode == FM ? "FM" : (currentMode == AM ? "AM" : (currentMode == USB ? "USB" : "LSB")), 7, 4, FONT_SMALL);

  const char *pty = currentMode == FM ? getProgramInfo() : "";
  if(pty && *pty)
  {
    spr.setTextColor(HJB_MUTED);
    spr.drawString(pty, 7, 21, FONT_DEFAULT);
  }

  // AF is intentionally omitted until the SI4735 library exposes a verified
  // alternative-frequency getter in this firmware version.

  bool hasVoltage = drawBattery(BATT_OFFSET_X, BATT_OFFSET_Y);
  drawWiFiIndicator(hasVoltage ? WIFI_OFFSET_X : BATT_OFFSET_X - 13, WIFI_OFFSET_Y);
  drawBleIndicator(BLE_OFFSET_X, BLE_OFFSET_Y);
  drawSaveIndicator(SAVE_OFFSET_X, SAVE_OFFSET_Y);

  // Small date readout when the firmware clock has a valid date.
  uint16_t year;
  uint8_t month, day, weekday;
  if(clockGetDate(&year, &month, &day, &weekday))
  {
    char dateText[12];
    snprintf(dateText, sizeof(dateText), "%02u.%02u", (unsigned)day, (unsigned)month);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(HJB_MUTED);
    spr.drawString(dateText, 174, 7, FONT_DEFAULT);
  }

  // Main display: keep the tuned frequency as the primary value for now.
  // The shared clock may be set by NTP, so it must not be mistaken for
  // an RDS clock. RDS CT can be added here once a dedicated validity flag
  // is exposed by the firmware.
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(HJB_YELLOW);
  spr.setFont(FONT_DIGITS);
  if(currentMode == FM)
    spr.drawFloat(currentFrequency / 100.0f, 2, 160, 53);
  else
    spr.drawNumber(currentFrequency, 160, 53);

  if(currentMode == FM)
  {
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(HJB_MUTED);
    spr.drawString("MHz", 259, 62, &lgfx::fonts::FreeSansBold9pt7b);
  }
  else
  {
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(HJB_MUTED);
    spr.drawString("kHz", 259, 62, &lgfx::fonts::FreeSansBold9pt7b);
  }

  const char *station = getStationName();
  if(station && *station)
  {
    spr.setTextColor(HJB_GREEN);
    // The shared station API marks long EiBi names with a leading 0xFF.
    // Keep the HJB color and fit the long name inside the display.
    if((uint8_t)station[0] == 0xFF)
    {
      spr.setTextDatum(MC_DATUM);
      spr.drawString(station + 1, 160, 96, FONT_SMALL);
    }
    else
    {
      spr.setTextDatum(MC_DATUM);
      spr.drawString(station, 160, 96, FONT_LARGE);
    }
  }

  const char *rt = currentMode == FM ? getRadioText() : "";
  if(rt && *rt)
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(0xFFFF);
    spr.setFont(FONT_DEFAULT);
    // Keep the RT line within the 320px display even for long station messages.
    char rtLine[48];
    snprintf(rtLine, sizeof(rtLine), "%.44s", rt);
    spr.drawString(rtLine, 160, 119, FONT_SMALL);
  }

  hjbDrawSignalGraph();
  hjbDrawSignalBars();
  hjbDrawScale();
}
