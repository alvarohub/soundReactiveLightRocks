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
  }
  spawned_ = false;
  lastSpawnMs_ = 0;
}

void BlobEngine::spawn(const StripSettings &s) {
  const float startPos = initialPos(s);
  const int8_t startDir = initialDir(s);
  for (size_t i = 0; i < kMaxBlobs; i++) {
    if (!blobs_[i].active) {
      blobs_[i].active = true;
      blobs_[i].amp = 1.0f;
      blobs_[i].pos = startPos;
      blobs_[i].dir = startDir;
      return;
    }
  }
  recycle(s);
}

void BlobEngine::recycle(const StripSettings &s) {
  // Every blob spawned before the pool filled is dimmer than this one will be,
  // so replacing the dimmest keeps the strip continuous instead of going silent.
  size_t dimmest = 0;
  for (size_t i = 1; i < kMaxBlobs; i++) {
    if (blobs_[i].amp < blobs_[dimmest].amp) {
      dimmest = i;
    }
  }
  blobs_[dimmest].active = true;
  blobs_[dimmest].amp = 1.0f;
  blobs_[dimmest].pos = initialPos(s);
  blobs_[dimmest].dir = initialDir(s);
}

void BlobEngine::trigger(const StripSettings &s) { spawn(s); }

void BlobEngine::update(float bandLevel, uint32_t nowMs, float dtSec,
                        const StripSettings &s) {
  // amp goes from 1 to 0 across fadeMs.
  const float decayPerSec = 1000.0f / (float)s.fadeMs;
  const float laps = (float)s.ledCount;
  const float maxPos = laps > 1.0f ? (laps - 1.0f) : 0.0f;
  const float halfWidth = (float)s.blobLeds * 0.5f;

  for (size_t i = 0; i < kMaxBlobs; i++) {
    Blob &b = blobs_[i];
    if (!b.active) {
      continue;
    }

    if (s.reverse == StripSettings::kPingPong) {
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

  if (bandLevel * 100.0f < (float)s.thresholdPct) {
    return;
  }
  if (!s.retrigger) {
    // Retrigger disabled: do not spawn a new blob while one is still active.
    if (activeCount() > 0) {
      return;
    }
  } else if (spawned_ && (uint32_t)(nowMs - lastSpawnMs_) < s.retriggerMs) {
    return;
  }

  spawn(s);
  spawned_ = true;
  lastSpawnMs_ = nowMs;
}

void BlobEngine::render(LedStrip &strip, const StripSettings &s) const {
  // blobLeds is the full width of the light group, so half of it each side of the
  // centre. Ping-pong bounces at the ends rather than wrapping across LED 0.
  const float halfWidth = (float)s.blobLeds * 0.5f;
  const bool wrapSeam =
      (s.loop != 0) && (s.reverse != StripSettings::kPingPong);
  for (size_t i = 0; i < kMaxBlobs; i++) {
    const Blob &b = blobs_[i];
    if (b.active) {
      strip.addRing(b.pos, halfWidth, b.amp, s.r, s.g, s.b, wrapSeam);
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
  return n;
}
