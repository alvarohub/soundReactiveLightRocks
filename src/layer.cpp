#include "layer.h"

#include <math.h>
#include <stddef.h>

namespace {

inline float initialPos(const LayerSettings &s, uint16_t ledCount) {
  if (s.reverse == LayerSettings::kReverse && ledCount > 1) {
    return (float)(ledCount - 1);
  }
  return 0.0f;
}

inline int8_t initialDir(const LayerSettings &s) {
  return s.reverse == LayerSettings::kReverse ? -1 : 1;
}

// Small HSV->RGB helper for the whip's direction colour (s = v = 1).
void hueToRgb(float h, uint8_t &r, uint8_t &g, uint8_t &b) {
  h -= floorf(h);
  const float sector = h * 6.0f;
  const int i = (int)sector;
  const float f = sector - (float)i;
  const float q = 1.0f - f;
  float rr = 0.0f, gg = 0.0f, bb = 0.0f;
  switch (i % 6) {
  case 0: rr = 1.0f; gg = f; break;
  case 1: rr = q; gg = 1.0f; break;
  case 2: gg = 1.0f; bb = f; break;
  case 3: gg = q; bb = 1.0f; break;
  case 4: rr = f; bb = 1.0f; break;
  default: rr = 1.0f; bb = q; break;
  }
  r = (uint8_t)(rr * 255.0f + 0.5f);
  g = (uint8_t)(gg * 255.0f + 0.5f);
  b = (uint8_t)(bb * 255.0f + 0.5f);
}

} // namespace

void LayerEngine::reset() {
  for (size_t i = 0; i < kMaxBlobs; i++) {
    blobs_[i].active = false;
    blobs_[i].amp = 0.0f;
    blobs_[i].pos = 0.0f;
    blobs_[i].dir = 1;
    blobs_[i].burst = 0;
    blobs_[i].custom = 0;
    blobs_[i].speedScale = 1.0f;
  }
  flashAmp_ = 0.0f;
  sparkleAmp_ = 0.0f;
  glowPhase_ = 0.0f;
  peak_ = 0.0f;
  held_ = false;
  heldMs_ = 0;
  spawned_ = false;
  lastSpawnMs_ = 0;
  lastHoldJetMs_ = 0;
}

Blob &LayerEngine::allocSlot() {
  for (size_t i = 0; i < kMaxBlobs; i++) {
    if (!blobs_[i].active) {
      return blobs_[i];
    }
  }
  // Pool full: recycle the dimmest active blob so the layer stays continuous.
  size_t dimmest = 0;
  for (size_t i = 1; i < kMaxBlobs; i++) {
    if (blobs_[i].amp < blobs_[dimmest].amp) {
      dimmest = i;
    }
  }
  return blobs_[dimmest];
}

void LayerEngine::spawnJet(const LayerSettings &s, uint16_t ledCount) {
  Blob &b = allocSlot();
  b.active = true;
  b.amp = 1.0f;
  b.pos = initialPos(s, ledCount);
  b.prevPos = b.pos;
  b.dir = initialDir(s);
  b.burst = 0;
}

void LayerEngine::spawnBurst(const LayerSettings &s, uint16_t ledCount) {
  const float center = ledCount > 1 ? (float)(ledCount - 1) * 0.5f : 0.0f;
  for (int8_t dir = -1; dir <= 1; dir += 2) {
    Blob &b = allocSlot();
    b.active = true;
    b.amp = 1.0f;
    b.pos = center;
    b.prevPos = center;
    b.dir = dir;
    b.burst = 1;
  }
}

void LayerEngine::spawnWhip(const LayerSettings &s, uint16_t ledCount) {
  const float swing = motion_.swing;
  // Direction of travel follows the horizontal component of the swing, so
  // whipping left sends the light one way and right the other.
  const int8_t dir = (motion_.ax + motion_.ay) >= 0.0f ? 1 : -1;

  Blob &b = allocSlot();
  b.active = true;
  b.amp = 1.0f;
  b.burst = 0;
  b.dir = dir;
  b.pos = dir > 0 ? 0.0f : (ledCount > 1 ? (float)(ledCount - 1) : 0.0f);
  b.prevPos = b.pos;
  b.custom = 1;
  hueToRgb(motion_.hue(), b.cr, b.cg, b.cb);

  // Speed scales with how hard the stick was whipped. The magnitude is baked
  // into a per-blob "speed" by using a short fade instead: the blob keeps the
  // layer speed, but faster whips get a shorter effective trail. To make the
  // travel speed itself depend on the swing, store it on the blob.
  b.speedScale = 0.35f + 2.4f * (swing > 2.0f ? 1.0f : swing / 2.0f);
  (void)s;
}

