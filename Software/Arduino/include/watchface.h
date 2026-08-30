#pragma once

#include <stdint.h>
#include <time.h>

#include "graphics.h"

namespace watchface {

constexpr int WIDTH  = 240;
constexpr int HEIGHT = 240;
constexpr int CENTER_X = 120;
constexpr int CENTER_Y = 119;
constexpr int RADIUS   = 120;
constexpr float DEGREES_TO_RAD = 3.14159265f / 180.0f;

constexpr uint16_t COLOR_BG        = 0x0000;
constexpr uint16_t COLOR_FACE      = 0xFFFF;
constexpr uint16_t COLOR_HOUR_HAND = 0xFFFF;
constexpr uint16_t COLOR_MIN_HAND  = 0xFFFF;
constexpr uint16_t COLOR_SEC_HAND  = 0xF800;
constexpr uint16_t COLOR_DATE_NUM  = 0xF800;
constexpr uint16_t COLOR_STEPS     = 0xFFFF;

void init();

void drawTicks(graphics::Graphics &display);
void drawDateRotatedCWRightOfCenter(graphics::Graphics &display, const tm &currentTime);
void drawStepsBelowCenter(graphics::Graphics &display, uint32_t stepsToday);
void drawBatteryRotatedCWLeftOfCenter(graphics::Graphics &display, uint8_t batteryPercent);
void drawThick3Line(graphics::Graphics &display, int x0, int y0, int x1, int y1, uint16_t color);
void drawFullFaceAndHands(
  graphics::Graphics &display,
  const tm &currentTime,
  uint32_t stepsToday,
  uint8_t batteryPercent,
  int &prev_hx, int &prev_hy,
  int &prev_mx, int &prev_my,
  int &prev_sx, int &prev_sy,
  int &prev_stx, int &prev_sty
);

void calcHourEnd(const tm &currentTime, int &hx, int &hy);
void calcMinuteEnd(const tm &currentTime, int &mx, int &my);
void calcSecondEnds(const tm &currentTime, int &sx, int &sy, int &tx, int &ty);

}  // namespace watchface

