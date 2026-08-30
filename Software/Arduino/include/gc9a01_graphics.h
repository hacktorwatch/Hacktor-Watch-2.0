#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "graphics.h"

namespace graphics {

class Gc9a01Graphics : public Graphics {
 public:
  Gc9a01Graphics(uint8_t pin_dc,
                 uint8_t pin_cs,
                 uint8_t pin_rst,
                 bool ips = true);

  bool begin(uint32_t freq_hz);

  void fillScreen(uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override;
  void fillTriangle(int16_t x0, int16_t y0,
                    int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2,
                    uint16_t color) override;
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) override;
  void fillSpan(int16_t x0, int16_t x1, int16_t y, uint16_t color);

  uint8_t getRotation() const override;
  void setRotation(uint8_t rotation) override;

  void drawText(int16_t x, int16_t y,
                const char *text,
                uint16_t colorText, uint16_t colorBG,
                uint8_t textSize) override;

  int16_t width() const override { return width_; }
  int16_t height() const override { return height_; }

  void displayOn() override;
  void displayOff() override;

 private:
  void hardwareReset();
  void initPanel();
  void startWrite();
  void endWrite();
  void writeCommand(uint8_t cmd);
  void writeCommandWithData(uint8_t cmd, const uint8_t *data, size_t len);
  void writeData(const uint8_t *data, size_t len);
  void writeData16(uint16_t value);
  void writeData16Repeat(uint16_t value, size_t count);
  void setAddrWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
  void drawPixel(int16_t x, int16_t y, uint16_t color);
  void pushPixels(const uint16_t *pixels, size_t count);
  void fillSpanInternal(int16_t x0, int16_t x1, int16_t y, uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawChar(int16_t x, int16_t y, char c,
                uint16_t colorText, uint16_t colorBG,
                uint8_t textSize);

  SPIClass spi_;
  SPISettings spi_settings_;
  uint8_t dc_;
  uint8_t cs_;
  uint8_t rst_;
  bool ips_;
  uint8_t rotation_ = 0;
  int16_t width_ = 240;
  int16_t height_ = 240;
  bool initialized_ = false;
};

}  // namespace graphics

