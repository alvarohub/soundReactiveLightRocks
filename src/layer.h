#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "settings.h"
#include "strip.h"

// One travelling light group on one layer.
struct Blob {
  float pos = 0.0f;  // position along the strip, in LEDs
  float amp = 0.0f;  // 0..1, falls linearly to nothing over fadeMs
  int8_t dir = 1;    // +1 forward, -1 reverse (ping-pong and centre bursts)
  uint8_t burst = 0; // 1 = spawned from the strip centre outward
  bool active = false;
};

// Renders ONE layer of light into a physical strip's linear frame buffer.
//
// The engine owns that layer's effect pool, full-strip flash envelope, sparkle
// shower envelope, pad-hold state and trigger timing. It never talks to the
// LEDs directly: the caller clears the physical strip, asks every layer to
// render into it, and then transmits once. That is what makes several layers
// blend additively without ever double-applying gamma.
//
// `ledCount` is a property of the physical strip, so it is passed in rather
// than stored here.
class LayerEngine {
public:
  // Ages and moves all active effects, then fires the configured effect when
  // the layer's trigger input says so.
  void update(float bandLevel, bool onBeat, uint32_t nowMs, float dtSec,
              const LayerSettings &s, uint16_t ledCount);

  // Adds this layer's active effects to the strip's frame buffer, scaled by the
  // layer's intensity weight. Does not clear or show the strip. Also records
  // this layer's own peak so the UI can show per-layer output.
  void render(LedStrip &strip, const LayerSettings &s);

  // Fires the layer's configured effect.
  void trigger(const LayerSettings &s, uint16_t ledCount);
  void triggerEffect(uint8_t effect, const LayerSettings &s, uint16_t ledCount);

  // Fingertip drum pad press / hold / release.
  void setPadHeld(bool held, uint8_t effect);

  void reset();

  size_t activeCount() const;

  // Brightest value this layer contributed to the last render, 0..1.
  float peak() const { return peak_; }

private:
  Blob &allocSlot();
  void spawnJet(const LayerSettings &s, uint16_t ledCount);
  void spawnBurst(const LayerSettings &s, uint16_t ledCount);

  Blob blobs_[kMaxBlobs];
  float flashAmp_ = 0.0f;
  float sparkleAmp_ = 0.0f;
  float peak_ = 0.0f;
  uint32_t sparkleSeed_ = 1;
  uint32_t lastSpawnMs_ = 0;
  uint32_t lastHoldJetMs_ = 0;
  uint32_t heldMs_ = 0;
  uint8_t heldEffect_ = LayerSettings::kEffectFlash;
  bool held_ = false;
  bool spawned_ = false;
};
