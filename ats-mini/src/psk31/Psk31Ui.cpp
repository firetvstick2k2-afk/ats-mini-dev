#include "Psk31Ui.h"
#include "Psk31Service.h"
#include "../audio/AudioCaptureService.h"
#include "../DecoderMenuUi.h"
#include "../../Common.h"
#include "../../Menu.h"
#include "../../Utils.h"
#include <stdio.h>
#include <string.h>

namespace {
Psk31Service service;
bool pageActive = false;
bool menuOpen = false;
uint8_t menuIndex = 0;
uint8_t restoreNetworkMode = NET_OFF;
uint8_t volumeBefore = 0;
bool volumeBoost = false;
uint32_t lastRefresh = 0;
constexpr uint8_t kMenuCount = 5;
}

bool psk31UiOpen() {
  restoreNetworkMode = wifiModeIdx;
  netStop();
  if (getWiFiStatus() != 0 || !service.enter(sharedAudioCapture(), true)) {
    service.exit();
    netInit(restoreNetworkMode);
    statusShow("PSK31 start failed", "Check Wi-Fi and audio input");
    return false;
  }
  volumeBefore = volume;
  volumeBoost = volume < 50;
  if (volumeBoost) { volume = 50; rx.setVolume(volume); }
  currentFrequency = rx.getFrequency();
  bands[bandIdx].currentFreq = currentFrequency;
  menuOpen = false; menuIndex = 0;
  pageActive = true; lastRefresh = 0;
  return true;
}

void psk31UiExit() {
  if (!pageActive) return;
  pageActive = false; menuOpen = false;
  service.exit();
  if (volumeBoost) { volume = volumeBefore; rx.setVolume(volume); volumeBoost = false; }
  netInit(restoreNetworkMode);
  currentCmd = CMD_NONE;
}

void psk31UiRotate(int16_t delta) {
  if (!pageActive || !menuOpen || !delta) return;
  int next = static_cast<int>(menuIndex) + (delta > 0 ? 1 : -1);
  while (next < 0) next += kMenuCount;
  menuIndex = static_cast<uint8_t>(next % kMenuCount);
}

void psk31UiClick() {
  if (!pageActive) return;
  if (!menuOpen) { menuOpen = true; menuIndex = 0; return; }
  switch (menuIndex) {
    case 0:
      if (!service.setRunning(!service.active()))
        statusShow("PSK31 receiver", "Could not change RX state");
      break;
    case 1: service.clearText(); break;
    case 2: service.resetDecoder(); break;
    case 3: menuOpen = false; break;
    default: psk31UiExit(); break;
  }
}

bool psk31UiTick(uint32_t now) {
  if (!pageActive || static_cast<uint32_t>(now - lastRefresh) < 100) return false;
  lastRefresh = now;
  return true;
}

void drawPsk31DecoderPage() {
  constexpr uint16_t bg = 0x0000, cyan = 0x07FF, green = 0x07E0, white = 0xFFFF, grey = 0x7BEF;
  spr.fillSprite(bg);
  char frequency[24] = {};
  snprintf(frequency, sizeof(frequency), "%lu.%03lu kHz",
           static_cast<unsigned long>(currentFrequency / 1000),
           static_cast<unsigned long>(currentFrequency % 1000));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(white);
  spr.drawString(frequency, 8, 5, &lgfx::fonts::Orbitron_Light_24);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(cyan);
  spr.drawString("BPSK31  31.25 Bd", 312, 12, FONT_SMALL);

  char status[48] = {};
  snprintf(status, sizeof(status), "%s  %s  LVL %u",
           service.active() ? "RUNNING" : "PAUSED",
           service.signal() ? "SIGNAL" : "SEARCH",
           static_cast<unsigned>(service.audioLevel()));
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(service.signal() ? green : grey);
  spr.drawString(status, 9, 35, FONT_SMALL);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(grey);
  spr.drawString("TONE 1000 Hz", 311, 35, FONT_SMALL);
  const uint16_t level = service.audioLevel() > 500 ? 500 : service.audioLevel();
  spr.drawRect(8, 50, 304, 9, cyan);
  spr.fillRect(10, 52, 300, 5, bg);
  if (level) spr.fillRect(10, 52, (300 * level) / 500, 5, service.signal() ? green : cyan);

  static char text[256] = {};
  const size_t length = service.copyText(text, sizeof(text));
  constexpr size_t charsPerLine = 32;
  const size_t visible = charsPerLine * 4;
  const size_t first = length > visible ? length - visible : 0;
  const int rowY[4] = {68, 91, 114, 137};
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(green);
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  for (uint8_t row = 0; row < 4; ++row) {
    const size_t offset = first + static_cast<size_t>(row) * charsPerLine;
    if (offset >= length) break;
    size_t count = length - offset;
    if (count > charsPerLine) count = charsPerLine;
    char line[charsPerLine + 1] = {};
    memcpy(line, text + offset, count);
    for (size_t i = 0; i < count; ++i) if (line[i] == '\n' || line[i] == '\r') line[i] = ' ';
    spr.drawString(line, 10, rowY[row]);
  }
  if (!length) {
    spr.setTextColor(grey);
    spr.setTextDatum(TL_DATUM);
    spr.setFont(FONT_SMALL);
    spr.drawString(service.active() ? "WAITING FOR PSK31 TEXT..." : "RECEIVER PAUSED", 10, 78);
  }
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(cyan);
  spr.setFont(FONT_SMALL);
  spr.drawString("CLICK: ACTIONS   LONG PRESS: EXIT", 160, 162);

  if (menuOpen) {
    const char* labels[kMenuCount] = {"STOP RX / START RX", "CLEAR TEXT", "RESET DECODER", "BACK", "EXIT PSK31"};
    const char* values[kMenuCount] = {service.active() ? "RUNNING" : "PAUSED", nullptr, nullptr, nullptr, nullptr};
    DecoderMenuUi::draw("PSK31 ACTIONS", labels, values, kMenuCount, menuIndex,
                        "TURN: SELECT", "CLICK: OPEN", nullptr, true);
  }
}
