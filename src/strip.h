#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"

// One NeoPixel strip, always treated as a closed ring.
//
// Each frame the caller clears the strip, adds whatever light it wants in linear
// intensity units, and calls show(), which gamma-encodes and clocks the result
// out. Positions are floating point LED indices so motion stays smooth; every
// distance is the shortest way around the ring, so a blob that straddles the seam
// is drawn on both ends.
class LedStrip {
public:
  // Configures the pin and RMT channel once, and allocates room for `count`
  // pixels.
  bool begin(int pin, uint16_t count, uint8_t rmtChannel = 0);
  bool setCount(uint16_t count);

  uint16_t count() const { return count_; }
  int pin() const { return pin_; }
  bool ready() const { return ready_; }

  void clear();
  void addRing(float centerPos, float halfWidth, float amp, uint8_t r, uint8_t g,
               uint8_t b, bool loop = true);
  void addFill(float amp, uint8_t r, uint8_t g, uint8_t b);
  void addSparkles(float amp, uint32_t seed, uint8_t r, uint8_t g, uint8_t b);
  void show(uint8_t brightnessPct = 100);

  // Calibration mode: blacks out the entire strip and lights only LED
  // `ledCount - 1` so the user can see the exact physical tip of the strip.
  void showCalibrationDot(uint16_t ledCount, uint8_t r, uint8_t g, uint8_t b,
                          uint8_t brightnessPct);

  // Brightest channel of the last rendered frame, 0..1, for the screen and the
  // web page.
  float peak() const { return peak_; }

private:
  int pin_ = -1;
  uint8_t channel_ = 0;
  uint16_t count_ = 0;
  uint16_t maxSentCount_ = 0;
  uint8_t lastBrightnessPct_ = 100;
  uint32_t lastCalibTxMs_ = 0;
  bool inCalib_ = false;
  bool rmtReady_ = false;
  bool ready_ = false;
  // Set whenever the frame changes, so a strip that is dark and stays dark does
  // not get clocked out again every frame.
  bool dirty_ = true;
  float buf_[kMaxLedsPerStrip][3] = {};
  uint8_t grb_[kMaxLedsPerStrip * 3] = {};
  float peak_ = 0.0f;
};
