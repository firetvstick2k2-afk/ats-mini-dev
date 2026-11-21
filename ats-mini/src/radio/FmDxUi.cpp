#include "FmDxUi.h"
#include "FmDxController.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include <stdio.h>
#include <string.h>

extern bool updateFrequency(int newFreq, bool wrap);

namespace {
  bool pageActive = false;
  uint32_t lastRefresh = 0;
  bool stereoPilot = false;
  uint8_t stereoBlend = 0;
  bool actionsOpen = false;
  uint8_t actionSelection = 0;
  uint8_t manualFilter = 3;

  constexpr uint16_t BLACK  = 0x0000;
  constexpr uint16_t WHITE  = 0xFFFF;
  constexpr uint16_t GREY   = 0x8410;
  constexpr uint16_t RED    = 0xF800;
  constexpr uint16_t YELLOW = 0xFFE0;
  constexpr uint16_t GREEN  = 0x07E0;
  constexpr uint16_t CYAN   = 0x07FF;
  constexpr uint16_t LINE   = 0x4208;

  void drawLabel(const char *label, int x, int y) {
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(GREY, BLACK);
    spr.drawString(label, x, y, FONT_DEFAULT);
  }

  void drawRssiMeter(int x, int y, uint8_t value) {
    // Convert the receiver's existing FM S-level (1..17) into 20 FT8-style blocks.
    int strength = getStrength(value);
    int filled = (strength * 20 + 16) / 17;
    if(filled < 0) filled = 0;
    if(filled > 20) filled = 20;

    constexpr int blockW = 7;
    constexpr int blockH = 10;
    constexpr int gap = 2;
    for(int i = 0; i < 20; ++i) {
      uint16_t color = i < 7 ? RED : (i < 14 ? YELLOW : GREEN);
      int bx = x + i * (blockW + gap);
      if(i < filled) spr.fillRect(bx, y, blockW, blockH, color);
      else spr.drawRect(bx, y, blockW, blockH, 0x39E7);
    }
  }

  void drawValueOrDash(const char *value, int x, int y, const lgfx::IFont *font = FONT_DEFAULT) {
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(value && value[0] ? WHITE : GREY, BLACK);
    spr.drawString(value && value[0] ? value : "--", x, y, font);
  }

  void updateStereoStatus() {
    // FM_RSQ_STATUS supplies both the live 19 kHz pilot flag and stereo blend.
    // Read it once per UI refresh; the getters below only inspect the cached response.
    rx.getCurrentReceivedSignalQuality();
    stereoPilot = rx.getCurrentPilot();
    stereoBlend = rx.getCurrentStereoBlend();
  }
}

bool fmDxUiOpen() {
  if(currentMode != FM) {
    statusShow("FM DX only", "Select the FM band first");
    return false;
  }
  if(!fmDxStart()) return false;
  pageActive = true;
  lastRefresh = 0;
  stereoPilot = false;
  stereoBlend = 0;
  currentCmd = CMD_FM_DX;
  return true;
}

void fmDxUiExit() {
  if(!pageActive) return;
  actionsOpen = false;
  pageActive = false;
  fmDxEndSession();
  currentCmd = CMD_NONE;
}

bool fmDxUiTick(uint32_t now) {
  if(!pageActive) return false;
  // A remote band change can bypass the local encoder while this page is open.
  // Close cleanly instead of leaving an FM-only page active on another mode.
  if(currentMode != FM) {
    fmDxUiExit();
    return true;
  }
  if(static_cast<uint32_t>(now - lastRefresh) < 500) return false;
  lastRefresh = now;
  updateStereoStatus();
  return true;
}

void fmDxUiRotate(int16_t delta) {
  if(!pageActive || !delta) return;
  if(actionsOpen) {
    int next = static_cast<int>(actionSelection) + (delta > 0 ? 1 : -1);
    if(next < 0) next = 2;
    if(next > 2) next = 0;
    actionSelection = static_cast<uint8_t>(next);
    return;
  }
  const Step *step = getCurrentStep();
  int stepKHz = step && step->step > 0 ? step->step : 10;
  updateFrequency(static_cast<int>(currentFrequency) + delta * stepKHz, true);
}

void fmDxUiClick() {
  if(!pageActive) return;
  if(!actionsOpen) {
    actionsOpen = true;
    actionSelection = 0;
    return;
  }
  switch(actionSelection) {
    case 0:
      if(fmDxActive()) fmDxStop();
      else fmDxStart();
      break;
    case 1:
      if(fmDxActive()) fmDxStop();
      manualFilter = manualFilter == 4 ? 2 : manualFilter + 1;
      fmDxSetManualBandwidth(manualFilter);
      break;
    case 2:
      actionsOpen = false;
      break;
  }
}

