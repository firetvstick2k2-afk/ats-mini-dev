#include "Common.h"
#include "Themes.h"
#include "Storage.h"
#include "Utils.h"
#include "Menu.h"
#include "WeatherService.h"
#include "NewsService.h"
#include "MoonCalendar.h"
#include "BleMode.h"
#include "Draw.h"
#include "src/ftx/FtxUi.h"
#include "src/rtty/RttyUi.h"
#include "src/cw/CwUi.h"
#include "src/js8/Js8Ui.h"
#include "src/radio/FmDxUi.h"
#include "FreqKeypad.h" // <--- Eingebundenes Keypad-Modul
#include "SpectrumScanner.h"
#include "AllBandSpectrum.h"
#include <math.h>

// Layout-Prototypen
void drawLayoutSmeter();
void drawLayoutNewUI();
void drawLayoutDecoder();
void drawLayoutDefault();
void drawLayoutClean();
void drawLayoutTEF6686();
void drawLayoutTinyRadio();
void drawLayoutHJB();
void drawLayoutDABStyle();

uint32_t lastTuneTime = 0;

//
// Draw preferences write indicator
//
void drawSaveIndicator(int x, int y)
{
  if(prefsAreWritten() || switchThemeEditor())
  {
    spr.fillRect(x+3, y+2, 3, 5, TH.save_icon);
    spr.fillTriangle(x+1, y+7, x+7, y+7, x+4, y+10, TH.save_icon);
    spr.drawLine(x, y+12, x, y+13, TH.save_icon);
    spr.drawLine(x, y+13, x+8, y+13, TH.save_icon);
    spr.drawLine(x+8, y+13, x+8, y+12, TH.save_icon);
  }
}

//
// Draw Bluetooth indicator
//
void drawBleIndicator(int x, int y)
{
  int8_t status = getBleStatus();

  if(status || switchThemeEditor())
  {
    uint16_t color = (status>0) ? TH.rf_icon_conn : TH.rf_icon;

    if(switchThemeEditor())
      color = millis()&0x2000? TH.rf_icon_conn : TH.rf_icon;

    spr.drawLine(x+3, y+1, x+3, y+13, color);
    spr.drawLine(x+3, y+1, x+6, y+4, color);
    spr.drawLine(x+6, y+4, x, y+10, color);
    spr.drawLine(x, y+4, x+6, y+10, color);
    spr.drawLine(x+6, y+10, x+3, y+13, color);
  }
}

//
// Draw WiFi indicator
//
void drawWiFiIndicator(int x, int y)
{
  int8_t status = getWiFiStatus();

  if(status || switchThemeEditor())
  {
    uint16_t color = (status>0) ? TH.rf_icon_conn : TH.rf_icon;

    if(switchThemeEditor())
      color = millis()&0x2000? TH.rf_icon_conn : TH.rf_icon;

    spr.fillArc(x, 15+y, 14, 13, 240, 300, color);
    spr.fillArc(x, 15+y, 9, 8, 240, 300, color);
    spr.fillArc(x, 15+y, 4, 3, 240, 300, color);
  }
}

//
// Draw operation status
//
bool drawStatus(int x, int y)
{
  if(statusLines[0][0] || statusLines[1][0])
  {
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(0x07E0);
    spr.drawString(statusLines[0], x, y, FONT_SMALL);
    spr.drawString(statusLines[1], x, y+17, FONT_SMALL);
    return(true);
  }

  return(false);
}

//
// Draw zoomed menu item
//
void drawZoomedMenu(const char *text, bool force)
{
  if (!zoomMenu && !force) return;

  spr.fillSmoothRoundRect(RDS_OFFSET_X - 72 + 1, RDS_OFFSET_Y - 3 + 1, 152, 26, 4, TH.menu_bg);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.menu_item);
  spr.drawString(text, RDS_OFFSET_X + 5, RDS_OFFSET_Y, FONT_LARGE);
  spr.drawRoundRect(RDS_OFFSET_X - 72, RDS_OFFSET_Y - 3, 154, 28, 4, TH.menu_border);
}

//
// Show overlay message in large letters
//
void drawMessage(const char *msg)
{
  if(sleepOn()) return;

  drawZoomedMenu(msg, true);
  spr.pushSprite(0, 0);
}

//
// Draw band and mode indicators (Farbe auf Weiß 0xFFFF angepasst)
//
void drawBandAndMode(const char *band, const char *mode, int x, int y)
{
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(0xFFFF);
  uint16_t band_width = spr.drawString(band, x, y);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(0xFFFF);
  spr.drawString(mode, x + band_width / 2 + 10, y);
}

//
// Draw radio text
//
void drawRadioText(int y, int ymax)
{
  const char *rt = getRadioText();
  
  spr.setTextColor(0x07E0);
  spr.setTextSize(1.0);

  if (rt && *rt) 
  {
    spr.setTextDatum(TC_DATUM);
    spr.drawString(rt, 160, y, 4);
  } 
  else 
  {
    spr.setTextDatum(TL_DATUM);

    char sigBuf[32];
    snprintf(sigBuf, sizeof(sigBuf), "S:%d | SNR:%d dB", rx.getCurrentRSSI(), rx.getCurrentSNR());
    spr.drawString(sigBuf, 90, y - 6, 4);
  }
}

