#include "ble_time_sync.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>
#include <BLEAdvertisedDevice.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "time_keeper.h"
#include "system_stats.h"
#include "debug_log.h"

namespace {

constexpr uint16_t CTS_SERVICE_UUID_16 = 0x1805;
constexpr uint16_t CURRENT_TIME_CHAR_UUID_16 = 0x2A2B;
constexpr unsigned long SYNC_INTERVAL_MS = 60UL * 60UL * 1000UL;  // 1 hour
constexpr unsigned long RETRY_INTERVAL_MS = 5UL * 60UL * 1000UL;   // 5 minutes
constexpr unsigned long QUICK_RETRY_MS = 60UL * 1000UL;            // 1 minute
constexpr uint32_t BLE_SCAN_DURATION_SEC = 5;                      // blocking scan duration

#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
constexpr BaseType_t kSyncTaskCore = tskNO_AFFINITY;
#else
constexpr BaseType_t kSyncTaskCore = 0;
#endif

volatile bool s_workerRunning = false;
volatile bool s_forceImmediate = false;
unsigned long s_nextSyncMs = 0;
unsigned long s_lastSyncMs = 0;

BLEUUID ctsServiceUuid((uint16_t)CTS_SERVICE_UUID_16);
BLEUUID currentTimeCharUuid((uint16_t)CURRENT_TIME_CHAR_UUID_16);

bool decodeCurrentTime(const uint8_t *data, size_t len, tm &out) {
  if (len < 7) {
    return false;
  }

  uint16_t year = static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
  uint8_t month = data[2];
  uint8_t day = data[3];
  uint8_t hours = data[4];
  uint8_t minutes = data[5];
  uint8_t seconds = data[6];
  uint8_t dow = (len >= 8) ? data[7] : 0;

  if (year < 1900 || month == 0 || month > 12 || day == 0 || day > 31) {
    return false;
  }
  if (hours >= 24 || minutes >= 60 || seconds >= 60) {
    return false;
  }

  out = {};
  out.tm_year = static_cast<int>(year) - 1900;
  out.tm_mon = static_cast<int>(month) - 1;
  out.tm_mday = static_cast<int>(day);
  out.tm_hour = static_cast<int>(hours);
  out.tm_min = static_cast<int>(minutes);
  out.tm_sec = static_cast<int>(seconds);
  if (dow >= 1 && dow <= 7) {
    out.tm_wday = (dow == 7) ? 0 : dow;
  } else {
    out.tm_wday = 0;
  }
  out.tm_isdst = -1;
  out.tm_yday = 0;
  return true;
}

bool syncFromDevice(BLEAdvertisedDevice device) {
  BLEClient *client = BLEDevice::createClient();
  if (client == nullptr) {
    LOG_PRINT(1, "[BLE] Failed to create client");
    return false;
  }

  bool connected = client->connect(device.getAddress(), device.getAddressType());
  if (!connected) {
    LOG_PRINT(1, "[BLE] Connect failed");
    delete client;
    return false;
  }

  BLERemoteService *service = client->getService(ctsServiceUuid);
  if (service == nullptr) {
    LOG_PRINT(1, "[BLE] CTS service missing");
    client->disconnect();
    delete client;
    return false;
  }

  BLERemoteCharacteristic *characteristic = service->getCharacteristic(currentTimeCharUuid);
  if (characteristic == nullptr) {
    LOG_PRINT(1, "[BLE] Current Time char missing");
    client->disconnect();
    delete client;
    return false;
  }

  String value = characteristic->readValue();
  tm newTime{};
  bool decoded = decodeCurrentTime(reinterpret_cast<const uint8_t *>(value.c_str()), value.length(), newTime);
  if (decoded) {
    time_keeper::setCurrentTime(newTime);
    s_lastSyncMs = millis();
    system_stats::recordBleSyncSuccess(newTime);
    LOG_PRINTF(1,
               "[BLE] Sync %04d-%02d-%02d %02d:%02d:%02d\n",
               newTime.tm_year + 1900,
               newTime.tm_mon + 1,
               newTime.tm_mday,
               newTime.tm_hour,
               newTime.tm_min,
               newTime.tm_sec);
  } else {
    LOG_PRINT(1, "[BLE] Decode failed");
  }

  client->disconnect();
  delete client;
  return decoded;
}

bool attemptSync() {
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(1349);
  scan->setWindow(449);

  BLEScanResults *results = scan->start(BLE_SCAN_DURATION_SEC, false);
  if (results == nullptr) {
    return false;
  }

  bool success = false;
  LOG_PRINTF(1, "[BLE] Scan found %d device(s)\n", results->getCount());
  for (int i = 0; i < results->getCount(); ++i) {
    BLEAdvertisedDevice device = results->getDevice(i);
    LOG_PRINTF(1, "[BLE] Device %d: %s\n", i, device.toString().c_str());
    if (device.haveServiceUUID() && device.isAdvertisingService(ctsServiceUuid)) {
      LOG_PRINT(1, "[BLE] Found CTS advert, attempting sync");
      success = syncFromDevice(device);
      if (success) {
        break;
      }
    }
  }
  scan->clearResults();
  if (!success) {
    LOG_PRINT(1, "[BLE] No CTS sync succeeded this scan");
  }
  return success;
}

void syncWorker(void *) {
  bool ok = attemptSync();
  unsigned long now = millis();
  if (ok) {
    LOG_PRINT(1, "[BLE] synchronized");
    s_nextSyncMs = now + SYNC_INTERVAL_MS;
  } else {
    LOG_PRINT(1, "[BLE] failed; will retry later");
    system_stats::recordBleSyncFailure();
    if (s_lastSyncMs == 0) {
      s_nextSyncMs = now + QUICK_RETRY_MS;
    } else {
      s_nextSyncMs = now + RETRY_INTERVAL_MS;
    }
  }
  s_workerRunning = false;
  vTaskDelete(nullptr);
}

}  // namespace

namespace ble_time_sync {

void init() {
  BLEDevice::init("HacktorWatch");
  s_nextSyncMs = millis();
  s_lastSyncMs = 0;
}

void service() {
  if (s_workerRunning) {
    return;
  }
  unsigned long now = millis();
  if (s_forceImmediate) {
    s_forceImmediate = false;
    s_nextSyncMs = now;
  }
  if (now < s_nextSyncMs) {
    return;
  }

  TaskHandle_t handle = nullptr;
  s_workerRunning = true;
  BaseType_t created = xTaskCreatePinnedToCore(
      syncWorker,
      "ble_time_sync",
      4096,
      nullptr,
      1,
      &handle,
      kSyncTaskCore);

  if (created != pdPASS) {
    s_workerRunning = false;
    s_nextSyncMs = now + QUICK_RETRY_MS;
  }
}

void requestImmediateSync() {
  s_forceImmediate = true;
}

}  // namespace ble_time_sync
