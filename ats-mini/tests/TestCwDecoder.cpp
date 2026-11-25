// Host-side regression tests for the ATS Mini CW decoder.
// Decoder algorithm and test cases are based on the MIT-licensed RESONIK
// 2.46-r5 CW decoder test suite. Preserve the upstream license notice.
#include "../src/cw/CwDecoder.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

static const char* morse(char c) {
  static const char* const letters[]={".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--.."};
  if(c>='A'&&c<='Z') return letters[c-'A'];
  return "";
}
static void emit(std::vector<int16_t>& out,bool on,float units,float wpm,float tone,float fs,float& phase) {
  const int n=(int)(units*1.2f/wpm*fs);
  for(int i=0;i<n;++i) {
    phase+=2.0f*3.14159265358979323846f*tone/fs;
    float v=2048.0f+(on?1200.0f*sinf(phase):0.0f);
    out.push_back((int16_t)v);
  }
}
static std::vector<int16_t> makeSignal(const char* msg,float wpm,float tone,float fs) {
  std::vector<int16_t> out; float phase=0;
  emit(out,false,10,wpm,tone,fs,phase);
  for(const char* p=msg;*p;++p) {
    if(*p==' ') { emit(out,false,4,wpm,tone,fs,phase); continue; }
    const char* code=morse(*p);
    for(const char* q=code;*q;++q) {
      emit(out,true,*q=='-'?3.0f:1.0f,wpm,tone,fs,phase);
      emit(out,false,1,wpm,tone,fs,phase);
    }
    emit(out,false,2,wpm,tone,fs,phase);
  }
  emit(out,false,20,wpm,tone,fs,phase);
  return out;
}
static std::string run(const std::vector<int16_t>& samples,float tone,float fs) {
  CwDecoder decoder; decoder.begin(fs,tone);
  for(size_t i=0;i<samples.size();i+=24) {
    const size_t count=samples.size()-i<24?samples.size()-i:24;
    decoder.push(&samples[i],(int)count);
  }
  return decoder.getText();
}
static bool endsWith(const std::string& value,const char* suffix) {
  while(!value.empty()&&value.back()==' ') value.pop_back();
  const std::string tail(suffix);
  return value.size()>=tail.size()&&value.compare(value.size()-tail.size(),tail.size(),tail)==0;
}
int main() {
  const char* message="TEST";
  const float tones[]={500.0f,700.0f,1000.0f};
  for(float tone:tones) {
    const auto samples=makeSignal(message,20.0f,tone,4000.0f);
    const std::string decoded=run(samples,tone,4000.0f);
    printf("CW 4 kHz / %.0f Hz: %s\n",tone,decoded.c_str());
    assert(endsWith(decoded,message));
  }
  const auto fast=makeSignal("CQ TEST",30.0f,700.0f,4000.0f);
  assert(endsWith(run(fast,700.0f,4000.0f),"CQ TEST"));
  puts("TestCwDecoder: all tests passed");
  return 0;
}
