#include "FmDxUi.h"
#include "FmDxController.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include <stdio.h>
#include <string.h>

extern bool updateFrequency(int newFreq, bool wrap);
extern bool consumeAbortPending();

namespace {
  bool pageActive = false;
  uint32_t lastRefresh = 0;
  bool stereoPilot = false;
  uint8_t stereoBlend = 0;
  bool actionsOpen = false;
  uint8_t actionSelection = 0;
  uint8_t manualFilter = 3;
  // SI4735 frequency units are 1 kHz; currentFrequency is displayed as /100.
  // One unit therefore produces the requested 0.01 MHz mini-step (100.00 -> 100.01).
  constexpr uint8_t FM_DX_STEP_COUNT = 6;
  constexpr uint8_t fmDxSteps[FM_DX_STEP_COUNT] = { 1, 5, 10, 20, 50, 100 };
  uint8_t fmDxStepSelection = 0;
  bool shortScanRunning = false;

  void fmDxSeekProgress(uint16_t frequency) {
    currentFrequency = frequency;
  }

  bool fmDxSeekAbort() {
    return consumeAbortPending();
  }

  void runShortScan() {
    if(currentMode != FM || shortScanRunning) return;
    shortScanRunning = true;
    actionsOpen = false;
    clearStationInfo();
    rssi = snr = 0;
    muteOn(MUTE_TEMP, true);
    consumeAbortPending();
    rx.seekStationProgress(fmDxSeekProgress, fmDxSeekAbort, 1);
    updateFrequency(rx.getFrequency(), true);
    clearStationInfo();
    identifyFrequency(currentFrequency);
    muteOn(MUTE_TEMP, false);
    shortScanRunning = false;
    lastRefresh = 0;
  }

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
    if(next < 0) next = 4;
    if(next > 4) next = 0;
    actionSelection = static_cast<uint8_t>(next);
    return;
  }
  const int stepKHz = fmDxSteps[fmDxStepSelection];
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
      runShortScan();
      break;
    case 1:
      if(fmDxActive()) fmDxStop();
      else fmDxStart();
      break;
    case 2:
      if(fmDxActive()) fmDxStop();
      manualFilter = manualFilter == 4 ? 2 : manualFilter + 1;
      fmDxSetManualBandwidth(manualFilter);
      break;
    case 3:
      fmDxStepSelection = static_cast<uint8_t>((fmDxStepSelection + 1) % FM_DX_STEP_COUNT);
      break;
    case 4:
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

  // RadioText is left-aligned and vertically centered in its dedicated text field.
  spr.drawFastHLine(7, 112, 306, LINE);
  drawLabel("RADIO TEXT", 7, 116);
  const char *radioText = getRadioText();
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(radioText && radioText[0] ? WHITE : GREY, BLACK);
  // Keep the larger 16 px font inside the 306 px text field without spilling into the footer.
  spr.setClipRect(7, 126, 306, 22);
  spr.drawString(radioText && radioText[0] ? radioText : "--", 7, 129, FONT_SMALL);
  spr.clearClipRect();

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
    spr.fillRoundRect(65, 17, 190, 136, 5, 0x0841);
    spr.drawRoundRect(65, 17, 190, 136, 5, CYAN);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(CYAN, 0x0841);
    spr.drawString("FM-DX ACTIONS", 160, 21, FONT_DEFAULT);
    char stepText[32];
    const uint16_t stepValue = fmDxSteps[fmDxStepSelection];
    snprintf(stepText, sizeof(stepText), "STEP: %u.%02u MHz",
             static_cast<unsigned>(stepValue / 100),
             static_cast<unsigned>(stepValue % 100));
    const char *items[] = { "SHORT SCAN", fmDxActive() ? "PAUSE AUTO" : "START AUTO", "FILTER: CHANGE", stepText, "BACK" };
    for(uint8_t n=0;n<5;++n) {
      int y=41+n*21;
      if(n==actionSelection) {
        spr.fillRoundRect(75,y-2,170,18,3,0x2104);
        spr.setTextColor(GREEN,0x2104);
      } else spr.setTextColor(WHITE,0x0841);
      spr.drawString(items[n],160,y,FONT_DEFAULT);
    }
  }
}
