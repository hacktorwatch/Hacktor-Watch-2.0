#pragma once

#include <stdint.h>

namespace pins {

inline constexpr uint8_t LCD_DC   = 5;
inline constexpr uint8_t LCD_CS   = 4;
inline constexpr uint8_t LCD_SCK  = 12;
inline constexpr uint8_t LCD_MOSI = 11;
inline constexpr uint8_t LCD_RST  = 3;
inline constexpr uint8_t LCD_BL   = 7;
inline constexpr uint8_t LCD_PWR  = 18;
inline constexpr uint8_t I2C_SDA  = 8;
inline constexpr uint8_t I2C_SCL  = 9;
inline constexpr uint8_t IMU_INT1 = 17;
inline constexpr uint8_t IMU_INT2 = 14;
inline constexpr uint8_t BTN_IO0  = 0;

}  // namespace pins
