#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class RttyService {
public:
  static constexpr uint8_t kAudioPin=11;
  static constexpr uint32_t kSampleRate=8000;
  bool begin();
  bool enter();
  void exit();
  bool active() const { return active_; }
  void update();
  void setBaud(float baud);
  float baud() const { return baud_; }
  float markHz() const { return markHz_; }
  float spaceHz() const { return spaceHz_; }
  bool signal() const { return signal_; }
  const char* text() const { return shownText_; }

private:
  struct AudioChunk { int16_t samples[64]; };
  static void captureTaskEntry(void* arg);
  static void workerTaskEntry(void* arg);
  void captureTask();
  void workerTask();
  void processSamples(const int16_t* samples,size_t count);
  bool estimateTones(const float* samples,float& low,float& high);

  QueueHandle_t queue_=nullptr;
  TaskHandle_t captureTask_=nullptr,workerTask_=nullptr;
  volatile bool active_=false;
  bool initialized_=false,signal_=false;
  float baud_=45.45f,markHz_=0,spaceHz_=0,toneLowHz_=0,toneHighHz_=0;
  float dc_=2048.0f;
  float frame_[512]{};
  size_t frameCount_=0;
  char shownText_[137]{};
  size_t textLength_=0;
  uint32_t lastUiMs_=0;
};
