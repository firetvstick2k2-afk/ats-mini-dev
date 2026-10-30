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

  // Top band selectors: the AIR slot becomes a live WEB status indicator.
  const char *bandsTop[] = {"LW", "MW", "SW", "VHF", "WEB"};
  const int bx[] = {14, 148, 282, 416, 550}; // positions are clamped to the 320px display below
  (void)bx;
  const int x[] = {3, 69, 135, 201, 267};
  for(int i=0;i<5;i++)
  {
    const bool selected = (i==3 && getCurrentBand()->bandType==FM_BAND_TYPE);
    const bool webActive = (i==4 && (wifiModeIdx==NET_AP_ONLY || wifiModeIdx==NET_AP_CONNECT));
    const uint16_t color = (selected || webActive) ? CYAN : GREY;
    spr.drawRect(x[i], 3, 50, 19, color);
    spr.setTextColor((selected || webActive) ? CYAN : GREY);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(bandsTop[i], x[i]+25, 12, FONT_SMALL);
  }

  // Brightness and volume status; compact text keeps the frequency dominant.
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(WHITE);
  spr.drawString("BR", 206, 7, FONT_DEFAULT);
  spr.drawNumber(currentBrt * 100 / 255, 230, 5, FONT_SMALL);
  spr.drawString("%", 245, 7, FONT_DEFAULT);
  spr.drawString("VOL", 275, 7, FONT_DEFAULT);
  spr.drawNumber(volume * 100 / 63, 306, 5, FONT_SMALL);
  spr.drawString("%", 319, 7, FONT_DEFAULT);

  // Frequency formatting is deliberately local to TinyRadio.
  // FM uses 10-kHz units (e.g. 10800 = 108.00 MHz); AM/SSB use kHz.
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
    // Shortwave / HF display in MHz. Include SSB fine tuning in the
    // displayed fractional kHz without changing the receiver frequency.
    int64_t hz = (int64_t)currentFrequency * 1000;
    if(isSSB()) hz += currentBFO;
    if(hz < 0) hz = 0;
    snprintf(freq, sizeof(freq), "%lld.%03lld",
      (long long)(hz / 1000000),
      (long long)((hz % 1000000) / 1000));
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
  {
    snprintf(freq, sizeof(freq), "%lu kHz", (unsigned long)currentFrequency);
  }
  spr.setTextColor(YELLOW);
  spr.setTextDatum(TL_DATUM);
  // Smaller font avoids clipping and leaves room for the unit.
  spr.drawString(freq, 8, 31, FONT_LARGE);
  if(showMHz)
  {
    spr.setTextColor(YELLOW);
    spr.drawString("MHz", 226, 49, FONT_SMALL);
  }

  // Mode and receiver controls. Enabled choices use the same cyan outline as the reference.
  const char *modes[] = {"FM","AM","LSB","USB"};
  const uint8_t modeValues[] = {FM, AM, LSB, USB};
  const int mx[] = {3, 53, 103, 153};
  for(int i=0;i<4;i++)
  {
    const bool active = currentMode==modeValues[i];
    spr.drawRect(mx[i], 75, 44, 22, active ? CYAN : GREY);
    spr.setTextColor(active ? CYAN : GREY);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(modes[i], mx[i]+22, 86, FONT_SMALL);
  }
  const char *controls[] = {"BW", "AGC", "SM"};
  const int cx[] = {203, 243, 283};
  const char *values[] = {"AUTO", "AUTO", "16dB"};
  for(int i=0;i<3;i++)
  {
    spr.drawRect(cx[i], 75, i==2 ? 34 : 36, 22, CYAN);
    spr.setTextColor(CYAN);
    spr.setTextDatum(MC_DATUM);
    char label[16]; snprintf(label, sizeof(label), "%s %s", controls[i], values[i]);
    spr.drawString(label, cx[i]+(i==2?17:18), 86, FONT_DEFAULT);
  }

  // Signal/status row.
  int rssi = rx.getCurrentRSSI();
  int snr = rx.getCurrentSNR();
  for(int i=0;i<10;i++)
    spr.fillRect(8+i*7, 101, 5, 9, i < (rssi>0 ? (rssi>100?10:rssi/10) : 0) ? GREEN : GREY);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(WHITE);
  char stats[24]; snprintf(stats, sizeof(stats), "%ddBuV", rssi);
  spr.drawString(stats, 82, 99, FONT_SMALL);
  snprintf(stats, sizeof(stats), "SN%ddB", snr);
  spr.drawString(stats, 143, 99, FONT_SMALL);
  spr.setTextColor(GREEN);
  spr.drawString("VALID", 206, 99, FONT_SMALL);
  spr.setTextColor(WHITE);
  spr.drawString("AFC OK", 258, 99, FONT_DEFAULT);

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
