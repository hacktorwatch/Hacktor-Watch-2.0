#pragma once

#include <Arduino.h>

namespace fuel_gauge {

constexpr uint8_t ADDRESS = 0x36;      // MAX17048 I2C address
constexpr uint8_t REG_SOC = 0x04;      // 8.8 fixed-point percent register
constexpr uint8_t REG_VCELL = 0x02;    // battery voltage register

bool readSOC(float &pct);
bool readVoltage(float &volts);

}  // namespace fuel_gauge
