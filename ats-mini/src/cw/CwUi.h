#pragma once
#include <stdint.h>
bool cwUiOpen();
void cwUiExit();
bool cwUiTick(uint32_t now);
void cwUiRotate(int16_t delta);
void cwUiClick();
void drawCwDecoderPage();
