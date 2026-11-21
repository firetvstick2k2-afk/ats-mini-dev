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
  // Text-first diagnostic layout: pure black, high-contrast white, one green accent.
  constexpr uint16_t black = 0x0000;
  constexpr uint16_t white = 0xFFFF;
  constexpr uint16_t muted = 0xBDF7;
  constexpr uint16_t green = 0x07E0;
  constexpr uint16_t line = 0x4208;

  spr.fillSprite(black);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(white, black);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawString("JS8 RX", 8, 13);

  Js8AudioMonitor::Metrics m{};
  const bool valid = audioMonitor.copyMetrics(m);
  const bool playback = audioMonitor.testPlaybackActive();

  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(playback ? green : (valid ? green : muted), black);
  spr.setFont(FONT_SMALL);
  spr.drawString(playback ? "TEST SIGNAL" : (valid ? "LIVE AUDIO" : "WAITING"), 312, 13);
  spr.drawFastHLine(8, 25, 304, line);

  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(muted, black);
  spr.setFont(FONT_SMALL);
  spr.drawString(playback ? "SOURCE: INTERNAL 8-FSK" : "SOURCE: GPIO 11", 8, 37);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(white, black);
  spr.drawString("NORMAL  6.25 BAUD", 312, 37);

  char row[80];
  if (valid) {
    snprintf(row, sizeof(row), "RATE  %.0f Hz     BLOCK  %.0f us",
             m.callbackRateHz, m.meanBlockIntervalUs);
  } else {
    snprintf(row, sizeof(row), "RATE  --           BLOCK  --");
  }
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(white, black);
  spr.drawString(row, 8, 55);

  if (valid) {
    snprintf(row, sizeof(row), "LEVEL  %.0f RMS     ADC  %d..%d",
             m.adcRmsAc, m.adcMin, m.adcMax);
  } else {
    snprintf(row, sizeof(row), "LEVEL  --           ADC  --");
  }
  spr.drawString(row, 8, 71);

  if (valid) {
    snprintf(row, sizeof(row), "DROP %lu   GAP %lu   CLIP %lu",
             static_cast<unsigned long>(m.queueDrops),
             static_cast<unsigned long>(m.discontinuities),
             static_cast<unsigned long>(m.clippedSamples));
  } else {
    snprintf(row, sizeof(row), "DROP --   GAP --   CLIP --");
  }
  spr.setTextColor(green, black);
  spr.drawString(row, 8, 87);

  spr.drawFastHLine(8, 99, 304, line);
  spr.setTextColor(white, black);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawString("TEST PAYLOAD (LOCAL ONLY)", 8, 112);

  // Fixed UI fixture only; it is not encoded in or decoded from audio.
  spr.setTextColor(green, black);
  spr.setFont(FONT_SMALL);
  spr.drawString("CQ ATS MINI - JS8 NORMAL TEST", 8, 132);
  spr.setTextColor(muted, black);
  if (m.testFrameComplete) {
    snprintf(row, sizeof(row), "LOCAL COSTAS: %s  (%u/21)",
             m.testSyncPass ? "PASS" : "CHECK", m.testMatchedPilots);
  } else {
    snprintf(row, sizeof(row), "LOCAL COSTAS: waiting for 12.64 s test frame");
  }
  spr.drawString(row, 8, 146);

  spr.drawFastHLine(8, 156, 304, line);
  spr.setTextColor(white, black);
  spr.setTextDatum(MC_DATUM);
  spr.drawString("CLICK: 12.64s TEST     HOLD: EXIT", 160, 165, FONT_SMALL);
}
