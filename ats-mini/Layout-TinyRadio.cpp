#include "Common.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include "BleMode.h"

// TinyRadio-inspired ATS Mini layout. Kept isolated from all existing layouts.
void drawLayoutTinyRadio()
{
  constexpr uint16_t BLACK = 0x0000, WHITE = 0xFFFF, GREY = 0x8410;
  constexpr uint16_t CYAN = 0x07FF, YELLOW = 0xFFE0, GREEN = 0x07E0, RED = 0xF800;
  spr.fillSprite(BLACK);

  // Compact top row: five selectors on the left; status values on the right.
  // All coordinates are designed for the physical 320x170 display.
  const char *bandsTop[] = {"LW", "MW", "SW", "VHF", "BT", "WEB"};
  const int bx[] = {2, 37, 72, 107, 142, 177};
  for(int i=0;i<6;i++)
  {
    const bool selected = (i==3 && getCurrentBand()->bandType==FM_BAND_TYPE);
    const bool webActive = (i==5 && (wifiModeIdx==NET_AP_ONLY || wifiModeIdx==NET_AP_CONNECT));
    const bool btActive = (i==4 && getBleStatus()!=0);
    const uint16_t color = (selected || webActive || btActive) ? CYAN : GREY;
    spr.drawRect(bx[i], 3, 32, 17, color);
    spr.setTextColor((selected || webActive || btActive) ? CYAN : GREY);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(bandsTop[i], bx[i]+16, 11, FONT_DEFAULT);
  }

  // Restore the real battery indicator; brightness and volume are not useful here.
  // Use the same battery drawing routine as the rest of the firmware.
  drawBattery(BATT_OFFSET_X, BATT_OFFSET_Y + 2);



  // Frequency and unit are formatted as one centered string.
  // FM frequencies are stored in 10-kHz units; AM/SSB values are in kHz.
  char freq[32];
  if(currentMode == FM)
  {
    snprintf(freq, sizeof(freq), "%lu.%02lu MHz",
      (unsigned long)(currentFrequency / 100),
      (unsigned long)(currentFrequency % 100));
  }
  else if(isSSB())
  {
    int32_t hz = (int32_t)currentFrequency * 1000 + currentBFO;
    if(hz < 0) hz = 0;
    snprintf(freq, sizeof(freq), "%lu.%03lu kHz",
      (unsigned long)(hz / 1000),
      (unsigned long)(hz % 1000));
  }
  else
  {
    snprintf(freq, sizeof(freq), "%lu kHz", (unsigned long)currentFrequency);
  }
  // Left-align frequency and unit at a fixed position so the right side stays free.
  // Move the baseline slightly down (about 1 mm on the 320x170 display).
  spr.setTextColor(YELLOW);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(freq, 8, 36, FONT_LARGE);

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

  // Station name (e.g. DLF) slightly higher, RadioText directly underneath.
  // Both rows are centered and kept clear of the frequency ruler.
  const char *rt = getRadioText();
  const char *station = getStationName();
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(CYAN);
  if(station && *station) spr.drawString(station, 160, 104, FONT_SMALL);
  spr.setTextColor(WHITE);
  if(rt && *rt) spr.drawString(rt, 160, 119, FONT_SMALL);

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
      if(getCurrentBand()->bandType==FM_BAND_TYPE)
      {
        // FM frequency is stored in 10-kHz units: 10790 means 107.90 MHz.
        snprintf(label,sizeof(label),"%ld.%02ld",(long)(tick/100),(long)(tick%100));
      }
      else if(tick>=1000)
      {
        // Shortwave scale values are in kHz; show MHz with one decimal.
        snprintf(label,sizeof(label),"%ld.%ldM",(long)(tick/1000),(long)((tick%1000)/100));
      }
      else
      {
        snprintf(label,sizeof(label),"%ldk",(long)tick);
      }
      spr.setTextColor(WHITE);
      spr.setTextDatum(TC_DATUM);
      spr.drawString(label, px, 155, FONT_DEFAULT);
    }
  }
  // Red tuning marker: a solid dot centered directly on the horizontal ruler.
  // Replaces the triangle and vertical tail to leave more room above for RadioText.
  spr.fillCircle(160, 142, 6, RED);

  // TinyRadio keeps its own full-width layout; do not overlay the Original sidebar.
}
