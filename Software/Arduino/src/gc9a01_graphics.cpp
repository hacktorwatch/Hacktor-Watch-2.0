#include "gc9a01_graphics.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#include "hardware_pins.h"

namespace {

constexpr uint16_t kScreenSize = 240;

// 5x7 font (ASCII 0x20..0x7F). Derived from public domain font data.
constexpr uint8_t kFont5x7[96][5] = {
  {0x00,0x00,0x00,0x00,0x00}, // ' '
  {0x00,0x00,0x5f,0x00,0x00}, // '!'
  {0x00,0x03,0x00,0x03,0x00}, // '"'
  {0x14,0x7f,0x14,0x7f,0x14}, // '#'
  {0x24,0x2a,0x7f,0x2a,0x12}, // '$'
  {0x23,0x13,0x08,0x64,0x62}, // '%'
  {0x36,0x49,0x55,0x22,0x50}, // '&'
  {0x00,0x05,0x03,0x00,0x00}, // '\''
  {0x00,0x1c,0x22,0x41,0x00}, // '('
  {0x00,0x41,0x22,0x1c,0x00}, // ')'
  {0x14,0x08,0x3e,0x08,0x14}, // '*'
  {0x08,0x08,0x3e,0x08,0x08}, // '+'
  {0x00,0x50,0x30,0x00,0x00}, // ','
  {0x08,0x08,0x08,0x08,0x08}, // '-'
  {0x00,0x60,0x60,0x00,0x00}, // '.'
  {0x20,0x10,0x08,0x04,0x02}, // '/' 
  {0x3e,0x51,0x49,0x45,0x3e}, // '0'
  {0x00,0x42,0x7f,0x40,0x00}, // '1'
  {0x42,0x61,0x51,0x49,0x46}, // '2'
  {0x21,0x41,0x45,0x4b,0x31}, // '3'
  {0x18,0x14,0x12,0x7f,0x10}, // '4'
  {0x27,0x45,0x45,0x45,0x39}, // '5'
  {0x3c,0x4a,0x49,0x49,0x30}, // '6'
  {0x01,0x71,0x09,0x05,0x03}, // '7'
  {0x36,0x49,0x49,0x49,0x36}, // '8'
  {0x06,0x49,0x49,0x29,0x1e}, // '9'
  {0x00,0x36,0x36,0x00,0x00}, // ':'
  {0x00,0x56,0x36,0x00,0x00}, // ';'
  {0x08,0x14,0x22,0x41,0x00}, // '<'
  {0x14,0x14,0x14,0x14,0x14}, // '='
  {0x00,0x41,0x22,0x14,0x08}, // '>'
  {0x02,0x01,0x51,0x09,0x06}, // '?'
  {0x3e,0x41,0x5d,0x59,0x4e}, // '@'
  {0x7e,0x11,0x11,0x11,0x7e}, // 'A'
  {0x7f,0x49,0x49,0x49,0x36}, // 'B'
  {0x3e,0x41,0x41,0x41,0x22}, // 'C'
  {0x7f,0x41,0x41,0x22,0x1c}, // 'D'
  {0x7f,0x49,0x49,0x49,0x41}, // 'E'
  {0x7f,0x09,0x09,0x09,0x01}, // 'F'
  {0x3e,0x41,0x49,0x49,0x7a}, // 'G'
  {0x7f,0x08,0x08,0x08,0x7f}, // 'H'
  {0x00,0x41,0x7f,0x41,0x00}, // 'I'
  {0x20,0x40,0x41,0x3f,0x01}, // 'J'
  {0x7f,0x08,0x14,0x22,0x41}, // 'K'
  {0x7f,0x40,0x40,0x40,0x40}, // 'L'
  {0x7f,0x02,0x0c,0x02,0x7f}, // 'M'
  {0x7f,0x04,0x08,0x10,0x7f}, // 'N'
  {0x3e,0x41,0x41,0x41,0x3e}, // 'O'
  {0x7f,0x09,0x09,0x09,0x06}, // 'P'
  {0x3e,0x41,0x51,0x21,0x5e}, // 'Q'
  {0x7f,0x09,0x19,0x29,0x46}, // 'R'
  {0x26,0x49,0x49,0x49,0x32}, // 'S'
  {0x01,0x01,0x7f,0x01,0x01}, // 'T'
  {0x3f,0x40,0x40,0x40,0x3f}, // 'U'
  {0x1f,0x20,0x40,0x20,0x1f}, // 'V'
  {0x7f,0x20,0x18,0x20,0x7f}, // 'W'
  {0x63,0x14,0x08,0x14,0x63}, // 'X'
  {0x07,0x08,0x70,0x08,0x07}, // 'Y'
  {0x61,0x51,0x49,0x45,0x43}, // 'Z'
  {0x00,0x7f,0x41,0x41,0x00}, // '['
  {0x02,0x04,0x08,0x10,0x20}, // '\\'
  {0x00,0x41,0x41,0x7f,0x00}, // ']'
  {0x04,0x02,0x01,0x02,0x04}, // '^'
  {0x80,0x80,0x80,0x80,0x80}, // '_'
  {0x00,0x01,0x02,0x04,0x00}, // '`'
  {0x20,0x54,0x54,0x54,0x78}, // 'a'
  {0x7f,0x48,0x44,0x44,0x38}, // 'b'
  {0x38,0x44,0x44,0x44,0x20}, // 'c'
  {0x38,0x44,0x44,0x48,0x7f}, // 'd'
  {0x38,0x54,0x54,0x54,0x18}, // 'e'
  {0x08,0x7e,0x09,0x01,0x02}, // 'f'
  {0x0c,0x52,0x52,0x52,0x3e}, // 'g'
  {0x7f,0x08,0x04,0x04,0x78}, // 'h'
  {0x00,0x44,0x7d,0x40,0x00}, // 'i'
  {0x20,0x40,0x44,0x3d,0x00}, // 'j'
  {0x7f,0x10,0x28,0x44,0x00}, // 'k'
  {0x00,0x41,0x7f,0x40,0x00}, // 'l'
  {0x7c,0x04,0x18,0x04,0x78}, // 'm'
  {0x7c,0x08,0x04,0x04,0x78}, // 'n'
  {0x38,0x44,0x44,0x44,0x38}, // 'o'
  {0x7c,0x14,0x14,0x14,0x08}, // 'p'
  {0x08,0x14,0x14,0x18,0x7c}, // 'q'
  {0x7c,0x08,0x04,0x04,0x08}, // 'r'
  {0x48,0x54,0x54,0x54,0x20}, // 's'
  {0x04,0x3f,0x44,0x40,0x20}, // 't'
  {0x3c,0x40,0x40,0x20,0x7c}, // 'u'
  {0x1c,0x20,0x40,0x20,0x1c}, // 'v'
  {0x3c,0x40,0x30,0x40,0x3c}, // 'w'
  {0x44,0x28,0x10,0x28,0x44}, // 'x'
  {0x0c,0x50,0x50,0x50,0x3c}, // 'y'
  {0x44,0x64,0x54,0x4c,0x44}, // 'z'
  {0x00,0x08,0x36,0x41,0x00}, // '{'
  {0x00,0x00,0x7f,0x00,0x00}, // '|'
  {0x00,0x41,0x36,0x08,0x00}, // '}'
  {0x10,0x08,0x08,0x10,0x08}, // '~'
  {0x00,0x00,0x00,0x00,0x00}, // DEL -> unused
};

constexpr uint8_t MADCTL_MY = 0x80;
constexpr uint8_t MADCTL_MX = 0x40;
constexpr uint8_t MADCTL_MV = 0x20;
constexpr uint8_t MADCTL_BGR = 0x08;

struct PanelCommand {
  uint8_t cmd;
  const uint8_t *data;
  uint8_t len;
  uint16_t delay_ms;
};

// Initialization commands ported from the Arduino_GFX library (MIT licensed).
constexpr uint8_t D0[] = {0x00};
constexpr uint8_t D14[] = {0x14};
constexpr uint8_t D40[] = {0x40};
constexpr uint8_t DFF[] = {0xFF};
constexpr uint8_t D0A[] = {0x0A};
constexpr uint8_t D21[] = {0x21};
constexpr uint8_t D80[] = {0x80};
constexpr uint8_t D01[] = {0x01};
constexpr uint8_t D20[] = {0x00, 0x20};
constexpr uint8_t D08[] = {0x08};
constexpr uint8_t D05[] = {0x05};
constexpr uint8_t D09080808[] = {0x08,0x08,0x08,0x08};
constexpr uint8_t D06[] = {0x06};
constexpr uint8_t D600104[] = {0x60,0x01,0x04};
constexpr uint8_t D13[] = {0x13};
constexpr uint8_t D22[] = {0x22};
constexpr uint8_t D11[] = {0x11};
constexpr uint8_t D100E[] = {0x10,0x0E};
constexpr uint8_t D210C02[] = {0x21,0x0C,0x02};
constexpr uint8_t D45090808262A[] = {0x45,0x09,0x08,0x08,0x26,0x2A};
constexpr uint8_t D43707236376F[] = {0x43,0x70,0x72,0x36,0x37,0x6F};
constexpr uint8_t D1B0B[] = {0x1B,0x0B};
constexpr uint8_t D77[] = {0x77};
constexpr uint8_t D63[] = {0x63};
constexpr uint8_t D0707040E0F09070803[] = {0x07,0x07,0x04,0x0E,0x0F,0x09,0x07,0x08,0x03};
constexpr uint8_t D34[] = {0x34};
constexpr uint8_t D620[] = {0x18,0x0D,0x71,0xED,0x70,0x70,0x18,0x0F,0x71,0xEF,0x70,0x70};
constexpr uint8_t D630[] = {0x18,0x11,0x71,0xF1,0x70,0x70,0x18,0x13,0x71,0xF3,0x70,0x70};
constexpr uint8_t D640[] = {0x28,0x29,0xF1,0x01,0xF1,0x00,0x07};
constexpr uint8_t D660[] = {0x3C,0x00,0xCD,0x67,0x45,0x45,0x10,0x00,0x00,0x00};
constexpr uint8_t D670[] = {0x00,0x3C,0x00,0x00,0x00,0x01,0x54,0x10,0x32,0x98};
constexpr uint8_t D740[] = {0x10,0x85,0x80,0x00,0x00,0x4E,0x00};
constexpr uint8_t D3E07[] = {0x3E,0x07};
const PanelCommand kInitCommands[] = {
  {0xEF, D0, 1, 0},
  {0xEB, D14, 1, 0},
  {0xFE, nullptr, 0, 0},
  {0xEF, nullptr, 0, 0},
  {0xEB, D14, 1, 0},
  {0x84, D40, 1, 0},
  {0x85, DFF, 1, 0},
  {0x86, DFF, 1, 0},
  {0x87, DFF, 1, 0},
  {0x88, D0A, 1, 0},
  {0x89, D21, 1, 0},
  {0x8A, D0, 1, 0},
  {0x8B, D80, 1, 0},
  {0x8C, D01, 1, 0},
  {0x8D, D01, 1, 0},
  {0x8E, DFF, 1, 0},
  {0x8F, DFF, 1, 0},
  {0xB6, D20, 2, 0},
  {0x36, D08, 1, 0},
  {0x3A, D05, 1, 0},
  {0x90, D09080808, 4, 0},
  {0xBD, D06, 1, 0},
  {0xBC, D0, 1, 0},
  {0xFF, D600104, 3, 0},
  {0xC3, D13, 1, 0},
  {0xC4, D13, 1, 0},
  {0xC9, D22, 1, 0},
  {0xBE, D11, 1, 0},
  {0xE1, D100E, 2, 0},
  {0xDF, D210C02, 3, 0},
  {0xF0, D45090808262A, 6, 0},
  {0xF1, D43707236376F, 6, 0},
  {0xF2, D45090808262A, 6, 0},
  {0xF3, D43707236376F, 6, 0},
  {0xED, D1B0B, 2, 0},
  {0xAE, D77, 1, 0},
  {0xCD, D63, 1, 0},
  {0x70, D0707040E0F09070803, 9, 0},
  {0xE8, D34, 1, 0},
  {0x62, D620, 12, 0},
  {0x63, D630, 12, 0},
  {0x64, D640, 7, 0},
  {0x66, D660, 10, 0},
  {0x67, D670, 10, 0},
  {0x74, D740, 7, 0},
  {0x98, D3E07, 2, 0},
  {0x35, nullptr, 0, 0},
  {0x21, nullptr, 0, 0},
  {0x11, nullptr, 0, 120},
  {0x29, nullptr, 0, 20},
};
}  // namespace

