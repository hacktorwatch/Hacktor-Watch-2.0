#include "display_manager.h"

#include <Arduino.h>

#include "hardware_pins.h"
#include "gc9a01_graphics.h"

namespace display_manager {
namespace {

graphics::Gc9a01Graphics *driver = nullptr;

void ensureCreated() {
  if (!driver) {
    constexpr uint8_t kResetPin = pins::LCD_RST;
    driver = new graphics::Gc9a01Graphics(
      pins::LCD_DC,
      pins::LCD_CS,
      kResetPin,
      true
    );
  }
}

}  // namespace

void init() {
  ensureCreated();
}

graphics::Graphics &get() {
  ensureCreated();
  return *driver;
}

void begin(uint32_t freq_hz) {
  ensureCreated();
  driver->begin(freq_hz);
}

void reinitializeAfterWake() {
  begin();
}

}  // namespace display_manager

