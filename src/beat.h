#pragma once

#include <stdint.h>

// What the beat detector reports for one analysis frame.
struct BeatInfo {
  bool onBeat = false;    // a beat started in this frame
  bool predicted = false; // ...from the tempo grid, with no onset to confirm it
  float pulse = 0.0f;     // 0..1, decays after each beat
  uint16_t periodMs = 0;  // beat period of the locked tempo, 0 = not locked
  uint16_t bpm = 0;       // 60..190 while locked, 0 while still listening
  uint8_t confidencePct = 0; // how well the last few onsets sat on the grid
  bool locked = false;    // the tempo grid is driving the beats
};

// Real-time onset detection and beat tracking for one layer band.
//
// Three stages, the ones a serious detector needs:
//
//  1. ONSET CONDITIONING. The raw spectral flux of the band is divided by its
//     own slow running average and log-compressed, so a quiet passage and a
//     loud one produce the same onset strength (level invariance) and a huge
//     transient cannot swamp the statistics.
//  2. PEAK PICKING. An onset is a *local maximum* of the conditioned signal
//     that clears an adaptive threshold (running mean + k * running deviation,
//     k set by the sensitivity), with a 1-frame lookahead so a rising edge
//     cannot fire twice.
//  3. TEMPO + PHASE. Onsets feed an inter-onset-interval histogram (folded by
//     2 and 3 so a missed beat still reinforces the real period, decayed so the
//     tempo can change). The histogram gives the period; a phase-locked loop
//     then runs a beat grid at that period and is pulled towards the onsets.
//     While locked, a beat is raised by the *grid*, so it survives a masked
//     onset, an off-beat (syncopation) does not double the tempo, and the
//     reported BPM is steady instead of being the jitter of the last intervals.
class BeatDetector {
public:
  void reset();

  // onsetStrength is the band's spectral flux this frame (>= 0).
  // level01 is the same band's level, 0..1: it gates silence, so a quiet room
  // can never produce a beat however the statistics drift.
  // sensitivityPct 0 = only obvious beats, 100 = eager.
  BeatInfo update(float onsetStrength, float level01, uint32_t dtMs,
                  uint8_t sensitivityPct);

  uint16_t bpm() const { return bpm_; }
  float pulse() const { return pulse_; }
  uint8_t confidencePct() const { return confPct_; }
  bool locked() const { return locked_; }
  // Phase of the beat grid, 0 = on a beat, 0.5 = the off-beat. Effects can use
  // it for swing or for a "breathe between beats" shape.
  float phase() const { return gridPhase_; }

private:
  // One detected onset (already peak-picked and conditioned).
  void onOnset(float strength, float floorW, float peakRaw, uint32_t dtMs);
  void updateTempo(uint32_t dtMs);
  void addIntervalBins(uint16_t ioiMs, float weight);
  float supportAround(float periodMs, float tol) const;
  // How well the last few onsets fit a grid at the current period, 0..1.
  // 1 = every onset sits on a beat, ~0.25 = they are all over the place (noise).
  float gridQuality(uint8_t *countOut) const;
  void fireBeat(bool predicted);

  // --- onset conditioning ---
  float onsetRef_ = 0.0f; // slow average of the raw flux
  float mean_ = 0.0f;     // running mean of the conditioned onset
  float dev_ = 0.0f;      // running mean absolute deviation
  bool primed_ = false;
  float prevW_ = 0.0f;    // previous conditioned value (peak picking)
  float candW_ = 0.0f;    // best value of the current rising edge
  float floorW_ = 0.0f;   // lowest value since the last onset
  float peakRaw_ = 0.0f;  // raw onset strength of the current peak
  float refPeak_ = 0.0f;  // typical peak, so weak hits can be discounted
  // Short history of onsets, for the grid-fit test that decides whether the
  // tempo is real or the detector is looking at noise.
  static constexpr uint8_t kFitOnsets = 8;
  uint16_t fitTime_[kFitOnsets] = {};
  uint8_t fitWeight_[kFitOnsets] = {};
  uint8_t fitPos_ = 0;
  uint8_t fitCount_ = 0;
  float fitQuality_ = 0.0f;
  uint16_t lastStrongMs_ = 0; // previous accented onset: the beat, not the off-beat
  bool lastStrongValid_ = false;
  uint16_t spanRefMs_ = 0;  // onset the period is measured from
  bool spanRefValid_ = false;
  uint8_t candFrames_ = 0;

  // --- tempo: inter-onset interval histogram ---
  static constexpr uint16_t kBinMs = 10;
  static constexpr uint16_t kBinLoMs = 250;
  static constexpr uint16_t kBinHiMs = 2000;
  static constexpr uint8_t kBinCount = (kBinHiMs - kBinLoMs) / kBinMs;
  static constexpr uint8_t kRecentOnsets = 6;
  uint8_t hist_[kBinCount] = {};
  uint16_t recentMs_[kRecentOnsets] = {};
  uint8_t recentCount_ = 0;
  uint8_t recentPos_ = 0;
  uint32_t clockMs_ = 0; // free-running time, wraps after 65 s
  float periodMs_ = 0.0f;
  float histPeak_ = 0.0f;

  // --- phase-locked loop ---
  float gridPhase_ = 0.0f; // 0..1 inside the current beat
  uint8_t rejectStreak_ = 0;
  bool locked_ = false;
  bool firedThisBeat_ = false;
  float sinceAcceptMs_ = 1000000.0f;
  uint32_t sinceFireMs_ = 1000000u;

  // --- output ---
  bool firedThisFrame_ = false;
  bool firedPredicted_ = false;
  float pulse_ = 0.0f;
  uint16_t bpm_ = 0;
  uint8_t confPct_ = 0;
};
