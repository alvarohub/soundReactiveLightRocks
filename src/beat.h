#pragma once

#include <stdint.h>

// What the beat detector reports for one analysis frame.
struct BeatInfo {
  bool onBeat = false;   // a beat started in this frame
  float pulse = 0.0f;    // 0..1, decays after each beat
  uint16_t periodMs = 0; // beat period from the tempo estimate, 0 = unknown
};

// Real-time onset detection and tempo tracking (one instance per strip).
//
// Onset function: spectral flux inside the strip's frequency band (the positive
// frame-to-frame change of the spectrum), which is the standard first stage of
// music onset detectors. Beats are picked with an adaptive threshold over
// running mean and variance, so the detector adapts to the loudness of whatever
// is playing, plus a refractory period that rejects double triggers.
//
// Tempo comes from the median of the last few inter-onset intervals, folded into
// a musically plausible range.
class BeatDetector {
public:
  static constexpr uint8_t kMaxIntervals = 8;

  void reset();

  // onsetStrength is the spectral flux of this frame (>= 0).
  // level01 is the band level that produced it, 0..1: it is used as a silence
  // gate, because in a quiet room the adaptive threshold sits at ~0 and noise
  // would otherwise register as a beat.
  // sensitivityPct goes from 0 (only obvious beats) to 100 (very eager).
  BeatInfo update(float onsetStrength, float level01, uint32_t dtMs,
                  uint8_t sensitivityPct);

  uint16_t bpm() const { return bpm_; }
  float pulse() const { return pulse_; }

private:
  void addInterval(uint32_t ms);

  float mean_ = 0.0f;
  float var_ = 0.0f;
  bool primed_ = false;
  uint32_t sinceBeatMs_ = 1000000u; // refractory timer, starts "long ago"
  float pulse_ = 0.0f;

  uint16_t ioi_[kMaxIntervals] = {};
  uint8_t ioiCount_ = 0;
  uint8_t ioiPos_ = 0;
  uint16_t bpm_ = 0;
};
