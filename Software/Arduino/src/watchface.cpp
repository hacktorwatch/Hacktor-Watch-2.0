#include "watchface.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "graphics_utils.h"
#include "debug_log.h"

namespace watchface {

namespace {
int16_t SIN60[60];
int16_t COS60[60];

inline int32_t mulFixed(int32_t c, int r) {
  return static_cast<int32_t>((c * r) / 10000);
}

void drawRotatedLabelBoxedCW(
  graphics::Graphics &display,
  int centerX, int centerY,
  const char *text,
  uint16_t colorText,
  uint16_t colorBG,
  uint8_t textSize,
  int maxChars
) {
  const int glyphW = 6 * textSize;
  const int glyphH = 8 * textSize;

  const int textW_max = maxChars * glyphW;
  const int textH     = glyphH;

  const int Xtl = centerX - textH / 2;
  const int Ytl = centerY - textW_max / 2;

  graphics::RotationScopeCW rotation(display);

  const int u = Ytl;
  const int v = (WIDTH - 1) - Xtl;

  display.fillRect(u - 2, v - 2, textH + 4, textW_max + 4, colorBG);

  const int curW = static_cast<int>(std::strlen(text)) * glyphW;
  const int curH = glyphH;

  const int u_text = u + (textH - curW) / 2;
  const int v_text = v + (textW_max - curH) / 2;

  display.drawText(u_text, v_text, text, colorText, colorBG, textSize);
}

void drawRotatedBatteryIconBoxedCW(
  graphics::Graphics &display,
  int centerX, int centerY,
  uint16_t colorFG, uint16_t colorBG,
  int reserveW, int reserveH,
  int bodyW, int bodyH,
  int nubW,  int nubH,
  uint8_t levelPercent
) {
  if (levelPercent > 100) levelPercent = 100;

  const int Xtl = centerX - reserveH / 2;
  const int Ytl = centerY - reserveW / 2;

  graphics::RotationScopeCW rotation(display);

  const int u = Ytl;
  const int v = (WIDTH - 1) - Xtl;

  display.fillRect(u - 2, v - 2, reserveH + 4, reserveW + 4, colorBG);

  const int u0 = u + (reserveH - bodyW) / 2;
  const int v0 = v + (reserveW - bodyH) / 2;

  display.drawRect(u0, v0, bodyW, bodyH, colorFG);

  const int nubX = u0 + bodyW;
  const int nubY = v0 + (bodyH - nubH) / 2;
  display.fillRect(nubX, nubY, nubW, nubH, colorFG);

  const int innerX = u0 + 1;
  const int innerY = v0 + 1;
  const int innerW = bodyW - 2;
  const int innerH = bodyH - 2;

  int fillW = (innerW * levelPercent) / 100;
  if (fillW < 0) fillW = 0;
  if (fillW > innerW) fillW = innerW;
  if (fillW > 0) {
    display.fillRect(innerX, innerY, fillW, innerH, colorFG);
  }
}

void drawStaticFace(
  graphics::Graphics &display,
  const tm &currentTime,
  uint32_t stepsToday,
  uint8_t batteryPercent
) {
  drawDateRotatedCWRightOfCenter(display, currentTime);
  drawStepsBelowCenter(display, stepsToday);
  drawBatteryRotatedCWLeftOfCenter(display, batteryPercent);
  drawTicks(display);
}

}  // namespace

void init() {
  for (int i = 0; i < 60; ++i) {
    double angle = static_cast<double>(i) * 6.0 * DEGREES_TO_RAD;
    SIN60[i] = static_cast<int16_t>(std::lround(std::sin(angle) * 10000.0));
    COS60[i] = static_cast<int16_t>(std::lround(std::cos(angle) * 10000.0));
  }
}

void drawTicks(graphics::Graphics &display) {
  for (int i = 0; i < 60; ++i) {
    int32_t cx = COS60[i];
    int32_t sy = SIN60[i];

    if (i % 5 == 0) {
      int x1 = CENTER_X + static_cast<int>(mulFixed(cx, static_cast<int>(RADIUS - 14)));
      int y1 = CENTER_Y + static_cast<int>(mulFixed(sy, static_cast<int>(RADIUS - 14)));
      int x2 = CENTER_X + static_cast<int>(mulFixed(cx, static_cast<int>(RADIUS - 2)));
      int y2 = CENTER_Y + static_cast<int>(mulFixed(sy, static_cast<int>(RADIUS - 2)));

      float dx = static_cast<float>(x2 - x1);
      float dy = static_cast<float>(y2 - y1);
      float len = std::sqrt(dx * dx + dy * dy);
      float nx = -dy / len;
      float ny = dx / len;
      float hw = 2.0f;
      int x1a = static_cast<int>(std::round(x1 + nx * hw));
      int y1a = static_cast<int>(std::round(y1 + ny * hw));
      int x1b = static_cast<int>(std::round(x1 - nx * hw));
      int y1b = static_cast<int>(std::round(y1 - ny * hw));
      int x2a = static_cast<int>(std::round(x2 + nx * hw));
      int y2a = static_cast<int>(std::round(y2 + ny * hw));
      int x2b = static_cast<int>(std::round(x2 - nx * hw));
      int y2b = static_cast<int>(std::round(y2 - ny * hw));
      display.fillTriangle(x1a, y1a, x1b, y1b, x2a, y2a, COLOR_FACE);
      display.fillTriangle(x2a, y2a, x2b, y2b, x1b, y1b, COLOR_FACE);
    } else {
      int x1 = CENTER_X + static_cast<int>(mulFixed(cx, static_cast<int>(RADIUS - 6)));
      int y1 = CENTER_Y + static_cast<int>(mulFixed(sy, static_cast<int>(RADIUS - 6)));
      int x2 = CENTER_X + static_cast<int>(mulFixed(cx, static_cast<int>(RADIUS - 2)));
      int y2 = CENTER_Y + static_cast<int>(mulFixed(sy, static_cast<int>(RADIUS - 2)));
      display.drawLine(x1, y1, x2, y2, COLOR_FACE);
    }
  }
}

void drawDateRotatedCWRightOfCenter(graphics::Graphics &display, const tm &currentTime) {
  static const char *WNAME[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  char weekStr[4];
  std::snprintf(weekStr, sizeof(weekStr), "%s", WNAME[currentTime.tm_wday]);
  char dayStr[3];
  std::snprintf(dayStr, sizeof(dayStr), "%02d", currentTime.tm_mday);

  const uint8_t txtSize = 2;
  const int glyphW = 6 * txtSize;
  const int w_week  = 3 * glyphW;
  const int w_space = 1 * glyphW;
  const int w_day   = 2 * glyphW;
  const int totalW  = w_week + w_space + w_day;

  const int Xc = CENTER_X + 25;
  const int Yc = CENTER_Y + 75;

  const int week_centerY = Yc - (totalW / 2) + (w_week / 2);
  const int day_centerY  = Yc - (totalW / 2) + w_week + w_space + (w_day / 2);

  drawRotatedLabelBoxedCW(
    display,
    Xc, week_centerY,
    weekStr, COLOR_FACE, COLOR_BG,
    txtSize,
    3
  );

  drawRotatedLabelBoxedCW(
    display,
    Xc - 6, day_centerY - 15,
    dayStr, COLOR_DATE_NUM, COLOR_BG,
    txtSize,
    2
  );
}

void drawStepsBelowCenter(graphics::Graphics &display, uint32_t stepsToday) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(stepsToday));
  drawRotatedLabelBoxedCW(display, CENTER_X - 25, CENTER_Y + 30, buf, COLOR_STEPS, COLOR_BG, 2, 6);
}

