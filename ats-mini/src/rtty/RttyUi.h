#pragma once
#include <stdint.h>
bool rttyUiOpen(float initialBaud = 45.45f);
void rttyUiExit();
bool rttyUiTick(uint32_t now);
void rttyUiRotate(int16_t delta);
void rttyUiClick();
void drawRttyDecoderPage();
