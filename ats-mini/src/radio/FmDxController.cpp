#include "FmDxController.h"
#include "Common.h"
#include "Menu.h"

namespace {
  bool active = false;
  uint8_t savedBandwidth = 0;
  uint8_t currentBandwidth = 3; // 60 kHz starting point
  uint32_t lastChangeAt = 0;
  uint8_t narrowSamples = 0;
  uint8_t wideSamples = 0;
  uint8_t normalSamples = 0;

  void applyBandwidth(uint8_t index, uint32_t now) {
    if(index == currentBandwidth) return;
    rx.setFmBandwidth(index);
    currentBandwidth = index;
    lastChangeAt = now;
    narrowSamples = wideSamples = normalSamples = 0;
  }
}

bool fmDxStart() {
  if(currentMode != FM || active) return active;
  const Bandwidth *bandwidth = getCurrentBandwidth();
  if(!bandwidth) return false;
  savedBandwidth = bandwidth->idx;
  currentBandwidth = 3;
  active = true;
  lastChangeAt = millis();
  narrowSamples = wideSamples = normalSamples = 0;
  rx.setFmBandwidth(currentBandwidth);
  return true;
}

void fmDxStop() {
  if(!active) return;
  // The receiver may already have switched to AM/SSB through a remote action.
  // Restore the saved FM filter only while the tuner is still in FM mode.
  if(currentMode == FM) rx.setFmBandwidth(savedBandwidth);
  active = false;
  narrowSamples = wideSamples = normalSamples = 0;
}

void fmDxTick(uint8_t signalRssi, uint8_t signalSnr, uint32_t now) {
  if(!active) return;
  if(currentMode != FM) { fmDxStop(); return; }
  if(static_cast<uint32_t>(now - lastChangeAt) < 6000) return;

  if(signalRssi >= 25 && signalSnr >= 18) {
    if(narrowSamples < 3) ++narrowSamples;
    wideSamples = normalSamples = 0;
    if(narrowSamples >= 3) applyBandwidth(4, now); // 40 kHz
  } else if(signalRssi <= 15 || signalSnr <= 8) {
    if(wideSamples < 3) ++wideSamples;
    narrowSamples = normalSamples = 0;
    if(wideSamples >= 3) applyBandwidth(2, now); // 84 kHz
  } else if(signalRssi >= 20 && signalSnr >= 13) {
    if(normalSamples < 3) ++normalSamples;
    narrowSamples = wideSamples = 0;
    if(normalSamples >= 3) applyBandwidth(3, now); // 60 kHz
  } else {
    narrowSamples = wideSamples = normalSamples = 0;
  }
}

bool fmDxActive() { return active; }
uint8_t fmDxBandwidthIndex() { return currentBandwidth; }

const char *fmDxBandwidthLabel() {
  switch(currentBandwidth) {
    case 1: return "110 kHz";
    case 2: return "84 kHz";
    case 3: return "60 kHz";
    case 4: return "40 kHz";
    default: return "AUTO";
  }
}