void LayerEngine::triggerEffect(uint8_t effect, const LayerSettings &s,
                                uint16_t ledCount) {
  switch (effect) {
  case LayerSettings::kEffectFlash:
    flashAmp_ = 1.0f;
    break;
  case LayerSettings::kEffectBurst:
    spawnBurst(s, ledCount);
    break;
  case LayerSettings::kEffectSparkle:
    sparkleAmp_ = 1.0f;
    break;
  case LayerSettings::kEffectWhip:
    spawnWhip(s, ledCount);
    break;
  case LayerSettings::kEffectGlow:
    // Glow breathes on its own; a trigger is a brief boost on top of it.
    flashAmp_ = 1.0f;
    break;
  case LayerSettings::kEffectJet:
  default:
    spawnJet(s, ledCount);
    break;
  }
}

void LayerEngine::trigger(const LayerSettings &s, uint16_t ledCount) {
  triggerEffect(s.effect, s, ledCount);
}

void LayerEngine::setPadHeld(bool held, uint8_t effect) {
  if (held) {
    if (!held_ || heldEffect_ != effect) {
      lastHoldJetMs_ = 0;
    }
    held_ = true;
    heldEffect_ = effect;
    heldMs_ = 0;
  } else {
    held_ = false;
    heldMs_ = 0;
    lastHoldJetMs_ = 0;
  }
}

