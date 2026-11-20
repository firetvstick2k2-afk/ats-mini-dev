#include "RttyService.h"
#include "RttyDecoder.h"
#include <math.h>
#include <string.h>

namespace {
constexpr size_t kQueueDepth=96;
constexpr uint32_t kWorkerStack=8192;
constexpr int kFrameSamples=512;
constexpr float kMinToneHz=350.0f,kMaxToneHz=1850.0f,kToneStepHz=8.0f;
constexpr int kToneBins=static_cast<int>((kMaxToneHz-kMinToneHz)/kToneStepHz)+1;
float coeff[kToneBins];
bool coeffReady=false;

void initCoefficients() {
  if(coeffReady) return;
  for(int i=0;i<kToneBins;++i) {
    const float f=kMinToneHz+i*kToneStepHz;
    coeff[i]=2.0f*cosf(2.0f*static_cast<float>(M_PI)*f/AudioCaptureService::kSampleRate);
  }
  coeffReady=true;
}
}

bool RttyService::begin(AudioCaptureService& capture) {
  if(initialized_) return capture_==&capture;
#if defined(LILYGO_SI473X)
  (void)capture;
  return false;
#else
  queue_=xQueueCreate(kQueueDepth,sizeof(AudioChunk));
  if(!queue_) return false;
  capture_=&capture;
  initCoefficients();
  if(xTaskCreatePinnedToCore(taskEntry,"rtty-worker",kWorkerStack,this,2,&task_,1)!=pdPASS) {
    vQueueDelete(queue_); queue_=nullptr; capture_=nullptr; return false;
  }
  initialized_=true;
  return true;
#endif
}

bool RttyService::enter(AudioCaptureService& capture) {
  exit();
#if defined(LILYGO_SI473X)
  (void)capture; return false;
#else
  if(!capture.begin() || !begin(capture)) return false;
  xQueueReset(queue_);
  frameCount_=0; dc_=2048.0f; markHz_=spaceHz_=0; signal_=false;
  shownText_[0]=0; textLength_=0; baud_=45.45f;
  rtty_init(); rtty_set_baud(baud_); rtty_set_polarity(RTTY_POL_AUTO);
  active_=true;
  if(!capture_->subscribe(this,onAudioSamples)) { active_=false; return false; }
  return true;
#endif
}

void RttyService::exit() {
  active_=false;
  if(capture_) capture_->unsubscribe(this);
  if(queue_) xQueueReset(queue_);
  frameCount_=0; signal_=false; markHz_=spaceHz_=0;
  rtty_set_tones(0,0);
}

void RttyService::setBaud(float value) {
  if(value!=45.45f && value!=50.0f && value!=75.0f) return;
  baud_=value;
  rtty_set_baud(value);
}

void RttyService::onAudioSamples(void* context,const int16_t* samples,size_t count,int64_t utcStartMicros) {
  (void)utcStartMicros;
  auto* self=static_cast<RttyService*>(context);
  if(!self || !self->active_ || !self->queue_ || !samples ||
     count==0 || count>AudioCaptureService::kBlockSamples) return;
  AudioChunk chunk{};
  memcpy(chunk.samples,samples,count*sizeof(int16_t));
  chunk.count=static_cast<uint8_t>(count);
  if(xQueueSend(self->queue_,&chunk,0)!=pdTRUE) {
    // Drop the oldest audio block to keep decoder latency bounded.
    AudioChunk discard{};
    xQueueReceive(self->queue_,&discard,0);
    xQueueSend(self->queue_,&chunk,0);
  }
}

void RttyService::taskEntry(void* arg) { static_cast<RttyService*>(arg)->workerTask(); }

void RttyService::workerTask() {
  AudioChunk chunk{};
  for(;;) {
    if(xQueueReceive(queue_,&chunk,portMAX_DELAY)!=pdTRUE) continue;
    if(active_) processSamples(chunk.samples,chunk.count);
  }
}

bool RttyService::estimateTones(const float* samples,float& low,float& high) {
  float power[kToneBins]{};
  for(int b=0;b<kToneBins;++b) {
    const float c=coeff[b];
    float q0=0,q1=0,q2=0;
    for(int n=0;n<kFrameSamples;++n) {
      const float window=0.5f-0.5f*cosf(2.0f*static_cast<float>(M_PI)*n/(kFrameSamples-1));
      q0=samples[n]*window+c*q1-q2;
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
    const float balance=weaker/(stronger+1.0f);
    const float score=weaker*(0.55f+0.45f*balance);
    if(score>bestScore) { bestScore=score; bestA=peaks[a]; bestB=peaks[b]; }
  }
  if(bestA<0 || bestB<0 || bestScore<1.0e7f) return false;
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
      if(estimateTones(frame_,low,high)) {
        if(markHz_==0 || fabsf(low-markHz_)>20 || fabsf(high-spaceHz_)>20) {
          rtty_set_tones(low,high);
          markHz_=rtty_mark_hz(); spaceHz_=rtty_space_hz();
        }
        signal_=true;
      } else {
        signal_=false;
      }
      if(rtty_mark_hz()>0) {
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
  const size_t n=rtty_take_text(decoded,sizeof(decoded));
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
  signal_=signal_ || rtty_active();
}
