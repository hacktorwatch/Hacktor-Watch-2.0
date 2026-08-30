#include "info_screen.h"

#include <cstdio>
#include <cstring>

#include "watchface.h"
#include "graphics_utils.h"
#include "esp_system.h"

namespace info_screen {

namespace {

const uint16_t COLOR_TEXT = watchface::COLOR_FACE;
const uint16_t COLOR_BG   = watchface::COLOR_BG;

const char *resetReasonToString(uint8_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN: return "UNKNOWN";
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "IWDG";
    case ESP_RST_TASK_WDT: return "TWDG";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLP";
    case ESP_RST_BROWNOUT: return "BROWN";
    case ESP_RST_SDIO: return "SDIO";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    case ESP_RST_EFUSE: return "EFUSE";
    case ESP_RST_PWR_GLITCH: return "PWRGL";
    case ESP_RST_CPU_LOCKUP: return "CPU";
    default: return "?";
  }
}

void formatTime(const tm &time, char *buffer, size_t len) {
  std::snprintf(buffer, len, "%02d:%02d:%02d", time.tm_hour, time.tm_min, time.tm_sec);
}

void formatDate(const tm &time, char *buffer, size_t len) {
  std::snprintf(buffer, len, "%04d-%02d-%02d", time.tm_year + 1900, time.tm_mon + 1, time.tm_mday);
}

void formatBleSync(const system_stats::Stats &stats, char *buffer, size_t len) {
  if (!stats.lastBleSyncValid) {
    std::snprintf(buffer, len, "Last BLE sync: never");
    return;
  }
  char dateBuf[16];
  char timeBuf[16];
  formatDate(stats.lastBleSyncTime, dateBuf, sizeof(dateBuf));
  formatTime(stats.lastBleSyncTime, timeBuf, sizeof(timeBuf));
  std::snprintf(buffer, len, "Last BLE sync: %s %s", dateBuf, timeBuf);
}

}  // namespace

void draw(graphics::Graphics &display, const system_stats::Stats &stats, const tm &currentTime, uint8_t batteryPercent, float batteryVoltage) {
  graphics::RotationScopeCW rotation(display);

  display.fillScreen(COLOR_BG);

  constexpr int glyphWBase = 6;
  constexpr int glyphHBase = 8;

  auto textMetrics = [&](const char *text, uint8_t textSize, uint16_t &w, uint16_t &h) {
    w = static_cast<uint16_t>(std::strlen(text) * glyphWBase * textSize);
    h = static_cast<uint16_t>(glyphHBase * textSize);
  };

  int cursorY = 18;

  auto printCentered = [&](uint8_t textSize, const char *text, int spacing) {
    uint16_t w, h;
    textMetrics(text, textSize, w, h);
    int16_t x = static_cast<int16_t>((display.width() - static_cast<int16_t>(w)) / 2);
    display.drawText(x, cursorY, text, COLOR_TEXT, COLOR_BG, textSize);
    cursorY += static_cast<int>(h) + spacing;
  };

  printCentered(2, "INFO", 10);

  char line[48];
  std::snprintf(line, sizeof(line), "Hard resets: %lu", static_cast<unsigned long>(stats.hardResetCount));
  printCentered(1, line, 6);

  std::snprintf(line, sizeof(line), "Soft resets: %lu", static_cast<unsigned long>(stats.softResetCount));
  printCentered(1, line, 6);

  std::snprintf(line, sizeof(line), "BLE ok/fail: %lu / %lu",
                static_cast<unsigned long>(stats.bleSyncSuccess),
                static_cast<unsigned long>(stats.bleSyncFailures));
  printCentered(1, line, 6);

  if (stats.lastBleSyncValid) {
    char dateBuf[16];
    char timeBuf[16];
    formatDate(stats.lastBleSyncTime, dateBuf, sizeof(dateBuf));
    formatTime(stats.lastBleSyncTime, timeBuf, sizeof(timeBuf));
    printCentered(1, "Last BLE sync:", 4);
    printCentered(1, dateBuf, 4);
    printCentered(1, timeBuf, 6);
  } else {
    printCentered(1, "Last BLE sync: --", 6);
  }

  char clockLine[24];
  char dateLine[24];
  formatTime(currentTime, clockLine, sizeof(clockLine));
  formatDate(currentTime, dateLine, sizeof(dateLine));
  printCentered(1, "Now:", 4);
  printCentered(1, clockLine, 4);
  printCentered(1, dateLine, 6);

  std::snprintf(line, sizeof(line), "Reset reason: %s", resetReasonToString(stats.lastResetReason));
  printCentered(1, line, 4);

  std::snprintf(line, sizeof(line), "Screen wakes: %lu", static_cast<unsigned long>(stats.screenTurnOns));
  printCentered(1, line, 4);

  std::snprintf(line, sizeof(line), "Battery: %u%%  %.2fV",
                static_cast<unsigned>(batteryPercent),
                static_cast<double>(batteryVoltage));
  printCentered(1, line, 0);

}

}  // namespace info_screen