void LayerEngine::update(float bandLevel, bool onBeat, const MotionState &motion,
                         uint32_t nowMs, float dtSec, const LayerSettings &s,
                         uint16_t ledCount) {
  motion_ = motion;
  const float decayPerSec = 1000.0f / (float)s.fadeMs;
  const float laps = (float)ledCount;
  const float maxPos = laps > 1.0f ? (laps - 1.0f) : 0.0f;
  const float halfWidth = (float)s.blobLeds * 0.5f;
  const uint32_t dtMs = (uint32_t)(dtSec * 1000.0f + 0.5f);

  sparkleSeed_ = sparkleSeed_ * 1664525u + 1013904223u + nowMs;

  // Glow is the one effect that does not need a trigger: it advances its own
  // phase, so the layer breathes continuously (mic and motion independent).
  if (s.effect == LayerSettings::kEffectGlow) {
    glowPhase_ += dtSec * 1000.0f / (float)s.glowPeriodMs;
    if (glowPhase_ >= 1.0f) {
      glowPhase_ -= floorf(glowPhase_);
    }
    // If beats are armed, snap the wave to its peak on each beat so the
    // breathing lands with the music instead of drifting against it.
    if ((s.triggers & kTrigBeat) && onBeat) {
      glowPhase_ = 0.5f; // a raised cosine peaks at half a cycle
    }
  } else {
    glowPhase_ = 0.0f;
  }

  // Safety timeout: if the browser disconnects while holding a pad, release
  // automatically after 2.5 s without a keepalive.
  if (held_) {
    heldMs_ += dtMs > 0 ? dtMs : 1;
    if (heldMs_ > 2500u) {
      held_ = false;
    }
  }

  // Sustain or repeat while a fingertip drum pad is held down.
  if (held_) {
    if (heldEffect_ == LayerSettings::kEffectFlash) {
      flashAmp_ = 1.0f;
    } else if (heldEffect_ == LayerSettings::kEffectSparkle) {
      sparkleAmp_ = 1.0f;
    } else {
      const uint32_t gapMs = s.retriggerMs >= 140 ? s.retriggerMs : 140;
      if (lastHoldJetMs_ == 0) {
        lastHoldJetMs_ = nowMs;
      } else if ((uint32_t)(nowMs - lastHoldJetMs_) >= gapMs) {
        triggerEffect(heldEffect_, s, ledCount);
        lastHoldJetMs_ = nowMs;
      }
    }
  }

  if (!(held_ && heldEffect_ == LayerSettings::kEffectFlash) &&
      flashAmp_ > 0.0f) {
    flashAmp_ -= decayPerSec * dtSec;
    if (flashAmp_ <= kBlobEpsilon) {
      flashAmp_ = 0.0f;
    }
  }

  if (!(held_ && heldEffect_ == LayerSettings::kEffectSparkle) &&
      sparkleAmp_ > 0.0f) {
    sparkleAmp_ -= decayPerSec * dtSec;
    if (sparkleAmp_ <= kBlobEpsilon) {
      sparkleAmp_ = 0.0f;
    }
  }

  for (size_t i = 0; i < kMaxBlobs; i++) {
    Blob &b = blobs_[i];
    if (!b.active) {
      continue;
    }

    // Remember where this blob was, so render() can sweep the path it covers
    // in this frame instead of drawing a single position.
    b.prevPos = b.pos;

    if (b.burst) {
      // Centre-outward shockwave: each half races from the middle to its end.
      b.pos += (float)s.speed * (float)b.dir * dtSec;
      if (s.loop && s.reverse == LayerSettings::kPingPong) {
        if (b.dir > 0 && b.pos >= maxPos) {
          b.pos = maxPos;
          b.dir = -1;
        } else if (b.dir < 0 && b.pos <= 0.0f) {
          b.pos = 0.0f;
          b.dir = 1;
        }
      } else if ((b.dir > 0 && (b.pos - halfWidth >= laps)) ||
                 (b.dir < 0 && (b.pos + halfWidth < 0.0f))) {
        b.amp = 0.0f;
        b.active = false;
        continue;
      }
    } else if (s.reverse == LayerSettings::kPingPong) {
      if (b.dir == 0) {
        b.dir = 1;
      }
      b.pos += (float)s.speed * (float)b.dir * dtSec;
      if (b.dir > 0 && b.pos >= maxPos) {
        b.pos = 2.0f * maxPos - b.pos;
        if (b.pos < 0.0f) {
          b.pos = 0.0f;
        }
        b.dir = -1;
      } else if (b.dir < 0) {
        if (s.loop) {
          if (b.pos <= 0.0f) {
            b.pos = -b.pos;
            if (b.pos > maxPos) {
              b.pos = maxPos;
            }
            b.dir = 1;
          }
        } else if (b.pos + halfWidth < 0.0f) {
          b.amp = 0.0f;
          b.active = false;
          continue;
        }
      }
    } else {
      const float dir = (s.reverse == LayerSettings::kReverse) ? -1.0f : 1.0f;
      const float spd = (b.custom ? (float)s.speed * b.speedScale
                                 : (float)s.speed);
      b.pos += spd * dir * dtSec;
      if (s.loop) {
        while (b.pos < 0.0f) {
          b.pos += laps;
        }
        while (b.pos >= laps) {
          b.pos -= laps;
        }
      } else {
        // One-shot: disappear once the trailing edge leaves the strip.
        if ((dir > 0.0f && (b.pos - halfWidth >= laps)) ||
            (dir < 0.0f && (b.pos + halfWidth < 0.0f))) {
          b.amp = 0.0f;
          b.active = false;
          continue;
        }
      }
    }

    b.amp -= decayPerSec * dtSec;
    if (b.amp <= kBlobEpsilon) {
      b.amp = 0.0f;
      b.active = false;
    }
  }

  // Trigger inputs: the microphone path or the pads, never both.
  //   kManual    -> pads / BtnA only
  //   kBeat      -> per-layer beat detector
  //   kThreshold -> band level reaches thresholdPct
  // Trigger inputs are additive: any armed source fires the layer. Tap is
  // evaluated by the caller, because a pad may request a different effect than
  // the layer's own, so kTrigTap only gates whether pads are allowed to.
  bool shouldTrigger = false;
  if ((s.triggers & kTrigThreshold) &&
      bandLevel * 100.0f >= (float)s.thresholdPct) {
    shouldTrigger = true;
  }
  if ((s.triggers & kTrigBeat) && onBeat) {
    shouldTrigger = true;
  }
  if ((s.triggers & kTrigAccel) && motion.ok &&
      motion.swing / MotionSensor::kSwingFullScale * 100.0f >=
          (float)s.motionThresholdPct) {
    shouldTrigger = true;
  }
  if ((s.triggers & kTrigGyro) && motion.ok &&
      motion.twist / MotionSensor::kTwistFullScale * 100.0f >=
          (float)s.motionThresholdPct) {
    shouldTrigger = true;
  }

  if (!shouldTrigger) {
    return;
  }
  if (!s.retrigger) {
    if (activeCount() > 0) {
      return; // single: wait until this layer is idle again
    }
  } else if (spawned_ && (uint32_t)(nowMs - lastSpawnMs_) < s.retriggerMs) {
    return;
  }

  triggerEffect(s.effect, s, ledCount);
  spawned_ = true;
  lastSpawnMs_ = nowMs;
}

