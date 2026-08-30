#pragma once

#include <Arduino.h>

namespace backlight {

void init(uint8_t pin);
void startFade(uint8_t targetDuty, uint16_t durationMs);
bool isIdle();
void update();
void prepareForSleep();
void restoreAfterSleep();
uint8_t currentDuty();

}  // namespace backlight

