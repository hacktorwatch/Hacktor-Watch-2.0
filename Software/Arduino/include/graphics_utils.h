#pragma once

#include <cstdint>

#include "graphics.h"

namespace graphics {

class RotationScopeCW {
 public:
  explicit RotationScopeCW(Graphics &display, uint8_t steps = 1)
      : display_(display), previous_(display.getRotation()) {
    uint8_t target = static_cast<uint8_t>((previous_ + steps) % 4);
    display_.setRotation(target);
  }

  RotationScopeCW(const RotationScopeCW &) = delete;
  RotationScopeCW &operator=(const RotationScopeCW &) = delete;

  ~RotationScopeCW() { display_.setRotation(previous_); }

 private:
  Graphics &display_;
  uint8_t previous_;
};

}  // namespace graphics

