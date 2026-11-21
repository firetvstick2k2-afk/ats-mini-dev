#include "CwUi.h"
#include "CwDecoderService.h"
#include "../audio/AudioCaptureService.h"
#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include <stdio.h>
#include <string.h>

namespace {
CwDecoderService service;
bool pageActive = false;
bool menuOpen = false;
bool menuEditing = false;
uint8_t menuIndex = 0;
uint8_t toneIndex = 4;
uint8_t restoreNetworkMode = NET_OFF;
uint32_t lastRefresh = 0;
const float toneChoices[] = {400.0f, 500.0f, 550.0f, 600.0f, 650.0f, 700.0f, 750.0f, 800.0f, 900.0f};
constexpr uint8_t kToneCount = sizeof(toneChoices) / sizeof(toneChoices[0]);
constexpr uint8_t kMenuCount = 5;

void applyTone() {
  service.setTone(toneChoices[toneIndex]);
  service.resetDecoder();
}
}

bool cwUiOpen() {
  restoreNetworkMode = wifiModeIdx;
  netStop();
  if (getWiFiStatus() != 0 || !service.enter(sharedAudioCapture(), true)) {
    service.exit();
    netInit(restoreNetworkMode);
    statusShow("CW decoder start failed", "Check Wi-Fi and audio input");
    return false;
  }
  toneIndex = 5;
  menuOpen = false;
  menuEditing = false;
  menuIndex = 0;
  pageActive = true;
  lastRefresh = 0;
  return true;
}

void cwUiExit() {
  if (!pageActive) return;
  pageActive = false;
  menuOpen = false;
  menuEditing = false;
  service.exit();
  netInit(restoreNetworkMode);
  currentCmd = CMD_NONE;
}

void cwUiRotate(int16_t delta) {
  if (!pageActive || !delta || !menuOpen) return;
  if (menuEditing) {
    int next = static_cast<int>(toneIndex) + delta;
    while (next < 0) next += kToneCount;
    toneIndex = static_cast<uint8_t>(next % kToneCount);
    applyTone();
    return;
  }
  int next = static_cast<int>(menuIndex) + delta;
  while (next < 0) next += kMenuCount;
  menuIndex = static_cast<uint8_t>(next % kMenuCount);
}

void cwUiClick() {
  if (!pageActive) return;
  if (!menuOpen) {
    menuOpen = true;
    menuEditing = false;
    menuIndex = 0;
    return;
  }
  if (menuEditing) {
    menuEditing = false;
    return;
  }
  switch (menuIndex) {
    case 0: menuEditing = true; break;
    case 1: service.clearText(); break;
    case 2: service.resetDecoder(); break;
    case 3: menuOpen = false; break;
    case 4: cwUiExit(); break;
  }
}

bool cwUiTick(uint32_t now) {
  if (!pageActive || static_cast<uint32_t>(now - lastRefresh) < 100) return false;
  lastRefresh = now;
  service.update();
  return true;
}

void drawCwDecoderPage() {
  constexpr uint16_t navy = 0x0008, blueA = 0x0359, blueB = 0x047D;
  constexpr uint16_t border = 0x04BF, green = 0x07E0, white = 0xFFFF;
  spr.fillSprite(navy);
  spr.drawRect(2, 2, 316, 166, 0x001F);

  spr.fillRect(8, 5, 304, 29, blueA);
  spr.drawRect(8, 5, 304, 29, border);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(white, blueA);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  char header[48];
  snprintf(header, sizeof(header), "%lu kHz  %s",
           static_cast<unsigned long>(currentFrequency),
           currentMode == FM ? "FM" : currentMode == USB ? "USB" : currentMode == LSB ? "LSB" : "AM");
  spr.drawString(header, 15, 19);

  const bool active = service.signal();
  char status[40];
  snprintf(status, sizeof(status), "%s  %d WPM",
           active ? "SIGNAL" : "NO RX", service.wpm());
  spr.setTextDatum(TR_DATUM);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.drawString(status, 304, 19);

  spr.fillRect(8, 39, 304, 22, blueB);
  spr.drawRect(8, 39, 304, 22, border);
  spr.setTextDatum(ML_DATUM);
  spr.setFont(&lgfx::fonts::FreeSans9pt7b);
  char info[48];
  snprintf(info, sizeof(info), "CW TONE  %.0f Hz", toneChoices[toneIndex]);
  spr.drawString(info, 15, 50);
  spr.setTextDatum(TR_DATUM);
  snprintf(info, sizeof(info), "LEVEL %u", static_cast<unsigned>(service.audioLevel()));
  spr.drawString(info, 304, 50);

  // A compact level bar gives immediate feedback from the shared audio input.
  const uint16_t level = service.audioLevel() > 500 ? 500 : service.audioLevel();
  spr.drawRect(8, 64, 304, 5, border);
  if (level) spr.fillRect(9, 65, (302 * level) / 500, 3, active ? green : blueB);

  const char* decoded = service.text();
  const size_t len = strlen(decoded);
  constexpr size_t charsPerLine = 31;
  const int rowY[4] = {73, 94, 115, 136};
  const size_t visibleChars = charsPerLine * 4;
  const size_t first = len > visibleChars ? len - visibleChars : 0;
  for (int row = 0; row < 4; ++row) {
    const uint16_t panel = (row & 1) ? blueB : blueA;
    spr.fillRect(8, rowY[row], 304, 18, panel);
    spr.drawRect(8, rowY[row], 304, 18, border);
    const size_t offset = first + static_cast<size_t>(row) * charsPerLine;
    if (offset < len) {
      size_t count = len - offset;
      if (count > charsPerLine) count = charsPerLine;
      char line[charsPerLine + 1];
      memcpy(line, decoded + offset, count);
      line[count] = 0;
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(white, panel);
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.drawString(line, 13, rowY[row] + 9);
    } else if (row == 0 && !active) {
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(white, panel);
      spr.setFont(&lgfx::fonts::FreeSans9pt7b);
      spr.drawString("WAITING FOR CW SIGNAL...", 13, rowY[row] + 9);
    }
  }

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(white, navy);
  spr.setFont(FONT_SMALL);
  spr.drawString("CLICK: ACTIONS   LONG PRESS: EXIT", 160, 162);

  if (menuOpen) {
    constexpr uint16_t panel = 0x0008, highlight = 0x0359;
    spr.fillRect(28, 31, 264, 126, panel);
    spr.drawRect(28, 31, 264, 126, green);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(white, panel);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.drawString("CW DECODER ACTIONS", 160, 41);
    const char* labels[kMenuCount] = {"TONE FREQUENCY", "CLEAR TEXT", "RESET DECODER", "BACK", "EXIT CW"};
    for (uint8_t i = 0; i < kMenuCount; ++i) {
      const int y = 58 + i * 18;
      const uint16_t bg = (i == menuIndex) ? highlight : panel;
      spr.fillRect(36, y - 7, 248, 16, bg);
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(white, bg);
      spr.setFont(FONT_SMALL);
      spr.drawString(labels[i], 43, y + 1);
      if (i == 0) {
        char value[20];
        snprintf(value, sizeof(value), "%.0f Hz%s", toneChoices[toneIndex],
                 menuEditing ? " *" : "");
        spr.setTextDatum(TR_DATUM);
        spr.drawString(value, 276, y + 1);
      }
    }
  }
}
