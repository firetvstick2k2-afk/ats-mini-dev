#include "BandOpeningService.h"
#include "Menu.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>


namespace {
constexpr char kFeedUrl[] = "https://www.mapforham.com/hf-propagation/data/solar.json";
constexpr char kVhfApiBase[] = "https://skeds.sq3swg.com/api/v1/public/";
constexpr char kWsprUrl[] = "https://db1.wspr.live/?query=SELECT+band%2Ccount%28%29+AS+spots%2CuniqExact%28rx_sign%29+AS+receivers%2Cmax%28distance%29+AS+max_km+FROM+wspr.rx+WHERE+time+%3E+now%28%29+-+INTERVAL+6+HOUR+AND+band+IN+%28-1%2C0%29+AND+rx_lat+BETWEEN+35+AND+72+AND+rx_lon+BETWEEN+-15+AND+35+GROUP+BY+band+FORMAT+JSON";
constexpr uint32_t kRefreshIntervalMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kVhfRefreshIntervalMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kWsprRefreshIntervalMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kWsprMaximumAgeMs = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kVhfMaximumAgeMs = 6UL * 60UL * 60UL * 1000UL;
constexpr size_t kMaxVhfResponseBytes = 32768;
constexpr uint32_t kRetryIntervalMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kFreshAgeMs = 2UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kMaximumAgeMs = 24UL * 60UL * 60UL * 1000UL;
constexpr size_t kMaxResponseBytes = 16384;
// These are the exact receive windows in BandManager.cpp, represented in kHz.
// Direct band_detail values are used where available; SWL group estimates and
// the 11m interpolation are visibly marked with "~". LW/MW use WSPR LF/MF
// activity as clearly labelled propagation proxies.
const BandOpeningService::Band kBands[BandOpeningService::kBandCount] = {
  {"LW Band", "LF ~", 100, 495, 0},
  {"MW1 Band", "MF ~", 100, 1800, 0},
  {"MW2 Band", "MF ~", 495, 1701, 0},
  {"MW3 Band", "MF ~", 1700, 3500, 0},
  {"160m Band", "AFU", 1800, 2000, 160},
  {"90m Band", "SWL ~", 3200, 3400, 0},
  {"80m Band", "AFU", 3500, 4000, 80},
  {"75m Band", "SWL ~", 3900, 4000, 0},
  {"60m Band", "AFU", 4700, 5100, 60},
  {"49m Band", "SWL ~", 5900, 6200, 0},
  {"40m Band", "AFU", 7000, 7300, 40},
  {"41m Band", "SWL ~", 7200, 7500, 0},
  {"31m Band", "SWL ~", 9400, 9900, 0},
  {"30m Band", "AFU", 10000, 10200, 30},
  {"25m Band", "SWL ~", 11600, 12100, 0},
  {"22m Band", "SWL ~", 13500, 13900, 0},
  {"20m Band", "AFU", 14000, 14400, 20},
  {"19m Band", "SWL ~", 15100, 15900, 0},
  {"17m Band", "AFU", 18000, 18200, 17},
  {"16m Band", "SWL ~", 17400, 18100, 0},
  {"15m Band (BC)", "SWL ~", 18900, 19100, 0},
  {"15m Band (AFU)", "AFU", 21000, 21500, 15},
  {"13m Band", "SWL ~", 21500, 21900, 0},
  {"12m Band", "AFU", 24800, 25000, 12},
  {"11m Band", "CB ~", 25600, 26100, 0},
  {"10m Band", "AFU", 28000, 29700, 10},
  {"2m Band", "DX-AKT", 144000, 146000, 0},
  {"70cm Band", "DX-AKT", 430000, 440000, 0},
  {"UKW / FM", "2m ~", 64000, 108000, 0}
};


int64_t civilDays(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned mp = month > 2 ? month - 3 : month + 9;
  const unsigned doy = (153 * mp + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + doe - 719468;
}
bool timestampIsFresh(const char* value, bool* stale = nullptr) {
  int year=0, month=0, day=0, hour=0, minute=0, second=0;
  if (!value || sscanf(value,"%4d-%2d-%2dT%2d:%2d:%2d",&year,&month,&day,&hour,&minute,&second)!=6)
    return false;
  if (year < 2024 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) return false;
  const int64_t days=civilDays(year,static_cast<unsigned>(month),static_cast<unsigned>(day));
  int64_t epoch=days*86400LL+hour*3600LL+minute*60LL+second;
  const size_t length=strlen(value);
  if (length>=6) {
    const char* zone=value+length-6;
    if ((*zone=='+' || *zone=='-') && zone[3]==':') {
      const int offsetHours=(zone[1]-'0')*10+(zone[2]-'0');
      const int offsetMinutes=(zone[4]-'0')*10+(zone[5]-'0');
      const int offset=(offsetHours*60+offsetMinutes)*60;
      epoch += (*zone=='+') ? -offset : offset;
    }
  }
  const time_t now=time(nullptr);
  // Without a synchronized system clock, the feed timestamp cannot be aged reliably.
  if (now < 1700000000) { if (stale) *stale = true; return true; }
  const int64_t age=static_cast<int64_t>(now)-epoch;
  // The feed normally refreshes every ~5 minutes, but serves its last valid
  // upstream values during outages. Keep a 2-hour grace window so a temporary
  // source outage does not blank every band; older data is still rejected.
  if (stale) *stale = age >= 2*60*60;
  return age >= -300 && age <= 24*60*60;
}

BandOpeningService::Status parseStatus(JsonVariantConst value) {
  if (!value.is<const char*>()) return BandOpeningService::Status::Unknown;
  const char* text = value.as<const char*>();
  if (!text) return BandOpeningService::Status::Unknown;
  if (strcmp(text, "Poor") == 0) return BandOpeningService::Status::Poor;
  if (strcmp(text, "Fair") == 0) return BandOpeningService::Status::Fair;
  if (strcmp(text, "Good") == 0) return BandOpeningService::Status::Good;
  return BandOpeningService::Status::Unknown;
}

// Small, bounded JSON GET helper for the public SQ3SWG read-only API.
bool fetchJsonDocument(const char* url, JsonDocument& doc, const char* tag) {
  WiFiClientSecure client;
  client.setTimeout(8000);
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setTimeout(8000);
  http.setReuse(false);
  http.setUserAgent("ATS-Mini-OS/1.0 (+https://github.com/firetvstick2k2-afk/ats-mini-dev)");
  if (!http.begin(client, url)) {
    return false;
  }
  http.addHeader("Accept", "application/json");
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const int length = http.getSize();
  if (length > static_cast<int>(kMaxVhfResponseBytes)) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  if (body.isEmpty() || body.length() > kMaxVhfResponseBytes) {
    return false;
  }
  doc.clear();
  const DeserializationError error = deserializeJson(doc, body);
  if (error) {
    return false;
  }
  return true;
}
}

const BandOpeningService::Band& BandOpeningService::band(uint8_t index) {
  return kBands[index < kBandCount ? index : 0];
}
const char* BandOpeningService::statusText(Status status) {
  switch (status) {
    case Status::Poor: return "SCHWACH";
    case Status::Fair: return "MITTEL";
    case Status::Good: return "GUT";
    default: return "KEINE DATEN";
  }
}
bool BandOpeningService::begin() {
  if (taskHandle_) return true;
  Snapshot initial;
  initial.waitingForWifi = !wifiConnected_;
  publish(initial);
  const BaseType_t created = xTaskCreatePinnedToCore(taskEntry, "BandOpenWorker", 12288, this, 1, &taskHandle_, 0);
  return created == pdPASS;
}
void BandOpeningService::update(bool wifiConnected) {
  const bool wasConnected = wifiConnected_;
  wifiConnected_ = wifiConnected;
  Snapshot connectionState = snapshot();
  connectionState.waitingForWifi = !wifiConnected;
  publish(connectionState);
  // Opening the monitor (or regaining router access) must trigger a fetch
  // immediately instead of waiting for the previous five-minute cadence.
  if (wifiConnected && !wasConnected && (!lastAttemptMs_ || millis() - lastAttemptMs_ >= kRefreshIntervalMs)) {
    refreshRequested_ = true;
  }
  if (wifiConnected != wasConnected) {
  }

}
void BandOpeningService::requestRefresh() {
  const uint32_t now = millis();
  if (!lastAttemptMs_ || now - lastAttemptMs_ >= kRefreshIntervalMs) {
    refreshRequested_ = true;
  } else {
  }
}
BandOpeningService::Snapshot BandOpeningService::snapshot() const {
  Snapshot copy;
  portENTER_CRITICAL(&snapshotMux_); copy = snapshot_; portEXIT_CRITICAL(&snapshotMux_);
  bool ageStale = true;
  if (copy.valid && !timestampIsFresh(copy.sourceUpdated, &ageStale)) {
    copy.valid = false;
    for (uint8_t i = 0; i < kBandCount; ++i) {
      // LW/MW (WSPR) and VHF/UHF/FM have independent source freshness.
      if (i > 3 && i != 26 && i != 27 && i != 28) copy.rows[i] = {};
    }
  }
  if (copy.vhf2mUpdatedAtMs &&
      millis() - copy.vhf2mUpdatedAtMs >= kVhfMaximumAgeMs) {
    copy.rows[26] = {};
    copy.rows[28] = {};
  }
  if (copy.vhf70cmUpdatedAtMs &&
      millis() - copy.vhf70cmUpdatedAtMs >= kVhfMaximumAgeMs)
    copy.rows[27] = {};
  if (copy.wsprUpdatedAtMs &&
      millis() - copy.wsprUpdatedAtMs >= kWsprMaximumAgeMs) {
    for (uint8_t i = 0; i < 4; ++i) copy.rows[i] = {};
  }
  copy.stale = ageStale;
  return copy;
}
void BandOpeningService::publish(const Snapshot& value) {
  portENTER_CRITICAL(&snapshotMux_); snapshot_ = value; portEXIT_CRITICAL(&snapshotMux_);
}
void BandOpeningService::taskEntry(void* context) {
  static_cast<BandOpeningService*>(context)->taskLoop();
}
void BandOpeningService::taskLoop() {
  uint32_t lastMemoryReportMs = millis();
  for (;;) {
    const uint32_t now = millis();
    if ((uint32_t)(now - lastMemoryReportMs) >= 60000UL) {
      lastMemoryReportMs = now;
    }
    const bool due = refreshRequested_ || !lastAttemptMs_ || now - lastAttemptMs_ >= kRefreshIntervalMs;
    if (wifiConnected_ && (!lastWsprAttemptMs_ || now - lastWsprAttemptMs_ >= kWsprRefreshIntervalMs)) {
      lastWsprAttemptMs_ = now;
      Snapshot current = snapshot();
      if (fetchWsprConditions(current)) {
        publish(current);
      } else {
      }
    }
    if (wifiConnected_ && (!lastVhfAttemptMs_ || now - lastVhfAttemptMs_ >= kVhfRefreshIntervalMs)) {
      lastVhfAttemptMs_ = now;
      Snapshot current = snapshot();
      if (fetchVhfConditions(current)) {
        publish(current);
      } else {
      }
    }
    if (wifiConnected_ && due) {
      const bool wasRequested = refreshRequested_;
      refreshRequested_ = false;
      lastAttemptMs_ = now;
      Snapshot current = snapshot();
      current.fetching = true;
      current.fetchFailed = false;
      publish(current);
      Snapshot fresh;
      if (fetchConditions(fresh)) {
        fresh.valid = true;
        fresh.stale = false;
        fresh.fetching = false;
        fresh.waitingForWifi = false;
        fresh.fetchFailed = false;
        fresh.updatedAtMs = millis();
        // Preserve the independently refreshed FM-DX value when the HF feed
        // snapshot is replaced; the two endpoints have different cadences.
        const Snapshot latest = snapshot();
        // VHF activity has its own source and refresh cadence; preserve it
        // when the HF/solar snapshot is replaced.
        fresh.rows[26] = latest.rows[26];
        fresh.rows[27] = latest.rows[27];
        fresh.rows[28] = latest.rows[28];
        fresh.vhf2mUpdatedAtMs = latest.vhf2mUpdatedAtMs;
        fresh.vhf70cmUpdatedAtMs = latest.vhf70cmUpdatedAtMs;
        for (uint8_t i = 0; i < 4; ++i) fresh.rows[i] = latest.rows[i];
        fresh.wsprUpdatedAtMs = latest.wsprUpdatedAtMs;
        publish(fresh);
      } else {
        current = snapshot();
        current.fetching = false;
        current.fetchFailed = true;
        current.stale = true;
        publish(current);
        lastAttemptMs_ = millis() - kRefreshIntervalMs + kRetryIntervalMs;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

bool BandOpeningService::fetchVhfConditions(Snapshot& result) {
  using Status = BandOpeningService::Status;
  const uint32_t startedAt = millis();
  bool anyUpdated = false;
  float europeEsMufMHz = 0.0f;
  JsonDocument doc;

  const String propagationUrl = String(kVhfApiBase) + "propagation";
  const bool propagationOk = fetchJsonDocument(propagationUrl.c_str(), doc, "PROP");
  if (propagationOk) {
    europeEsMufMHz = doc["europe"]["highest"]["esMuf"] | 0.0f;
  }

  // The service asks for a six-hour spot window. A spot proves reported
  // activity, not that the whole band is closed when no spot is present.
  vTaskDelay(pdMS_TO_TICKS(1100));
  const String twoMeterUrl = String(kVhfApiBase) + "spots?band=144&hours=6&limit=30";
  const bool twoMeterOk = fetchJsonDocument(twoMeterUrl.c_str(), doc, "144");
  if (twoMeterOk || propagationOk) {
    size_t spots = twoMeterOk ? doc["spots"].size() : 0;
    Status status = Status::Unknown;
    if (europeEsMufMHz >= 144.0f || spots >= 3) status = Status::Good;
    else if (spots > 0) status = Status::Fair;
    // No spots and no 2m Es opening means unknown, never a false "closed".
    result.rows[26].day = status;
    result.rows[26].night = status;
    result.vhf2mUpdatedAtMs = millis();
    anyUpdated = true;
  } else {
  }

  // UKW/FM is intentionally a personal estimate: mirror the 2m status.
  // 2m Sporadic-E openings are a strong indicator for possible FM-DX, but
  // this is not a direct measurement of the 88-108 MHz broadcast band.
  result.rows[28] = result.rows[26];

  vTaskDelay(pdMS_TO_TICKS(1100));
  const String seventyCmUrl = String(kVhfApiBase) + "spots?band=432&hours=6&limit=30";
  const bool seventyCmOk = fetchJsonDocument(seventyCmUrl.c_str(), doc, "432");
  if (seventyCmOk) {
    const size_t spots = doc["spots"].size();
    Status status = Status::Unknown;
    if (spots >= 3) status = Status::Good;
    else if (spots > 0) status = Status::Fair;
    // The public feed has activity spots, not a reliable "band closed" flag.
    result.rows[27].day = status;
    result.rows[27].night = status;
    result.vhf70cmUpdatedAtMs = millis();
    anyUpdated = true;
  } else {
  }

  return anyUpdated;
}


bool BandOpeningService::fetchWsprConditions(Snapshot& result) {
  using Status = BandOpeningService::Status;
  const uint32_t startedAt = millis();
  WiFiClientSecure client;
  client.setTimeout(10000);
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setTimeout(10000);
  http.setReuse(false);
  http.setUserAgent("ATS-Mini-OS/1.0 (+https://github.com/firetvstick2k2-afk/ats-mini-dev)");
  if (!http.begin(client, kWsprUrl)) {
    return false;
  }
  http.addHeader("Accept", "application/json");
  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const int length = http.getSize();
  if (length > 8192) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  if (body.isEmpty() || body.length() > 8192) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, body);
  if (error) {
    return false;
  }
  JsonArrayConst rows = doc["data"].as<JsonArrayConst>();
  if (rows.isNull()) {
    return false;
  }

  bool foundLf = false;
  bool foundMf = false;
  Status lfStatus = Status::Unknown;
  Status mfStatus = Status::Unknown;
  uint32_t lfSpots = 0, mfSpots = 0;
  uint32_t lfReceivers = 0, mfReceivers = 0;
  uint32_t lfMaxKm = 0, mfMaxKm = 0;

  for (JsonObjectConst item : rows) {
    const int band = item["band"] | 99;
    const uint32_t spots = item["spots"] | 0U;
    const uint32_t receivers = item["receivers"] | 0U;
    const uint32_t maxKm = item["max_km"] | 0U;
    if (band == -1) {
      foundLf = true;
      lfSpots = spots; lfReceivers = receivers; lfMaxKm = maxKm;
      if (spots > 0) lfStatus = (receivers >= 3 && maxKm >= 500) ? Status::Good : Status::Fair;
    } else if (band == 0) {
      foundMf = true;
      mfSpots = spots; mfReceivers = receivers; mfMaxKm = maxKm;
      if (spots > 0) mfStatus = (receivers >= 3 && maxKm >= 500) ? Status::Good : Status::Fair;
    }
  }

  if (!foundLf && !foundMf) {
    return false;
  }

  // WSPR band -1 is LF (136 kHz); band 0 is MF (474.2 kHz).
  // These are propagation proxies, not direct LW/MW broadcast measurements.
  result.rows[0] = {};
  for (uint8_t i = 1; i <= 3; ++i) result.rows[i] = {};
  if (foundLf) {
    result.rows[0].day = lfStatus;
    result.rows[0].night = lfStatus;
  }
  if (foundMf) {
    for (uint8_t i = 1; i <= 3; ++i) {
      result.rows[i].day = mfStatus;
      result.rows[i].night = mfStatus;
    }
  }
  result.wsprUpdatedAtMs = millis();
  return true;
}

bool BandOpeningService::fetchConditions(Snapshot& result) {
  using Status = BandOpeningService::Status;
  const uint32_t startedAt = millis();
  WiFiClientSecure client;
  client.setTimeout(8000);
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setTimeout(8000);
  http.setReuse(false);
  http.setUserAgent("ATS-Mini-OS/1.0 (+https://github.com/firetvstick2k2-afk/Mini_OS)");
  const char* responseHeaders[] = {"Content-Type", "Server"};
  http.collectHeaders(responseHeaders, 2);
  if (!http.begin(client, kFeedUrl)) {
    return false;
  }
  http.addHeader("Accept", "application/json");
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const int length = http.getSize();
  if (length > static_cast<int>(kMaxResponseBytes)) {
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  if (body.isEmpty() || body.length() > kMaxResponseBytes) {
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, body);
  if (error) {
    return false;
  }
  const char* updated = doc["updated"] | "";
  bool sourceStale = true;
  if (!updated[0] || !timestampIsFresh(updated, &sourceStale)) {
    return false;
  }
  result.stale = sourceStale;
  strncpy(result.sourceUpdated, updated, sizeof(result.sourceUpdated) - 1);
  result.sourceUpdated[sizeof(result.sourceUpdated) - 1] = '\0';
  JsonArrayConst details = doc["bands_detail"].as<JsonArrayConst>();
  if (details.isNull() || details.size() == 0) {
    return false;
  }

  uint8_t matchedBands = 0;
  for (uint8_t i = 0; i < kBandCount; ++i) {
    result.rows[i] = {};
    const uint16_t meters = kBands[i].bandMeters;
    if (!meters) continue;
    bool foundBand = false;
    for (JsonObjectConst item : details) {
      if ((item["band_m"] | 0) == meters) {
        foundBand = true;
        result.rows[i].day = parseStatus(item["day"]);
        result.rows[i].night = parseStatus(item["night"]);
        if (result.rows[i].day != Status::Unknown || result.rows[i].night != Status::Unknown)
          ++matchedBands;
        break;
      }
    }
  }
  // Map For HAM also publishes four wider HF groups. Use those published
  // day/night estimates for the SWL broadcast bands that have no individual
  // band_detail entry. The UI marks these rows with "~" to distinguish them.
  const JsonArrayConst groups = doc["bands"].as<JsonArrayConst>();
  const auto groupStatus = [&groups](const char* groupName, bool day) -> Status {
    for (JsonObjectConst item : groups) {
      const char* name = item["name"] | "";
      if (strcmp(name, groupName) == 0)
        return parseStatus(day ? item["day"] : item["night"]);
    }
    return Status::Unknown;
  };
  const auto setSwlEstimate = [&result, &groupStatus](uint8_t row,
      const char* groupName) {
    result.rows[row].day = groupStatus(groupName, true);
    result.rows[row].night = groupStatus(groupName, false);
  };
  setSwlEstimate(5, "80m-40m");   // 90m
  setSwlEstimate(7, "80m-40m");   // 75m
  setSwlEstimate(9, "80m-40m");   // 49m
  setSwlEstimate(11, "80m-40m");  // 41m
  setSwlEstimate(12, "30m-20m"); // 31m
  setSwlEstimate(14, "30m-20m"); // 25m
  setSwlEstimate(15, "30m-20m"); // 22m
  setSwlEstimate(17, "17m-15m"); // 19m
  setSwlEstimate(19, "17m-15m"); // 16m
  setSwlEstimate(20, "17m-15m"); // 15m broadcast
  setSwlEstimate(22, "12m-10m"); // 13m


  // 11m CB is not supplied as a direct Map For HAM assessment.
  // Estimate it from the adjacent 10m and 12m amateur-band assessments.
  // If both neighbours agree, use that status; if they differ, stay cautious
  // and use Fair. If either neighbour is unknown, keep 11m unknown.
  const auto estimateBetween = [](Status lower, Status upper) -> Status {
    if (lower == Status::Unknown || upper == Status::Unknown)
      return Status::Unknown;
    return lower == upper ? lower : Status::Fair;
  };
  result.rows[24].day = estimateBetween(result.rows[25].day, result.rows[23].day);
  result.rows[24].night = estimateBetween(result.rows[25].night, result.rows[23].night);

  // At least one directly rated amateur band must have a usable assessment.
  // The derived 11m estimate must not make an otherwise empty feed valid.
  const bool any = matchedBands > 0;
  return any;
}


BandOpeningService bandOpeningService;
uint8_t bandOpeningPage = 0;

void bandOpeningUiOpen() {
  bandOpeningPage = 0;
  bandOpeningService.begin();
  bandOpeningService.update(getWiFiStatus() == 2);
  currentCmd = CMD_BAND_OPENING;
}
void bandOpeningUiExit() { currentCmd = CMD_NONE; }
void bandOpeningUiRotate(int16_t delta) {
  if (!delta) return;
  int page = static_cast<int>(bandOpeningPage) + delta;
  while (page < 0) page += BandOpeningService::kPageCount;
  bandOpeningPage = static_cast<uint8_t>(page % BandOpeningService::kPageCount);
}
void bandOpeningUiClick() { bandOpeningService.requestRefresh(); }
