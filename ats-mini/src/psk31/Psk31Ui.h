#pragma once
#include <stdint.h>
bool psk31UiOpen();
void psk31UiExit();
void psk31UiRotate(int16_t delta);
void psk31UiClick();
bool psk31UiTick(uint32_t now);
void drawPsk31DecoderPage();
