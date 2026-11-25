#include "RttyService.h"
#include "RttyDecoder.h"
#include <esp_timer.h>
#include <math.h>
#include <string.h>

namespace {
constexpr size_t kQueueDepth=24;
constexpr uint32_t kWorkerStack=8192;
constexpr uint32_t kCaptureStack=4096;
constexpr int kFrameSamples=1024;
constexpr float kMinToneHz=300.0f,kMaxToneHz=3500.0f,kToneStepHz=12.0f;
constexpr int kToneBins=static_cast<int>((kMaxToneHz-kMinToneHz)/kToneStepHz)+1;
float coeff[kToneBins],hann[kFrameSamples];
bool coeffReady=false;

void initCoefficients() {
  if(coeffReady) return;
  for(int n=0;n<kFrameSamples;++n)
    hann[n]=0.5f-0.5f*cosf(2.0f*static_cast<float>(M_PI)*n/(kFrameSamples-1));
  for(int i=0;i<kToneBins;++i) {
    const float f=kMinToneHz+i*kToneStepHz;
    coeff[i]=2.0f*cosf(2.0f*static_cast<float>(M_PI)*f/RttyService::kSampleRate);
  }
  coeffReady=true;
}
}

bool RttyService::begin() {
  if(initialized_) return true;
#if defined(LILYGO_SI473X)
  return false;
#else
  queue_=xQueueCreate(kQueueDepth,sizeof(AudioChunk));
  engineMutex_=xSemaphoreCreateMutex();
  if(!queue_ || !engineMutex_) { if(queue_) vQueueDelete(queue_); if(engineMutex_) vSemaphoreDelete(engineMutex_); queue_=nullptr; engineMutex_=nullptr; return false; }
  initCoefficients();
  if(xTaskCreatePinnedToCore(captureTaskEntry,"rtty-capture",kCaptureStack,this,3,&captureTask_,0)!=pdPASS) {
    vQueueDelete(queue_); vSemaphoreDelete(engineMutex_); queue_=nullptr; engineMutex_=nullptr; return false;
  }
  if(xTaskCreatePinnedToCore(workerTaskEntry,"rtty-worker",kWorkerStack,this,2,&workerTask_,1)!=pdPASS) {
    vTaskDelete(captureTask_); captureTask_=nullptr;
    vQueueDelete(queue_); vSemaphoreDelete(engineMutex_); queue_=nullptr; engineMutex_=nullptr; return false;
  }
  initialized_=true;
  return true;
#endif
}

bool RttyService::enter() {
  exit();
  if(!begin()) return false;
  pinMode(kAudioPin,INPUT);
  xQueueReset(queue_);
  frameCount_=0; dc_=2048.0f;
  markHz_=spaceHz_=toneLowHz_=toneHighHz_=0;
  signal_=false; signalHoldFrames_=0; shownText_[0]=0; textLength_=0; baud_=45.45f;
  polarity_=RTTY_POL_AUTO;
  xSemaphoreTake(engineMutex_,portMAX_DELAY);
  rtty_init(); rtty_set_baud(baud_); rtty_set_polarity(static_cast<RttyPolarity>(polarity_));
  xSemaphoreGive(engineMutex_);
  active_=true;
  return true;
}

void RttyService::exit() {
  active_=false;
  if(queue_) xQueueReset(queue_);
  if(engineMutex_) {
    xSemaphoreTake(engineMutex_,portMAX_DELAY);
    frameCount_=0; signal_=false; signalHoldFrames_=0;
    markHz_=spaceHz_=toneLowHz_=toneHighHz_=0;
    rtty_set_tones(0,0);
    xSemaphoreGive(engineMutex_);
  } else {
    frameCount_=0; signal_=false; signalHoldFrames_=0;
    markHz_=spaceHz_=toneLowHz_=toneHighHz_=0;
  }
}

void RttyService::setBaud(float value) {
  if(value!=45.45f && value!=50.0f && value!=75.0f) return;
  baud_=value;
  if(engineMutex_) { xSemaphoreTake(engineMutex_,portMAX_DELAY); rtty_set_baud(value); xSemaphoreGive(engineMutex_); }
}

void RttyService::setPolarity(uint8_t value) {
  if(value>RTTY_POL_REVERSE) return;
  polarity_=value;
  if(engineMutex_) {
    xSemaphoreTake(engineMutex_,portMAX_DELAY);
    rtty_set_polarity(static_cast<RttyPolarity>(polarity_));
    markHz_=rtty_mark_hz(); spaceHz_=rtty_space_hz();
    xSemaphoreGive(engineMutex_);
  }
}

void RttyService::clearText() {
  shownText_[0]=0;
  textLength_=0;
  if(engineMutex_) {
    char discard[65];
    xSemaphoreTake(engineMutex_,portMAX_DELAY);
    while(rtty_take_text(discard,sizeof(discard))>0) {}
    xSemaphoreGive(engineMutex_);
  }
}

void RttyService::resetDecoder() {
  if(!engineMutex_) return;
  xSemaphoreTake(engineMutex_,portMAX_DELAY);
  rtty_init();
  rtty_set_baud(baud_);
  rtty_set_polarity(static_cast<RttyPolarity>(polarity_));
  if(toneLowHz_>0 && toneHighHz_>0) rtty_set_tones(toneLowHz_,toneHighHz_);
  frameCount_=0;
  signal_=false; signalHoldFrames_=0;
  markHz_=spaceHz_=0;
  xSemaphoreGive(engineMutex_);
  clearText();
}

