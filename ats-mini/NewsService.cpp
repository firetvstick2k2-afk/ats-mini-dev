#include "NewsService.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <new>
namespace {
constexpr size_t MAX_ENTRY_BYTES=8192; // bounded per-article XML buffer
constexpr uint32_t TIMEOUT_MS=12000, REFRESH_MS=1800000;
constexpr int64_t MIN_EPOCH=1700000000LL;
String tagValue(const String& block,const char* tag) {
  String open=String("<")+tag;
  int a=block.indexOf(open);
  while(a>=0) {
    int boundary=a+open.length();
    if(boundary>=block.length() || block[boundary]=='>' || isspace((uint8_t)block[boundary])) break;
    a=block.indexOf(open,boundary);
  }
  if(a<0)return "";
  int end=block.indexOf('>',a); if(end<0)return "";
  String close=String("</")+tag+">";
  int b=block.indexOf(close,end+1);
  return b<0?String(""):block.substring(end+1,b);
}
void utf8(String& out,uint32_t c) {
  if(c<0x80)out+=(char)c;
  else if(c<0x800){out+=(char)(0xC0|(c>>6));out+=(char)(0x80|(c&63));}
  else if(c<0x10000&&!(c>=0xD800&&c<=0xDFFF)){out+=(char)(0xE0|(c>>12));out+=(char)(0x80|((c>>6)&63));out+=(char)(0x80|(c&63));}
  else if(c<=0x10FFFF){out+=(char)(0xF0|(c>>18));out+=(char)(0x80|((c>>12)&63));out+=(char)(0x80|((c>>6)&63));out+=(char)(0x80|(c&63));}
}
String entities(String s) {
  s.replace("&amp;","&");s.replace("&lt;","<");s.replace("&gt;",">");s.replace("&quot;","\"");s.replace("&apos;","'");s.replace("&#39;","'");
  s.replace("&nbsp;"," ");s.replace("&ndash;","-");s.replace("&mdash;","-");s.replace("&hellip;","...");s.replace("&euro;","EUR");
  for(uint8_t i=0;i<24;i++){int p=s.indexOf("&#");if(p<0)break;int e=s.indexOf(';',p);if(e<0)break;String t=s.substring(p+2,e);bool x=t.length()>1&&(t[0]=='x'||t[0]=='X');char* tail=nullptr;long c=strtol(x?t.c_str()+1:t.c_str(),&tail,x?16:10);if(tail&&!*tail&&c>=32&&c<=0x10FFFF){String r;utf8(r,(uint32_t)c);s.replace(s.substring(p,e+1),r);}else s.remove(p,e-p+1);}
  return s;
}
String clean(String s) {
  s=entities(s);
  s.replace("<![CDATA[","");s.replace("]]>","");
  String out;out.reserve(s.length());bool tag=false,space=false;
  for(size_t i=0;i<s.length();i++){char c=s[i];if(c=='<'){tag=true;space=true;continue;}if(c=='>'){tag=false;continue;}if(tag)continue;if(isspace((uint8_t)c)||(uint8_t)c<32){space=true;continue;}if(space&&out.length())out+=' ';space=false;out+=c;}
  out=entities(out);out.trim();while(out.indexOf("  ")>=0)out.replace("  "," ");return out;
}
int64_t parseDate(String s) {
  s.trim();struct tm t={};char* e=strptime(s.c_str(),"%a, %d %b %Y %H:%M:%S",&t);
  if(!e){memset(&t,0,sizeof(t));e=strptime(s.c_str(),"%d %b %Y %H:%M:%S",&t);}
  if(!e){memset(&t,0,sizeof(t));e=strptime(s.c_str(),"%Y-%m-%dT%H:%M:%S",&t);}
  if(!e)return 0;
  int y=t.tm_year+1900; unsigned m=(unsigned)t.tm_mon+1, d=(unsigned)t.tm_mday;
  y-=m<=2; const int era=(y>=0?y:y-399)/400; const unsigned yoe=(unsigned)(y-era*400);
  const unsigned mp=(unsigned)((int)m+(m>2?-3:9)); const unsigned doy=(153*mp+2)/5+d-1;
  const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
  const int64_t days=(int64_t)era*146097+(int64_t)doe-719468;
  return days*86400LL+(int64_t)t.tm_hour*3600LL+(int64_t)t.tm_min*60LL+t.tm_sec;
}
void copy(char* dst,size_t cap,const String& s){if(!cap)return;size_t n=min((size_t)s.length(),cap-1);while(n&&n<s.length()&&(((uint8_t)s[n]&0xC0)==0x80))--n;memcpy(dst,s.c_str(),n);dst[n]=0;}
String hostOf(const char* url){String s(url);int a=s.indexOf("://");a=a<0?0:a+3;int b=s.indexOf('/',a);if(b<0)b=s.length();s=s.substring(a,b);int c=s.indexOf(':');if(c>=0)s.remove(c);return s;}
void add(NewsService::Snapshot& s,String title,String source,String date,String body,uint8_t sourceIndex){
  title=clean(title);source=clean(source);body=clean(body);if(title.length()<3||s.count>=NewsService::MAX_ITEMS)return;
  int64_t epoch=parseDate(date);time_t now=time(nullptr);if(epoch>0&&now>MIN_EPOCH&&epoch<(int64_t)now-172800LL)return;
  for(uint8_t i=0;i<s.count;i++)if(!strcasecmp(s.items[i].title,title.c_str()))return;
  auto& v=s.items[s.count++];copy(v.title,sizeof(v.title),title);copy(v.source,sizeof(v.source),source);copy(v.date,sizeof(v.date),date);copy(v.text,sizeof(v.text),body.length()?body:title);v.publishedAt=epoch;v.sourceIndex=sourceIndex;
}
void sortItems(NewsService::Snapshot& s){for(uint8_t i=0;i<s.count;i++)for(uint8_t j=i+1;j<s.count;j++)if(s.items[j].publishedAt>s.items[i].publishedAt){auto t=s.items[i];s.items[i]=s.items[j];s.items[j]=t;}}
}
NewsService newsService;
bool NewsService::validFeedUrl(const char* u){if(!u)return false;size_t n=strlen(u);return n==0||(n<MAX_URL_BYTES&&(!strncmp(u,"https://",8)||!strncmp(u,"http://",7)));}
void NewsService::loadConfiguration(){Preferences p;if(!p.begin("news",true,"settings"))return;for(uint8_t i=0;i<MAX_FEEDS;i++){char k[16];snprintf(k,sizeof(k),"url%u",i);p.getString(k,"").toCharArray(configuration_.feeds[i].url,MAX_URL_BYTES);snprintf(k,sizeof(k),"on%u",i);configuration_.feeds[i].enabled=p.getBool(k,false)&&configuration_.feeds[i].url[0]&&validFeedUrl(configuration_.feeds[i].url);}p.end();configurationLoaded_=true;}
bool NewsService::begin(){
  if(!configMutex_)configMutex_=xSemaphoreCreateMutex();
  if(!snapshotMutex_)snapshotMutex_=xSemaphoreCreateMutex();
  if(!configMutex_||!snapshotMutex_)return false;
  if(!snapshot_){
    void* memory=heap_caps_malloc(sizeof(Snapshot),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!memory)memory=malloc(sizeof(Snapshot));
    if(!memory)return false;
    snapshot_=new(memory) Snapshot();
  }
  if(!configurationLoaded_)loadConfiguration();
  if(taskHandle_)return true;
  return xTaskCreatePinnedToCore(taskEntry,"NewsWorker",12288,this,1,&taskHandle_,0)==pdPASS;
}
NewsService::Configuration NewsService::configuration()const{Configuration c={};if(configMutex_){xSemaphoreTake(configMutex_,portMAX_DELAY);c=configuration_;xSemaphoreGive(configMutex_);}else c=configuration_;return c;}
bool NewsService::setConfiguration(const Configuration& c){for(uint8_t i=0;i<MAX_FEEDS;i++)if(!validFeedUrl(c.feeds[i].url))return false;Preferences p;if(!p.begin("news",false,"settings"))return false;for(uint8_t i=0;i<MAX_FEEDS;i++){char k[16];snprintf(k,sizeof(k),"url%u",i);if(p.putString(k,c.feeds[i].url)==0&&c.feeds[i].url[0]){p.end();return false;}snprintf(k,sizeof(k),"on%u",i);p.putBool(k,c.feeds[i].enabled&&c.feeds[i].url[0]);}p.end();if(configMutex_)xSemaphoreTake(configMutex_,portMAX_DELAY);configuration_=c;configurationLoaded_=true;if(configMutex_)xSemaphoreGive(configMutex_);requestRefresh();return true;}
bool NewsService::hasEnabledFeeds()const{Configuration c=configuration();for(uint8_t i=0;i<MAX_FEEDS;i++)if(c.feeds[i].enabled&&c.feeds[i].url[0])return true;return false;}

