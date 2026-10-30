#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include <time.h>
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
  int lastX = x0 + 3;
  int lastYR = y0 + h - 3 - map(rssi, 0, 100, 0, h - 7);
  int lastYB = y0 + h - 3 - map(snr, 0, 40, 0, h - 7);
  for(int i = 1; i < 14; ++i)
  {
    int x = x0 + 3 + i * 4;
    int jitterR = ((millis() / 140 + i * 7) % 9) - 4;
    int jitterB = ((millis() / 180 + i * 5) % 11) - 5;
    int yr = constrain(lastYR + jitterR, y0 + 3, y0 + h - 3);
    int yb = constrain(lastYB + jitterB, y0 + 3, y0 + h - 3);
    spr.drawLine(lastX, lastYR, x, yr, HJB_RED);
    spr.drawLine(lastX, lastYB, x, yb, HJB_BLUE);
    lastX = x; lastYR = yr; lastYB = yb;
  }
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
    int x = left + i * 7;
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
  float pos = (float)(currentFrequency - band->minimumFreq) / (float)span;
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

  // Main display: use the received RDS clock on FM only when valid;
  // otherwise retain the tuned frequency as the primary value.
  // clockGet() is the existing clock API. RDS Clock Time can set this clock
  // when NTP is unavailable; use it only when the firmware clock is valid.
  const char *rdsTime = currentMode == FM ? clockGet() : nullptr;
  bool hasRdsTime = rdsTime && rdsTime[0] && strchr(rdsTime, ':');
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(HJB_YELLOW);
  spr.setFont(FONT_DIGITS);
  if(hasRdsTime)
  {
    spr.drawString(rdsTime, 160, 53);
  }
  else if(currentMode == FM)
  {
    spr.drawFloat(currentFrequency / 100.0f, 2, 160, 53);
  }
  else
  {
    spr.drawNumber(currentFrequency, 160, 53);
  }

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
  if(station && *station && (uint8_t)station[0] != 0xFF)
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(HJB_GREEN);
    spr.drawString(station, 160, 96, FONT_LARGE);
  }
  else if(station && (uint8_t)station[0] == 0xFF && station[1])
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(HJB_GREEN);
    spr.drawString(station + 1, 160, 96, FONT_SMALL);
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