//
// Draw frequency
//
void drawFrequency(uint32_t freq, int x, int y, int ux, int uy, uint8_t hl)
{
  static uint32_t lastFreq = 0;

  if (freq != lastFreq) {
    lastFreq = freq;
    lastTuneTime = millis();
  }

  uint16_t unitColor = ((millis() - lastTuneTime) < 800) ? 0xFFE0 : 0x07E0;

  struct Line { int x, y, w; };

  const Line hlDigitsFM[] =
  {
    { x - 30 - 32 * 0 -  0, y + 28, 27 },
    { x - 30 - 32 * 0 - 16, y + 28, 27 + 16 },
    { x - 30 - 32 * 1 -  0, y + 28, 27 },
    { x - 30 - 32 * 1 - 22, y + 28, 27 + 22 },
    { x - 30 - 32 * 2 - 12, y + 28, 27 },
    { x - 30 - 32 * 2 - 28, y + 28, 27 + 16 },
    { x - 30 - 32 * 3 - 12, y + 28, 27 },
    { x - 30 - 32 * 3 - 28, y + 28, 27 + 16 },
    { x - 30 - 32 * 4 +  4, y + 28, 11 },
  };

  const Line hlDigitsAMSSB[] =
  {
    { x + 12 + 14 * 2 -  0, y + 28, 12 },
    { x + 12 + 14 * 2 -  7, y + 28, 12 + 7 },
    { x + 12 + 14 * 1 -  0, y + 28, 12 },
    { x + 12 + 14 * 1 -  7, y + 28, 12 + 7 },
    { x + 12 + 14 * 0 -  0, y + 28, 12 },
    { x + 12 + 14 * 0 - 11, y + 28, 12 + 11 },
    { x - 30 - 32 * 0 -  0, y + 28, 27 },
    { x - 30 - 32 * 0 - 16, y + 28, 27 + 16 },
    { x - 30 - 32 * 1 -  0, y + 28, 27 },
    { x - 30 - 32 * 1 - 16, y + 28, 27 + 16 },
    { x - 30 - 32 * 2 -  0, y + 28, 27 },
    { x - 30 - 32 * 2 - 16, y + 28, 27 + 16 },
    { x - 30 - 32 * 3 -  0, y + 28, 27 },
    { x - 30 - 32 * 3 - 16, y + 28, 27 + 16 },
    { x - 30 - 32 * 4 -  0, y + 28, 27 },
  };

  bool selectOn = hl & 0x80;
  const struct Line *li;
  hl &= 0x7F;

  spr.setTextDatum(MR_DATUM);
  spr.setTextColor(TH.freq_text);

  if(currentMode==FM)
  {
    li = hl<ITEM_COUNT(hlDigitsFM)? &hlDigitsFM[hl] : 0;
    spr.drawFloat(freq/100.00, 2, x, y, FONT_DIGITS);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(unitColor);
    spr.drawString("MHz", ux, uy + 10, &lgfx::fonts::FreeSansBold9pt7b);
  }
  else
  {
    li = hl<ITEM_COUNT(hlDigitsAMSSB)? &hlDigitsAMSSB[hl] : 0;

    if(isSSB())
    {
      char text[32];
      freq = freq * 1000 + currentBFO;
      sprintf(text, "%3.3lu", freq / 1000);
      spr.drawString(text, x, y, FONT_DIGITS);
      spr.setTextDatum(ML_DATUM);
      sprintf(text, ".%3.3lu", freq % 1000);
      spr.drawString(text, 4+x, 17+y, FONT_LARGE);
    }
    
else
{
  // AM: Ganzzahl ohne künstliche Nachkommastellen
  spr.drawNumber(freq, x, y, FONT_DIGITS);
}

    spr.setTextColor(unitColor);
    spr.setTextDatum(ML_DATUM);
    spr.drawString("kHz", ux, uy + 10, &lgfx::fonts::FreeSansBold9pt7b);
  }

  if(li)
  {
    if(selectOn)
    {
      spr.fillRoundRect(li->x + 1, li->y - 1, li->w - 2, 3, 1, TH.freq_hl_sel);
      spr.fillTriangle(li->x, li->y, li->x + 2, li->y - 2, li->x + 2, li->y - 2, TH.freq_hl_sel);
      spr.fillTriangle(li->x + li->w - 1, li->y, li->x + li->w - 3, li->y - 2, li->x + li->w - 3, li->y + 2, TH.freq_hl_sel);
    }
    else
    {
      spr.fillRoundRect(li->x, li->y - 1, li->w, 3, 1, TH.freq_hl);
    }
  }
}

