#pragma once

#include "graphics.h"

namespace display_manager {

void init();
graphics::Graphics &get();
void begin(uint32_t freq_hz = 40000000ul);
void reinitializeAfterWake();

}  // namespace display_manager

