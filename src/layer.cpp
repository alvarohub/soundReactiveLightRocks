#include "layer.h"

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

} // namespace

void LayerEngine::reset() {
  for (size_t i = 0; i < kMaxBlobs; i++) {
    blobs_[i].active = false;
    blobs_[i].amp = 0.0f;
    blobs_[i].pos = 0.0f;
    blobs_[i].dir = 1;
    blobs_[i].burst = 0;
  }
  flashAmp_ = 0.0f;
  sparkleAmp_ = 0.0f;
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
    b.dir = dir;
    b.burst = 1;
  }
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

void LayerEngine::update(float bandLevel, bool onBeat, uint32_t nowMs,
                         float dtSec, const LayerSettings &s,
                         uint16_t ledCount) {
  const float decayPerSec = 1000.0f / (float)s.fadeMs;
  const float laps = (float)ledCount;
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
      b.pos += (float)s.speed * dir * dtSec;
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
  bool shouldTrigger = false;
  if (s.mode == LayerSettings::kBeat) {
    shouldTrigger = onBeat;
  } else if (s.mode == LayerSettings::kThreshold) {
    shouldTrigger = (bandLevel * 100.0f >= (float)s.thresholdPct);
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
  const float w = (float)s.intensityPct / 100.0f;
  peak_ = 0.0f;

  if (flashAmp_ > 0.0f) {
    strip.addFill(flashAmp_ * w, s.r, s.g, s.b);
    peak_ = flashAmp_ * w;
  }
  if (sparkleAmp_ > 0.0f) {
    strip.addSparkles(sparkleAmp_ * w, sparkleSeed_, s.r, s.g, s.b);
    if (sparkleAmp_ * w > peak_) {
      peak_ = sparkleAmp_ * w;
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
    const float amp = b.amp * w;
    strip.addRing(b.pos, halfWidth, amp, s.r, s.g, s.b,
                  b.burst ? false : wrapSeam);
    if (amp > peak_) {
      peak_ = amp;
    }
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
