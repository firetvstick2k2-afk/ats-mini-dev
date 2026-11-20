#include "FtxUi.h"

#include <time.h>
#include <stdio.h>

#include "FtxDecoderService.h"
#include "../audio/AudioCaptureService.h"
#include "Common.h"
#include "Menu.h"
#include "Themes.h"
#include "Utils.h"

namespace {
AudioCaptureService audioCapture;
FtxDecoderService decoderService;
bool pageActive = false;
uint8_t restoreNetworkMode = NET_OFF;
uint32_t lastPageRefresh = 0;
FtxMode pageMode = FtxMode::FT8;
}

bool ftxUiOpen(FtxMode mode)
{
  if(!clockAvailable())
  {
    statusShow("UTC time not available", "Wait for NTP or RDS CT");
    return false;
  }

  pageMode = mode;
  restoreNetworkMode = wifiModeIdx;
  netStop();

  if(getWiFiStatus() != 0 || !clockAvailable() ||
     !decoderService.enter(mode, true, clockAvailable(), audioCapture))
  {
    decoderService.exit();
    netInit(restoreNetworkMode);
    statusShow("Decoder start failed", "Check UTC and audio input");
    return false;
  }

  pageActive = true;
  lastPageRefresh = 0;
  return true;
}

void ftxUiExit()
{
  if(!pageActive) return;
  pageActive = false;
  decoderService.exit();
  netInit(restoreNetworkMode);
  currentCmd = CMD_NONE;
}

bool ftxUiTick(uint32_t now)
{
  if(!pageActive || static_cast<uint32_t>(now - lastPageRefresh) < 500) return false;
  lastPageRefresh = now;
  return true;
}

void drawFtxDecoderPage()
{
  const uint16_t bg = 0x0000, blue = 0x001F, cyan = 0x07FF, green = 0x07E0, grey = 0x4208;
  spr.fillSprite(bg);
  spr.drawRoundRect(3, 3, 314, 164, 5, blue);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(cyan);
  spr.drawString(pageMode == FtxMode::FT4 ? "FT4 DECODER" : "FT8 DECODER",
                 10, 7, &lgfx::fonts::Orbitron_Light_24);

  time_t now = time(nullptr);
  struct tm utc{};
  gmtime_r(&now, &utc);
  char timeText[20];
  snprintf(timeText, sizeof(timeText), "UTC %02d:%02d:%02d",
           utc.tm_hour, utc.tm_min, utc.tm_sec);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(cyan);
  spr.drawString(timeText, 308, 12, FONT_SMALL);

  char frequencyText[40];
  snprintf(frequencyText, sizeof(frequencyText), "%u.%03u MHz  %s",
    currentFrequency / 1000, currentFrequency % 1000,
    currentMode == USB ? "USB" : currentMode == LSB ? "LSB" : "AM");
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(green);
  spr.drawString(frequencyText, 12, 31, FONT_SMALL);

  const uint16_t level = decoderService.audioLevel();
  const int barWidth = level > 1800 ? 180 : static_cast<int>(level) / 10;
  spr.setTextColor(grey);
  spr.drawString("AUDIO", 12, 49, FONT_SMALL);
  spr.drawRoundRect(66, 49, 240, 12, 3, blue);
  if(barWidth > 0) spr.fillRoundRect(68, 51, barWidth > 236 ? 236 : barWidth, 8, 2, green);

  spr.setTextColor(cyan);
  spr.drawString(decoderService.decoderCollecting() ? "RECEIVING SLOT" : "WAITING FOR UTC SLOT",
                 12, 68, FONT_SMALL);
  char stats[64];
  snprintf(stats, sizeof(stats), "BLOCKS %lu/%lu  DECODES %lu  DROP %lu",
    static_cast<unsigned long>(decoderService.decoderBlocks()),
    static_cast<unsigned long>(decoderService.decoderMaxBlocks()),
    static_cast<unsigned long>(decoderService.decodedSlots()),
    static_cast<unsigned long>(decoderService.droppedBlocks()));
  spr.setTextColor(grey);
  spr.drawString(stats, 12, 85, FONT_SMALL);

  FtxDecodedMessage messages[FtxDecoderService::kResultCapacity]{};
  const size_t count = decoderService.copyResults(messages, FtxDecoderService::kResultCapacity);
  spr.setTextColor(green);
  if(!count)
    spr.drawString("No decoded messages yet", 12, 110, FONT_SMALL);
  else
  {
    for(size_t i = 0; i < count && i < 4; ++i)
    {
      char line[FTX_MAX_MESSAGE_LENGTH + 20];
      snprintf(line, sizeof(line), "%+.0f dB  %s", messages[i].snrDb, messages[i].text);
      spr.drawString(line, 12, 104 + static_cast<int>(i) * 12, FONT_SMALL);
    }
  }

  spr.setTextColor(cyan);
  spr.setTextDatum(TL_DATUM);
  spr.drawString("CLICK: EXIT", 12, 151, FONT_SMALL);
  spr.setTextDatum(TR_DATUM);
  spr.drawString("HOLD: EXIT", 308, 151, FONT_SMALL);
}