void RttyService::captureTaskEntry(void* arg) { static_cast<RttyService*>(arg)->captureTask(); }
void RttyService::workerTaskEntry(void* arg) { static_cast<RttyService*>(arg)->workerTask(); }

void RttyService::captureTask() {
  AudioChunk chunk{};
  size_t count=0;
  int64_t base=0,next=0;
  uint64_t index=0;
  for(;;) {
    if(!active_) {
      count=0; base=next=0; index=0;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if(!base) { base=next=esp_timer_get_time(); index=0; }
    while(esp_timer_get_time()<next) taskYIELD();
    ++index;
    next=base+static_cast<int64_t>(index*1000000ULL/kSampleRate);
    int value=analogRead(kAudioPin);
    if(value<0) value=2048;
    chunk.samples[count++]=static_cast<int16_t>(value);
    if(count==64) {
      if(xQueueSend(queue_,&chunk,0)!=pdTRUE) {
        AudioChunk discard{};
        xQueueReceive(queue_,&discard,0);
        xQueueSend(queue_,&chunk,0);
      }
      count=0;
    }
    if(index%(kSampleRate/2)==0) { vTaskDelay(1); base=next=esp_timer_get_time(); index=0; }
    if(esp_timer_get_time()-next>10000) { base=next=esp_timer_get_time(); index=0; }
  }
}

void RttyService::workerTask() {
  AudioChunk chunk{};
  for(;;) {
    if(xQueueReceive(queue_,&chunk,portMAX_DELAY)!=pdTRUE) continue;
    if(active_ && engineMutex_) { xSemaphoreTake(engineMutex_,portMAX_DELAY); if(active_) processSamples(chunk.samples,64); xSemaphoreGive(engineMutex_); }
  }
}

bool RttyService::estimateTones(const float* samples,float& low,float& high) {
  float power[kToneBins]{};
  for(int b=0;b<kToneBins;++b) {
    const float c=coeff[b];
    float q0=0,q1=0,q2=0;
    for(int n=0;n<kFrameSamples;++n) {
      q0=samples[n]*hann[n]+c*q1-q2;
      q2=q1; q1=q0;
    }
    power[b]=q1*q1+q2*q2-c*q1*q2;
  }
  int peaks[16]; float peakPower[16]; int peakCount=0;
  for(int i=2;i<kToneBins-2;++i) {
    if(power[i]<=power[i-1] || power[i]<power[i+1]) continue;
    if(peakCount<16) { peaks[peakCount]=i; peakPower[peakCount++]=power[i]; }
    else {
      int weakest=0;
      for(int j=1;j<16;++j) if(peakPower[j]<peakPower[weakest]) weakest=j;
      if(power[i]>peakPower[weakest]) { peaks[weakest]=i; peakPower[weakest]=power[i]; }
    }
  }
  float bestScore=0; int bestA=-1,bestB=-1;
  for(int a=0;a<peakCount;++a) for(int b=a+1;b<peakCount;++b) {
    const float shift=(peaks[b]-peaks[a])*kToneStepHz;
    if(shift<80.0f || shift>1000.0f) continue;
    const float weaker=fminf(peakPower[a],peakPower[b]);
    const float stronger=fmaxf(peakPower[a],peakPower[b]);
    const float score=weaker*(0.55f+0.45f*weaker/(stronger+1.0f));
    if(score>bestScore) { bestScore=score; bestA=peaks[a]; bestB=peaks[b]; }
  }
  if(bestA<0 || bestB<0 || bestScore<500.0f) return false;
  low=kMinToneHz+bestA*kToneStepHz;
  high=kMinToneHz+bestB*kToneStepHz;
  return true;
}

void RttyService::processSamples(const int16_t* samples,size_t count) {
  if(!active_ || !samples) return;
  for(size_t i=0;i<count;++i) {
    const float raw=static_cast<float>(samples[i]);
    dc_+=(raw-dc_)*0.001f;
    frame_[frameCount_++]=(raw-dc_)/2048.0f;
    if(frameCount_==kFrameSamples) {
      float low=0,high=0;
      const bool found=estimateTones(frame_,low,high);
      if(found) {
        if(toneLowHz_==0 || fabsf(low-toneLowHz_)>24 || fabsf(high-toneHighHz_)>24) {
          rtty_set_tones(low,high);
          toneLowHz_=low; toneHighHz_=high;
        }
        signalHoldFrames_=8; // RESONIK keeps decoding through short fades (~1 s)
      } else if(signalHoldFrames_>0) {
        --signalHoldFrames_;
      }
      signal_=signalHoldFrames_>0;
      if(signal_ && rtty_mark_hz()>0) {
        rtty_process(frame_,kFrameSamples);
        markHz_=rtty_mark_hz(); spaceHz_=rtty_space_hz();
      }
      frameCount_=0;
    }
  }
}

void RttyService::update() {
  if(!active_ || millis()-lastUiMs_<100) return;
  lastUiMs_=millis();
  char decoded[65]{};
  size_t n=0;
  if(engineMutex_) { xSemaphoreTake(engineMutex_,portMAX_DELAY); n=rtty_take_text(decoded,sizeof(decoded)); signal_=signal_ || rtty_active(); xSemaphoreGive(engineMutex_); }
  if(n) {
    for(size_t i=0;i<n;++i) {
      if(decoded[i]=='\r') continue;
      if(textLength_>=sizeof(shownText_)-1) {
        memmove(shownText_,shownText_+34,sizeof(shownText_)-35);
        textLength_=sizeof(shownText_)-35;
      }
      shownText_[textLength_++]=decoded[i];
    }
    shownText_[textLength_]=0;
  }
}