uint8_t NewsService::itemCount()const{if(!snapshotMutex_||!snapshot_)return 0;xSemaphoreTake(snapshotMutex_,portMAX_DELAY);uint8_t n=snapshot_->count;xSemaphoreGive(snapshotMutex_);return n;}
bool NewsService::isFetching()const{if(!snapshotMutex_||!snapshot_)return false;xSemaphoreTake(snapshotMutex_,portMAX_DELAY);bool v=snapshot_->fetching;xSemaphoreGive(snapshotMutex_);return v;}
bool NewsService::hasFailed()const{if(!snapshotMutex_)return false;xSemaphoreTake(snapshotMutex_,portMAX_DELAY);bool v=snapshot_->failed;xSemaphoreGive(snapshotMutex_);return v;}
bool NewsService::getItem(uint8_t i,Item& item)const{if(!snapshotMutex_)return false;xSemaphoreTake(snapshotMutex_,portMAX_DELAY);bool ok=i<snapshot_->count;if(ok)item=snapshot_->items[i];xSemaphoreGive(snapshotMutex_);return ok;}
void NewsService::requestRefresh(){refreshRequested_=true;}
void NewsService::update(bool wifi,bool active){wifiConnected_=wifi;active_=active;if(active&&(!lastAttemptMs_||millis()-lastAttemptMs_>=REFRESH_MS))requestRefresh();}
void NewsService::taskEntry(void* p){static_cast<NewsService*>(p)->taskLoop();}
void NewsService::taskLoop(){for(;;){if(refreshRequested_&&active_&&wifiConnected_){refreshRequested_=false;lastAttemptMs_=millis();if(snapshotMutex_){xSemaphoreTake(snapshotMutex_,portMAX_DELAY);snapshot_->fetching=true;snapshot_->failed=false;xSemaphoreGive(snapshotMutex_);}void* memory=heap_caps_malloc(sizeof(Snapshot),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!memory)memory=malloc(sizeof(Snapshot));Snapshot* fresh=memory?new(memory) Snapshot():nullptr;bool ok=fresh&&fetchFeeds(*fresh);if(fresh){fresh->fetching=false;fresh->failed=!ok;fresh->valid=fresh->count>0;fresh->updatedAtMs=millis();if(snapshotMutex_){xSemaphoreTake(snapshotMutex_,portMAX_DELAY);*snapshot_=*fresh;xSemaphoreGive(snapshotMutex_);}fresh->~Snapshot();heap_caps_free(fresh);}else if(snapshotMutex_){xSemaphoreTake(snapshotMutex_,portMAX_DELAY);snapshot_->fetching=false;snapshot_->failed=true;xSemaphoreGive(snapshotMutex_);}}vTaskDelay(pdMS_TO_TICKS(250));}}
bool NewsService::fetchFeeds(Snapshot& s){Configuration c=configuration();bool any=false,success=false;for(uint8_t i=0;i<MAX_FEEDS&&s.count<MAX_ITEMS;i++)if(c.feeds[i].enabled&&c.feeds[i].url[0]){any=true;success=fetchFeed(i,c.feeds[i],s)||success;}sortItems(s);return any&&success;}
bool NewsService::fetchFeed(uint8_t i,const Feed& f,Snapshot& s){
  Serial.printf("[NEWS] Feed %u start: %s\\n",(unsigned)(i+1),f.url);
  WiFiClientSecure client; client.setInsecure(); client.setTimeout(TIMEOUT_MS);
  HTTPClient http; http.setConnectTimeout(TIMEOUT_MS); http.setTimeout(TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); http.setRedirectLimit(5);
  http.setReuse(false); http.setUserAgent("Mozilla/5.0 (compatible; ATS-Mini-RSS/1.0)");
  if(!http.begin(client,f.url)){Serial.printf("[NEWS] Feed %u begin failed\\n",(unsigned)(i+1));return false;}
  http.addHeader("Accept","application/rss+xml, application/atom+xml, application/xml, text/xml, */*");
  int status=http.GET();
  if(status!=HTTP_CODE_OK){Serial.printf("[NEWS] Feed %u HTTP error: %d (%s)\\n",(unsigned)(i+1),status,http.errorToString(status).c_str());http.end();return false;}

  const String fallback=hostOf(f.url);
  String entry,probe;
  entry.reserve(MAX_ENTRY_BYTES);
  probe.reserve(8);
  bool collecting=false,oversize=false;
  uint8_t parsed=0;
  const uint8_t before=s.count;
  size_t received=0;
  uint32_t lastDataMs=millis();
  uint8_t yieldCounter=0;
  auto* stream=http.getStreamPtr();
  if(!stream){Serial.printf("[NEWS] Feed %u stream unavailable\\n",(unsigned)(i+1));http.end();return false;}

  while((stream->connected()||stream->available())&&
        (uint32_t)(millis()-lastDataMs)<TIMEOUT_MS&&s.count<MAX_ITEMS){
    if(!stream->available()){vTaskDelay(pdMS_TO_TICKS(1));continue;}
    int value=stream->read();
    if(value<0)continue;
    lastDataMs=millis();
    const char ch=(char)value;
    ++received;
    probe+=ch;
    if(probe.length()>8)probe.remove(0,1);

    if(!collecting){
      if(probe.endsWith("<item")||probe.endsWith("<entry")){
        collecting=true;
        oversize=false;
        entry=probe;
      }
    }else{
      if(!oversize){
        entry+=ch;
        if(entry.length()>MAX_ENTRY_BYTES){oversize=true;entry="";}
      }
      if(probe.endsWith("</item>")||probe.endsWith("</entry>")){
        if(!oversize){
          String title=tagValue(entry,"title");
          String encoded=tagValue(entry,"content:encoded"),desc=tagValue(entry,"description");
          if(!encoded.length())encoded=tagValue(entry,"content");
          if(!encoded.length())encoded=tagValue(entry,"summary");
          String body=encoded.length()>=desc.length()?encoded:desc;
          String date=tagValue(entry,"pubDate");
          if(!date.length())date=tagValue(entry,"published");
          if(!date.length())date=tagValue(entry,"updated");
          if(!date.length())date=tagValue(entry,"dc:date");
          String source=tagValue(entry,"source"); if(!source.length())source=fallback;
          if(clean(title).length()>=3)++parsed;
          add(s,title,source,date,body,i);
        }else{
          Serial.printf("[NEWS] Feed %u skipped article: over %u bytes\\n",(unsigned)(i+1),(unsigned)MAX_ENTRY_BYTES);
        }
        collecting=false;
        oversize=false;
        entry="";
      }
    }
    if((++yieldCounter&0xFF)==0)vTaskDelay(1);
  }
  http.end();
  Serial.printf("[NEWS] Feed %u streamed=%u bytes parsed=%u added=%u total=%u\\n",
    (unsigned)(i+1),(unsigned)received,(unsigned)parsed,(unsigned)(s.count-before),(unsigned)s.count);
  if(collecting)Serial.printf("[NEWS] Feed %u incomplete final article ignored\\n",(unsigned)(i+1));
  return parsed>0;
}
