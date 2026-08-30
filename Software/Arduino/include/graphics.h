#pragma once

#include <cstdint>

namespace graphics {

class Graphics {
 public:
  virtual ~Graphics() = default;

  virtual void fillScreen(uint16_t color) = 0;
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) = 0;
  virtual void fillTriangle(
    int16_t x0, int16_t y0,
    int16_t x1, int16_t y1,
    int16_t x2, int16_t y2,
    uint16_t color
  ) = 0;
  virtual void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) = 0;

  virtual uint8_t getRotation() const = 0;
  virtual void setRotation(uint8_t rotation) = 0;

  virtual void drawText(
    int16_t x, int16_t y,
    const char *text,
    uint16_t colorText, uint16_t colorBG,
    uint8_t textSize
  ) = 0;

  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;

  virtual void displayOn() = 0;
  virtual void displayOff() = 0;
};

}  // namespace graphics

