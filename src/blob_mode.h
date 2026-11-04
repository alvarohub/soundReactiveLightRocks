#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "settings.h"
#include "strip.h"

// One travelling light group on one strip.
struct Blob {
  float pos = 0.0f;  // position along the strip, in LEDs
  float amp = 0.0f;  // 0..1, falls linearly to nothing over fadeMs
  int8_t dir = 1;    // +1 forward, -1 reverse (used by ping-pong bounce)
  bool active = false;
};

// Mode 1: a blob of light orbits the ring and fades out.
//
// The engine owns the blob pool and the trigger timing for one strip. Spawning is
// driven by the caller's band level rather than by the engine itself, so the
// audio path stays in one place in main.cpp.
class BlobEngine {
public:
  // Ages and moves every blob, then spawns one if the band is loud enough and
  // the retrigger gap has elapsed.
  void update(float bandLevel, uint32_t nowMs, float dtSec, const StripSettings &s);

  // Adds the blobs to the strip's frame. Does not clear or show the strip.
  void render(LedStrip &strip, const StripSettings &s) const;

  // Absolute trigger, used by the on-screen test button.
  void trigger(const StripSettings &s);

  void reset();

  size_t activeCount() const;

private:
  void spawn(const StripSettings &s);
  void recycle(const StripSettings &s);

  Blob blobs_[kMaxBlobs];
  uint32_t lastSpawnMs_ = 0;
  bool spawned_ = false;
};
