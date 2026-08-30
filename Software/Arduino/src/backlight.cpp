#include "backlight.h"

namespace backlight {
namespace {
constexpr int PWM_CHANNEL = 4;
constexpr int PWM_FREQUENCY = 5000;  // Hz
constexpr int PWM_RESOLUTION_BITS = 8;

volatile uint8_t s_current = 255;      // current duty
volatile uint8_t s_target  = 255;      // desired duty
volatile int8_t  s_step    = 0;        // +1 / -1 / 0
volatile uint32_t s_stepIntervalMs = 4;  // ms per step
uint32_t s_lastStepMs = 0;
uint8_t s_pin = 0xFF;  // invalid pin sentinel

inline uint8_t clamp8(int value) {
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<uint8_t>(value);
}

inline int imax(int a, int b) { return (a > b) ? a : b; }

inline bool configured() { return s_pin != 0xFF; }
}  // namespace

void init(uint8_t pin) {
  s_pin = pin;
  pinMode(s_pin, OUTPUT);
  ledcAttachChannel(s_pin, PWM_FREQUENCY, PWM_RESOLUTION_BITS, PWM_CHANNEL);
  ledcWrite(s_pin, s_current);
}

void startFade(uint8_t targetDuty, uint16_t durationMs) {
  s_target = targetDuty;
  if (s_target == s_current) {
    s_step = 0;
    return;
  }
  int delta = static_cast<int>(s_target) - static_cast<int>(s_current);
  int steps = (delta < 0) ? -delta : delta;
  if (steps == 0) steps = 1;
  s_stepIntervalMs = static_cast<uint32_t>(imax(1, static_cast<int>(durationMs) / steps));
  s_step = (delta > 0) ? +1 : -1;
  s_lastStepMs = millis();
}

bool isIdle() {
  return s_step == 0 || s_current == s_target;
}

void update() {
  if (s_step == 0) {
    return;
  }
  uint32_t now = millis();
  if (now - s_lastStepMs < s_stepIntervalMs) {
    return;
  }
  s_lastStepMs = now;

  int next = static_cast<int>(s_current) + s_step;
  s_current = clamp8(next);
  if (configured()) {
    ledcWrite(s_pin, s_current);
  }
  if (s_current == s_target) {
    s_step = 0;
  }
}

void prepareForSleep() {
  if (!configured()) {
    return;
  }
  ledcWrite(s_pin, 0);
  ledcDetach(s_pin);
  pinMode(s_pin, INPUT);
  s_current = 0;
  s_target = 0;
  s_step = 0;
}

void restoreAfterSleep() {
  if (!configured()) {
    return;
  }
  pinMode(s_pin, OUTPUT);
  ledcAttachChannel(s_pin, PWM_FREQUENCY, PWM_RESOLUTION_BITS, PWM_CHANNEL);
  ledcWrite(s_pin, s_current);
}

uint8_t currentDuty() {
  return s_current;
}

}  // namespace backlight

