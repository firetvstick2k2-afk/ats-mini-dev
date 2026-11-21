#ifndef FM_DX_CONTROLLER_H
#define FM_DX_CONTROLLER_H

#include <stdint.h>

bool fmDxStart();
void fmDxStop();
void fmDxSetManualBandwidth(uint8_t index);
void fmDxEndSession();
void fmDxTick(uint8_t rssi, uint8_t snr, uint32_t now);
bool fmDxActive();
uint8_t fmDxBandwidthIndex();
const char *fmDxBandwidthLabel();

#endif
