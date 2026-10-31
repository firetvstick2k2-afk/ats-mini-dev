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
  // A short rolling trace: SNR is the mostly calm reference line, while RSSI
  // follows real receiver measurements and can rise/fall like the original HJB.
  const int x0 = 7, y0 = 56, w = 62, h = 40;
  const int graphLeft = x0 + 3, graphRight = x0 + w - 3;
  const int graphWidth = graphRight - graphLeft;
  const int graphBottom = y0 + h - 4;
  const int graphTop = y0 + 4;
  static uint8_t rssiHistory[56] = {};
  static int snrLevel = 0;
  static uint32_t lastSample = 0;
  const uint32_t now = millis();
  const int rssi = constrain((int)rx.getCurrentRSSI(), 0, 100);
  const int snr = constrain((int)rx.getCurrentSNR(), 0, 40);
  if (now - lastSample >= 100 || lastSample == 0)
  {
    for (int i = 0; i < 55; ++i) rssiHistory[i] = rssiHistory[i + 1];
    rssiHistory[55] = (uint8_t)rssi;
    snrLevel = snr;
    lastSample = now;
  }

  spr.drawRect(x0, y0, w, h, 0x4208);
  const int snrY = graphBottom - map(snrLevel, 0, 40, 0, graphBottom - graphTop);
  spr.drawFastHLine(graphLeft, snrY, graphWidth, HJB_BLUE);
  for (int i = 1; i < 56; ++i)
  {
    const int x1 = graphLeft + ((i - 1) * graphWidth) / 55;
    const int x2 = graphLeft + (i * graphWidth) / 55;
    const int y1 = graphBottom - map((int)rssiHistory[i - 1], 0, 100, 0, graphBottom - graphTop);
    const int y2 = graphBottom - map((int)rssiHistory[i], 0, 100, 0, graphBottom - graphTop);
    spr.drawLine(x1, y1, x2, y2, HJB_RED);
  }
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(HJB_RED);
  spr.drawString("RSSI", x0 + 2, y0 - 10, FONT_DEFAULT);
  spr.setTextColor(HJB_BLUE);
  spr.drawString("SNR", x0 + 34, y0 - 10, FONT_DEFAULT);
}

static void hjbDrawSignalBars()
{
  const int x = 258;
  const int rssi = constrain((int)rx.getCurrentRSSI(), 0, 100);
  const int snr = constrain((int)rx.getCurrentSNR(), 0, 40);
  const bool stereo = currentMode == FM && fmStereoIdx != FM_STEREO_MONO && rx.getCurrentPilot();

  // Center stereo above the RSSI bars, slightly higher than before.
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(stereo ? HJB_GREEN : HJB_MUTED);
  spr.drawString(stereo ? "STEREO" : "MONO", x + 31, 31, FONT_SMALL);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(HJB_MUTED);
  spr.drawString("RSSI", x, 48, FONT_DEFAULT);
  spr.drawString("SNR", x, 70, FONT_DEFAULT);
  const int rssiBars = constrain(map(rssi, 0, 100, 0, 8), 0, 8);
  const int snrBars = constrain(map(snr, 0, 40, 0, 8), 0, 8);
  for (int i = 0; i < 8; ++i)
  {
    uint16_t c1 = i < rssiBars ? (i < 5 ? HJB_GREEN : HJB_YELLOW) : 0x2104;
    uint16_t c2 = i < snrBars ? (i < 5 ? HJB_BLUE : HJB_YELLOW) : 0x2104;
    spr.fillRect(x + i * 8, 60, 6, 6, c1);
    spr.fillRect(x + i * 8, 82, 6, 6, c2);
  }
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

  // Compact top row; FM and PTY are centered above the frequency.
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(HJB_GREEN);
  // Match the readable size of the voltage indicator in the top row.
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawString(currentMode == FM ? "FM" : (currentMode == AM ? "AM" : (currentMode == USB ? "USB" : "LSB")), 7, 5);
  spr.setFont(FONT_DEFAULT);

  const char *pty = currentMode == FM ? getProgramInfo() : "";
  if(pty && *pty)
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(0xFFFF);
    spr.drawString(pty, 160, 12, FONT_SMALL);
  }

  // AF is intentionally omitted until the SI4735 library exposes a verified
  // alternative-frequency getter in this firmware version.

  bool hasVoltage = drawBattery(BATT_OFFSET_X, BATT_OFFSET_Y);
  drawWiFiIndicator(hasVoltage ? WIFI_OFFSET_X : BATT_OFFSET_X - 13, WIFI_OFFSET_Y);
  drawBleIndicator(WIFI_OFFSET_X - 18, WIFI_OFFSET_Y);
  drawSaveIndicator(SAVE_OFFSET_X, SAVE_OFFSET_Y);

  // Show the clock only after a valid RDS CT group has actually been received.
  // Otherwise the tuned frequency remains visible, even if NTP set the system clock.
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(HJB_YELLOW);
  spr.setFont(FONT_DIGITS);
  const char *rdsClock = (currentMode == FM && rdsTimeAvailable()) ? getRdsTime() : NULL;
  if(rdsClock && *rdsClock)
    spr.drawString(rdsClock, 160, 53, FONT_DIGITS);
  else if(currentMode == FM)
    spr.drawFloat(currentFrequency / 100.0f, 2, 160, 53);
  else
    spr.drawNumber(currentFrequency, 160, 53);


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
  // Reuse the standard original-style frequency ruler: moving scale, fixed tuned marker.
  drawScale(isSSB() ? (currentFrequency + currentBFO / 1000) : currentFrequency);

  // Match the TEF6686 layout: render the sidebar only while a menu command is active.
  if (currentCmd != CMD_NONE &&
      currentCmd != CMD_SCAN &&
      currentCmd != CMD_FREQ &&
      currentCmd != CMD_FREQ_KEYPAD)
  {
    drawSideBar(currentCmd, MENU_OFFSET_X, MENU_OFFSET_Y, MENU_DELTA_X);
  }
}
