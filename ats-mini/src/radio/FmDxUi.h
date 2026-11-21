#ifndef FM_DX_UI_H
#define FM_DX_UI_H

#include <stdint.h>

bool fmDxUiOpen();
void fmDxUiExit();
bool fmDxUiTick(uint32_t now);
void fmDxUiRotate(int16_t delta);
void fmDxUiClick();
void drawFmDxPage();

#endif
