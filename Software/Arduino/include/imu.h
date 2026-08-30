#pragma once

#include <Arduino.h>

namespace imu {

constexpr uint8_t ADDRESS = 0x6A;  // IMU I2C address (SA0=GND)

// Register map constants
constexpr uint8_t REG_WHO_AM_I        = 0x0F;
constexpr uint8_t REG_CTRL1_XL        = 0x10;
constexpr uint8_t REG_CTRL3_C         = 0x12;
constexpr uint8_t REG_CTRL10_C        = 0x19;
constexpr uint8_t REG_FUNC_CFG_ACCESS = 0x01;
constexpr uint8_t REG_EMB_FUNC_EN_A   = 0x04;
constexpr uint8_t REG_EMB_FUNC_EN_B   = 0x05;
constexpr uint8_t REG_MD1_CFG         = 0x5E;
constexpr uint8_t REG_MD2_CFG         = 0x5F;
constexpr uint8_t REG_FUNC_SRC        = 0x53;
constexpr uint8_t REG_TILT_SRC        = 0x52;
constexpr uint8_t REG_STEP_COUNTER_L  = 0x4B;
constexpr uint8_t REG_PEDO_DEB_REG    = 0x2F;
constexpr uint8_t REG_WAKE_UP_THS     = 0x5B;
constexpr uint8_t REG_WAKE_UP_DUR     = 0x5C;

// Bit definitions
constexpr uint8_t CTRL3_C_BDU         = (1 << 6);
constexpr uint8_t CTRL3_C_IF_INC      = (1 << 2);
constexpr uint8_t CTRL3_C_SW_RESET    = (1 << 0);
constexpr uint8_t EMB_PEDO_EN_A_BIT   = 0x40;
constexpr uint8_t EMB_STEP_DET_EN_B   = 0x80;
constexpr uint8_t EMB_TILT_EN_B       = 0x20;
constexpr uint8_t MD1_INT1_STEP_DET   = 0x08;
constexpr uint8_t MD2_INT2_TILT       = 0x02;

uint8_t read8(uint8_t reg);
void write8(uint8_t reg, uint8_t value);
bool read16(uint8_t reg, uint16_t &out);
bool waitWhoAmI(uint16_t timeout_ms);
void softReset();
bool enableHardwarePedometer(uint16_t &initialStepCount);
bool enableTiltOnInt2();
void setAccelODR(uint8_t odr_bits);

}  // namespace imu

