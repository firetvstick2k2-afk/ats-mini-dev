#pragma once
#include <stdint.h>
bool rttyUiOpen();
void rttyUiExit();
bool rttyUiTick(uint32_t now);
void rttyUiRotate(int16_t delta);
void rttyUiClick();
void drawRttyDecoderPage();
