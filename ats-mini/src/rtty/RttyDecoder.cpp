#include "RttyDecoder.h"
#include <math.h>
#include <string.h>

// Port of the RESONIK R5 RTTY approach (DigiDecoders.h, MIT):
// bit-long coherent tone filters, independent recent-peak normalisation,
// simultaneous normal/reversed framers, and framing-error-based polarity
// selection. Adapted to the ATS Mini's existing 8 kHz float input/API.

namespace {
constexpr int kTextMax = 64;
constexpr int kFilterMax = 400;
constexpr float kPi = 3.14159265358979323846f;

struct ToneFilter {
  float c=1, s=0, dc=1, ds=0;
  float re[kFilterMax]{}, im[kFilterMax]{};
  int n=132, pos=0;
  float sumRe=0, sumIm=0;

  void begin(float fs, float hz, int len) {
    if(len<8) len=8;
    if(len>kFilterMax) len=kFilterMax;
    n=len; pos=0; sumRe=sumIm=0;
    memset(re,0,sizeof(re)); memset(im,0,sizeof(im));
    setFreq(fs,hz);
    c=1; s=0;
  }
  void setFreq(float fs,float hz) {
    const float w=2.0f*kPi*hz/fs;
    dc=cosf(w); ds=sinf(w);
  }
  float push(float x) {
    const float r=x*c, i=-x*s;
    const float nc=c*dc-s*ds, ns=c*ds+s*dc;
    c=nc; s=ns;
    sumRe+=r-re[pos]; sumIm+=i-im[pos];
    re[pos]=r; im[pos]=i;
    if(++pos>=n) {
      pos=0;
      const float g=1.5f-0.5f*(c*c+s*s);
      c*=g; s*=g;
    }
    return sqrtf(sumRe*sumRe+sumIm*sumIm)/(float)n;
  }
};

struct Framer {
  float spb=176.0f, prev=1.0f, t=0.0f;
  int state=0, bits=0, k=0;
  void reset(float samplesPerBit) {
    spb=samplesPerBit; prev=1; t=0; state=0; bits=0; k=0;
  }
  // -1: no character; -2: framing error; >=0: five-bit ITA2 code
  int push(float d) {
    int result=-1;
    if(state==0) {
      if(prev>0.0f && d<=0.0f) { state=1; t=0.0f; }
    } else {
      t+=1.0f;
      const float at=((float)k+0.5f)*spb;
      if(t>=at) {
        const bool one=d>0.0f;
        if(k==0) {
          if(one) state=0;
          else { bits=0; k=1; }
        } else if(k<=5) {
          if(one) bits|=1<<(k-1);
          k++;
        } else {
          result=one?bits:-2;
          state=0; k=0;
        }
        if(state==0) k=0;
      }
    }
    prev=d;
    return result;
  }
};

ToneFilter tones[2]; // [0] low, [1] high
Framer framers[2];   // [0] low=mark; [1] high=mark
float fs=8000.0f, baud=45.45f, samplesPerBit=8000.0f/45.45f;
float lowHz=0.0f, highHz=0.0f;
float peak[2]={0,0};
float score[2]={0,0};
int selected=0;
RttyPolarity polarity=RTTY_POL_AUTO;
bool figures[2]={false,false};
char text[kTextMax+1]{};
size_t textLength=0;
uint32_t decodedCount=0;
uint32_t framingErrors=0;

void putChar(char c) {
  if(!c) return;
  if(textLength>=kTextMax) {
    memmove(text,text+1,kTextMax-1);
    textLength=kTextMax-1;
  }
  text[textLength++]=c;
}

void outputCode(int p,int code) {
  static const char letters[32]={
    0,'E','\n','A',' ','S','I','U','\r','D','R','J','N','F','C','K',
    'T','Z','L','W','H','Y','P','Q','O','B','G',0,'M','X','V',0
  };
  static const char figures[32]={
    0,'3','\n','-',' ','\'', '8','7','\r','$','4','\'',',','!',':','(',
    '5','+ ',')','2','#','6','0','1','9','?','&',0,'.','/','=',0
  };
  if(code==0x1B) { figures[p]=true; return; }
  if(code==0x1F) { figures[p]=false; return; }
  char c=(figures[p]?figures:letters)[code&31];
  if(code==0x04) figures[p]=false; // unshift on space
  if(c=='\r') return;
  putChar(c);
  if(c) decodedCount++;
}

void configure() {
  samplesPerBit=fs/baud;
  int length=(int)(samplesPerBit+0.5f);
  const float centre=(lowHz+highHz)*0.5f;
  const float shift=highHz-lowHz;
  tones[0].begin(fs,centre-shift*0.5f,length);
  tones[1].begin(fs,centre+shift*0.5f,length);
  framers[0].reset(samplesPerBit);
  framers[1].reset(samplesPerBit);
  peak[0]=peak[1]=0;
  score[0]=score[1]=0;
  selected=(polarity==RTTY_POL_REVERSE)?1:0;
  figures[0]=figures[1]=false;
}

} // namespace

