#include "BandOpeningService.h"
#include "../../DebugLog.h"
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
void bandLogf(const char* format, ...) {
  char line[320];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  size_t length = strlen(line);
  while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
    line[--length] = '\0';
  atsDebugLogf("%s", line);
}
constexpr char kFeedUrl[] = "https://www.mapforham.com/hf-propagation/data/solar.json";
constexpr char kFmMufUrl[] = "https://fmdx.org/includes/tools/get_muf.php";
constexpr char kVhfApiBase[] = "https://skeds.sq3swg.com/api/v1/public/";
constexpr uint32_t kRefreshIntervalMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kVhfRefreshIntervalMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kVhfMaximumAgeMs = 6UL * 60UL * 60UL * 1000UL;
constexpr size_t kMaxVhfResponseBytes = 32768;
constexpr uint32_t kRetryIntervalMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kFreshAgeMs = 2UL * 60UL * 60UL * 1000UL;
constexpr uint32_t kMaximumAgeMs = 24UL * 60UL * 60UL * 1000UL;
constexpr size_t kMaxResponseBytes = 16384;
// These are the exact receive windows in BandManager.cpp, represented in kHz.
// Rows without a matching Map For HAM band_detail entry intentionally remain grey.
const BandOpeningService::Band kBands[BandOpeningService::kBandCount] = {
  {"LW Band", "SWL", 100, 495, 0},
  {"MW1 Band", "SWL", 100, 1800, 0},
  {"MW2 Band", "SWL", 495, 1701, 0},
  {"MW3 Band", "SWL", 1700, 3500, 0},
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
  {"UKW / FM", "FM-DX", 64000, 108000, 0}
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
    bandLogf("[BANDOPEN][VHF][%s] http.begin FAILED", tag);
    return false;
  }
  http.addHeader("Accept", "application/json");
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    bandLogf("[BANDOPEN][VHF][%s] GET failed http=%d error=%s",
      tag, status, http.errorToString(status).c_str());
    http.end();
    return false;
  }
  const int length = http.getSize();
  if (length > static_cast<int>(kMaxVhfResponseBytes)) {
    bandLogf("[BANDOPEN][VHF][%s] response too large length=%d", tag, length);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  if (body.isEmpty() || body.length() > kMaxVhfResponseBytes) {
    bandLogf("[BANDOPEN][VHF][%s] empty/oversize body bytes=%u",
      tag, static_cast<unsigned>(body.length()));
    return false;
  }
  doc.clear();
  const DeserializationError error = deserializeJson(doc, body);
  if (error) {
    bandLogf("[BANDOPEN][VHF][%s] JSON parse FAILED: %s", tag, error.c_str());
    return false;
  }
  bandLogf("[BANDOPEN][VHF][%s] JSON OK bytes=%u", tag,
    static_cast<unsigned>(body.length()));
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
  bandLogf("[BANDOPEN] worker %s\n", created == pdPASS ? "created" : "creation failed");
  atsStandardLogf("[BANDOPEN][STATUS] HF monitor worker %s",
                  created == pdPASS ? "started" : "FAILED to start");
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
    atsDebugLogf("[BANDOPEN] Wi-Fi available; immediate refresh requested");
    atsStandardLogf("[BANDOPEN][STATUS] Wi-Fi available; HF data refresh requested");
  }
  if (wifiConnected != wasConnected) {
    bandLogf("[BANDOPEN][GATE] monitor_data_access=%s wifi_status=%d snapshot_valid=%u",
      wifiConnected ? "ON" : "OFF", static_cast<int>(WiFi.status()), snapshot().valid ? 1U : 0U);
    atsStandardLogf("[BANDOPEN][STATUS] Wi-Fi %s; monitor data access %s",
      wifiConnected ? "connected" : "disconnected",
      wifiConnected ? "available" : "waiting");
  }

}
void BandOpeningService::requestRefresh() {
  const uint32_t now = millis();
  if (!lastAttemptMs_ || now - lastAttemptMs_ >= kRefreshIntervalMs) {
    refreshRequested_ = true;
    atsDebugLogf("[BANDOPEN] manual refresh requested");
    atsStandardLogf("[BANDOPEN][STATUS] Manual HF monitor refresh requested");
  } else {
    atsDebugLogf("[BANDOPEN] manual refresh throttled; five-minute interval not reached");
  }
}
BandOpeningService::Snapshot BandOpeningService::snapshot() const {
  Snapshot copy;
  portENTER_CRITICAL(&snapshotMux_); copy = snapshot_; portEXIT_CRITICAL(&snapshotMux_);
  bool ageStale = true;
  if (copy.valid && !timestampIsFresh(copy.sourceUpdated, &ageStale)) {
    copy.valid = false;
    for (uint8_t i = 0; i < kBandCount; ++i) {
      if (i != 26 && i != 27) copy.rows[i] = {};
    }
  }
  if (copy.vhf2mUpdatedAtMs &&
      millis() - copy.vhf2mUpdatedAtMs >= kVhfMaximumAgeMs)
    copy.rows[26] = {};
  if (copy.vhf70cmUpdatedAtMs &&
      millis() - copy.vhf70cmUpdatedAtMs >= kVhfMaximumAgeMs)
    copy.rows[27] = {};
  copy.stale = ageStale;
  if (copy.fmMufValid && copy.fmMufUpdatedAtMs) {
    const uint32_t mufAge = millis() - copy.fmMufUpdatedAtMs;
    copy.fmMufStale = mufAge >= kFreshAgeMs;
    if (mufAge >= kMaximumAgeMs) copy.fmMufValid = false;
  }
  return copy;
}
void BandOpeningService::publish(const Snapshot& value) {
  portENTER_CRITICAL(&snapshotMux_); snapshot_ = value; portEXIT_CRITICAL(&snapshotMux_);
}
void BandOpeningService::taskEntry(void* context) {
  static_cast<BandOpeningService*>(context)->taskLoop();
}
void BandOpeningService::taskLoop() {
  atsDebugLogf("[BANDOPEN] worker entered");
  uint32_t lastMemoryReportMs = millis();
  for (;;) {
    const uint32_t now = millis();
    if ((uint32_t)(now - lastMemoryReportMs) >= 60000UL) {
      lastMemoryReportMs = now;
      bandLogf("[BANDOPEN][MEM] stage=periodic free_heap=%u max_block=%u min_free=%u task_stack_high_water=%u",
        static_cast<unsigned>(ESP.getFreeHeap()),
        static_cast<unsigned>(ESP.getMaxAllocHeap()),
        static_cast<unsigned>(ESP.getMinFreeHeap()),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    const bool due = refreshRequested_ || !lastAttemptMs_ || now - lastAttemptMs_ >= kRefreshIntervalMs;
    if (wifiConnected_ && (!lastVhfAttemptMs_ || now - lastVhfAttemptMs_ >= kVhfRefreshIntervalMs)) {
      lastVhfAttemptMs_ = now;
      Snapshot current = snapshot();
      bandLogf("[BANDOPEN][VHF] cycle start heap=%u",
        static_cast<unsigned>(ESP.getFreeHeap()));
      if (fetchVhfConditions(current)) {
        publish(current);
        atsStandardLogf("[BANDOPEN][STATUS] VHF/UHF activity data refreshed");
      } else {
        bandLogf("[BANDOPEN][VHF] cycle failed; cached values retained");
      }
    }
    if (wifiConnected_ && (!lastMufAttemptMs_ || now - lastMufAttemptMs_ >= kRefreshIntervalMs)) {
      lastMufAttemptMs_ = now;
      float muf = 0.0f;
      bandLogf("[BANDOPEN][FM_MUF] cycle start wifi=%d heap=%u",
        static_cast<int>(WiFi.status()), static_cast<unsigned>(ESP.getFreeHeap()));
      const bool ok = fetchFmMuf(muf);
      bandLogf("[BANDOPEN][FM_MUF] cycle result=%s value_mhz=%.1f heap=%u",
        ok ? "OK" : "FAILED", ok ? muf : 0.0f, static_cast<unsigned>(ESP.getFreeHeap()));
      atsStandardLogf("[BANDOPEN][STATUS] FM MUF update %s%s",
        ok ? "OK" : "FAILED", ok ? "" : " (previous value retained if available)");
      Snapshot current = snapshot();
      if (ok) {
        current.fmMufMHz = muf; current.fmMufValid = true;
        current.fmMufStale = false; current.fmMufUpdatedAtMs = millis();
      } else {
        current.fmMufStale = !current.fmMufUpdatedAtMs || (millis() - current.fmMufUpdatedAtMs >= kFreshAgeMs);
      }
      publish(current);
    }
    if (wifiConnected_ && due) {
      const bool wasRequested = refreshRequested_;
      refreshRequested_ = false;
      lastAttemptMs_ = now;
      bandLogf("[BANDOPEN][HF] cycle start trigger=%s wifi=%d heap=%u",
        wasRequested ? "IMMEDIATE" : "SCHEDULED", static_cast<int>(WiFi.status()),
        static_cast<unsigned>(ESP.getFreeHeap()));
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
        fresh.vhf2mUpdatedAtMs = latest.vhf2mUpdatedAtMs;
        fresh.vhf70cmUpdatedAtMs = latest.vhf70cmUpdatedAtMs;
        fresh.fmMufMHz = latest.fmMufMHz;
        fresh.fmMufValid = latest.fmMufValid;
        fresh.fmMufStale = latest.fmMufStale;
        fresh.fmMufUpdatedAtMs = latest.fmMufUpdatedAtMs;
        publish(fresh);
        bandLogf("[BANDOPEN][HF] publish SUCCESS source_updated=%s elapsed_ms=%lu valid=1 stale=0",
          fresh.sourceUpdated, static_cast<unsigned long>(millis() - now));
        atsStandardLogf("[BANDOPEN][STATUS] HF data updated successfully; source=%s elapsed=%lu ms",
          fresh.sourceUpdated, static_cast<unsigned long>(millis() - now));
      } else {
        current = snapshot();
        current.fetching = false;
        current.fetchFailed = true;
        current.stale = true;
        publish(current);
        lastAttemptMs_ = millis() - kRefreshIntervalMs + kRetryIntervalMs;
        bandLogf("[BANDOPEN][HF] publish FAILED; retained_valid=%u retained_source=%s retry_in_ms=%lu",
          current.valid ? 1U : 0U, current.sourceUpdated,
          static_cast<unsigned long>(kRetryIntervalMs));
        atsStandardLogf("[BANDOPEN][STATUS] HF data update FAILED; cached_data=%s; retry in %lu ms",
          current.valid ? "retained" : "not available",
          static_cast<unsigned long>(kRetryIntervalMs));
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

bool BandOpeningService::fetchFmMuf(float& value) {
  const uint32_t startedAt = millis();
  atsDebugLogf("[BANDOPEN][FM_MUF] HTTPS begin");
  WiFiClientSecure client;
  client.setTimeout(8000);
  client.useBuiltinCACertBundle();
  HTTPClient http;
  http.setTimeout(8000);
  http.setReuse(false);
  if (!http.begin(client, kFmMufUrl)) {
    atsDebugLogf("[BANDOPEN][FM_MUF] http.begin FAILED");
    return false;
  }
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    bandLogf("[BANDOPEN][FM_MUF] GET failed http=%d error=%s elapsed_ms=%lu",
      status, http.errorToString(status).c_str(), static_cast<unsigned long>(millis() - startedAt));
    http.end();
    return false;
  }
  const int length = http.getSize();
  bandLogf("[BANDOPEN][FM_MUF] HTTP 200 content_length=%d", length);
  if (length > 8192) { http.end(); atsDebugLogf("[BANDOPEN][FM_MUF] response rejected: too large"); return false; }
  String body = http.getString();
  http.end();
  bandLogf("[BANDOPEN][FM_MUF] body_bytes=%u elapsed_ms=%lu",
    static_cast<unsigned>(body.length()), static_cast<unsigned long>(millis() - startedAt));
  if (body.isEmpty() || body.length() > 8192 || body.startsWith("<")) {
    atsDebugLogf("[BANDOPEN][FM_MUF] response body empty/oversize/HTML");
    return false;
  }
  // Keep the complete short upstream payload in the USB log. The endpoint
  // currently returns HTTP 200, but the expected JSON field is not being
  // accepted; logging the exact payload avoids guessing its schema.
  bandLogf("[BANDOPEN][FM_MUF] payload=%s", body.c_str());
  JsonDocument doc;
  const DeserializationError parseError = deserializeJson(doc, body);
  if (parseError) {
    bandLogf("[BANDOPEN][FM_MUF] JSON parse FAILED: %s", parseError.c_str());
    return false;
  }
  JsonVariantConst field = doc["europe"]["max_frequency"];
  if (field.isUnbound() || field.isNull()) {
    atsDebugLogf("[BANDOPEN][FM_MUF] JSON field europe.max_frequency missing/null");
    return false;
  }
  if (!field.is<const char*>() && !field.is<float>() && !field.is<double>() && !field.is<int>()) {
    bandLogf("[BANDOPEN][FM_MUF] JSON field type unsupported: string=%u number=%u",
      field.is<const char*>() ? 1U : 0U,
      (field.is<float>() || field.is<double>() || field.is<int>()) ? 1U : 0U);
    return false;
  }
  const char* text = field.is<const char*>() ? field.as<const char*>() : nullptr;
  if (field.is<const char*>() && (!text || !text[0] || strcmp(text, "No data") == 0)) {
    bandLogf("[BANDOPEN][FM_MUF] JSON field contains no usable data: %s", text ? text : "(null)");
    return false;
  }
  char* parseEnd = nullptr;
  const float parsed = text ? strtof(text, &parseEnd) : field.as<float>();
  if (text && (parseEnd == text || (parseEnd && *parseEnd != '\0'))) {
    bandLogf("[BANDOPEN][FM_MUF] numeric conversion failed: %s", text);
    return false;
  }
  if (!isfinite(parsed) || parsed < 30.0f || parsed > 300.0f) {
    bandLogf("[BANDOPEN][FM_MUF] value rejected parsed=%.2f", parsed);
    return false;
  }
  value = parsed;
  bandLogf("[BANDOPEN][FM_MUF] value accepted %.2f MHz", value);
  return true;
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
    bandLogf("[BANDOPEN][VHF][PROP] Europe highest Es MUF=%.1f MHz",
      europeEsMufMHz);
  }

  // The service asks for a six-hour spot window. A spot proves reported
  // activity, not that the whole band is closed when no spot is present.
  vTaskDelay(pdMS_TO_TICKS(1100));
  const String twoMeterUrl = String(kVhfApiBase) + "spots?band=144&hours=6&limit=100";
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
    bandLogf("[BANDOPEN][VHF][144] spots=%u es_muf=%.1f status=%s",
      static_cast<unsigned>(spots), europeEsMufMHz, statusText(status));
  } else {
    bandLogf("[BANDOPEN][VHF][144] no usable source response; previous value retained");
  }

  vTaskDelay(pdMS_TO_TICKS(1100));
  const String seventyCmUrl = String(kVhfApiBase) + "spots?band=432&hours=6&limit=100";
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
    bandLogf("[BANDOPEN][VHF][432] spots=%u status=%s",
      static_cast<unsigned>(spots), statusText(status));
  } else {
    bandLogf("[BANDOPEN][VHF][432] no usable source response; previous value retained");
  }

  bandLogf("[BANDOPEN][VHF] cycle result=%s elapsed_ms=%lu",
    anyUpdated ? "UPDATED" : "FAILED",
    static_cast<unsigned long>(millis() - startedAt));
  return anyUpdated;
}

bool BandOpeningService::fetchConditions(Snapshot& result) {
  const uint32_t startedAt = millis();
  bandLogf("[BANDOPEN][HF] HTTPS begin url=MapForHAM wifi=%d heap=%u",
    static_cast<int>(WiFi.status()), static_cast<unsigned>(ESP.getFreeHeap()));
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
    atsDebugLogf("[BANDOPEN][HF] http.begin FAILED (TLS/URL/client setup)");
    return false;
  }
  http.addHeader("Accept", "application/json");
  atsDebugLogf("[BANDOPEN][HF] http.begin OK; GET started with JSON Accept and firmware User-Agent");
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    bandLogf("[BANDOPEN][HF] GET failed http=%d error=%s content_type=%s server=%s elapsed_ms=%lu",
      status, http.errorToString(status).c_str(), http.header("Content-Type").c_str(),
      http.header("Server").c_str(), static_cast<unsigned long>(millis() - startedAt));
    http.end();
    return false;
  }
  const int length = http.getSize();
  bandLogf("[BANDOPEN][HF] HTTP 200 content_length=%d", length);
  if (length > static_cast<int>(kMaxResponseBytes)) {
    http.end();
    bandLogf("[BANDOPEN][HF] response rejected too large length=%d limit=%u",
      length, static_cast<unsigned>(kMaxResponseBytes));
    return false;
  }
  String body = http.getString();
  http.end();
  bandLogf("[BANDOPEN][HF] body_bytes=%u elapsed_ms=%lu heap=%u",
    static_cast<unsigned>(body.length()), static_cast<unsigned long>(millis() - startedAt),
    static_cast<unsigned>(ESP.getFreeHeap()));
  if (body.isEmpty() || body.length() > kMaxResponseBytes) {
    bandLogf("[BANDOPEN][HF] response rejected empty_or_oversize bytes=%u",
      static_cast<unsigned>(body.length()));
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, body);
  if (error) {
    bandLogf("[BANDOPEN][HF] JSON parse FAILED: %s", error.c_str());
    return false;
  }
  bandLogf("[BANDOPEN][HF] JSON parse OK document_bytes=%u",
    static_cast<unsigned>(body.length()));
  const char* updated = doc["updated"] | "";
  bool sourceStale = true;
  if (!updated[0] || !timestampIsFresh(updated, &sourceStale)) {
    bandLogf("[BANDOPEN][HF] timestamp REJECTED value=%s system_epoch=%lld",
      updated, static_cast<long long>(time(nullptr)));
    return false;
  }
  bandLogf("[BANDOPEN][HF] timestamp accepted value=%s system_epoch=%lld",
    updated, static_cast<long long>(time(nullptr)));
  result.stale = sourceStale;
  strncpy(result.sourceUpdated, updated, sizeof(result.sourceUpdated) - 1);
  result.sourceUpdated[sizeof(result.sourceUpdated) - 1] = '\0';
  JsonArrayConst details = doc["bands_detail"].as<JsonArrayConst>();
  if (details.isNull() || details.size() == 0) {
    atsDebugLogf("[BANDOPEN][HF] JSON has no bands_detail array");
    return false;
  }
  bandLogf("[BANDOPEN][HF] bands_detail entries=%u",
    static_cast<unsigned>(details.size()));

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
    bandLogf("[BANDOPEN][HF][ESTIMATE] %s via %s day=%s night=%s",
      kBands[row].name, groupName, statusText(result.rows[row].day),
      statusText(result.rows[row].night));
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
    bandLogf("[BANDOPEN][HF][BAND] %s band_m=%u found=%u day=%s night=%s",
      kBands[i].name, static_cast<unsigned>(meters), foundBand ? 1U : 0U,
      statusText(result.rows[i].day), statusText(result.rows[i].night));
  }
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
  bandLogf("[BANDOPEN][HF][ESTIMATE] 11m CB from 10m+12m day=%s night=%s",
    statusText(result.rows[24].day), statusText(result.rows[24].night));

  // At least one directly rated amateur band must have a usable assessment.
  // The derived 11m estimate must not make an otherwise empty feed valid.
  const bool any = matchedBands > 0;
  bandLogf("[BANDOPEN][HF] parsed=%u/10 source_updated=%s total_elapsed_ms=%lu result=%s",
    static_cast<unsigned>(matchedBands), updated, static_cast<unsigned long>(millis() - startedAt),
    any ? "VALID" : "NO_RATED_BANDS");
  return any;
}
