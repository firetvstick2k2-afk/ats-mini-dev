#include "FmDxUi.h"
#include "FmDxController.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include <stdio.h>

namespace {
  bool pageActive = false;
  uint32_t lastRefresh = 0;
}

bool fmDxUiOpen() {
  if(currentMode != FM) {
    statusShow("FM DX only", "Select the FM band first");
    return false;
  }
  if(!fmDxStart()) return false;
  pageActive = true;
  lastRefresh = 0;
  currentCmd = CMD_FM_DX;
  return true;
}

void fmDxUiExit() {
  if(!pageActive) return;
  pageActive = false;
  fmDxStop();
  currentCmd = CMD_NONE;
}

bool fmDxUiTick(uint32_t now) {
  if(!pageActive || static_cast<uint32_t>(now - lastRefresh) < 500) return false;
  lastRefresh = now;
  return true;
}

void fmDxUiRotate(int16_t delta) { (void)delta; }

void fmDxUiClick() {
  if(!pageActive) return;
  if(fmDxActive()) fmDxStop();
  else fmDxStart();
}

void drawFmDxPage() {
  constexpr uint16_t black = 0x0000;
  constexpr uint16_t navy = 0x0821;
  constexpr uint16_t blue = 0x001F;
  constexpr uint16_t cyan = 0x07FF;
  constexpr uint16_t green = 0x07E0;
  constexpr uint16_t white = 0xFFFF;
  constexpr uint16_t muted = 0xBDF7;

  spr.fillSprite(black);
  spr.fillRect(8, 7, 4, 22, cyan);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(white, black);
  spr.drawString("FM DX ASSISTANT", 18, 8, FONT_SMALL);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(fmDxActive() ? green : muted, black);
  spr.drawString(fmDxActive() ? "AUTO ACTIVE" : "PAUSED", 310, 8, FONT_SMALL);
  spr.drawFastHLine(8, 31, 304, blue);

  spr.fillRoundRect(12, 40, 296, 48, 6, navy);
  spr.drawRoundRect(12, 40, 296, 48, 6, cyan);
  char frequency[24];
  snprintf(frequency, sizeof(frequency), "%u.%02u MHz",
           currentFrequency / 100, currentFrequency % 100);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(green, navy);
  spr.drawString(frequency, 160, 53, FONT_LARGE);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(muted, black);
  spr.drawString("SIGNAL", 18, 99, FONT_SMALL);
  spr.drawString("SNR", 174, 99, FONT_SMALL);
  char value[20];
  snprintf(value, sizeof(value), "%u", rssi);
  spr.setTextColor(white, black);
  spr.drawString(value, 18, 117, FONT_LARGE);
  snprintf(value, sizeof(value), "%u dB", snr);
  spr.drawString(value, 174, 117, FONT_LARGE);

  spr.fillRoundRect(12, 145, 296, 17, 3, navy);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(cyan, navy);
  char width[32];
  snprintf(width, sizeof(width), "FM FILTER  %s", fmDxBandwidthLabel());
  spr.drawString(width, 160, 153, FONT_SMALL);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(muted, black);
  spr.drawString("CLICK: AUTO / PAUSE     HOLD: EXIT", 160, 168, FONT_DEFAULT);
}
