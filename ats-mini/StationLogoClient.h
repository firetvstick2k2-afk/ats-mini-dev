#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct StationLogoState {
  uint32_t frequencyKHz = 0;
  uint16_t piCode = 0;
  char stationName[50] = {};
  char radioText[100] = {};
};

// Port of Mini_OS StationLogoClient for the legacy ATS Mini radio state.
class StationLogoClient {
public:
  bool begin();
  void update(const StationLogoState& state, bool wifiConnected);
  bool hasLocalLogo(const StationLogoState& state);
  bool lockLogo(const char* key, const uint8_t*& data, size_t& length, bool& isJpg);
  bool isLookupPending(const char* key) const;
  void unlockLogo();
private:
  static void taskEntry(void* arg);
  void runLookup(const StationLogoState& state);
  bool storeLogoInRam(const String& logo, const char* key, const char* source);
  volatile bool busy_ = false;
  bool haveRequested_ = false;
  char lastRequestKey_[40] = {};
  uint32_t lastAttemptMs_ = 0;
  bool lastAttemptHadWifi_ = false;
  SemaphoreHandle_t logoMutex_ = nullptr;
  uint8_t* logoData_ = nullptr;
  size_t logoLength_ = 0;
  bool logoIsJpg_ = false;
  char logoKey_[40] = {};
};

bool dabLogoBegin();
StationLogoClient& dabLogoClient();