namespace graphics {

Gc9a01Graphics::Gc9a01Graphics(uint8_t pin_dc,
                               uint8_t pin_cs,
                               uint8_t pin_rst,
                               bool ips)
    : spi_(HSPI),
      dc_(pin_dc),
      cs_(pin_cs),
      rst_(pin_rst),
      ips_(ips) {}

bool Gc9a01Graphics::begin(uint32_t freq_hz) {
  pinMode(dc_, OUTPUT);
  pinMode(cs_, OUTPUT);
  digitalWrite(dc_, HIGH);
  digitalWrite(cs_, HIGH);

  if (rst_ != 0xFF) {
    pinMode(rst_, OUTPUT);
    digitalWrite(rst_, HIGH);
  }

  spi_.end();
  spi_.begin(pins::LCD_SCK, -1, pins::LCD_MOSI, cs_);
  delay(20);
  spi_settings_ = SPISettings(freq_hz, MSBFIRST, SPI_MODE0);

  hardwareReset();
  initPanel();

  initialized_ = true;
  return true;
}

void Gc9a01Graphics::hardwareReset() {
  if (rst_ == 0xFF) {
    writeCommand(0x01);  // Software reset
    delay(120);
    return;
  }
  digitalWrite(rst_, HIGH);
  delay(120);
  digitalWrite(rst_, LOW);
  delay(20);
  digitalWrite(rst_, HIGH);
  delay(120);
}

void Gc9a01Graphics::initPanel() {
  for (const auto &item : kInitCommands) {
    if (item.data && item.len) {
      writeCommandWithData(item.cmd, item.data, item.len);
    } else {
      writeCommand(item.cmd);
    }
    if (item.delay_ms) {
      delay(item.delay_ms);
    }
  }
  setRotation(1);
}

void Gc9a01Graphics::fillScreen(uint16_t color) {
  fillRect(0, 0, width_, height_, color);
}

void Gc9a01Graphics::startWrite() {
  spi_.beginTransaction(spi_settings_);
  digitalWrite(cs_, LOW);
}

void Gc9a01Graphics::endWrite() {
  digitalWrite(cs_, HIGH);
  spi_.endTransaction();
}

void Gc9a01Graphics::writeCommand(uint8_t cmd) {
  startWrite();
  digitalWrite(dc_, LOW);
  spi_.write(cmd);
  digitalWrite(dc_, HIGH);
  endWrite();
}

void Gc9a01Graphics::writeCommandWithData(uint8_t cmd, const uint8_t *data, size_t len) {
  startWrite();
  digitalWrite(dc_, LOW);
  spi_.write(cmd);
  digitalWrite(dc_, HIGH);
  if (data && len) {
    spi_.writeBytes(data, len);
  }
  endWrite();
}

void Gc9a01Graphics::writeData(const uint8_t *data, size_t len) {
  spi_.writeBytes(data, len);
}

void Gc9a01Graphics::writeData16(uint16_t value) {
  uint8_t buffer[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
  writeData(buffer, 2);
}

void Gc9a01Graphics::writeData16Repeat(uint16_t value, size_t count) {
  uint8_t hi = static_cast<uint8_t>(value >> 8);
  uint8_t lo = static_cast<uint8_t>(value & 0xFF);
  uint8_t chunk[64];
  for (size_t i = 0; i < sizeof(chunk); i += 2) {
    chunk[i] = hi;
    chunk[i + 1] = lo;
  }
  while (count > 0) {
    size_t batch = std::min(count, sizeof(chunk) / 2);
    spi_.writeBytes(chunk, batch * 2);
    count -= batch;
  }
}

void Gc9a01Graphics::setAddrWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  digitalWrite(dc_, LOW);
  spi_.write(0x2A);
  digitalWrite(dc_, HIGH);
  writeData16(x0);
  writeData16(x1);

  digitalWrite(dc_, LOW);
  spi_.write(0x2B);
  digitalWrite(dc_, HIGH);
  writeData16(y0);
  writeData16(y1);

  digitalWrite(dc_, LOW);
  spi_.write(0x2C);
  digitalWrite(dc_, HIGH);
}

void Gc9a01Graphics::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  if (x >= width_ || y >= height_) return;
  int16_t x1 = std::min<int16_t>(x + w - 1, width_ - 1);
  int16_t y1 = std::min<int16_t>(y + h - 1, height_ - 1);
  if (x1 < 0 || y1 < 0) return;
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  const int16_t spanWidth = x1 - x + 1;
  const int16_t spanHeight = y1 - y + 1;

