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
uint8_t restoreNetworkMode = NET_OFF;
uint32_t lastRefresh = 0;
constexpr uint8_t kMenuCount = 4;
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
  switch (menuIndex) {
    case 0: service.clearText(); break;
    case 1: service.resetDecoder(); break;
    case 2: menuOpen = false; break;
    case 3: cwUiExit(); break;
  }
}

bool cwUiTick(uint32_t now) {
  if (!pageActive || static_cast<uint32_t>(now - lastRefresh) < 100) return false;
  lastRefresh = now;
  service.update();
  return true;
}

void drawCwDecoderPage() {
  constexpr uint16_t bg = 0x0000, panel = 0x0010, panelAlt = 0x0020;
  constexpr uint16_t border = 0x07FF, green = 0x07E0, cyan = 0x07FF, white = 0xFFFF, grey = 0x8410;
  spr.fillSprite(bg);
  spr.drawRect(2, 2, 316, 166, cyan);

  spr.fillRect(8, 5, 304, 29, panel);
  spr.drawRect(8, 5, 304, 29, border);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(green, panel);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  char header[48];
  snprintf(header, sizeof(header), "%lu kHz  %s",
           static_cast<unsigned long>(currentFrequency),
           currentMode == FM ? "FM" : currentMode == USB ? "USB" : currentMode == LSB ? "LSB" : "AM");
  spr.drawString(header, 15, 19);

  const bool active = service.signal();
  const bool audioPresent = service.audioPresent();
  char status[40];
  snprintf(status, sizeof(status), "%s %s %d WPM",
           audioPresent ? "AUDIO" : "NO AUDIO", active ? "CW" : "SEARCH", service.wpm());
  spr.setTextDatum(TR_DATUM);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  spr.setTextColor(audioPresent ? green : grey, panel);
  spr.drawString(status, 304, 19);

  spr.fillRect(8, 39, 304, 22, panelAlt);
  spr.drawRect(8, 39, 304, 22, border);
  spr.setTextDatum(ML_DATUM);
  spr.setFont(&lgfx::fonts::FreeSans9pt7b);
  char info[48];
  snprintf(info, sizeof(info), "AUTO TONE  %.0f Hz", service.tone());
  spr.setTextColor(cyan, panelAlt);
  spr.drawString(info, 15, 50);
  spr.setTextDatum(TR_DATUM);
  snprintf(info, sizeof(info), "LEVEL %u", static_cast<unsigned>(service.audioLevel()));
  spr.drawString(info, 304, 50);

  // A compact level bar gives immediate feedback from the shared audio input.
  const uint16_t level = service.audioLevel() > 500 ? 500 : service.audioLevel();
  spr.drawRect(8, 64, 304, 5, border);
  if (level) spr.fillRect(9, 65, (302 * level) / 500, 3, service.audioPresent() ? green : cyan);

  const char* decoded = service.text();
  const size_t len = strlen(decoded);
  constexpr size_t charsPerLine = 31;
  const int rowY[4] = {73, 94, 115, 136};
  const size_t visibleChars = charsPerLine * 4;
  const size_t first = len > visibleChars ? len - visibleChars : 0;
  for (int row = 0; row < 4; ++row) {
    const uint16_t rowPanel = (row & 1) ? panelAlt : panel;
    spr.fillRect(8, rowY[row], 304, 18, rowPanel);
    spr.drawRect(8, rowY[row], 304, 18, border);
    const size_t offset = first + static_cast<size_t>(row) * charsPerLine;
    if (offset < len) {
      size_t count = len - offset;
      if (count > charsPerLine) count = charsPerLine;
      char line[charsPerLine + 1];
      memcpy(line, decoded + offset, count);
      line[count] = 0;
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(green, rowPanel);
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.drawString(line, 13, rowY[row] + 9);
    } else if (row == 0 && !active) {
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(grey, rowPanel);
      spr.setFont(&lgfx::fonts::FreeSans9pt7b);
      spr.drawString("WAITING FOR CW SIGNAL...", 13, rowY[row] + 9);
    }
  }

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(cyan, bg);
  spr.setFont(FONT_SMALL);
  spr.drawString("CLICK: ACTIONS   LONG PRESS: EXIT", 160, 162);

  if (menuOpen) {
    constexpr uint16_t menuPanel = 0x0010, highlight = 0x03E0;
    spr.fillRect(28, 31, 264, 126, menuPanel);
    spr.drawRect(28, 31, 264, 126, cyan);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(cyan, menuPanel);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.drawString("CW DECODER ACTIONS", 160, 41);
    const char* labels[kMenuCount] = {"CLEAR TEXT", "RESET DECODER", "BACK", "EXIT CW"};
    for (uint8_t i = 0; i < kMenuCount; ++i) {
      const int y = 58 + i * 18;
      const uint16_t rowBg = (i == menuIndex) ? highlight : menuPanel;
      spr.fillRect(36, y - 7, 248, 16, rowBg);
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor((i == menuIndex) ? 0x0000 : white, rowBg);
      spr.setFont(FONT_SMALL);
      spr.drawString(labels[i], 43, y + 1);
    }
  }
}