//
// Draw tuner scale Roehrenradiostyle
//
void drawScale(uint32_t freq)
{
  spr.fillCircle(160, 140, 5, 0xF800);
  spr.drawFastVLine(160, 145, 24, 0xF800);
  spr.drawFastVLine(161, 145, 24, 0xF800);
  spr.drawFastHLine(0, 148, 320, TH.scale_line);
  spr.drawFastHLine(0, 169, 320, TH.scale_line);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TH.scale_text);

  int16_t slack = 3;
  int16_t offset = ((freq % 10) / 10.0 + slack) * 8;
  freq = freq / 10 - 20 - slack;

  const Band *band = getCurrentBand();
  uint32_t minFreq = band->minimumFreq / 10;
  uint32_t maxFreq = band->maximumFreq / 10;

  for(int i=0 ; i<(slack + 41 + slack) ; i++, freq++)
  {
    int16_t x = i * 8 - offset;
    if(freq >= minFreq && freq <= maxFreq)
    {
      uint16_t lineColor = (i==20) && (!offset || (!(freq%5) && offset==1))?
        0xF800 : TH.scale_line;

      if((freq % 10) == 0)
      {
        spr.drawFastVLine(x, 149, 20, lineColor);
        spr.drawFastVLine(x + 1, 149, 20, lineColor);

        if(currentMode == FM)
          spr.drawFloat(freq / 10.0, 1, x, 140, FONT_SMALL);
        else if(freq >= 100)
          spr.drawFloat(freq / 100.0, 3, x, 140, FONT_SMALL);
        else
          spr.drawNumber(freq * 10, x, 140, FONT_SMALL);
      }
      else if((freq % 5) == 0 && (freq % 10) != 0)
      {
        spr.drawFastVLine(x, 157, 12, lineColor);
      }
      else
      {
        spr.drawFastVLine(x, 162, 7, lineColor);
      }
    }
  }
}

//
// Standard S-Meter
//
void drawSMeter(int strength, int x, int y)
{
  static int peakBar = 0;
  static uint32_t lastPeakTime = 0;
  static uint32_t lastDecayTime = 0;

  uint32_t now = millis();

  if (strength >= peakBar) {
    peakBar = strength;
    lastPeakTime = now;
    lastDecayTime = now;
  } 
  else if (now - lastPeakTime > 1200) {
    if (now - lastDecayTime > 150) {
      if (peakBar > 0) peakBar--;
      lastDecayTime = now;
    }
  }

  spr.drawTriangle(x + 1, y + 1, x + 11, y + 1, x + 6, y + 6, 0xFCA0);
  spr.drawLine(x + 6, y + 1, x + 6, y + 14, 0xFCA0);

  for(int i = 0; i < 17; i++)
  {
    int barX = 15 + x + (i * 4);

    if (i == peakBar && peakBar > 0)
    {
      spr.fillRect(barX, 2 + y, 2, 12, 0xF800);
    }
    else if (i < 10 && i < strength)
    {
      spr.fillRect(barX, 2 + y, 2, 12, 0xFCA0);
    }
    else if (i < strength)
    {
      spr.fillRect(barX, 2 + y, 2, 12, TH.smeter_bar_plus);
    }
    else
    {
      spr.fillRect(barX, 2 + y, 2, 12, TH.smeter_bar_empty);
    }
  }
}

//
// Kompaktes S-Meter im H.J. Berndt Design
//
void drawCompactSMeter(int strength, int x, int y)
{
  int smeterX = x;
  int smeterY = y;

  spr.drawRoundRect(smeterX, smeterY, 96, 22, 2, 0xFFFF);
  
  spr.fillTriangle(smeterX + 6, smeterY + 11, smeterX + 12, smeterY + 6, smeterX + 12, smeterY + 16, 0xFFFF);
  spr.fillTriangle(smeterX + 90, smeterY + 11, smeterX + 84, smeterY + 6, smeterX + 84, smeterY + 16, 0xFFFF);

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(0xFFFF);
  char sigText[16];
  sprintf(sigText, "SIG %02d/%02d", rx.getCurrentRSSI(), rx.getCurrentSNR());
  spr.drawString(sigText, smeterX + 48, smeterY + 3, FONT_SMALL);

  int bars = map(strength, 0, 60, 0, 10);
  bars = constrain(bars, 0, 10);
  for (int i = 0; i < 10; i++) {
    uint16_t color = (i < bars) ? 0x07E0 : TH.smeter_bar_empty;
    spr.fillRect(smeterX + 16 + (i * 6), smeterY + 14, 4, 6, color);
  }
}

//
// Draw stereo indicator
//
void drawStereoIndicator(int x, int y, bool stereo)
{
  if(stereo)
  {
    spr.fillRect(15 + x, 7 + y, 4 * 17 - 2, 2, TH.bg);
  }
}

//
// Draw RDS station name
//
void drawStationName(const char *name, int x, int y)
{
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(TH.rds_text);
  spr.drawString(name, x, y, FONT_LARGE);
}

//
// Draw long (EIBI) station name
//
void drawLongStationName(const char *name, int x, int y)
{
  int width = spr.textWidth(name, FONT_SMALL);
  spr.setTextColor(TH.rds_text);

  if((x + width) >= 320)
  {
    spr.setTextDatum(TL_DATUM);
    spr.drawString(name, x, y, FONT_SMALL);
  }
  else if(width <= 60)
  {
    spr.setTextDatum(TC_DATUM);
    spr.drawString(name, x + (320 - x) / 3, y, FONT_SMALL);
  }
  else
  {
    spr.setTextDatum(TC_DATUM);
    spr.drawString(name, x + (320 - x + width) / 4, y, FONT_SMALL);
  }
}

