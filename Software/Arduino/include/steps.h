#pragma once

#include <Arduino.h>

namespace steps {

void init(uint16_t initialHardwareCount);
void resetDailyBaseline();

void flagInterrupt();
void serviceInterrupt();
void pollWatchdog(unsigned long now);

uint32_t hardwareTotal();
uint32_t today();
uint32_t baseline();

}  // namespace steps

