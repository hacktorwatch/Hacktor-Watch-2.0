#include "imu.h"

#include <Wire.h>

namespace imu {

uint8_t read8(uint8_t reg) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(static_cast<int>(ADDRESS), 1);
  return Wire.available() ? Wire.read() : 0;
}

void write8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

bool read16(uint8_t reg, uint16_t &out) {
  Wire.beginTransmission(ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  Wire.requestFrom(static_cast<int>(ADDRESS), 2);
  if (Wire.available() < 2) {
    return false;
  }
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  out = static_cast<uint16_t>((hi << 8) | lo);
  return true;
}

bool waitWhoAmI(uint16_t timeout_ms) {
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    Wire.beginTransmission(ADDRESS);
    Wire.write(REG_WHO_AM_I);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(static_cast<int>(ADDRESS), 1) == 1) {
      if (Wire.read() == 0x6A) {
        return true;
      }
    }
    delay(10);
  }
  return false;
}

void softReset() {
  write8(REG_CTRL3_C, CTRL3_C_SW_RESET);
  uint32_t start = millis();
  while (millis() - start < 200 && (read8(REG_CTRL3_C) & CTRL3_C_SW_RESET)) {
    delay(5);
  }
  write8(REG_CTRL3_C, CTRL3_C_BDU | CTRL3_C_IF_INC);
}

bool enableHardwarePedometer(uint16_t &initialStepCount) {
  initialStepCount = 0;
  write8(REG_CTRL1_XL, 0x40);  // accel 104 Hz
  write8(REG_FUNC_CFG_ACCESS, 0x80);
  write8(REG_EMB_FUNC_EN_A, read8(REG_EMB_FUNC_EN_A) | EMB_PEDO_EN_A_BIT);
  write8(REG_EMB_FUNC_EN_B, read8(REG_EMB_FUNC_EN_B) | EMB_STEP_DET_EN_B);
  write8(REG_FUNC_CFG_ACCESS, 0x00);
  write8(REG_CTRL10_C, 0x3C);
  write8(REG_FUNC_CFG_ACCESS, 0x80);
  write8(REG_PEDO_DEB_REG, read8(REG_PEDO_DEB_REG) | 0x02);
  write8(REG_FUNC_CFG_ACCESS, 0x00);
  write8(REG_MD1_CFG, MD1_INT1_STEP_DET);
  (void)read8(REG_FUNC_SRC);
  uint16_t s16;
  if (read16(REG_STEP_COUNTER_L, s16)) {
    initialStepCount = s16;
  }
  return true;
}

bool enableTiltOnInt2() {
  write8(REG_FUNC_CFG_ACCESS, 0x80);
  uint8_t enB = read8(REG_EMB_FUNC_EN_B) | EMB_TILT_EN_B;
  write8(REG_EMB_FUNC_EN_B, enB);
  write8(REG_FUNC_CFG_ACCESS, 0x00);

  write8(REG_WAKE_UP_THS, 0x04);
  write8(REG_WAKE_UP_DUR, 0x00);

  write8(REG_MD2_CFG, MD2_INT2_TILT);

  (void)read8(REG_TILT_SRC);
  (void)read8(REG_FUNC_SRC);

  return true;
}

void setAccelODR(uint8_t odr_bits) {
  uint8_t value = read8(REG_CTRL1_XL);
  value = static_cast<uint8_t>((value & 0x0F) | odr_bits);
  write8(REG_CTRL1_XL, value);
}

}  // namespace imu

