#include "power_manager.h"

#include <Arduino.h>
#include "esp_sleep.h"

#include "app_state.h"
#include "backlight.h"
#include "hardware_pins.h"
#include "imu.h"
#include "watchface.h"
#include "display_manager.h"

namespace {

void lcdBusTriState() {
  backlight::prepareForSleep();

  pinMode(pins::LCD_SCK,  INPUT);
  pinMode(pins::LCD_MOSI, INPUT);
  pinMode(pins::LCD_CS,   INPUT);
  pinMode(pins::LCD_DC,   INPUT);
}

void lcdBusRestore() {
  pinMode(pins::LCD_SCK,  OUTPUT);
  pinMode(pins::LCD_MOSI, OUTPUT);
  pinMode(pins::LCD_CS,   OUTPUT);
  pinMode(pins::LCD_DC,   OUTPUT);

  backlight::restoreAfterSleep();
}

}  // namespace

namespace power_manager {

void panelSleep(bool on) {
  auto &powerState = app_state::get().power;
  if (on) {
    backlight::startFade(0, 1000);
    powerState.pendingPanelOff = true;
  } else {
    display_manager::get().displayOn();
    backlight::startFade(255, 50);
  }
}

void sleepUntilTilt() {
  auto &runtime = app_state::get();
  auto &displayState = runtime.display;
  auto &powerState = runtime.power;

  esp_sleep_enable_ext1_wakeup(1ULL << pins::IMU_INT2, ESP_EXT1_WAKEUP_ANY_HIGH);

  (void)imu::read8(imu::REG_TILT_SRC);
  (void)imu::read8(imu::REG_FUNC_SRC);

  lcdBusTriState();
  delay(2);
  digitalWrite(pins::LCD_PWR, LOW);

  displayState.rtcBaseMs = millis();
  setCpuFrequencyMhz(20);
  esp_light_sleep_start();

  setCpuFrequencyMhz(160);
  digitalWrite(pins::LCD_PWR, HIGH);
  delay(20);

  lcdBusRestore();

  display_manager::reinitializeAfterWake();
  display_manager::get().fillScreen(watchface::COLOR_BG);

  (void)imu::read8(imu::REG_TILT_SRC);
  (void)imu::read8(imu::REG_FUNC_SRC);
  powerState.tiltIrqFlag = false;
}

void serviceTiltIRQ() {
  auto &runtime = app_state::get();
  auto &powerState = runtime.power;
  if (!powerState.tiltIrqFlag) {
    return;
  }
  powerState.tiltIrqFlag = false;
  (void)imu::read8(imu::REG_TILT_SRC);
  (void)imu::read8(imu::REG_FUNC_SRC);
  if (powerState.displayOn) {
    powerState.displayExpireMs = millis() + DISPLAY_ON_TIMEOUT_MS;
  }
}

}  // namespace power_manager
