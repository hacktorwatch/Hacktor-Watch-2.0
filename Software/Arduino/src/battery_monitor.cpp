#include "battery_monitor.h"

#include <math.h>
#include <Arduino.h>

#include "app_state.h"
#include "fuel_gauge.h"

namespace battery_monitor {

void poll() {
  auto &state = app_state::get();
  auto &battery = state.battery;
  unsigned long now = millis();
  if (now - battery.lastPollMs < 60000UL) {
    return;
  }
  battery.lastPollMs = now;

  float soc;
  if (fuel_gauge::readSOC(soc)) {
    if (soc < 0.0f) soc = 0.0f;
    if (soc > 100.0f) soc = 100.0f;
    battery.percent = static_cast<uint8_t>(lroundf(soc));
  }

  float volts;
  if (fuel_gauge::readVoltage(volts)) {
    battery.voltage = volts;
  }
}

}  // namespace battery_monitor
