#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
class NewsService {
public:
  static constexpr uint8_t MAX_FEEDS=4, MAX_ITEMS=40, MAX_ITEMS_PER_FEED=10;
  static constexpr size_t MAX_URL_BYTES=192, TITLE_BYTES=144, SOURCE_BYTES=48, DATE_BYTES=32, TEXT_BYTES=1200;
  struct Feed { char url[MAX_URL_BYTES]={}; bool enabled=false; };
  struct Configuration { Feed feeds[MAX_FEEDS]={}; };
  struct Item { char title[TITLE_BYTES]={}; char source[SOURCE_BYTES]={}; char date[DATE_BYTES]={}; char text[TEXT_BYTES]={}; int64_t publishedAt=0; uint8_t sourceIndex=0; };
  struct Snapshot { Item items[MAX_ITEMS]={}; uint8_t count=0; uint32_t updatedAtMs=0; bool valid=false; bool fetching=false; bool failed=false; };
  bool begin(); void update(bool wifiConnected,bool active); void requestRefresh();
  Configuration configuration() const; bool setConfiguration(const Configuration& value);
  bool hasEnabledFeeds() const;
  uint8_t itemCount() const; bool isFetching() const; bool hasFailed() const; bool getItem(uint8_t index, Item& item) const;
  static bool validFeedUrl(const char* url);
private:
  static void taskEntry(void* context); void taskLoop(); void loadConfiguration();
  bool fetchFeeds(Snapshot& result); bool fetchFeed(uint8_t index,const Feed& feed,Snapshot& result);
  Configuration configuration_={}; Snapshot* snapshot_=nullptr;
  mutable SemaphoreHandle_t configMutex_=nullptr,snapshotMutex_=nullptr;
  TaskHandle_t taskHandle_=nullptr;
  volatile bool wifiConnected_=false,active_=false,refreshRequested_=false;
  bool configurationLoaded_=false; uint32_t lastAttemptMs_=0;
};
extern NewsService newsService;
extern uint8_t newsCurrentIndex;
extern uint8_t newsCurrentPage;
