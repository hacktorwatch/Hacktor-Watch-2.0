#include "system_stats.h"

#include <Arduino.h>
#include <string.h>
#include <Preferences.h>
#include "esp_system.h"

namespace {

constexpr uint32_t kStatsMagic = 0x48535453;  // 'HSTS'

struct PersistedStats {
  uint32_t magic;
  system_stats::Stats stats;
};

RTC_NOINIT_ATTR PersistedStats s_persisted;
uint32_t s_version = 0;

bool isHardReset(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
    case ESP_RST_EXT:
    case ESP_RST_BROWNOUT:
    case ESP_RST_PANIC:
    case ESP_RST_UNKNOWN:
    case ESP_RST_SDIO:
    case ESP_RST_USB:
    case ESP_RST_JTAG:
    case ESP_RST_EFUSE:
    case ESP_RST_PWR_GLITCH:
    case ESP_RST_CPU_LOCKUP:
      return true;
    default:
      return false;
  }
}

void resetStatsForHardBoot(system_stats::Stats &stats, uint32_t hardCount, uint32_t screenCount) {
  stats.hardResetCount = hardCount;
  stats.softResetCount = 0;
  stats.bleSyncSuccess = 0;
  stats.bleSyncFailures = 0;
  stats.screenTurnOns = screenCount;
  stats.lastBleSyncValid = false;
  memset(&stats.lastBleSyncTime, 0, sizeof(stats.lastBleSyncTime));
}

}  // namespace

namespace system_stats {

void init() {
  if (s_persisted.magic != kStatsMagic) {
    memset(&s_persisted, 0, sizeof(s_persisted));
    s_persisted.magic = kStatsMagic;
  }

  esp_reset_reason_t reason = esp_reset_reason();
  system_stats::Stats &stats = s_persisted.stats;

  Preferences prefs;
  prefs.begin("sysstats", false);

  uint32_t storedHard = prefs.getUInt("hard", 0);
  uint32_t storedScreen = prefs.getUInt("screen", 0);
  stats.screenTurnOns = storedScreen;

  if (isHardReset(reason)) {
    uint32_t newHard = storedHard + 1;
    resetStatsForHardBoot(stats, newHard, storedScreen);
    prefs.putUInt("hard", newHard);
    prefs.putUInt("soft", stats.softResetCount);
  } else {
    stats.hardResetCount = storedHard;
    stats.softResetCount += 1;
    prefs.putUInt("soft", stats.softResetCount);
    if (stats.hardResetCount == 0) {
      stats.hardResetCount = storedHard == 0 ? 1 : storedHard;
    }
  }
  prefs.putUInt("screen", stats.screenTurnOns);
  stats.lastResetReason = static_cast<uint8_t>(reason);
  ++s_version;

  prefs.end();
}

void recordBleSyncSuccess(const tm &syncedTime) {
  system_stats::Stats &stats = s_persisted.stats;
  stats.bleSyncSuccess += 1;
  stats.lastBleSyncValid = true;
  stats.lastBleSyncTime = syncedTime;
  ++s_version;
}

void recordBleSyncFailure() {
  system_stats::Stats &stats = s_persisted.stats;
  stats.bleSyncFailures += 1;
  ++s_version;
}

void recordScreenOnEvent() {
  system_stats::Stats &stats = s_persisted.stats;
  stats.screenTurnOns += 1;
  Preferences prefs;
  prefs.begin("sysstats", false);
  prefs.putUInt("screen", stats.screenTurnOns);
  prefs.end();
  ++s_version;
}

const Stats &current() {
  return s_persisted.stats;
}

uint32_t version() {
  return s_version;
}

}  // namespace system_stats