//
// Draw scan graphs
//
void drawScanGraphs(uint32_t freq)
{
  int16_t offset = (freq % 10) / 10.0 * 8;
  freq = freq / 10 - 20;

  const Band *band = getCurrentBand();
  uint32_t minFreq = band->minimumFreq / 10;
  uint32_t maxFreq = band->maximumFreq / 10;

  for(int i=0 ; i<41 ; i++, freq++)
  {
    int16_t x = i * 8 - offset;

    if(freq >= minFreq && freq <= maxFreq)
    {
      if((freq % 5) == 0) {
        for(int y=0; y<42; y+=2) {
          spr.drawPixel(x, 169-y, TH.scan_grid);
        }
      }

      if((freq+1) <= maxFreq) {
        for(int xd=x; xd<(x+8); xd+=2) {
          spr.drawPixel(xd, 169-40, TH.scan_grid);
          spr.drawPixel(xd, 169-30, TH.scan_grid);
          spr.drawPixel(xd, 169-20, TH.scan_grid);
          spr.drawPixel(xd, 169-10, TH.scan_grid);
          spr.drawPixel(xd, 169-0, TH.scan_grid);
        }
        int snr1 = 40 * scanGetSNR(freq * 10);
        int snr2 = 40 * scanGetSNR((freq+1) * 10);
        spr.drawLine(x, 169-snr1, x+8, 169-snr2, TH.scan_snr);
        int rssi1 = 40 * scanGetRSSI(freq * 10);
        int rssi2 = 40 * scanGetRSSI((freq+1) * 10);
        spr.drawLine(x, 169-rssi1, x+8, 169-rssi2, TH.scan_rssi);
      }
    }
  }
  spr.fillTriangle(156, 125, 160, 130, 164, 125, TH.scale_pointer);
  spr.drawLine(160, 130, 160, 169, TH.scale_pointer);
}

static void drawDualRxScreen()
{
  const uint16_t panelBg = 0x0861, frameBlue = 0x001F, brightBlue = 0x07FF;
  // Dual RX alternates between A and B, so retain each slot's last measured RSSI.
  static int slotStrength[2] = {0, 0};
  if (dualRxSetupStage == 4 && dualRxDisplaySlot < 2)
    slotStrength[dualRxDisplaySlot] = rx.getCurrentRSSI();

  spr.fillSprite(0x0000);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(brightBlue);
  // TEF6686 layout uses FreeSans Bold for status text and the MHz/kHz label.
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawString("DUAL RX", 160, 12);
  spr.drawFastHLine(12, 27, 296, frameBlue);
  if(dualRxSetupStage < 3)
  {
    spr.setFont(FONT_SMALL);
    spr.setTextColor(0xFFFF);
    spr.drawString(dualRxSetupStage == 1 ? "SET FREQUENCY AND MODE A" : "SET FREQUENCY AND MODE B", 160, 58);
    spr.setTextColor(0xBDF7);
    if(dualRxSetupStage == 1)
    {
      spr.drawString("Press encoder to open frequency keypad", 160, 86);
      spr.drawString("Choose frequency, mode and press OK", 160, 106);
    }
    else
    {
      char savedA[40];
      const VfoState &a = dualRxVfos[0];
      const bool aIsFm = a.band < getTotalBands() && bands[a.band].bandType == FM_BAND_TYPE;
      if(aIsFm) snprintf(savedA, sizeof(savedA), "A: %.2f MHz  %s", a.freq / 100.0f, bandModeDesc[a.mode]);
      else snprintf(savedA, sizeof(savedA), "A: %u kHz  %s", a.freq, bandModeDesc[a.mode]);
      spr.setTextColor(0x07E0);
      spr.drawString(savedA, 160, 84);
      spr.setTextColor(0xBDF7);
      spr.drawString("Press encoder to enter frequency B", 160, 108);
    }
    spr.setTextColor(brightBlue);
    spr.drawString(dualRxSetupStage == 1 ? "STEP 1 OF 2" : "STEP 2 OF 2", 160, 139);
    return;
  }
  for(uint8_t i=0; i<2; ++i)
  {
    int y = i == 0 ? 32 : 99;
    const VfoState &v = dualRxVfos[i];
    spr.fillRect(10, y, 300, 56, panelBg);
    spr.drawRect(10, y, 300, 56, frameBlue);
    spr.drawRect(11, y+1, 298, 54, brightBlue);
    spr.setFont(FONT_SMALL);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(0xBDF7);
    // Move the complete A/B + RX/WAIT labels slightly down so they clear the top frame.
    spr.drawString(i == 0 ? "A" : "B", 19, y+12);
    spr.setTextColor((dualRxSetupStage == 4 && i == dualRxDisplaySlot) ? 0x07E0 : 0x7BEF);
    spr.drawString((dualRxSetupStage == 4 && i == dualRxDisplaySlot) ? "RX" : "WAIT", 36, y+12);
    char freqText[24];
    bool isFm = v.band < getTotalBands() && bands[v.band].bandType == FM_BAND_TYPE;
    if(isFm) snprintf(freqText, sizeof(freqText), "%.2f", v.freq / 100.0f);
    else snprintf(freqText, sizeof(freqText), "%u", v.freq);
    // Keep the exact 7-segment FONT_DIGITS used by Layout-Default.
    // MR_DATUM is vertically centered, so y+28 centers the 48px digits in the 56px frame.
    spr.setFont(FONT_DIGITS);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(0xFFFF);
    spr.drawString(freqText, 258, y+28);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(0x07E0);
    spr.drawString(isFm ? "MHz" : "kHz", 265, y+27);
    spr.setFont(FONT_SMALL);
    spr.setTextColor(0xBDF7);
    spr.drawString(v.mode < getTotalModes() ? bandModeDesc[v.mode] : "?", 265, y+41);

    // Compact per-slot S-meter at lower left. Values are retained between A/B scans.
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(0xBDF7);
    spr.drawString("S", 19, y+25);
    int bars = constrain(map(slotStrength[i], 0, 60, 0, 10), 0, 10);
    for (int bar = 0; bar < 10; ++bar) {
      uint16_t barColor = bar < bars ? (bar < 7 ? 0x07E0 : 0xFFE0) : 0x4208;
      spr.fillRect(31 + bar * 5, y+40, 3, 9, barColor);
    }
  }
  // Keep the encoder instruction visible within the 170px display height.
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(0x07E0);
  spr.drawString(dualRxSetupStage == 4 ? "Click Encoder to Stop Dual RX" : "Click Encoder to Start", 160, 165);
}