void rtty_init() {
  lowHz=highHz=0;
  textLength=0; text[0]=0;
  decodedCount=framingErrors=0;
  peak[0]=peak[1]=score[0]=score[1]=0;
  framers[0].reset(samplesPerBit); framers[1].reset(samplesPerBit);
  figures[0]=figures[1]=false;
  selected=0;
}

void rtty_set_tones(float f1,float f2) {
  if(f1<=0 || f2<=0) {
    lowHz=highHz=0;
    peak[0]=peak[1]=score[0]=score[1]=0;
    framers[0].reset(samplesPerBit); framers[1].reset(samplesPerBit);
    return;
  }
  const float lo=fminf(f1,f2), hi=fmaxf(f1,f2);
  if(lowHz>0 && fabsf(lo-lowHz)<1.0f && fabsf(hi-highHz)<1.0f) return;
  lowHz=lo; highHz=hi;
  configure();
}

void rtty_set_baud(float value) {
  if(value<30.0f || value>300.0f || fabsf(value-baud)<0.001f) return;
  baud=value;
  if(lowHz>0) configure();
  else samplesPerBit=fs/baud;
}

void rtty_set_polarity(RttyPolarity value) {
  if(value>RTTY_POL_REVERSE) return;
  polarity=value;
  if(value==RTTY_POL_NORMAL) selected=0;
  else if(value==RTTY_POL_REVERSE) selected=1;
}

float rtty_mark_hz() {
  if(lowHz<=0) return 0;
  return selected==0?lowHz:highHz;
}
float rtty_space_hz() {
  if(lowHz<=0) return 0;
  return selected==0?highHz:lowHz;
}
float rtty_baud() { return baud; }
bool rtty_active() { return lowHz>0 && (decodedCount>0 || score[0]>0 || score[1]>0); }

void rtty_process(const float *x,int n) {
  if(!x || n<=0 || lowHz<=0) return;
  for(int i=0;i<n;i++) {
    const float low=tones[0].push(x[i]);
    const float high=tones[1].push(x[i]);
    peak[0]=low>peak[0]?low:peak[0]*0.9995f;
    peak[1]=high>peak[1]?high:peak[1]*0.9995f;
    const float d=low/(peak[0]+1e-3f)-high/(peak[1]+1e-3f);
    for(int p=0;p<2;p++) {
      if(polarity==RTTY_POL_NORMAL && p==1) continue;
      if(polarity==RTTY_POL_REVERSE && p==0) continue;
      const int code=framers[p].push(p?-d:d);
      if(code!=-1) score[p]*=0.97f;
      if(code==-2) { score[p]-=1.0f; framingErrors++; }
      else if(code>=0) {
        score[p]+=1.0f;
        if(polarity==RTTY_POL_AUTO) {
          if(score[p]>score[1-p]+3.0f) selected=p;
        } else selected=(polarity==RTTY_POL_REVERSE)?1:0;
        if(p==selected) outputCode(p,code);
      }
    }
  }
}

size_t rtty_take_text(char *buf,size_t size) {
  if(!buf || size==0) return 0;
  const size_t count=textLength<size-1?textLength:size-1;
  memcpy(buf,text,count);
  buf[count]=0;
  if(count<textLength) {
    memmove(text,text+count,textLength-count);
    textLength-=count;
  } else textLength=0;
  return count;
}
