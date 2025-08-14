#pragma once
#include_next <cstdint>

#define ROWS 16
#define COLS 16

struct PixelDisplay {
  virtual ~PixelDisplay() = default;
  virtual void clear() = 0;
  virtual void setPixel(uint8_t x, uint8_t y, uint8_t value,
                        uint8_t brightness = 255) = 0;
};
