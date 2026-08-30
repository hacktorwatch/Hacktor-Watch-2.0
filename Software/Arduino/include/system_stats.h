#pragma once

#include <time.h>
#include <stdint.h>

namespace system_stats {

struct Stats {
  uint32_t hardResetCount = 0;
  uint32_t softResetCount = 0;
  uint32_t bleSyncSuccess = 0;
  uint32_t bleSyncFailures = 0;
  uint32_t screenTurnOns = 0;
  bool lastBleSyncValid = false;
  tm lastBleSyncTime{};
  uint8_t lastResetReason = 0;
};

void init();
void recordBleSyncSuccess(const tm &syncedTime);
void recordBleSyncFailure();
void recordScreenOnEvent();
const Stats &current();
uint32_t version();

}  // namespace system_stats