void drawFmDxPage() {
  spr.fillSprite(BLACK);

  // Header: compact instrument-style status line.
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(WHITE, BLACK);
  spr.drawString("FM DX MONITOR", 7, 3, FONT_DEFAULT);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(fmDxActive() ? GREEN : YELLOW, BLACK);
  spr.drawString(fmDxActive() ? "DX AUTO" : "AUTO PAUSED", 313, 3, FONT_DEFAULT);
  spr.drawFastHLine(7, 14, 306, LINE);

  // Frequency stays left aligned; the unit is separate to preserve room on the right.
  char frequency[16];
  snprintf(frequency, sizeof(frequency), "%u.%02u",
           static_cast<unsigned>(currentFrequency / 100),
           static_cast<unsigned>(currentFrequency % 100));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(WHITE, BLACK);
  spr.drawString(frequency, 7, 17, FONT_LARGE);
  spr.setTextColor(GREY, BLACK);
  spr.drawString("MHz", 103, 32, FONT_SMALL);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(CYAN, BLACK);
  spr.drawString(fmDxBandwidthLabel(), 313, 20, FONT_SMALL);

  // Live stereo state from the SI4735 pilot detector and blend metric.
  char stereoText[20];
  if(fmStereoIdx == FM_STEREO_MONO) {
    snprintf(stereoText, sizeof(stereoText), "MONO LOCK");
  } else if(stereoPilot) {
    snprintf(stereoText, sizeof(stereoText), "STEREO %u%%",
             static_cast<unsigned>(stereoBlend));
  } else {
    snprintf(stereoText, sizeof(stereoText), "MONO");
  }
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(stereoPilot && fmStereoIdx != FM_STEREO_MONO ? GREEN : GREY, BLACK);
  spr.drawString(stereoText, 313, 39, FONT_SMALL);

  // Station identity and valid RDS clock.
  spr.drawFastHLine(7, 49, 306, LINE);
  drawLabel("STATION", 7, 53);
  const char *station = getStationName();
  drawValueOrDash(station, 57, 51, FONT_SMALL);
  const char *rdsTime = getRdsTime();
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(rdsTime && rdsTime[0] ? GREEN : GREY, BLACK);
  spr.drawString(rdsTime && rdsTime[0] ? rdsTime : "--:--", 313, 53, FONT_SMALL);

  // FT8-inspired segmented signal meter and separate SNR value.
  drawLabel("RSSI", 7, 76);
  drawRssiMeter(43, 76, rssi);
  drawLabel("SNR", 237, 73);
  char snrText[12];
  snprintf(snrText, sizeof(snrText), "%u dB", static_cast<unsigned>(snr));
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(WHITE, BLACK);
  spr.drawString(snrText, 313, 73, FONT_SMALL);

  // RDS metadata.
  spr.drawFastHLine(7, 92, 306, LINE);
  drawLabel("PI", 7, 97);
  uint16_t pi = getRdsPiCode();
  char piText[8];
  if(pi) snprintf(piText, sizeof(piText), "%04X", static_cast<unsigned>(pi));
  else piText[0] = '\0';
  drawValueOrDash(piText, 25, 97);

  drawLabel("PTY", 76, 97);
  const char *pty = getProgramInfo();
  drawValueOrDash(pty, 101, 97);

  // RadioText gets a fixed two-line area; blank RDS data remains visibly unavailable.
  spr.drawFastHLine(7, 112, 306, LINE);
  drawLabel("RADIO TEXT", 7, 116);
  const char *radioText = getRadioText();
  if(radioText && radioText[0]) {
    char line1[48];
    char line2[48];
    size_t len = strlen(radioText);
    size_t split = len > 47 ? 47 : len;
    memcpy(line1, radioText, split);
    line1[split] = '\0';
    size_t remaining = len > split ? len - split : 0;
    if(remaining > 47) remaining = 47;
    if(remaining) memcpy(line2, radioText + split, remaining);
    line2[remaining] = '\0';
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(WHITE, BLACK);
    spr.drawString(line1, 7, 131, FONT_SMALL);
    if(line2[0]) spr.drawString(line2, 7, 141, FONT_SMALL);
  } else {
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(GREY, BLACK);
    spr.drawString("--", 7, 131, FONT_SMALL);
  }

  // Footer: selected filter and operation hint.
  spr.drawFastHLine(7, 150, 306, LINE);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(CYAN, BLACK);
  spr.drawString("FILTER", 7, 155, FONT_DEFAULT);
  spr.setTextColor(WHITE, BLACK);
  spr.drawString(fmDxBandwidthLabel(), 52, 155, FONT_DEFAULT);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(GREY, BLACK);
  spr.drawString("CLICK: ACTIONS  HOLD: EXIT", 313, 155, FONT_DEFAULT);

  if(actionsOpen) {
    spr.fillRoundRect(65, 35, 190, 104, 5, 0x0841);
    spr.drawRoundRect(65, 35, 190, 104, 5, CYAN);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(CYAN, 0x0841);
    spr.drawString("FM-DX ACTIONS", 160, 41, FONT_DEFAULT);
    const char *items[] = { fmDxActive() ? "PAUSE AUTO" : "START AUTO", "FILTER: CHANGE", "BACK" };
    for(uint8_t n=0;n<3;++n) {
      int y=65+n*22;
      if(n==actionSelection) {
        spr.fillRoundRect(75,y-2,170,19,3,0x2104);
        spr.setTextColor(GREEN,0x2104);
      } else spr.setTextColor(WHITE,0x0841);
      spr.drawString(items[n],160,y,FONT_DEFAULT);
    }
  }
}
