#pragma once

#include "FtxDecoder.h"

bool ftxUiOpen(FtxMode mode);
void ftxUiExit();
void ftxUiClick();
bool ftxUiTick(uint32_t now);
void drawFtxDecoderPage();
