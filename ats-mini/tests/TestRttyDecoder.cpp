#include "../src/rtty/RttyDecoder.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

namespace {
constexpr float kFs=8000.0f, kBaud=45.45f, kLow=1200.0f, kHigh=1370.0f;
constexpr float kPi=3.14159265358979323846f;

void appendTone(std::vector<float>& out, int samples, float hz, float& phase) {
  for(int i=0;i<samples;++i) {
    out.push_back(0.8f*sinf(phase));
    phase += 2.0f*kPi*hz/kFs;
    if(phase>2.0f*kPi) phase-=2.0f*kPi;
  }
}
void appendBit(std::vector<float>& out, float units, bool mark, bool reversed,
               float& phase, int& totalSamples) {
  int end=(int)lroundf((totalSamples/kFs+units/kBaud)*kFs);
  int count=end-totalSamples;
  const bool lowIsMark=!reversed;
  const bool useLow=(mark==lowIsMark);
  appendTone(out,count,useLow?kLow:kHigh,phase);
  totalSamples=end;
}
std::string decode(bool reversed) {
  std::vector<float> samples;
  float phase=0; int total=0;
  for(int i=0;i<3;++i) appendBit(samples,1.0f,true,reversed,phase,total);
  const int codes[]={16,1,5,16}; // TEST in ITA2
  for(int code:codes) {
    appendBit(samples,1.0f,false,reversed,phase,total); // start
    for(int bit=0;bit<5;++bit)
      appendBit(samples,1.0f,(code&(1<<bit))!=0,reversed,phase,total);
    appendBit(samples,1.5f,true,reversed,phase,total); // stop
  }
  rtty_init();
  rtty_set_baud(kBaud);
  rtty_set_polarity(RTTY_POL_AUTO);
  rtty_set_tones(kLow,kHigh);
  for(size_t i=0;i<samples.size();i+=64) {
    float block[64]{};
    size_t n=samples.size()-i<64?samples.size()-i:64;
    for(size_t j=0;j<n;++j) block[j]=samples[i+j];
    rtty_process(block,(int)n);
  }
  char out[65]{};
  rtty_take_text(out,sizeof(out));
  return out;
}
}
int main() {
  const std::string normal=decode(false);
  const std::string reverse=decode(true);
  printf("normal: %s\nreverse: %s\n",normal.c_str(),reverse.c_str());
  assert(normal.find("TEST")!=std::string::npos);
  assert(reverse.find("TEST")!=std::string::npos);
  return 0;
}
