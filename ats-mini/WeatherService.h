#pragma once

#include <Arduino.h>

class WeatherService {
public:
  static constexpr size_t POSTAL_CODE_BYTES = 12;
  static constexpr uint8_t FORECAST_DAYS = 5;

  struct Day {
    int16_t maximumC = 0;
    int16_t minimumC = 0;
    uint8_t precipitationChance = 0;
    uint8_t weatherCode = 0;
  };

  struct Snapshot {
    char location[48] = {};
    char country[32] = {};
    char description[32] = {};
    int16_t temperatureC = 0;
    int16_t feelsLikeC = 0;
    uint8_t humidity = 0;
    uint8_t precipitation = 0;
    uint8_t snowChance = 0;
    uint16_t pressureHpa = 0;
    uint16_t windKmh = 0;
    uint8_t weatherCode = 0;
    Day days[FORECAST_DAYS] = {};
    char weekdays[FORECAST_DAYS][8] = {};
    uint32_t updatedAtMs = 0;
    bool valid = false;
    bool stale = true;
    bool fetching = false;
  };

  bool begin();
  void update(bool wifiConnected, bool enabled);
  bool setPostalCode(const char* postalCode);
  void requestRefresh();
  String postalCode() const;
  bool hasConfiguredLocation() const { return postalCode_[0] != '\0'; }
  Snapshot snapshot() const;

private:
  static void taskEntry(void* context);
  void taskLoop();
  bool fetchWeather(const char* postalCode, Snapshot& result);
  bool fetchJson(const String& url, String& body);
  void publish(const Snapshot& value);
  void loadSettings();

  char postalCode_[POSTAL_CODE_BYTES] = {};
  mutable portMUX_TYPE snapshotMux_ = portMUX_INITIALIZER_UNLOCKED;
  Snapshot snapshot_{};
  TaskHandle_t taskHandle_ = nullptr;
  volatile bool refreshRequested_ = false;
  volatile bool wifiConnected_ = false;
  volatile bool weatherEnabled_ = false;
  uint32_t lastAttemptMs_ = 0;
  bool settingsLoaded_ = false;
  uint32_t lastStartAttemptMs_ = 0;
};

extern WeatherService weatherService;
extern uint8_t weatherPage;

enum WeatherUiState : uint8_t {
  WEATHER_UI_IDLE = 0,
  WEATHER_UI_WAITING_WIFI,
  WEATHER_UI_LOADING,
  WEATHER_UI_READY,
  WEATHER_UI_NO_LOCATION
};
extern volatile uint8_t weatherUiState;