void drawBatteryRotatedCWLeftOfCenter(graphics::Graphics &display, uint8_t batteryPercent) {
  char numStr[4];
  std::snprintf(numStr, sizeof(numStr), "%u", static_cast<unsigned>(batteryPercent));
  const char *pctStr = "%";

  const uint8_t txtSize = 2;
  const int glyphW = 6 * txtSize;
  const int w_num_max = 3 * glyphW;
  const int w_space   = 1 * glyphW;
  const int w_pct     = 1 * glyphW;

  const int iconReserveW = 24;
  const int iconReserveH = 16;
  const int bodyW        = 20;
  const int bodyH        = 10;
  const int nubW         = 3;
  const int nubH         = 4;

  const int totalW = iconReserveW + w_space + w_num_max + w_space + w_pct;

  const int Xc = CENTER_X + 25;
  const int Yc = CENTER_Y - 40;

  int cursor = Yc - (totalW / 2);

  const int icon_centerY = cursor + iconReserveW / 2;
  cursor += iconReserveW + w_space;

  const int num_centerY  = cursor + w_num_max / 2;
  cursor += w_num_max + w_space;

  const int pct_centerY  = cursor + w_pct / 2;

  drawRotatedBatteryIconBoxedCW(
    display,
    Xc - 5, icon_centerY - 4,
    COLOR_FACE, COLOR_BG,
    iconReserveW, iconReserveH,
    bodyW, bodyH, nubW, nubH,
    batteryPercent
  );

  drawRotatedLabelBoxedCW(
    display,
    Xc, num_centerY - 7,
    numStr, COLOR_FACE, COLOR_BG,
    txtSize, 3
  );

  drawRotatedLabelBoxedCW(
    display,
    Xc - 12, pct_centerY - 27,
    pctStr, COLOR_DATE_NUM, COLOR_BG,
    txtSize, 1
  );
}

