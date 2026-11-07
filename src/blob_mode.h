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
  int8_t dir = 1;    // +1 forward, -1 reverse (used by ping-pong & center burst)
  uint8_t burst = 0; // 1 = spawned from strip center outward
  bool active = false;
};

// Strip effect engine: owns the travelling blob/burst pool, full-strip flash
// envelope, sparkle shower envelope, pad hold state, and trigger timing for one
// strip.
class BlobEngine {
public:
  // Ages and moves all active effects, then triggers s.effect if the active
  // input (band threshold or beat detector) fires and the retrigger gap has
  // elapsed.
  void update(float bandLevel, bool onBeat, uint32_t nowMs, float dtSec,
              const StripSettings &s);

  // Adds all active effects (flash, sparkle, jets/bursts) to the strip's linear
  // frame buffer. Does not clear or show the strip.
  void render(LedStrip &strip, const StripSettings &s) const;

  // Fires the strip's configured effect (or an explicit pad effect).
  void trigger(const StripSettings &s);
  void triggerEffect(uint8_t effect, const StripSettings &s);

  // Fingertip drum pad press / hold / release from the Web UI.
  void setPadHeld(bool held, uint8_t effect, const StripSettings &s);

  void reset();

  size_t activeCount() const;

private:
  Blob &allocSlot();
  void spawnJet(const StripSettings &s);
  void spawnBurst(const StripSettings &s);

  Blob blobs_[kMaxBlobs];
  float flashAmp_ = 0.0f;
  float sparkleAmp_ = 0.0f;
  uint32_t sparkleSeed_ = 1;
  uint32_t lastSpawnMs_ = 0;
  uint32_t lastHoldJetMs_ = 0;
  uint32_t heldMs_ = 0;
  uint8_t heldEffect_ = StripSettings::kEffectFlash;
  bool held_ = false;
  bool spawned_ = false;
};