  startWrite();
  setAddrWindow(x, y, x1, y1);

  static uint8_t lineBuf[240 * 2];
  const uint8_t hi = static_cast<uint8_t>(color >> 8);
  const uint8_t lo = static_cast<uint8_t>(color & 0xFF);
  const int bytesPerLine = spanWidth * 2;
  if (bytesPerLine > static_cast<int>(sizeof(lineBuf))) {
    endWrite();
    return;
  }
  for (int16_t i = 0; i < spanWidth; ++i) {
    lineBuf[i * 2]     = hi;
    lineBuf[i * 2 + 1] = lo;
  }
  for (int16_t row = 0; row < spanHeight; ++row) {
    writeData(lineBuf, bytesPerLine);
  }
  endWrite();
}

void Gc9a01Graphics::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y + h - 1, w, color);
  if (h > 2) {
    drawFastVLine(x, y + 1, h - 2, color);
    drawFastVLine(x + w - 1, y + 1, h - 2, color);
  }
}

void Gc9a01Graphics::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if (h <= 0) return;
  if (x < 0 || x >= width_) return;
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (y + h > height_) {
    h = height_ - y;
  }
  if (h <= 0) return;
  startWrite();
  setAddrWindow(x, y, x, y + h - 1);
  writeData16Repeat(color, h);
  endWrite();
}

