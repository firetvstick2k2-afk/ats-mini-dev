#pragma once

#include "Common.h"
#include <stdio.h>

// Shared compact decoder menu styled after the ATS Mini main menu.
// Three rows are visible at once; selection and scrolling are kept by each UI.
namespace DecoderMenuUi {
static constexpr uint16_t kBg = 0x0000;
static constexpr uint16_t kCyan = 0x07FF;
static constexpr uint16_t kBlue = 0x001F;
static constexpr uint16_t kWhite = 0xFFFF;
static constexpr uint16_t kMuted = 0x7BEF;
static constexpr uint8_t kVisibleRows = 3;

inline uint8_t firstVisible(uint8_t selected, uint8_t count) {
  if (count <= kVisibleRows) return 0;
  uint8_t first = selected > 1 ? selected - 1 : 0;
  const uint8_t maxFirst = count - kVisibleRows;
  return first > maxFirst ? maxFirst : first;
}

inline void draw(const char* title,
                 const char* const* labels,
                 const char* const* values,
                 uint8_t count,
                 uint8_t selected,
                 const char* footerLeft = "TURN: SELECT",
                 const char* footerRight = "CLICK: OPEN",
                 const char* status = nullptr,
                 bool outlineSelection = false) {
  constexpr int x = 20, y = 8, w = 280, h = 154;
  spr.fillRect(x, y, w, h, kBg);
  spr.drawRect(x, y, w, h, kBlue);
  spr.fillRect(x, y, 3, h, kCyan);

  spr.setFont(FONT_SMALL);
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(kCyan);
  spr.drawString(title ? title : "DECODER", x + 12, y + 5);
  char counter[16];
  snprintf(counter, sizeof(counter), "%02u / %02u",
           static_cast<unsigned>(selected + 1), static_cast<unsigned>(count));
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(kCyan);
  spr.drawString(counter, x + w - 12, y + 5);

  const uint8_t first = firstVisible(selected, count);
  for (uint8_t row = 0; row < kVisibleRows; ++row) {
    const uint8_t index = first + row;
    if (index >= count) break;
    const int rowY = y + 29 + row * 27;
    const bool active = index == selected;
    if (active) {
      if (outlineSelection) {
        spr.drawRoundRect(x + 7, rowY - 3, w - 22, 23, 3, kCyan);
      } else {
        spr.fillRoundRect(x + 7, rowY - 3, w - 22, 23, 3, kBlue);
        spr.fillRect(x + 7, rowY - 3, 3, 23, kCyan);
      }
    }
    char numbered[8];
    snprintf(numbered, sizeof(numbered), "%02u", static_cast<unsigned>(index + 1));
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(active ? kCyan : kMuted);
    spr.drawString(numbered, x + 14, rowY);
    spr.setTextColor(active ? kWhite : kMuted);
    if (labels && labels[index]) spr.drawString(labels[index], x + 36, rowY);
    if (values && values[index] && values[index][0]) {
      spr.setTextDatum(TR_DATUM);
      spr.setTextColor(active ? kCyan : kMuted);
      spr.drawString(values[index], x + w - 25, rowY);
    }
  }

  if (count > kVisibleRows) {
    const int trackX = x + w - 10, trackY = y + 29, trackH = 78;
    spr.drawFastVLine(trackX, trackY, trackH, kBlue);
    const int thumbH = trackH * kVisibleRows / count;
    const int maxFirst = count - kVisibleRows;
    const int thumbY = trackY + (trackH - thumbH) * first / maxFirst;
    spr.fillRoundRect(trackX - 2, thumbY, 5, thumbH, 2, kCyan);
  }

  if (status && status[0]) {
    spr.setFont(FONT_DEFAULT);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(kMuted);
    spr.drawString(status, x + w / 2, y + 117);
  }
  spr.drawFastHLine(x + 10, y + 125, w - 20, kBlue);
  spr.setFont(FONT_DEFAULT);
  spr.setTextDatum(BL_DATUM);
  spr.setTextColor(kMuted);
  if (footerLeft) spr.drawString(footerLeft, x + 12, y + 149);
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(kCyan);
  if (footerRight) spr.drawString(footerRight, x + w - 12, y + 149);
}
} // namespace DecoderMenuUi