// Weather report rendering is kept separate from the radio layouts.
static void weatherAscii(const char* input, char* output, size_t capacity)
{
  if(!capacity) return;
  size_t out = 0;
  for(size_t i = 0; input && input[i] && out + 1 < capacity; ++i)
  {
    const uint8_t c = static_cast<uint8_t>(input[i]);
    if(c == 0xC3 && input[i+1])
    {
      const uint8_t next = static_cast<uint8_t>(input[i+1]);
      const char* replacement = nullptr;
      if(next == 0xA4) replacement = "ae";
      else if(next == 0x84) replacement = "Ae";
      else if(next == 0xB6) replacement = "oe";
      else if(next == 0x96) replacement = "Oe";
      else if(next == 0xBC) replacement = "ue";
      else if(next == 0x9C) replacement = "Ue";
      else if(next == 0x9F) replacement = "ss";
      if(replacement)
      {
        for(size_t j=0; replacement[j] && out + 1 < capacity; ++j) output[out++] = replacement[j];
        ++i;
        continue;
      }
    }
    output[out++] = static_cast<char>(c);
  }
  output[out] = '\0';
}

static void drawWeatherIcon(int cx, int cy, uint8_t code, int scale)
{
  const uint16_t cloud = 0xBDF7;
  const bool storm = code >= 95;
  const bool rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82) || (code >= 85 && code <= 86);
  const bool snow = code >= 71 && code <= 77;
  const bool fog = code == 45 || code == 48;
  if(code == 0 || code == 1 || code == 2)
  {
    spr.fillCircle(cx, cy-8*scale, 9*scale, 0xFFE0);
    for(int a=0; a<8; ++a)
    {
      const float angle=a*0.785398f;
      spr.drawLine(cx+static_cast<int>(cosf(angle)*13*scale),cy-8*scale+static_cast<int>(sinf(angle)*13*scale),
                   cx+static_cast<int>(cosf(angle)*18*scale),cy-8*scale+static_cast<int>(sinf(angle)*18*scale),0xFFE0);
    }
  }
  if(code != 0)
  {
    spr.fillCircle(cx-9*scale,cy+2*scale,9*scale,cloud);
    spr.fillCircle(cx+2*scale,cy-4*scale,12*scale,cloud);
    spr.fillCircle(cx+13*scale,cy+3*scale,8*scale,cloud);
    spr.fillRoundRect(cx-16*scale,cy,34*scale,11*scale,5*scale,cloud);
  }
  if(rain)
  {
    for(int i=0;i<3;++i) spr.drawLine(cx-10*scale+i*10*scale,cy+15*scale,cx-14*scale+i*10*scale,cy+23*scale,0x5DDF);
  }
  else if(snow)
  {
    for(int i=0;i<3;++i) spr.drawCircle(cx-10*scale+i*10*scale,cy+19*scale,2*scale,0xFFFF);
  }
  else if(storm)
  {
    spr.fillTriangle(cx+1*scale,cy+10*scale,cx-7*scale,cy+25*scale,cx+1*scale,cy+25*scale,0xFFE0);
    spr.fillTriangle(cx+1*scale,cy+20*scale,cx+10*scale,cy+20*scale,cx-1*scale,cy+34*scale,0xFFE0);
  }
  else if(fog)
  {
    for(int i=0;i<3;++i) spr.drawFastHLine(cx-18*scale,cy+14*scale+i*5*scale,36*scale,0x7BEF);
  }
}

