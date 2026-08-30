#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <time.h>

namespace app_state {

struct DisplayState {
  tm currentTime{};
  int prevHourX = 0;
  int prevHourY = 0;
  int prevMinuteX = 0;
  int prevMinuteY = 0;
  int prevSecondX = 0;
  int prevSecondY = 0;
  int prevSecondTailX = 0;
  int prevSecondTailY = 0;
  unsigned long lastTickMs = 0;
  unsigned long rtcBaseMs = 0;
  enum class Screen : uint8_t { Watchface = 0, Info = 1 };
  Screen activeScreen = Screen::Watchface;
  bool infoNeedsRedraw = false;
  uint32_t infoShownVersion = 0;
  int infoLastDrawnSecond = -1;
};

struct BatteryState {
  uint8_t percent = 0;
  float voltage = 0.0f;
  unsigned long lastPollMs = 0;
};

struct PowerState {
  bool displayOn = true;
  uint32_t displayExpireMs = 0;
  bool pendingSleep = false;
  bool pendingPanelOff = false;
  bool tiltIrqFlag = false;
};

struct Runtime {
  DisplayState display;
  BatteryState battery;
  PowerState power;
};

Runtime &get();

}  // namespace app_state