void Gc9a01Graphics::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if (w <= 0) return;
  fillSpan(x, x + w - 1, y, color);
}

void Gc9a01Graphics::fillSpanInternal(int16_t x0, int16_t x1, int16_t y, uint16_t color) {
  if (y < 0 || y >= height_) return;
  if (x0 > x1) std::swap(x0, x1);
  if (x1 < 0 || x0 >= width_) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= width_) x1 = width_ - 1;
  startWrite();
  setAddrWindow(x0, y, x1, y);
  int count = x1 - x0 + 1;
  writeData16Repeat(color, count);
  endWrite();
}

void Gc9a01Graphics::fillSpan(int16_t x0, int16_t x1, int16_t y, uint16_t color) {
  fillSpanInternal(x0, x1, y, color);
}

void Gc9a01Graphics::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  startWrite();
  setAddrWindow(x, y, x, y);
  writeData16(color);
  endWrite();
}

void Gc9a01Graphics::pushPixels(const uint16_t *pixels, size_t count) {
  constexpr size_t kChunk = 64;
  uint8_t buffer[kChunk * 2];
  while (count > 0) {
    size_t batch = std::min(count, kChunk);
    for (size_t i = 0; i < batch; ++i) {
      uint16_t value = pixels[i];
      buffer[2 * i]     = static_cast<uint8_t>(value >> 8);
      buffer[2 * i + 1] = static_cast<uint8_t>(value & 0xFF);
    }
    writeData(buffer, batch * 2);
    pixels += batch;
    count  -= batch;
  }
}