static void drawMoonCalendarScreen()
{
  const MoonCalendar::Snapshot moon = MoonCalendar::calculate(time(nullptr));
  const int w = spr.width();
  constexpr uint16_t BLUE=0x001F, CYAN=0x07FF, GREEN=0x07E0;
  constexpr uint16_t WHITE_TEXT=0xFFFF, MUTED_TEXT=0x8410, MOON_GOLD=0xFFE0;
  spr.fillScreen(TH.bg);
  spr.drawRoundRect(3,3,w-6,164,6,BLUE);
  spr.setTextDatum(TL_DATUM);
  spr.setFont(FONT_LARGE);
  spr.setTextColor(CYAN,TH.bg);
  spr.drawString("MONDKALENDER",12,8);
  spr.drawFastHLine(10,28,w-20,BLUE);

  if(!moon.valid)
  {
    spr.setTextDatum(MC_DATUM);
    spr.setFont(FONT_SMALL);
    spr.setTextColor(WHITE_TEXT,TH.bg);
    const char* message = moonUiState == MOON_UI_WAITING_WIFI
      ? "Waiting for WiFi..." : "Synchronizing clock...";
    const char* detail = moonUiState == MOON_UI_WAITING_WIFI
      ? "Verbinde mit gespeichertem Netzwerk"
      : "NTP-Zeitabgleich wird durchgefuehrt";
    spr.drawCenterString(message,w/2,70);
    spr.setFont(FONT_DEFAULT);
    spr.setTextColor(MUTED_TEXT,TH.bg);
    spr.drawCenterString(detail,w/2,96);
    spr.drawString("Klick: Zurueck",12,145);
    return;
  }

  // Kompakter Mond: mehr Platz fuer die Phasenbezeichnung rechts.
  const int cx=58, cy=84, radius=24;
  spr.fillCircle(cx,cy,radius,MOON_GOLD);
  const float phase=moon.phase;
  if(phase < 0.03f || phase > 0.97f)
    spr.fillCircle(cx,cy,radius,0x2104);
  else if(!(phase > 0.47f && phase < 0.53f))
  {
    const int offset=static_cast<int>(4.0f*radius*
      (phase < 0.5f ? -phase : (1.0f-phase)));
    spr.fillCircle(cx+offset,cy,radius,0x2104);
  }
  spr.drawCircle(cx,cy,radius,MOON_GOLD);

  spr.setTextColor(GREEN,TH.bg);
  spr.setFont(FONT_SMALL);
  spr.drawString(moon.phaseName,101,42);
  char value[40];
  snprintf(value,sizeof(value),"%u%% beleuchtet",moon.illuminationPercent);
  spr.setTextColor(WHITE_TEXT,TH.bg);
  spr.setFont(FONT_SMALL);
  spr.drawString(value,101,65);
  snprintf(value,sizeof(value),"Mondalter: %.1f Tage",moon.ageDays);
  spr.setFont(FONT_DEFAULT);
  spr.setTextColor(MUTED_TEXT,TH.bg);
  spr.drawString(value,101,87);

  auto drawEvent = [&](const char* label, time_t epoch, int y) {
    struct tm local{};
    char dateText[28]="--";
    if(epoch > 0 && localtime_r(&epoch,&local))
      strftime(dateText,sizeof(dateText),"%d.%m. %H:%M",&local);
    spr.setFont(FONT_SMALL);
    spr.setTextColor(CYAN,TH.bg);
    spr.drawString(label,12,y);
    spr.setTextColor(WHITE_TEXT,TH.bg);
    spr.drawRightString(dateText,w-12,y);
  };
  drawEvent("NAECHSTER VOLLMOND",moon.nextFullMoon,116);
  drawEvent("NAECHSTER NEUMOND",moon.nextNewMoon,137);
  spr.setFont(FONT_DEFAULT);
  spr.setTextColor(MUTED_TEXT,TH.bg);
  spr.drawString("Zeiten: Lokalzeit  |  Klick: Zurueck",12,155);
}


void drawNewsScreen()
{
  const uint8_t newsCount = newsService.itemCount();
  const bool fetching = newsService.isFetching();
  const bool failed = newsService.hasFailed();
  const int w = spr.width();
  constexpr uint16_t BLUE=0x001F, CYAN=0x07FF, GREEN=0x07E0, WHITE=0xFFFF, MUTED=0x8410, PANEL=0x0000;
  spr.fillScreen(PANEL); spr.drawRoundRect(3,3,w-6,164,6,BLUE);
  spr.setTextDatum(TL_DATUM); spr.setFont(FONT_SMALL); spr.setTextColor(CYAN,PANEL);
  spr.drawString("NACHRICHTEN",10,6); spr.setFont(FONT_DEFAULT); spr.setTextColor(MUTED,PANEL);
  if(!newsCount)
  {
    spr.drawFastHLine(9,23,w-18,BLUE); spr.setTextDatum(MC_DATUM); spr.setTextColor(WHITE,PANEL);
    spr.drawCenterString(fetching ? "RSS-Feeds werden geladen..." : "Keine Nachrichten vorhanden",w/2,72);
    spr.setTextColor(MUTED,PANEL);
    spr.drawCenterString(failed ? "Abruf fehlgeschlagen / Feed pruefen" : "RSS-Adressen in der Weboberflaeche eintragen",w/2,94);
    spr.drawString("Lang druecken: Zurueck",10,148); return;
  }
  const uint8_t index=newsCurrentIndex%newsCount;
  NewsService::Item item;
  if(!newsService.getItem(index,item)) return;
  spr.setTextDatum(TR_DATUM); char meta[20]; snprintf(meta,sizeof(meta),"%u/%u",index+1,newsCount); spr.drawString(meta,w-10,8);
  spr.setTextDatum(TL_DATUM); spr.setTextColor(WHITE,PANEL); spr.setFont(FONT_DEFAULT);
  const char* title=item.title; char titleLine[44];
  for(uint8_t row=0;row<2 && *title;row++)
  {
    size_t n=0,lastSpace=0;
    while(title[n] && n<40){titleLine[n]=title[n];if(title[n]==' ')lastSpace=n;++n;}
    if(title[n] && lastSpace>8)n=lastSpace;
    titleLine[n]='\0'; spr.drawString(titleLine,10,21+row*11); title+=n; while(*title==' ')++title;
  }
  spr.setTextColor(GREEN,PANEL); spr.drawString(item.source,10,44);
  spr.setTextColor(MUTED,PANEL); spr.setTextDatum(TR_DATUM); spr.drawString(item.date,w-10,44);
  spr.drawFastHLine(9,56,w-18,BLUE); spr.setTextDatum(TL_DATUM); spr.setTextColor(WHITE,PANEL);
  const char* body=item.text; size_t offset=(size_t)newsCurrentPage*240; size_t length=strlen(body); if(offset>=length)offset=0; body+=offset;
  for(uint8_t row=0;row<6 && *body;row++)
  {
    char line[42]; size_t n=0,lastSpace=0;
    while(body[n] && n<40){line[n]=body[n];if(body[n]==' ')lastSpace=n;++n;}
    if(body[n] && lastSpace>8)n=lastSpace;
    line[n]='\0'; spr.drawString(line,10,61+row*13); body+=n; while(*body==' ')++body;
  }
  spr.setTextColor(MUTED,PANEL); spr.setTextDatum(TL_DATUM);
  spr.drawString("Dreh: Textseiten  Klick: Naechste Meldung",10,145);
  spr.setTextDatum(TR_DATUM); char pageText[20]; snprintf(pageText,sizeof(pageText),"SEITE %u",newsCurrentPage+1); spr.drawString(pageText,w-10,155);
}

