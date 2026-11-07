#include "beat.h"

#include <math.h>

namespace {

constexpr float kPulseTauMs = 120.0f;     // how quickly the beat flash indicator fades
constexpr float kMeanTauMs = 900.0f;      // window of the adaptive threshold
constexpr uint32_t kMinIntervalMs = 180;  // refractory: ceiling of 333 BPM
constexpr uint32_t kMaxIntervalMs = 1500; // 40 BPM, below this it is not a tempo

// Absolute floor so noise in a silent room cannot trigger a beat. The sister
// project used 0.02; this port had lowered it to 0.008, which let room noise
// through once the adaptive threshold had decayed towards zero.
constexpr float kMinStrength = 0.02f;

// Band level below which the input counts as silence. 0.06 is just above the
// noise floor of a quiet room on this microphone, so nothing can fire until
// there is actual sound.
constexpr float kSilenceLevel = 0.06f;

constexpr float kSaturateMs = 1000000.0f; // refractory timer saturates here

// Musically plausible range for the reported tempo.
constexpr float kMinBpm = 60.0f;
constexpr float kMaxBpm = 170.0f;

} // namespace

void BeatDetector::reset() {
  mean_ = 0.0f;
  var_ = 0.0f;
  primed_ = false;
  sinceBeatMs_ = 1000000u;
  pulse_ = 0.0f;
  ioiCount_ = 0;
  ioiPos_ = 0;
  bpm_ = 0;
  for (uint8_t i = 0; i < kMaxIntervals; i++) {
    ioi_[i] = 0;
  }
}

BeatInfo BeatDetector::update(float onsetStrength, float level01,
                              uint32_t dtMs, uint8_t sensPct) {
  if (dtMs == 0) {
    dtMs = 1;
  }

  BeatInfo out;

  // Silence gate: no sound in this band means no beat, however the adaptive
  // statistics happen to look. The statistics still track the (zero) signal so
  // the first real onset after silence is not treated as a huge jump.
  const bool silent = level01 < kSilenceLevel;

  pulse_ *= expf(-(float)dtMs / kPulseTauMs);
  if (pulse_ < 0.001f) {
    pulse_ = 0.0f;
  }

  const float step = (float)dtMs;
  if ((float)sinceBeatMs_ + step < kSaturateMs) {
    sinceBeatMs_ += dtMs;
  } else {
    sinceBeatMs_ = (uint32_t)kSaturateMs;
  }

  // If no beat has arrived for two full maximum intervals, or the band went
  // quiet, drop the tempo lock so silence never reports a stale BPM.
  if (silent || sinceBeatMs_ > kMaxIntervalMs * 2u) {
    bpm_ = 0;
    ioiCount_ = 0;
    ioiPos_ = 0;
  }

  const float x = silent ? 0.0f
                         : (onsetStrength > 0.0f ? onsetStrength : 0.0f);

  if (!primed_) {
    // Start from the current signal instead of from zero, otherwise the first
    // few frames look like a huge onset.
    mean_ = x;
    var_ = 0.0f;
    primed_ = true;
    out.pulse = pulse_;
    out.periodMs = bpm_ ? (uint16_t)(60000u / bpm_) : 0;
    return out;
  }

  // Sensitivity lowers the threshold: less sensitive = more sigmas above mean.
  const float k = 3.2f - 2.6f * (float)sensPct / 100.0f;
  const float threshold = mean_ + k * sqrtf(var_);

  const bool strongEnough = !silent && x > kMinStrength && x > mean_ * 1.10f;
  if (strongEnough && x > threshold && sinceBeatMs_ >= kMinIntervalMs) {
    out.onBeat = true;
    pulse_ = 1.0f;
    if (sinceBeatMs_ <= kMaxIntervalMs) {
      addInterval(sinceBeatMs_);
    }
    sinceBeatMs_ = 0;
  }

  // Statistics are updated after the test, so an onset never raises the
  // threshold that has to detect it.
  const float a = 1.0f - expf(-(float)dtMs / kMeanTauMs);
  const float d = x - mean_;
  mean_ += a * d;
  var_ = (1.0f - a) * (var_ + a * d * d);

  out.pulse = pulse_;
  out.periodMs = bpm_ ? (uint16_t)(60000u / bpm_) : 0;
  return out;
}

void BeatDetector::addInterval(uint32_t ms) {
  ioi_[ioiPos_] = (uint16_t)ms;
  ioiPos_ = (uint8_t)((ioiPos_ + 1) % kMaxIntervals);
  if (ioiCount_ < kMaxIntervals) {
    ioiCount_++;
  }
  if (ioiCount_ < 3) {
    return; // too few intervals to trust a tempo
  }

  // Median of the stored intervals: robust against a skipped or doubled beat.
  uint16_t sorted[kMaxIntervals];
  for (uint8_t i = 0; i < ioiCount_; i++) {
    sorted[i] = ioi_[i];
  }
  for (uint8_t i = 1; i < ioiCount_; i++) {
    const uint16_t v = sorted[i];
    int8_t j = (int8_t)(i - 1);
    while (j >= 0 && sorted[j] > v) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = v;
  }
  const uint16_t median = sorted[ioiCount_ / 2];

  float bpm = median > 0 ? 60000.0f / (float)median : 0.0f;
  if (bpm <= 0.0f) {
    return;
  }
  // A missed beat doubles the interval, a doubled beat halves it: fold the
  // estimate back into the plausible range rather than reporting 2x or 0.5x.
  while (bpm < kMinBpm) {
    bpm *= 2.0f;
  }
  while (bpm > kMaxBpm) {
    bpm *= 0.5f;
  }
  bpm_ = (uint16_t)(bpm + 0.5f);
}
