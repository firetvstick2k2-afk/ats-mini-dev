#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// Lossy SPSC buffer: producers only copy samples; analysis runs in the UI tick.
class DecoderSpectrum {
public:
  static constexpr uint8_t kBins=32;
  static constexpr uint16_t kWindow=256;
  void setSampleRate(float rate) { sampleRate_=rate; }
  void reset() { head_=tail_=0; for(uint8_t i=0;i<kBins;++i) bins_[i]=0; }
  void push(float sample) {
    const uint16_t head=head_;
    const uint16_t next=static_cast<uint16_t>((head+1)&(kCapacity-1));
    if(next==tail_) return;
    samples_[head]=sample;
    head_=next;
  }
  bool update() {
    uint16_t tail=tail_;
    const uint16_t head=head_;
    if(static_cast<uint16_t>((head-tail)&(kCapacity-1))<kWindow) return false;
    float frame[kWindow];
    for(uint16_t i=0;i<kWindow;++i) { frame[i]=samples_[tail]; tail=static_cast<uint16_t>((tail+1)&(kCapacity-1)); }
    tail_=tail;
    constexpr float pi=3.14159265358979323846f;
    for(uint8_t b=0;b<kBins;++b) {
      const float frequency=250.0f+static_cast<float>(b)*100.0f;
      const float omega=2.0f*pi*frequency/sampleRate_;
      const float coeff=2.0f*cosf(omega);
      float q1=0,q2=0;
      for(uint16_t n=0;n<kWindow;++n) {
        const float window=0.5f-0.5f*cosf(2.0f*pi*n/(kWindow-1));
        const float q0=frame[n]*window+coeff*q1-q2;
        q2=q1; q1=q0;
      }
      const float power=q1*q1+q2*q2-coeff*q1*q2;
      const float magnitude=sqrtf(power>0?power:0);
      uint8_t height=static_cast<uint8_t>(magnitude*2.2f);
      if(height>9) height=9;
      if(height>bins_[b]) bins_[b]=height;
      else if(bins_[b]>0) --bins_[b];
    }
    return true;
  }
  const uint8_t* bins() const { return bins_; }
private:
  static constexpr uint16_t kCapacity=512;
  float sampleRate_=4000.0f;
  float samples_[kCapacity]{};
  volatile uint16_t head_=0,tail_=0;
  uint8_t bins_[kBins]{};
};