static void drawWeatherScreen()
{
  const WeatherService::Snapshot weather = weatherService.snapshot();
  const int w = spr.width();
  constexpr uint16_t FRAME=0x001F, WHITE_TEXT=0xFFFF, MUTED_TEXT=0xA514, PANEL=0x0000;
  spr.fillScreen(PANEL);
  spr.fillRoundRect(3,3,w-6,164,7,PANEL);
  spr.drawRoundRect(3,3,w-6,164,7,FRAME);
  spr.setTextDatum(TL_DATUM);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.setTextColor(WHITE_TEXT,PANEL);
  if(weatherUiState != WEATHER_UI_READY || !weather.valid)
  {
    spr.drawString("WETTERBERICHT",12,12);
    spr.drawFastHLine(10,29,w-20,FRAME);
    spr.fillRoundRect(25,48,w-50,72,6,0x0841);
    spr.drawRoundRect(25,48,w-50,72,6,FRAME);
    spr.setTextDatum(MC_DATUM);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.setTextColor(WHITE_TEXT,0x0841);
    const char* message = "Waiting for WiFi...";
    const char* detail = "Verbinde mit gespeichertem Netzwerk";
    if(weatherUiState == WEATHER_UI_LOADING)
    {
      message = "Loading weather data...";
      detail = "Bitte einen Moment warten";
    }
    else if(weatherUiState == WEATHER_UI_NO_LOCATION)
    {
      message = "Postleitzahl fehlt";
      detail = "PLZ auf der WLAN-Webseite einstellen";
    }
    spr.drawCenterString(message,w/2,70);
    spr.setFont(FONT_DEFAULT);
    spr.setTextColor(MUTED_TEXT,0x0841);
    spr.drawCenterString(detail,w/2,96);
    spr.setTextDatum(TL_DATUM);
    spr.setFont(FONT_DEFAULT);
    spr.setTextColor(MUTED_TEXT,PANEL);
    spr.drawString("Lang druecken: Zurueck",12,145);
    return;
  }

  char displayLocation[sizeof(weather.location)*2+1];
  weatherAscii(weather.location,displayLocation,sizeof(displayLocation));
  spr.drawString(displayLocation,12,8);
  spr.setFont(FONT_DEFAULT);
  spr.setTextColor(MUTED_TEXT,PANEL);
  spr.setTextDatum(TR_DATUM);
  spr.drawString(weather.stale ? "LETZTE DATEN" : (weatherPage == 0 ? "AKTUELL 1/2" : "VORSCHAU 2/2"),w-12,12);
  spr.drawFastHLine(10,27,w-20,FRAME);

  if(weatherPage == 0)
  {
    drawWeatherIcon(58,78,weather.weatherCode,2);
    char temperature[16];
    snprintf(temperature,sizeof(temperature),"%d",weather.temperatureC);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(WHITE_TEXT,PANEL);
    spr.setFont(FONT_DIGITS);
    spr.drawString(temperature,126,34);
    spr.setFont(&lgfx::fonts::FreeSansBold18pt7b);
    spr.drawString("C",235,48);
    spr.drawCircle(229,48,3,WHITE_TEXT);
    spr.setFont(&lgfx::fonts::FreeSans9pt7b);
    spr.drawString(weather.description,126,83);
    char feels[32];
    snprintf(feels,sizeof(feels),"Gefuehlt %d C",weather.feelsLikeC);
    spr.setFont(FONT_SMALL);
    spr.setTextColor(MUTED_TEXT,PANEL);
    spr.drawString(feels,126,105);

    const int x0=9, y=127, gap=4, boxW=(w-18-3*gap)/4, boxH=30;
    const char* labels[]={"FEUCHTE","WIND","REGEN","DRUCK"};
    char values[4][20];
    snprintf(values[0],sizeof(values[0]),"%u%%",weather.humidity);
    snprintf(values[1],sizeof(values[1]),"%u km/h",weather.windKmh);
    snprintf(values[2],sizeof(values[2]),"%u%%",weather.precipitation);
    snprintf(values[3],sizeof(values[3]),"%u hPa",weather.pressureHpa);
    for(int i=0;i<4;++i)
    {
      const int x=x0+i*(boxW+gap);
      spr.drawRoundRect(x,y,boxW,boxH,2,FRAME);
      spr.setFont(FONT_DEFAULT); spr.setTextColor(MUTED_TEXT,PANEL); spr.setTextDatum(TL_DATUM);
      spr.drawString(labels[i],x+4,y+3);
      spr.setFont(FONT_SMALL); spr.setTextColor(WHITE_TEXT,PANEL);
      spr.drawString(values[i],x+4,y+15);
    }
  }
  else
  {
    const int left=7, gap=3, cardW=(w-14-4*gap)/5, top=34;
    for(uint8_t i=0;i<WeatherService::FORECAST_DAYS;++i)
    {
      const int x=left+i*(cardW+gap);
      spr.drawRoundRect(x,top,cardW,124,3,FRAME);
      spr.setFont(FONT_SMALL); spr.setTextDatum(MC_DATUM); spr.setTextColor(WHITE_TEXT,PANEL);
      spr.drawCenterString(weather.weekdays[i],x+cardW/2,top+7);
      drawWeatherIcon(x+cardW/2,top+43,weather.days[i].weatherCode,1);
      char value[12];
      snprintf(value,sizeof(value),"%d",weather.days[i].maximumC);
      spr.setFont(FONT_LARGE); spr.drawCenterString(value,x+cardW/2,top+65);
      snprintf(value,sizeof(value),"%d",weather.days[i].minimumC);
      spr.setFont(FONT_SMALL); spr.setTextColor(MUTED_TEXT,PANEL);
      spr.drawCenterString(value,x+cardW/2,top+94);
      snprintf(value,sizeof(value),"%u%%",weather.days[i].precipitationChance);
      spr.setFont(FONT_DEFAULT); spr.drawCenterString(value,x+cardW/2,top+110);
      spr.drawFastHLine(x+5,top+122,cardW-10,0x5DDF);
    }
  }
  spr.setTextDatum(TL_DATUM);
  spr.setFont(FONT_DEFAULT);
  spr.setTextColor(MUTED_TEXT,PANEL);
  spr.drawString("Drehen/Klick: Seite  |  Lang: Zurueck",12,155);
}