void Gc9a01Graphics::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  if (x0 == x1) {
    if (y0 > y1) std::swap(y0, y1);
    drawFastVLine(x0, y0, y1 - y0 + 1, color);
    return;
  }
  if (y0 == y1) {
    if (x0 > x1) std::swap(x0, x1);
    drawFastHLine(x0, y0, x1 - x0 + 1, color);
    return;
  }

  int16_t dx = std::abs(x1 - x0);
  int16_t sx = x0 < x1 ? 1 : -1;
  int16_t dy = -std::abs(y1 - y0);
  int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;

  while (true) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = err << 1;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Gc9a01Graphics::fillTriangle(int16_t x0, int16_t y0,
                                  int16_t x1, int16_t y1,
                                  int16_t x2, int16_t y2,
                                  uint16_t color) {
  if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
  if (y1 > y2) { std::swap(y1, y2); std::swap(x1, x2); }
  if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }

  auto edgeInterpolate = [](int16_t y0, int16_t x0, int16_t y1, int16_t x1, int16_t y) -> int16_t {
    if (y1 == y0) return x0;
    return x0 + (int32_t)(x1 - x0) * (y - y0) / (y1 - y0);
  };

  for (int16_t y = y0; y <= y1; ++y) {
    int16_t xa = edgeInterpolate(y0, x0, y2, x2, y);
    int16_t xb = edgeInterpolate(y0, x0, y1, x1, y);
    fillSpan(xa, xb, y, color);
  }
  for (int16_t y = y1 + 1; y <= y2; ++y) {
    int16_t xa = edgeInterpolate(y0, x0, y2, x2, y);
    int16_t xb = edgeInterpolate(y1, x1, y2, x2, y);
    fillSpan(xa, xb, y, color);
  }
}

