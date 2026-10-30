#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"

// TinyRadio-inspired ATS Mini layout. Kept isolated from all existing layouts.
void drawLayoutTinyRadio()
{
  constexpr uint16_t BLACK = 0x0000, WHITE = 0xFFFF, GREY = 0x8410;
  constexpr uint16_t CYAN = 0x07FF, YELLOW = 0xFFE0, GREEN = 0x07E0, RED = 0xF800;
  spr.fillSprite(BLACK);

  // Compact top row: five selectors on the left; status values on the right.
  // All coordinates are designed for the physical 320x170 display.
  const char *bandsTop[] = {"LW", "MW", "SW", "VHF", "WEB"};
  const int bx[] = {2, 37, 72, 107, 142};
  for(int i=0;i<5;i++)
  {
    const bool selected = (i==3 && getCurrentBand()->bandType==FM_BAND_TYPE);
    const bool webActive = (i==4 && (wifiModeIdx==NET_AP_ONLY || wifiModeIdx==NET_AP_CONNECT));
    const uint16_t color = (selected || webActive) ? CYAN : GREY;
    spr.drawRect(bx[i], 3, 32, 17, color);
    spr.setTextColor((selected || webActive) ? CYAN : GREY);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(bandsTop[i], bx[i]+16, 11, FONT_DEFAULT);
  }

  // Tiny status readouts stay to the right of the band selectors.
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(WHITE);
  char topStatus[12];
  snprintf(topStatus, sizeof(topStatus), "B%u%%", (unsigned)(currentBrt * 100 / 255));
  spr.drawString(topStatus, 215, 5, FONT_DEFAULT);
  snprintf(topStatus, sizeof(topStatus), "V%u%%", (unsigned)(volume * 100 / 63));
  spr.drawString(topStatus, 268, 5, FONT_DEFAULT);
  spr.drawRect(280, 4, 36, 14, (wifiModeIdx==NET_AP_ONLY || wifiModeIdx==NET_AP_CONNECT) ? CYAN : GREY);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor((wifiModeIdx==NET_AP_ONLY || wifiModeIdx==NET_AP_CONNECT) ? CYAN : GREY);
  spr.drawString((wifiModeIdx==NET_AP_ONLY || wifiModeIdx==NET_AP_CONNECT) ? "WEB" : "OFF", 298, 11, FONT_DEFAULT);

  // Frequency: size and unit are constrained to the upper display region.
  char freq[24];
  bool showMHz = false;
  if(currentMode == FM)
  {
    snprintf(freq, sizeof(freq), "%lu.%02lu",
      (unsigned long)(currentFrequency / 100),
      (unsigned long)(currentFrequency % 100));
    showMHz = true;
  }
  else if(currentFrequency >= 1000)
  {
    int64_t hz = (int64_t)currentFrequency * 1000;
    if(isSSB()) hz += currentBFO;
    if(hz < 0) hz = 0;
    snprintf(freq, sizeof(freq), "%lu.%03lu",
      (unsigned long)(hz / 1000000),
      (unsigned long)((hz % 1000000) / 1000));
    showMHz = true;
  }
  else if(isSSB())
  {
    int32_t hz = (int32_t)currentFrequency * 1000 + currentBFO;
    if(hz < 0) hz = 0;
    snprintf(freq, sizeof(freq), "%lu.%03lu kHz",
      (unsigned long)(hz / 1000), (unsigned long)(hz % 1000));
  }
  else
    snprintf(freq, sizeof(freq), "%lu kHz", (unsigned long)currentFrequency);

  spr.setTextColor(YELLOW);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(freq, 8, 25, FONT_LARGE);
  if(showMHz)
  {
    spr.setTextColor(YELLOW);
    spr.setTextDatum(TR_DATUM);
    spr.drawString("MHz", 310, 49, FONT_SMALL);
  }

  // Mode row and controls: fixed non-overlapping columns.
  const char *modes[] = {"FM","AM","LSB","USB"};
  const uint8_t modeValues[] = {FM, AM, LSB, USB};
  const int mx[] = {3, 43, 83, 123};
  for(int i=0;i<4;i++)
  {
    const bool active = currentMode==modeValues[i];
    spr.drawRect(mx[i], 66, 36, 21, active ? CYAN : GREY);
    spr.setTextColor(active ? CYAN : GREY);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(modes[i], mx[i]+18, 76, FONT_DEFAULT);
  }
  const int cx[] = {164, 215, 270};
  const int cw[] = {47, 51, 46};
  const char *labels[] = {"BW AUTO", "AGC AUTO", "SM 16dB"};
  for(int i=0;i<3;i++)
  {
    spr.drawRect(cx[i], 66, cw[i], 21, CYAN);
    spr.setTextColor(CYAN);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(labels[i], cx[i]+cw[i]/2, 76, FONT_DEFAULT);
  }

  // Signal/status row: labels remain separated and inside the screen.
  int rssi = rx.getCurrentRSSI();
  int snr = rx.getCurrentSNR();
  for(int i=0;i<8;i++)
    spr.fillRect(5+i*6, 93, 4, 8, i < (rssi>0 ? (rssi>80?8:rssi/10) : 0) ? GREEN : GREY);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(WHITE);
  char stats[20];
  snprintf(stats, sizeof(stats), "%ddBuV", rssi);
  spr.drawString(stats, 56, 91, FONT_DEFAULT);
  snprintf(stats, sizeof(stats), "SN%d", snr);
  spr.drawString(stats, 105, 91, FONT_DEFAULT);
  spr.setTextColor(GREEN);
  spr.drawString("VALID", 145, 91, FONT_DEFAULT);
  spr.setTextColor(WHITE);
  spr.drawString(isSSB() ? "BFO" : "AFC", 198, 91, FONT_DEFAULT);
  spr.drawString("OK", 230, 91, FONT_DEFAULT);
  spr.drawString("RF0", 256, 91, FONT_DEFAULT);

  // Radio text is shown above the ruler when available.
  const char *rt = getRadioText();
  const char *station = getStationName();
  spr.setTextColor(WHITE);
  spr.setTextDatum(TC_DATUM);
  if(rt && *rt) spr.drawString(rt, 160, 125, FONT_SMALL);
  else if(station && *station) spr.drawString(station, 160, 125, FONT_SMALL);

  // Frequency ruler, centered on the tuned frequency.
  const uint32_t f = currentFrequency + (isSSB() ? currentBFO/1000 : 0);
  spr.drawFastHLine(0, 142, DISPLAY_WIDTH, WHITE);
  for(int i=0;i<=20;i++)
  {
    int px=i*16;
    bool major=(i%4)==0;
    spr.drawFastVLine(px, 142, major?12:6, GREY);
    if(major)
    {
      int32_t delta=(i-10)*((getCurrentBand()->bandType==FM_BAND_TYPE)?100:50);
      int32_t tick=(int32_t)f+delta;
      char label[16];
      if(tick>=1000) snprintf(label,sizeof(label),"%ld.%ldM",(long)(tick/1000),(long)((tick%1000)/100));
      else snprintf(label,sizeof(label),"%ldk",(long)tick);
      spr.setTextColor(WHITE);
      spr.setTextDatum(TC_DATUM);
      spr.drawString(label, px, 155, FONT_DEFAULT);
    }
  }
  // Red pointer at the tuned frequency.
  spr.fillTriangle(153, 135, 167, 135, 160, 143, RED);
  spr.drawFastVLine(160, 143, 9, RED);

  // Keep the normal menu/sidebar visible in TinyRadio, as in the other layouts.
  // Keypad and scanner screens are handled earlier by Draw.cpp.
  if(currentCmd != CMD_NONE && currentCmd != CMD_SCAN && currentCmd != CMD_FREQ)
    drawSideBar(currentCmd, MENU_OFFSET_X, MENU_OFFSET_Y, MENU_DELTA_X);
}