//
 // Draw screen according to given command
//
void drawScreen()
{
  if(sleepOn()) return;

  // Full-screen TEF6686-style menu replaces the compact sidebar menu.
  if(currentCmd == CMD_MENU || currentCmd == CMD_SETTINGS || currentCmd == CMD_SPECTRUM_MENU || currentCmd == CMD_DECODER_MENU)
  {
    drawFullScreenMenu(currentCmd);
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_NEWS)
  {
    drawNewsScreen(); spr.pushSprite(0,0); return;
  }

  if(currentCmd == CMD_WEATHER)
  {
    drawWeatherScreen();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_MOON_CALENDAR)
  {
    drawMoonCalendarScreen();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_VOLUME)
  {
    drawVolumePopup();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_BRT)
  {
    drawBrightnessPopup();
    spr.pushSprite(0, 0);
    return;
  }

  // All settings submenus use the same full-screen popup renderer instead of
  // falling back to the legacy compact sidebar widgets.
  if(currentCmd > CMD_SETTINGS && currentCmd <= CMD_ABOUT)
  {
    drawSettingsPopup(currentCmd);
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_FTX_DECODER)
  {
    drawFtxDecoderPage();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_RTTY_DECODER)
  {
    drawRttyDecoderPage();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_FM_DX)
  {
    drawFmDxPage();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_JS8_MONITOR)
  {
    drawJs8MonitorPage();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_CW_DECODER)
  {
    drawCwDecoderPage();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_DECODER_FREQUENCIES)
  {
    drawDecoderFrequencyMenu();
    spr.pushSprite(0, 0);
    return;
  }

  // Dedicated scanner keypad; independent from radio/Dual RX keypad.
  if(currentCmd == CMD_ALL_SPECTRUM_INPUT)
  {
    drawAllBandSpectrumInput();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_SPECTRUM_INPUT)
  {
    drawSpectrumScannerInput();
    spr.pushSprite(0, 0);
    return;
  }

  // Keypad-Overlay sofort zeichnen und anzeigen, wenn aktiv
  if(currentCmd == CMD_FREQ_KEYPAD)
  {
    drawFreqKeypad();
    spr.pushSprite(0, 0);
    return;
  }

  spr.fillSprite(TH.bg);

  if(currentCmd == CMD_ALL_SPECTRUM)
  {
    drawAllBandSpectrum();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_SPECTRUM)
  {
    drawSpectrumScanner();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd == CMD_DUAL_RX)
  {
    drawDualRxScreen();
    spr.pushSprite(0, 0);
    return;
  }

  if(currentCmd==CMD_ABOUT)
  {
    drawAbout();
    return;
  }

    
  if(uiLayoutIdx == 8)
  {
    if(currentMode == FM) drawLayoutDABStyle();
    else drawLayoutTinyRadio(); // DAB Style is FM-only; TinyRadio is the selected alternative for non-FM bands.
  }
  else switch(uiLayoutIdx)
  {
    case 1:
      drawLayoutSmeter();
      break;
    case 2:
      drawLayoutNewUI();
      break;
    case 3:
      drawLayoutDecoder();
      break;
    case 4:
      drawLayoutClean();
      break;
    case 5:
      drawLayoutTEF6686();
      break;
    case 6:
      drawLayoutTinyRadio();
      break;
    case 7:
      drawLayoutHJB();
      break;
    default:
      // UI_DEFAULT (index 0) is the original Layout-Default.
      // TEF6686 is selected explicitly at index 5 and remains the fresh-install default.
      drawLayoutDefault();
      break;
  }


  spr.pushSprite(0, 0);
}
