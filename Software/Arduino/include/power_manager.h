#pragma once

#include "app_state.h"

namespace power_manager {

inline constexpr uint32_t DISPLAY_ON_TIMEOUT_MS = 10000;  // time the screen stays on after last activity

void panelSleep(bool on);
void sleepUntilTilt();
void serviceTiltIRQ();
inline void flagTiltInterrupt() {
  app_state::get().power.tiltIrqFlag = true;
}

}  // namespace power_manager