void LayerEngine::render(LedStrip &strip, const LayerSettings &s) {
  // Intensity is meant as "share of the visible brightness", so compensate for
  // the output gamma: without this, 50 % on the slider shows as ~10 % of the
  // light and layers cannot be balanced by eye.
  const float shown = (float)s.intensityPct / 100.0f;
  const float w = powf(shown, 1.0f / kRenderGamma);
  peak_ = 0.0f;

  if (s.effect == LayerSettings::kEffectGlow) {
    const float wave = 0.5f - 0.5f * cosf(6.28318531f * glowPhase_);
    const float amp = wave > 1.0f ? 1.0f : wave;
    strip.addFill(amp, w, s.r, s.g, s.b);
    if (amp > peak_) {
      peak_ = powf(amp, kRenderGamma) * shown;
    }
    // A trigger adds a transient boost on top of the breathing.
    if (flashAmp_ > 0.0f) {
      strip.addFill(flashAmp_ * 0.6f, w, s.r, s.g, s.b);
    }
  } else if (flashAmp_ > 0.0f) {
    strip.addFill(flashAmp_, w, s.r, s.g, s.b);
    peak_ = powf(flashAmp_ > 1.0f ? 1.0f : flashAmp_, kRenderGamma) * shown;
  }
  if (sparkleAmp_ > 0.0f) {
    strip.addSparkles(sparkleAmp_, w, sparkleSeed_, s.r, s.g, s.b);
    const float sp = powf(sparkleAmp_ > 1.0f ? 1.0f : sparkleAmp_, kRenderGamma) * shown;
    if (sp > peak_) {
      peak_ = sp;
    }
  }

  // blobLeds is the full width of the light group, so half each side of the
  // centre. Ping-pong and centre bursts do not wrap across LED 0.
  const float halfWidth = (float)s.blobLeds * 0.5f;
  const bool wrapSeam =
      (s.loop != 0) && (s.reverse != LayerSettings::kPingPong);
  for (size_t i = 0; i < kMaxBlobs; i++) {
    const Blob &b = blobs_[i];
    if (!b.active) {
      continue;
    }
    const uint8_t rr = b.custom ? b.cr : s.r;
    const uint8_t gg = b.custom ? b.cg : s.g;
    const uint8_t bb = b.custom ? b.cb : s.b;
    drawBlob(strip, b, halfWidth, w, rr, gg, bb, b.burst ? false : wrapSeam);
    const float peakAmp = powf(b.amp, kRenderGamma) * shown;
    if (peakAmp > peak_) {
      peak_ = peakAmp;
    }
  }
}

void LayerEngine::drawBlob(LedStrip &strip, const Blob &b, float halfWidth,
                           float weight, uint8_t r, uint8_t g, uint8_t bb,
                           bool wrapSeam) const {
  const uint16_t ledCount = strip.count();
  // How far did it travel since the previous frame? On a ring the shortest way
  // round, otherwise a straight difference.
  const float laps = (float)ledCount;
  float travel = b.pos - b.prevPos;
  if (wrapSeam && laps > 1.0f) {
    while (travel > laps * 0.5f) {
      travel -= laps;
    }
    while (travel < -laps * 0.5f) {
      travel += laps;
    }
  }

  // Sub-frame ghosts. One sample per frame is only fine while the blob moves
  // less than its own width; past that, consecutive frames stop touching and it
  // reads as teleporting. Above 500 LED/s and 31 fps a blob jumps ~16 LEDs.
  constexpr float kGhostStep = 0.6f; // LEDs between ghosts
  constexpr int kMaxGhosts = 12;
  int ghosts = 1 + (int)(fabsf(travel) / kGhostStep);
  if (ghosts > kMaxGhosts) {
    ghosts = kMaxGhosts;
  }

  if (ghosts <= 1) {
    strip.addRing(b.pos, halfWidth, b.amp, weight, r, g, bb, wrapSeam);
    return;
  }

  // The same light spread over the swept path: each ghost carries 1/ghosts of
  // the amplitude, so a fast blob streaks rather than getting brighter.
  const float share = b.amp / (float)ghosts;
  for (int i = 0; i < ghosts; i++) {
    const float t = (float)(i + 1) / (float)ghosts;
    float pos = b.prevPos + travel * t;
    if (wrapSeam && laps > 1.0f) {
      while (pos < 0.0f) {
        pos += laps;
      }
      while (pos >= laps) {
        pos -= laps;
      }
    }
    strip.addRing(pos, halfWidth, share, weight, r, g, bb, wrapSeam);
  }
}

size_t LayerEngine::activeCount() const {
  size_t n = 0;
  for (size_t i = 0; i < kMaxBlobs; i++) {
    if (blobs_[i].active) {
      n++;
    }
  }
  if (flashAmp_ > 0.0f) {
    n++;
  }
  if (sparkleAmp_ > 0.0f) {
    n++;
  }
  return n;
}
