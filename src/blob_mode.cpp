#include "blob_mode.h"

#include <stddef.h>

namespace {

inline float initialPos(const StripSettings &s) {
  if (s.reverse == StripSettings::kReverse && s.ledCount > 1) {
    return (float)(s.ledCount - 1);
  }
  return 0.0f;
}

inline int8_t initialDir(const StripSettings &s) {
  return s.reverse == StripSettings::kReverse ? -1 : 1;
}

} // namespace

void BlobEngine::reset() {
  for (size_t i = 0; i < kMaxBlobs; i++) {
    blobs_[i].active = false;
    blobs_[i].amp = 0.0f;
    blobs_[i].pos = 0.0f;
    blobs_[i].dir = 1;
    blobs_[i].burst = 0;
  }
  flashAmp_ = 0.0f;
  sparkleAmp_ = 0.0f;
  held_ = false;
  heldMs_ = 0;
  spawned_ = false;
  lastSpawnMs_ = 0;
  lastHoldJetMs_ = 0;
}

Blob &BlobEngine::allocSlot() {
  for (size_t i = 0; i < kMaxBlobs; i++) {
    if (!blobs_[i].active) {
      return blobs_[i];
    }
  }
  // Pool full: recycle the dimmest active blob so the strip stays continuous.
  size_t dimmest = 0;
  for (size_t i = 1; i < kMaxBlobs; i++) {
    if (blobs_[i].amp < blobs_[dimmest].amp) {
      dimmest = i;
    }
  }
  return blobs_[dimmest];
}

void BlobEngine::spawnJet(const StripSettings &s) {
  Blob &b = allocSlot();
  b.active = true;
  b.amp = 1.0f;
  b.pos = initialPos(s);
  b.dir = initialDir(s);
  b.burst = 0;
}

void BlobEngine::spawnBurst(const StripSettings &s) {
  const float center =
      s.ledCount > 1 ? (float)(s.ledCount - 1) * 0.5f : 0.0f;
  for (int8_t dir = -1; dir <= 1; dir += 2) {
    Blob &b = allocSlot();
    b.active = true;
    b.amp = 1.0f;
    b.pos = center;
    b.dir = dir;
    b.burst = 1;
  }
}

void BlobEngine::triggerEffect(uint8_t effect, const StripSettings &s) {
  switch (effect) {
  case StripSettings::kEffectFlash:
    flashAmp_ = 1.0f;
    break;
  case StripSettings::kEffectBurst:
    spawnBurst(s);
    break;
  case StripSettings::kEffectSparkle:
    sparkleAmp_ = 1.0f;
    break;
  case StripSettings::kEffectJet:
  default:
    spawnJet(s);
    break;
  }
}

void BlobEngine::trigger(const StripSettings &s) {
  triggerEffect(s.effect, s);
}

void BlobEngine::setPadHeld(bool held, uint8_t effect, const StripSettings &s) {
  (void)s;
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

void BlobEngine::update(float bandLevel, bool onBeat, uint32_t nowMs,
                        float dtSec, const StripSettings &s) {
  // amp goes from 1 to 0 across fadeMs.
  const float decayPerSec = 1000.0f / (float)s.fadeMs;
  const float laps = (float)s.ledCount;
  const float maxPos = laps > 1.0f ? (laps - 1.0f) : 0.0f;
  const float halfWidth = (float)s.blobLeds * 0.5f;
  const uint32_t dtMs = (uint32_t)(dtSec * 1000.0f + 0.5f);

  sparkleSeed_ = sparkleSeed_ * 1664525u + 1013904223u + nowMs;

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
    if (heldEffect_ == StripSettings::kEffectFlash) {
      flashAmp_ = 1.0f;
    } else if (heldEffect_ == StripSettings::kEffectSparkle) {
      sparkleAmp_ = 1.0f;
    } else {
      const uint32_t gapMs = s.retriggerMs >= 140 ? s.retriggerMs : 140;
      if (lastHoldJetMs_ == 0) {
        lastHoldJetMs_ = nowMs;
      } else if ((uint32_t)(nowMs - lastHoldJetMs_) >= gapMs) {
        triggerEffect(heldEffect_, s);
        lastHoldJetMs_ = nowMs;
      }
    }
  }

  // Decay full-strip flash when not held.
  if (!(held_ && heldEffect_ == StripSettings::kEffectFlash) &&
      flashAmp_ > 0.0f) {
    flashAmp_ -= decayPerSec * dtSec;
    if (flashAmp_ <= kBlobEpsilon) {
      flashAmp_ = 0.0f;
    }
  }

  // Decay sparkle shower when not held.
  if (!(held_ && heldEffect_ == StripSettings::kEffectSparkle) &&
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

    if (b.burst) {
      // Center-outward shockwave: each half races from the middle toward its end.
      b.pos += (float)s.speed * (float)b.dir * dtSec;
      if (s.loop && s.reverse == StripSettings::kPingPong) {
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
    } else if (s.reverse == StripSettings::kPingPong) {
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
          // One-shot ping-pong: completed 0 -> end -> 0 round-trip.
          b.amp = 0.0f;
          b.active = false;
          continue;
        }
      }
    } else {
      const float dir = (s.reverse == StripSettings::kReverse) ? -1.0f : 1.0f;
      b.pos += (float)s.speed * dir * dtSec;
      if (s.loop) {
        while (b.pos < 0.0f) {
          b.pos += laps;
        }
        while (b.pos >= laps) {
          b.pos -= laps;
        }
      } else {
        // One-shot mode: disappear once the trailing edge leaves the strip.
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

  // Evaluate trigger source for this strip:
  //   - kManual:    microphone ignored (pads / button A only)
  //   - kBeat:      triggered by the per-strip beat detector
  //   - kThreshold: triggered when bandLevel >= thresholdPct
  bool shouldTrigger = false;
  if (s.mode == StripSettings::kBeat) {
    shouldTrigger = onBeat;
  } else if (s.mode == StripSettings::kThreshold) {
    shouldTrigger = (bandLevel * 100.0f >= (float)s.thresholdPct);
  }

  if (!shouldTrigger) {
    return;
  }
  if (!s.retrigger) {
    // Retrigger disabled: do not trigger while any effect is still active.
    if (activeCount() > 0) {
      return;
    }
  } else if (spawned_ && (uint32_t)(nowMs - lastSpawnMs_) < s.retriggerMs) {
    return;
  }

  triggerEffect(s.effect, s);
  spawned_ = true;
  lastSpawnMs_ = nowMs;
}

void BlobEngine::render(LedStrip &strip, const StripSettings &s) const {
  if (flashAmp_ > 0.0f) {
    strip.addFill(flashAmp_, s.r, s.g, s.b);
  }
  if (sparkleAmp_ > 0.0f) {
    strip.addSparkles(sparkleAmp_, sparkleSeed_, s.r, s.g, s.b);
  }

  // blobLeds is the full width of the light group, so half of it each side of the
  // centre. Ping-pong and center bursts do not wrap across LED 0.
  const float halfWidth = (float)s.blobLeds * 0.5f;
  const bool wrapSeam =
      (s.loop != 0) && (s.reverse != StripSettings::kPingPong);
  for (size_t i = 0; i < kMaxBlobs; i++) {
    const Blob &b = blobs_[i];
    if (b.active) {
      strip.addRing(b.pos, halfWidth, b.amp, s.r, s.g, s.b,
                    b.burst ? false : wrapSeam);
    }
  }
}

size_t BlobEngine::activeCount() const {
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
