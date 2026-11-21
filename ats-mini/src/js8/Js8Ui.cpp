#include "Js8Ui.h"
#include "Js8AudioMonitor.h"
#include "../audio/AudioCaptureService.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include <stdio.h>

namespace {
Js8AudioMonitor audioMonitor;
bool pageActive = false;
uint32_t lastRefresh = 0;
}

bool js8UiOpen() {
  if (!audioMonitor.begin(sharedAudioCapture())) {
    statusShow("JS8 audio monitor failed", "Check audio input / memory");
    return false;
  }
  pageActive = true;
  lastRefresh = 0;
  return true;
}

void js8UiExit() {
  if (!pageActive) return;
  pageActive = false;
  audioMonitor.stop();
  currentCmd = CMD_NONE;
}

bool js8UiTick(uint32_t now) {
  if (!pageActive || static_cast<uint32_t>(now - lastRefresh) < 250) return false;
  lastRefresh = now;
  return true;
}

void js8UiRotate(int16_t delta) {
  (void)delta;
}

void js8UiClick() {
  if (!pageActive) return;
  if (audioMonitor.testPlaybackActive()) audioMonitor.stopTestPlayback();
  else audioMonitor.startTestPlayback();
}

void drawJs8MonitorPage() {
  constexpr uint16_t navy = 0x0008, panel = 0x001F, panelAlt = 0x0359;
  constexpr uint16_t border = 0x04BF, green = 0x07E0, cyan = 0x07FF, white = 0xFFFF;
  spr.fillSprite(navy);
  spr.drawRect(2, 2, 316, 166, panel);
  spr.fillRect(8, 5, 304, 25, panelAlt);
  spr.drawRect(8, 5, 304, 25, border);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(green, panelAlt);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawString("JS8 RX  /  AUDIO MONITOR", 15, 17);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(white, panelAlt);
  spr.setFont(FONT_SMALL);
  spr.drawString("NORMAL: 6.25 BAUD", 304, 17);

  Js8AudioMonitor::Metrics m{};
  const bool valid = audioMonitor.copyMetrics(m);
  const bool playback = audioMonitor.testPlaybackActive();
  spr.fillRect(8, 35, 304, 27, panel);
  spr.drawRect(8, 35, 304, 27, border);
  spr.setTextColor(valid ? green : cyan, panel);
  spr.setTextDatum(ML_DATUM);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawString(playback ? "TEST PLAYBACK: 8-FSK" :
                 (valid ? "LIVE AUDIO MONITOR" : "WAITING FOR AUDIO..."), 15, 48);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(white, panel);
  char value[64];
  if (playback) snprintf(value, sizeof(value), "SYNTH / NOMINAL 4 kHz");
  else snprintf(value, sizeof(value), "GPIO %u / NOMINAL 4 kHz",
                static_cast<unsigned>(AudioCaptureService::kPin));
  spr.setFont(FONT_SMALL);
  spr.drawString(value, 304, 48);

  const int rowsY[] = {67, 86, 105, 124};
  const char* labels[] = {"PROC. RATE EST.", "BLOCK INTERVAL", "ADC MIN / MAX", "DC MEAN / AC RMS"};
  char values[4][48]{};
  if (valid) {
    snprintf(values[0], sizeof(values[0]), "%.1f Hz", m.callbackRateHz);
    snprintf(values[1], sizeof(values[1]), "%.0f us (%lu..%lu)",
             m.meanBlockIntervalUs, static_cast<unsigned long>(m.minBlockIntervalUs),
             static_cast<unsigned long>(m.maxBlockIntervalUs));
    snprintf(values[2], sizeof(values[2]), "%d / %d", m.adcMin, m.adcMax);
    snprintf(values[3], sizeof(values[3]), "%.1f / %.1f", m.adcMean, m.adcRmsAc);
  } else {
    for (uint8_t i = 0; i < 4; ++i) snprintf(values[i], sizeof(values[i]), "--");
  }
  for (uint8_t i = 0; i < 4; ++i) {
    const uint16_t bg = (i & 1) ? panelAlt : panel;
    spr.fillRect(8, rowsY[i], 304, 17, bg);
    spr.drawRect(8, rowsY[i], 304, 17, border);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(white, bg);
    spr.setFont(FONT_SMALL);
    spr.drawString(labels[i], 14, rowsY[i] + 8);
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(green, bg);
    spr.drawString(values[i], 304, rowsY[i] + 8);
  }
  spr.fillRect(8, 145, 304, 14, navy);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(cyan, navy);
  spr.setFont(FONT_SMALL);
  if (valid) {
    snprintf(value, sizeof(value), "BLOCKS %lu DROP %lu GAP %lu CLIP %lu",
             static_cast<unsigned long>(m.callbackBlocks),
             static_cast<unsigned long>(m.queueDrops),
             static_cast<unsigned long>(m.discontinuities),
             static_cast<unsigned long>(m.clippedSamples));
  } else {
    snprintf(value, sizeof(value), "WAITING FOR FIRST MEASUREMENT WINDOW");
  }
  spr.drawString(value, 12, 152);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(white, navy);
  spr.drawString("CLICK: TEST PLAY/STOP   LONG PRESS: EXIT", 160, 164, FONT_SMALL);
}
