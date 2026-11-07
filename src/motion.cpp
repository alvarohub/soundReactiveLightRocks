#include "motion.h"

#include <M5Unified.h>
#include <math.h>

namespace {

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

} // namespace

bool MotionSensor::begin() {
  begun_ = M5.Imu.isEnabled();
  if (!begun_) {
    Serial.println("[imu] no IMU on this board, motion triggers disabled");
  }
  state_ = MotionState();
  return begun_;
}

void MotionSensor::update() {
  if (!begun_) {
    return;
  }

  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  float gx = 0.0f, gy = 0.0f, gz = 0.0f;
  const bool gotA = M5.Imu.getAccel(&ax, &ay, &az);
  const bool gotG = M5.Imu.getGyro(&gx, &gy, &gz);
  if (!gotA && !gotG) {
    state_.ok = false;
    return;
  }

  state_.ok = true;
  state_.ax = ax;
  state_.ay = ay;
  state_.az = az;
  state_.gx = gx;
  state_.gy = gy;
  state_.gz = gz;

  // Removing gravity is what makes a stationary stick read 0 and a whip read
  // large, instead of "1 g all the time".
  const float mag = sqrtf(ax * ax + ay * ay + az * az);
  state_.swing = fabsf(mag - 1.0f);
  state_.twist = sqrtf(gx * gx + gy * gy + gz * gz);
}

float MotionSensor::swingNorm() const {
  return clamp01(state_.swing / kSwingFullScale);
}

float MotionSensor::twistNorm() const {
  return clamp01(state_.twist / kTwistFullScale);
}

float MotionState::hue() const {
  const float ang = atan2f(ay, ax);   // -pi..pi
  float h = ang / 6.28318531f + 0.5f; // 0..1
  if (h < 0.0f) {
    h += 1.0f;
  }
  if (h >= 1.0f) {
    h -= 1.0f;
  }
  return h;
}
