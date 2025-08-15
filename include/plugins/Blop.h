#pragma once

#include "PixelDisplay.h"
#include "Plugin.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>

class BlobPlugin : public Plugin {
public:
  static constexpr float aspect_ratio = 1.5f;
  static constexpr uint8_t X_MAX = 16;
  static constexpr uint8_t Y_MAX = static_cast<uint8_t>(X_MAX * aspect_ratio);

  static constexpr uint8_t NUM_BALLS = 5;

  BlobPlugin(PixelDisplay &display);
  virtual ~BlobPlugin() {}

  void setup() override;
  void loop() override;
  const char *getName() const override;

private:
  PixelDisplay &display;
  struct Ball {
    float x, y;
    float vx, vy;
  };

  Ball balls[NUM_BALLS];

  static constexpr float RADIUS = 8.0f;
  static constexpr float SPEED = .1f;
  static constexpr float CAP_VALUE = NUM_BALLS * .75;
  static constexpr float GAMMA = 0.71f;
  static constexpr float brightness = 128.0f;

  float attenuation(float d, float radius) const;
  uint8_t toneMap(float v) const;
  void updatePositions();
};