void drawThick3Line(graphics::Graphics &display, int x0, int y0, int x1, int y1, uint16_t color) {
  display.drawLine(x0, y0, x1, y1, color);
  display.drawLine(x0 - 1, y0 - 1, x1 - 1, y1 - 1, color);
  display.drawLine(x0 + 1, y0 + 1, x1 + 1, y1 + 1, color);
}

void drawFullFaceAndHands(
  graphics::Graphics &display,
  const tm &currentTime,
  uint32_t stepsToday,
  uint8_t batteryPercent,
  int &prev_hx, int &prev_hy,
  int &prev_mx, int &prev_my,
  int &prev_sx, int &prev_sy,
  int &prev_stx, int &prev_sty
) {
#if HACKTOR_DEBUG_LEVEL >= 1
  uint32_t startUs = micros();
#endif
  display.fillScreen(COLOR_BG);
  drawStaticFace(display, currentTime, stepsToday, batteryPercent);

  int hx, hy, mx, my, sx, sy, tx, ty;
  calcHourEnd(currentTime, hx, hy);
  calcMinuteEnd(currentTime, mx, my);
  calcSecondEnds(currentTime, sx, sy, tx, ty);

  drawThick3Line(display, CENTER_X, CENTER_Y, hx, hy, COLOR_HOUR_HAND);
  drawThick3Line(display, CENTER_X, CENTER_Y, mx, my, COLOR_MIN_HAND);
  display.drawLine(CENTER_X, CENTER_Y, sx, sy, COLOR_SEC_HAND);
  display.drawLine(CENTER_X, CENTER_Y, tx, ty, COLOR_SEC_HAND);
  display.fillCircle(CENTER_X, CENTER_Y, 6, COLOR_FACE);
  display.fillCircle(CENTER_X, CENTER_Y, 3, COLOR_SEC_HAND);

  prev_hx = hx; prev_hy = hy;
  prev_mx = mx; prev_my = my;
  prev_sx = sx; prev_sy = sy;
  prev_stx = tx; prev_sty = ty;
#if HACKTOR_DEBUG_LEVEL >= 1
  uint32_t elapsedUs = micros() - startUs;
  LOG_PRINTF(1, "[display] full face %lu us\n", static_cast<unsigned long>(elapsedUs));
#endif
}

void calcHourEnd(const tm &currentTime, int &hx, int &hy) {
  int index = ((currentTime.tm_hour % 12) * 5) + (currentTime.tm_min / 12);
  int32_t cx = COS60[index];
  int32_t sy = SIN60[index];
  hx = CENTER_X + static_cast<int>(mulFixed(cx, static_cast<int>(RADIUS * 0.56f)));
  hy = CENTER_Y + static_cast<int>(mulFixed(sy, static_cast<int>(RADIUS * 0.56f)));
}

void calcMinuteEnd(const tm &currentTime, int &mx, int &my) {
  int index = currentTime.tm_min % 60;
  int32_t cx = COS60[index];
  int32_t sy = SIN60[index];
  mx = CENTER_X + static_cast<int>(mulFixed(cx, static_cast<int>(RADIUS * 0.84f)));
  my = CENTER_Y + static_cast<int>(mulFixed(sy, static_cast<int>(RADIUS * 0.84f)));
}

void calcSecondEnds(const tm &currentTime, int &sx, int &sy, int &tx, int &ty) {
  int index = currentTime.tm_sec % 60;
  int32_t cx = COS60[index];
  int32_t sy_fixed = SIN60[index];
  sx = CENTER_X + static_cast<int>(mulFixed(cx, static_cast<int>(RADIUS * 0.90f)));
  sy = CENTER_Y + static_cast<int>(mulFixed(sy_fixed, static_cast<int>(RADIUS * 0.90f)));
  tx = CENTER_X - static_cast<int>(mulFixed(cx, static_cast<int>(RADIUS * 0.12f)));
  ty = CENTER_Y - static_cast<int>(mulFixed(sy_fixed, static_cast<int>(RADIUS * 0.12f)));
}

}  // namespace watchface
