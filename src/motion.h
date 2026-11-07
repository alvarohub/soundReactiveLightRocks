#pragma once

#include <stdint.h>

// One frame's worth of motion data from the built-in IMU.
struct MotionState {
  float ax = 0.0f, ay = 0.0f, az = 0.0f; // acceleration, in g
  float gx = 0.0f, gy = 0.0f, gz = 0.0f; // angular rate, in deg/s
  float swing = 0.0f;  // |accel| with gravity removed, 0..~2 g
  float twist = 0.0f;  // |gyro|, deg/s
  bool ok = false;     // false when the board has no usable IMU

  // Direction of the swing as a hue in 0..1, used to colour the whip effect.
  // The horizontal plane is the most intuitive "which way did I whip it".
  float hue() const;
};

// Reads the on-board IMU once per frame and exposes it in the units the light
// layers want: `swing` is the magnitude of the *extra* acceleration (gravity
// removed, so a stick lying still reads 0), and `twist` is the angular rate.
// Both are also published normalised 0..1 against a fixed full scale.
class MotionSensor {
public:
  static constexpr float kSwingFullScale = 2.0f;  // g
  static constexpr float kTwistFullScale = 500.0f; // deg/s

  bool begin();
  void update();

  const MotionState &state() const { return state_; }

  float swingNorm() const;
  float twistNorm() const;

  // Direction of the swing as a hue in 0..1, used to colour the whip effect.
  // Uses the horizontal plane when the stick is mostly upright, so a left/right
  // or forward/back whip gives clearly different colours.

private:
  MotionState state_;
  bool begun_ = false;
};