void Gc9a01Graphics::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  drawFastHLine(x0 - r, y0, 2 * r + 1, color);
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    drawFastHLine(x0 - x, y0 + y, 2 * x + 1, color);
    drawFastHLine(x0 - x, y0 - y, 2 * x + 1, color);
    drawFastHLine(x0 - y, y0 + x, 2 * y + 1, color);
    drawFastHLine(x0 - y, y0 - x, 2 * y + 1, color);
  }
}

uint8_t Gc9a01Graphics::getRotation() const {
  return rotation_;
}

void Gc9a01Graphics::setRotation(uint8_t rotation) {
  rotation_ = rotation % 4;
  uint8_t madctl;
  switch (rotation_) {
    case 0:
      madctl = 0x08;  // BGR
      break;
    case 1:
      madctl = 0x68;  // MX | MV | BGR
      break;
    case 2:
      madctl = 0xC8;  // MX | MY | BGR
      break;
    case 3:
    default:
      madctl = 0xA8;  // MY | MV | BGR
      break;
  }
  width_ = kScreenSize;
  height_ = kScreenSize;
  writeCommandWithData(0x36, &madctl, 1);
}

void Gc9a01Graphics::drawChar(int16_t x, int16_t y, char c,
                              uint16_t colorText, uint16_t colorBG,
                              uint8_t textSize) {
  if (c < 0x20 || c > 0x7F) {
    c = '?';
  }
  if (textSize == 0) {
    return;
  }

  constexpr int kBaseW = 6;
  constexpr int kBaseH = 8;
  constexpr int kMaxScale = 4;
  static uint16_t glyphPixels[kBaseW * kBaseH * kMaxScale * kMaxScale];

  int scale = textSize;
  if (scale > kMaxScale) {
    scale = kMaxScale;
  }

  const int glyphW = kBaseW * scale;
  const int glyphH = kBaseH * scale;
  const size_t totalPixels = static_cast<size_t>(glyphW) * static_cast<size_t>(glyphH);

  std::fill_n(glyphPixels, totalPixels, colorBG);

  const uint8_t *glyph = kFont5x7[c - 0x20];
  for (int col = 0; col < 5; ++col) {
    uint8_t bits = glyph[col];
    for (int row = 0; row < 7; ++row) {
      if (bits & (1 << row)) {
        for (int dy = 0; dy < scale; ++dy) {
          uint16_t *dest = glyphPixels + (row * scale + dy) * glyphW + col * scale;
          for (int dx = 0; dx < scale; ++dx) {
            dest[dx] = colorText;
          }
        }
      }
    }
  }

  startWrite();
  setAddrWindow(x, y, x + glyphW - 1, y + glyphH - 1);
  pushPixels(glyphPixels, totalPixels);
  endWrite();
}

void Gc9a01Graphics::drawText(int16_t x, int16_t y,
                              const char *text,
                              uint16_t colorText, uint16_t colorBG,
                              uint8_t textSize) {
  if (!text) return;

  constexpr int kBaseW = 6;
  constexpr int kBaseH = 8;
  constexpr int kMaxScale = 4;
  int scale = textSize == 0 ? 1 : textSize;
  if (scale > kMaxScale) {
    scale = kMaxScale;
  }
  const int advance = kBaseW * scale;
  const int lineAdvance = kBaseH * scale;

  int16_t cursorX = x;
  while (*text) {
    if (*text == '\n') {
      cursorX = x;
      y += lineAdvance;
      ++text;
      continue;
    }
    drawChar(cursorX, y, *text, colorText, colorBG, scale);
    cursorX += advance;
    ++text;
  }
}

void Gc9a01Graphics::displayOn() {
  writeCommand(0x11);
  delay(120);
  writeCommand(0x29);
  delay(20);
  setRotation(rotation_);
}

void Gc9a01Graphics::displayOff() {
  writeCommand(0x28);
  delay(20);
  writeCommand(0x10);
  delay(120);
}

}  // namespace graphics

