#include "steps.h"

#include <Arduino.h>
#include <Preferences.h>

#include "imu.h"

namespace steps {
namespace {
volatile bool irqFlag = false;
uint32_t hwTotal = 0;
uint16_t hwPrev16 = 0;
uint32_t currentBaseline = 0;
uint32_t stepsToday = 0;
unsigned long lastPollMs = 0;

Preferences prefs;
bool prefsStarted = false;
uint32_t persistedHwTotal = 0;
uint32_t persistedBaseline = 0;
unsigned long lastPersistMs = 0;

constexpr const char *kPrefsNamespace = "steps";
constexpr uint32_t kPersistVersion = 1;
constexpr unsigned long kPersistIntervalMs = 5UL * 60UL * 1000UL;   // 5 minutes - writes in NVS only if this interval has elapsed AND
constexpr uint32_t kPersistStepDelta = 100;                         //writes in NVS only if delta is more than this number of steps

void ensurePrefs() {
  if (!prefsStarted) {
    prefs.begin(kPrefsNamespace, false);
    prefsStarted = true;
  }
}

void updateSteps(uint16_t s16) {
  if (s16 < hwPrev16) {
    hwTotal += 0x10000 - hwPrev16 + s16;
  } else {
    hwTotal += s16 - hwPrev16;
  }
  hwPrev16 = s16;
  stepsToday = (hwTotal >= currentBaseline) ? hwTotal - currentBaseline : 0;
}

void maybePersist(bool force = false) {
  ensurePrefs();
  unsigned long now = millis();
  uint32_t delta = (hwTotal >= persistedHwTotal) ? (hwTotal - persistedHwTotal) : 0;
  if (!force) {
    if (delta < kPersistStepDelta) {
      return;
    }
    if ((now - lastPersistMs) < kPersistIntervalMs) {
      return;
    }
  }

  prefs.putUInt("ver", kPersistVersion);
  prefs.putUInt("hw", hwTotal);
  prefs.putUInt("base", currentBaseline);

  persistedHwTotal = hwTotal;
  persistedBaseline = currentBaseline;
  lastPersistMs = now;
}
}  // namespace

void init(uint16_t initialHardwareCount) {
  ensurePrefs();

  uint32_t storedVersion = prefs.getUInt("ver", 0);
  if (storedVersion == kPersistVersion) {
    persistedHwTotal = prefs.getUInt("hw", 0);
    persistedBaseline = prefs.getUInt("base", 0);
  } else {
    persistedHwTotal = 0;
    persistedBaseline = 0;
  }

  hwTotal = persistedHwTotal;
  hwPrev16 = initialHardwareCount;
  currentBaseline = (persistedBaseline <= hwTotal) ? persistedBaseline : hwTotal;
  stepsToday = (hwTotal >= currentBaseline) ? hwTotal - currentBaseline : 0;
  lastPollMs = millis();
  lastPersistMs = lastPollMs;
  irqFlag = false;
}

void resetDailyBaseline() {
  currentBaseline = hwTotal;
  stepsToday = 0;
  maybePersist(true);
}

void flagInterrupt() {
  irqFlag = true;
}

void serviceInterrupt() {
  if (!irqFlag) {
    return;
  }
  irqFlag = false;
  (void)imu::read8(imu::REG_FUNC_SRC);
  uint16_t s16;
  if (imu::read16(imu::REG_STEP_COUNTER_L, s16)) {
    updateSteps(s16);
    maybePersist();
  }
}

void pollWatchdog(unsigned long now) {
  if (now - lastPollMs < 1000UL) {
    return;
  }
  lastPollMs = now;
  uint16_t s16;
  if (imu::read16(imu::REG_STEP_COUNTER_L, s16)) {
    updateSteps(s16);
    maybePersist();
  }
}

uint32_t hardwareTotal() {
  return hwTotal;
}

uint32_t today() {
  return stepsToday;
}

uint32_t baseline() {
  return currentBaseline;
}

}  // namespace steps
