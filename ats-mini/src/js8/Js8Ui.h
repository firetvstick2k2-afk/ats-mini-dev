#pragma once
#include <stdint.h>
bool js8UiOpen();
void js8UiExit();
bool js8UiTick(uint32_t now);
void js8UiRotate(int16_t delta);
void js8UiClick();
void drawJs8MonitorPage();
