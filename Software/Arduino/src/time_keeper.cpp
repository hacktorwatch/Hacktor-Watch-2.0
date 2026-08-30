#include "time_keeper.h"

#include "app_state.h"
#include "steps.h"

#include <Arduino.h>
#include "esp_attr.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h> //

namespace {

struct PersistedTime {
  uint32_t magic;
  int32_t sec;
  int32_t min;
  int32_t hour;
  int32_t mday;
  int32_t mon;
  int32_t year;
  int32_t wday;
};

constexpr uint32_t kPersistMagic = 0x54494D45;  // 'TIME'

RTC_NOINIT_ATTR PersistedTime s_rtcPersisted;

int weekdayFromDate(int y, int m, int d) {
  if (m < 3) {
    m += 12;
    y--;
  }
  int K = y % 100;
  int J = y / 100;
  int h = (d + 13 * (m + 1) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
  return (h + 6) % 7;
}

bool isLeapYear(int year) {
  if ((year % 4) != 0) {
    return false;
  }
  if ((year % 100) != 0) {
    return true;
  }
  return (year % 400) == 0;
}

int daysInMonth(int year, int monthZeroBased) {
  static const int daysPerMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  int base = daysPerMonth[monthZeroBased];
  if (monthZeroBased == 1 && isLeapYear(year)) {
    return 29;
  }
  return base;
}

tm buildCompileTimeTm() {
  tm compileTime{};
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mStr[4] = {};
  int day = 0;
  int year = 0;
  int hour = 0;
  int min = 0;
  int sec = 0;

  sscanf(__DATE__, "%3s %d %d", mStr, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);
  int month = static_cast<int>((strstr(months, mStr) - months) / 3) + 1;

  compileTime.tm_sec = sec;
  compileTime.tm_min = min;
  compileTime.tm_hour = hour;
  compileTime.tm_mday = day;
  compileTime.tm_mon = month - 1;
  compileTime.tm_year = year - 1900;
  compileTime.tm_wday = weekdayFromDate(year, month, day);
  compileTime.tm_isdst = -1;
  compileTime.tm_yday = 0;

  return compileTime;
}

bool loadPersistedTime(tm &out) {
  if (s_rtcPersisted.magic != kPersistMagic) {
    return false;
  }

  if (s_rtcPersisted.sec < 0 || s_rtcPersisted.sec >= 60) {
    return false;
  }
  if (s_rtcPersisted.min < 0 || s_rtcPersisted.min >= 60) {
    return false;
  }
  if (s_rtcPersisted.hour < 0 || s_rtcPersisted.hour >= 24) {
    return false;
  }
  if (s_rtcPersisted.mon < 0 || s_rtcPersisted.mon >= 12) {
    return false;
  }
  if (s_rtcPersisted.mday <= 0 || s_rtcPersisted.mday > 31) {
    return false;
  }
  if (s_rtcPersisted.year < 70 || s_rtcPersisted.year > 300) {  // tm_year (years since 1900)
    return false;
  }
  if (s_rtcPersisted.wday < 0 || s_rtcPersisted.wday >= 7) {
    return false;
  }

  out.tm_sec = s_rtcPersisted.sec;
  out.tm_min = s_rtcPersisted.min;
  out.tm_hour = s_rtcPersisted.hour;
  out.tm_mday = s_rtcPersisted.mday;
  out.tm_mon = s_rtcPersisted.mon;
  out.tm_year = s_rtcPersisted.year;
  out.tm_wday = s_rtcPersisted.wday;
  out.tm_isdst = -1;
  out.tm_yday = 0;

  return true;
}

void persistCurrentTime(const tm &current) {
  s_rtcPersisted.sec = current.tm_sec;
  s_rtcPersisted.min = current.tm_min;
  s_rtcPersisted.hour = current.tm_hour;
  s_rtcPersisted.mday = current.tm_mday;
  s_rtcPersisted.mon = current.tm_mon;
  s_rtcPersisted.year = current.tm_year;
  s_rtcPersisted.wday = current.tm_wday;
  s_rtcPersisted.magic = kPersistMagic;
}
}  // namespace

namespace time_keeper {

void initializeFromCompileTime() {
  auto &state = app_state::get();
  auto &display = state.display;
  if (!loadPersistedTime(display.currentTime)) {
    display.currentTime = buildCompileTimeTm();
    persistCurrentTime(display.currentTime);
  }
}

void setCurrentTime(const tm &newTime) {
  auto &state = app_state::get();
  auto &display = state.display;
  unsigned long now = millis();
  display.currentTime = newTime;
  display.rtcBaseMs = now;
  display.lastTickMs = now;
  persistCurrentTime(display.currentTime);
}

void applyElapsedWalltime() {
  auto &state = app_state::get();
  auto &display = state.display;
  unsigned long now = millis();
  unsigned long elapsed = now - display.rtcBaseMs;
  if (elapsed == 0) {
    return;
  }
  unsigned long secs = elapsed / 1000UL;
  if (secs == 0) {
    return;
  }
  display.rtcBaseMs += secs * 1000UL;

  for (unsigned long i = 0; i < secs; ++i) {
    if (++display.currentTime.tm_sec >= 60) {
      display.currentTime.tm_sec = 0;
      if (++display.currentTime.tm_min >= 60) {
        display.currentTime.tm_min = 0;
        if (++display.currentTime.tm_hour >= 24) {
          display.currentTime.tm_hour = 0;
          display.currentTime.tm_wday = (display.currentTime.tm_wday + 1) % 7;
          int year = display.currentTime.tm_year + 1900;
          int monthZeroBased = display.currentTime.tm_mon;
          int monthDays = daysInMonth(year, monthZeroBased);
          if (++display.currentTime.tm_mday > monthDays) {
            display.currentTime.tm_mday = 1;
            if (++display.currentTime.tm_mon >= 12) {
              display.currentTime.tm_mon = 0;
              ++display.currentTime.tm_year;
            }
          }
          steps::resetDailyBaseline();
        }
      }
    }
  }

  persistCurrentTime(display.currentTime);
}

}  // namespace time_keeper
