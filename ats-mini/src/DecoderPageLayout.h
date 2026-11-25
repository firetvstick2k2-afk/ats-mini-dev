#pragma once
#include "Common.h"
#include <stdio.h>
#include <string.h>

// Shared FT8-family receive page geometry for CW and RTTY.
namespace DecoderPageLayout {
inline void draw(uint32_t frequencyKHz, const char* status,
                 const char* decoded, const char* waitingText,
                 uint16_t level, uint16_t levelMax) {
  constexpr uint16_t bg=0x0000, blue=0x001F, cyan=0x07FF;
  constexpr int meterSegments=37, yellowStart=25, redStart=32;
  spr.fillSprite(bg);
  char frequencyText[24];
  snprintf(frequencyText,sizeof(frequencyText),"%lu.%03lu",
           static_cast<unsigned long>(frequencyKHz/1000),
           static_cast<unsigned long>(frequencyKHz%1000));
  spr.setTextColor(0xFFFF);
  spr.setTextDatum(TL_DATUM);
  const int frequencyWidth=spr.drawString(frequencyText,8,5,&lgfx::fonts::Orbitron_Light_24);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(cyan);
  spr.drawString("kHz",8+frequencyWidth+7,19,FONT_SMALL);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(cyan);
  spr.drawString(status ? status : "",205,19,FONT_SMALL);
  char counter[16];
  snprintf(counter,sizeof(counter),"TXT %03u",
           static_cast<unsigned>(decoded ? strlen(decoded) : 0));
  spr.setTextDatum(TR_DATUM);
  spr.drawString(counter,311,10,FONT_SMALL);

  const int activeSegments=levelMax
    ? static_cast<int>((static_cast<uint32_t>(level>levelMax?levelMax:level)*meterSegments)/levelMax)
    : 0;
  spr.drawRect(8,34,304,12,blue);
  spr.fillRect(10,36,300,8,bg);
  for(int segment=0;segment<meterSegments;++segment) {
    const int x=10+segment*8;
    const uint16_t zoneColor=segment<yellowStart?0x07E0:(segment<redStart?0xFFE0:0xF800);
    spr.fillRect(x,36,6,8,segment<activeSegments?zoneColor:0x2104);
  }

  const size_t length=decoded?strlen(decoded):0;
  constexpr size_t charsPerLine=34;
  const size_t first=length>charsPerLine*4?length-charsPerLine*4:0;
  for(int row=0;row<4;++row) {
    const size_t offset=first+static_cast<size_t>(row)*charsPerLine;
    const int y=51+row*28;
    if(offset<length) {
      size_t count=length-offset;
      if(count>charsPerLine) count=charsPerLine;
      char line[charsPerLine+1];
      memcpy(line,decoded+offset,count);
      line[count]=0;
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.setTextDatum(TL_DATUM);
      spr.setTextColor(0x07E0);
      spr.drawString(line,14,y+7);
    } else if(row==0) {
      spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(cyan);
      spr.drawString(waitingText ? waitingText : "WAITING...",14,y+14);
    }
  }
}
} // namespace DecoderPageLayout
