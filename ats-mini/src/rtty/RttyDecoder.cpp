#include "RttyDecoder.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

// Port of the RESONIK R5 RTTY approach (DigiDecoders.h, MIT):
// bit-long coherent tone filters, independent recent-peak normalisation,
// simultaneous normal/reversed framers, and framing-error-based polarity
// selection. Adapted to the ATS Mini's existing 8 kHz float input/API.

namespace {
constexpr int kTextMax=64, kFilterMax=400;
constexpr float kPi=3.14159265358979323846f;
struct ToneFilter {
  float c=1,s=0,dc=1,ds=0,re[kFilterMax]{},im[kFilterMax]{};
  int n=132,pos=0; float sumRe=0,sumIm=0;
  void begin(float fs,float hz,int len) {
    if(len<8) len=8; if(len>kFilterMax) len=kFilterMax;
    n=len; pos=0; sumRe=sumIm=0;
    memset(re,0,sizeof(re)); memset(im,0,sizeof(im));
    setFreq(fs,hz); c=1; s=0;
  }
  void setFreq(float fs,float hz) { float w=2*kPi*hz/fs; dc=cosf(w); ds=sinf(w); }
  float push(float x) {
    float r=x*c,i=-x*s,nc=c*dc-s*ds,ns=c*ds+s*dc; c=nc; s=ns;
    sumRe+=r-re[pos]; sumIm+=i-im[pos]; re[pos]=r; im[pos]=i;
    if(++pos>=n) { pos=0; float g=1.5f-0.5f*(c*c+s*s); c*=g; s*=g; }
    return sqrtf(sumRe*sumRe+sumIm*sumIm)/(float)n;
  }
};
struct Framer {
  float spb=176,prev=1,t=0; int state=0,bits=0,k=0;
  void reset(float v) { spb=v; prev=1; t=0; state=bits=k=0; }
  int push(float d) {
    int r=-1;
    if(!state) { if(prev>0 && d<=0) { state=1; t=0; } }
    else {
      t+=1; if(t>=((float)k+0.5f)*spb) {
        bool one=d>0;
        if(k==0) { if(one) state=0; else { bits=0; k=1; } }
        else if(k<=5) { if(one) bits|=1<<(k-1); k++; }
        else { r=one?bits:-2; state=0; k=0; }
        if(!state) k=0;
      }
    }
    prev=d; return r;
  }
};
ToneFilter tones[2]; Framer framers[2];
float baud=45.45f,spb=8000.0f/45.45f,loHz=0,hiHz=0,peak[2]{},score[2]{};
int selected=0; RttyPolarity polarity=RTTY_POL_AUTO; bool figs[2]{};
char text[65]{}; size_t textLen=0;
void put(char c) {
  if(!c) return;
  if(textLen>=64) { memmove(text,text+1,63); textLen=63; }
  text[textLen++]=c;
}
void emit(int p,int code) {
  static const char ltrs[32]={0,'E','\n','A',' ','S','I','U','\r','D','R','J','N','F','C','K','T','Z','L','W','H','Y','P','Q','O','B','G',0,'M','X','V',0};
  static const char fig[32]={0,'3','\n','-',' ','\'','8','7','\r','$','4','\'',',','!',':','(','5','+',')','2','#','6','0','1','9','?','&',0,'.','/','=',0};
  if(code==0x1B) { figs[p]=true; return; }
  if(code==0x1F) { figs[p]=false; return; }
  char c=(figs[p]?fig:ltrs)[code&31];
  if(code==0x04) figs[p]=false;
  if(c=='\r') return;
  put(c);
}
void configure() {
  spb=8000.0f/baud; int len=(int)(spb+0.5f);
  float centre=(loHz+hiHz)*0.5f,shift=hiHz-loHz;
  tones[0].begin(8000,centre-shift*0.5f,len);
  tones[1].begin(8000,centre+shift*0.5f,len);
  framers[0].reset(spb); framers[1].reset(spb);
  peak[0]=peak[1]=score[0]=score[1]=0;
  selected=polarity==RTTY_POL_REVERSE?1:0; figs[0]=figs[1]=false;
}
}
void rtty_init() {
  loHz=hiHz=0; textLen=0; text[0]=0;
  peak[0]=peak[1]=score[0]=score[1]=0;
  framers[0].reset(spb); framers[1].reset(spb);
  figs[0]=figs[1]=false; selected=0;
}
void rtty_set_tones(float a,float b) {
  if(a<=0 || b<=0) { loHz=hiHz=0; peak[0]=peak[1]=score[0]=score[1]=0; return; }
  float l=fminf(a,b),h=fmaxf(a,b);
  if(loHz>0 && fabsf(l-loHz)<1 && fabsf(h-hiHz)<1) return;
  loHz=l; hiHz=h; configure();
}
void rtty_set_baud(float b) {
  if(b<30 || b>300 || fabsf(b-baud)<0.001f) return;
  baud=b; if(loHz>0) configure(); else spb=8000.0f/baud;
}
void rtty_set_polarity(RttyPolarity p) {
  if(p>RTTY_POL_REVERSE) return; polarity=p;
  if(p==RTTY_POL_NORMAL) selected=0; else if(p==RTTY_POL_REVERSE) selected=1;
}
float rtty_mark_hz() { return loHz<=0?0:(selected==0?loHz:hiHz); }
float rtty_space_hz() { return loHz<=0?0:(selected==0?hiHz:loHz); }
float rtty_baud() { return baud; }
bool rtty_active() { return loHz>0 && (score[0]>0 || score[1]>0); }
void rtty_process(const float*x,int n) {
  if(!x || n<=0 || loHz<=0) return;
  for(int i=0;i<n;i++) {
    float a=tones[0].push(x[i]),b=tones[1].push(x[i]);
    peak[0]=a>peak[0]?a:peak[0]*0.9995f; peak[1]=b>peak[1]?b:peak[1]*0.9995f;
    float d=a/(peak[0]+1e-3f)-b/(peak[1]+1e-3f);
    for(int p=0;p<2;p++) {
      if((polarity==RTTY_POL_NORMAL && p==1)||(polarity==RTTY_POL_REVERSE && p==0)) continue;
      int code=framers[p].push(p?-d:d);
      if(code!=-1) score[p]*=0.97f;
      if(code==-2) score[p]-=1;
      else if(code>=0) {
        score[p]+=1;
        if(polarity==RTTY_POL_AUTO) { if(score[p]>score[1-p]+3) selected=p; }
        else selected=polarity==RTTY_POL_REVERSE?1:0;
        if(p==selected) emit(p,code);
      }
    }
  }
}
size_t rtty_take_text(char*buf,size_t size) {
  if(!buf || !size) return 0;
  size_t n=textLen<size-1?textLen:size-1;
  memcpy(buf,text,n); buf[n]=0;
  if(n<textLen) { memmove(text,text+n,textLen-n); textLen-=n; } else textLen=0;
  return n;
}
