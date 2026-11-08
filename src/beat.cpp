#include "beat.h"

#include <math.h>

namespace {

// ---- onset conditioning -------------------------------------------------
constexpr float kRefTauMs = 1500.0f;  // running average of the raw flux
constexpr float kStatTauMs = 600.0f;  // window of the adaptive threshold
constexpr float kMinW = 0.28f;        // hard floor on the conditioned onset
constexpr float kRefFloor = 1e-5f;    // only guards the division
constexpr float kMinRise = 0.55f;     // an onset must RISE out of the noise floor

// ---- onset peak picking -------------------------------------------------
constexpr uint32_t kMinOnsetGapMs = 90; // two onsets closer than this are one

// ---- tempo --------------------------------------------------------------
constexpr float kMinPeriodMs = 316.0f;  // 190 BPM
constexpr float kMaxPeriodMs = 1000.0f; // 60 BPM
constexpr float kBinDecay = 0.93f;      // per onset: the tempo may change
constexpr float kPeriodSmooth = 0.20f;  // EMA towards the histogram period
constexpr float kPeriodJump = 0.08f;    // a change this big is taken, not averaged
constexpr float kWeight2 = 0.45f;       // ioi/2 reinforces the doubled tempo
constexpr float kWeight3 = 0.25f;       // ioi/3 the tripled one
constexpr float kStrongWeight = 0.68f;  // an onset this accented counts as a beat
constexpr float kStrongWeightBias = 0.85f;

// ---- phase-locked loop --------------------------------------------------
constexpr float kPhaseTol = 0.28f;   // an onset within +-28 % of a beat is on it
constexpr float kPhaseKp = 0.30f;    // how hard an onset pulls the grid
constexpr uint8_t kUnlockStreak = 3; // onsets off the grid that break the lock
// Grid-fit test. The score of a uniformly scattered onset train is ~0.24 with
// this width, so 0.55 cleanly separates music from noise.
constexpr float kFitSigma = 0.18f;
constexpr float kFitLockQuality = 0.55f;
constexpr float kFitUnlockQuality = 0.34f;
constexpr uint8_t kFitMinOnsets = 4;
constexpr float kLostGridMs = 2.2f;  // multiples of the period, then unlock
constexpr float kMinHistPeak = 1.2f; // histogram must show a period at all
// Metrical prior: a mild preference for the 80..160 BPM range, so an octave
// error (200 instead of 100 BPM) needs much more evidence to win.
constexpr float kPriorCentreMs = 500.0f;
constexpr float kPriorSigma = 0.60f;
// A period whose double is at least this well supported is the off-beat level.
constexpr float kSuperPeriodShare = 0.55f;

// ---- output -------------------------------------------------------------
constexpr float kPulseTauMs = 120.0f;
constexpr float kSilenceLevel = 0.06f;

// ---- helpers ------------------------------------------------------------
inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

void BeatDetector::reset() {
  onsetRef_ = 0.0f;
  mean_ = 0.0f;
  dev_ = 0.0f;
  primed_ = false;
  prevW_ = 0.0f;
  candW_ = 0.0f;
  floorW_ = 0.0f;
  candFrames_ = 0;
  for (uint8_t i = 0; i < kBinCount; i++) {
    hist_[i] = 0;
  }
  for (uint8_t i = 0; i < kRecentOnsets; i++) {
    recentMs_[i] = 0;
  }
  recentCount_ = 0;
  recentPos_ = 0;
  clockMs_ = 0;
  peakRaw_ = 0.0f;
  refPeak_ = 0.0f;
  spanRefMs_ = 0;
  spanRefValid_ = false;
  lastStrongMs_ = 0;
  lastStrongValid_ = false;
  fitPos_ = 0;
  fitCount_ = 0;
  fitQuality_ = 0.0f;
  for (uint8_t i = 0; i < kFitOnsets; i++) {
    fitTime_[i] = 0;
    fitWeight_[i] = 0;
  }
  periodMs_ = 0.0f;
  histPeak_ = 0.0f;
  gridPhase_ = 0.0f;
  rejectStreak_ = 0;
  locked_ = false;
  firedThisBeat_ = false;
  sinceAcceptMs_ = 1000000.0f;
  sinceFireMs_ = 1000000u;
  pulse_ = 0.0f;
  bpm_ = 0;
  confPct_ = 0;
}

BeatInfo BeatDetector::update(float onsetStrength, float level01,
                              uint32_t dtMs, uint8_t sensPct) {
  if (dtMs == 0) {
    dtMs = 1;
  }
  const float dt = (float)dtMs;
  clockMs_ += dtMs;

  BeatInfo out;
  pulse_ *= expf(-dt / kPulseTauMs);
  if (pulse_ < 0.001f) {
    pulse_ = 0.0f;
  }

  const bool silent = level01 < kSilenceLevel;
  if (silent) {
    // Nothing is playing: forget the grid and the tempo rather than reporting a
    // stale BPM, but keep the onset statistics decaying so the first real sound
    // is not mistaken for a huge jump.
    locked_ = false;
    rejectStreak_ = 0;
    bpm_ = 0;
    confPct_ = 0;
    periodMs_ = 0.0f;
    for (uint8_t i = 0; i < kBinCount; i++) {
      hist_[i] = (uint8_t)(hist_[i] * 3 / 4);
    }
    recentCount_ = 0;
  }

  // ---- 1. onset conditioning -------------------------------------------
  // Divide by the running average of the flux, then log-compress: the same hit
  // reads the same no matter how loud the passage or how the gain was set.
  const float raw = silent ? 0.0f : (onsetStrength > 0.0f ? onsetStrength : 0.0f);
  if (!primed_) {
    onsetRef_ = raw > kRefFloor ? raw : kRefFloor;
    mean_ = 0.0f;
    dev_ = 0.0f;
    primed_ = true;
  }
  const float aRef = 1.0f - expf(-dt / kRefTauMs);
  onsetRef_ += aRef * (raw - onsetRef_);
  if (onsetRef_ < kRefFloor) {
    onsetRef_ = kRefFloor;
  }
  const float w = silent ? 0.0f : log1pf(raw / onsetRef_);

  // ---- 2. peak picking (one frame of lookahead) ------------------------
  float threshold = 0.0f;
  if (!silent) {
    const float k = 1.70f - 1.00f * (float)sensPct / 100.0f; // 1.7 -> 0.7
    threshold = mean_ + k * dev_;
    if (threshold < kMinW) {
      threshold = kMinW;
    }
  }

  // The floor of the signal since the last onset: an onset has to be a RISE out
  // of it, which is what separates a hit from the local maxima of noise.
  if (w < floorW_) {
    floorW_ = w;
  }

  const bool riseStopped = w <= prevW_;
  const float peakW = candW_;
  const uint8_t peakAge = candFrames_;
  const bool onsetHere =
      !silent && riseStopped && peakAge > 0 && peakW > threshold &&
      (peakW - floorW_) >= kMinRise &&
      (sinceFireMs_ > kMinOnsetGapMs || locked_);

  if (onsetHere) {
    onOnset(peakW, floorW_, peakRaw_, dtMs);
    candW_ = 0.0f;
    candFrames_ = 0;
    floorW_ = w;
  } else if (w > candW_) {
    candW_ = w;
    peakRaw_ = raw;
    candFrames_ = 1;
  } else if (candFrames_ < 255) {
    candFrames_++;
  }
  prevW_ = w;

  // Statistics follow the signal, never lead it: an onset must not raise the
  // threshold that has to detect it.
  const float a = 1.0f - expf(-dt / kStatTauMs);
  mean_ += a * (w - mean_);
  dev_ += a * (fabsf(w - mean_) - dev_);

  // ---- 3. tempo grid ---------------------------------------------------
  if (periodMs_ > 0.0f) {
    gridPhase_ += dt / periodMs_;
    if (gridPhase_ >= 1.0f) {
      gridPhase_ -= floorf(gridPhase_);
      firedThisBeat_ = false;
    }
    // A beat is raised by the grid once the tempo is trusted, so a masked or
    // swallowed onset still gives a beat; below that, onsets do it themselves.
    if (locked_ && !firedThisBeat_ && !silent &&
        sinceFireMs_ > (uint32_t)(periodMs_ * 0.5f)) {
      fireBeat(true);
    }
  }

  sinceAcceptMs_ = clampf(sinceAcceptMs_ + dt, 0.0f, 1000000.0f);
  if ((float)sinceFireMs_ < 1000000.0f) {
    sinceFireMs_ += dtMs;
  }

  // Nothing has landed on the grid for a couple of beats: stop pretending.
  if (locked_ && periodMs_ > 0.0f &&
      sinceAcceptMs_ > kLostGridMs * periodMs_) {
    locked_ = false;
    bpm_ = 0;
  }

  out.onBeat = firedThisFrame_;
  out.predicted = firedPredicted_;
  out.pulse = pulse_;
  out.periodMs = periodMs_ > 0.0f ? (uint16_t)periodMs_ : 0;
  out.bpm = bpm_;
  out.confidencePct = confPct_;
  out.locked = locked_;
  firedThisFrame_ = false;
  firedPredicted_ = false;
  return out;
}

float BeatDetector::gridQuality(uint8_t *countOut) const {
  if (countOut != nullptr) {
    *countOut = fitCount_;
  }
  if (periodMs_ <= 0.0f || fitCount_ < 3) {
    return 0.0f;
  }
  float errs[kFitOnsets];
  float sumW = 0.0f;
  float sumWE = 0.0f;
  for (uint8_t i = 0; i < fitCount_; i++) {
    const float ageMs = (float)(uint16_t)((uint16_t)clockMs_ - fitTime_[i]);
    float e = gridPhase_ - ageMs / periodMs_;
    e -= floorf(e);            // 0..1
    if (e > 0.5f) {
      e -= 1.0f;               // -0.5..0.5: signed distance to the nearest beat
    }
    errs[i] = e;
    const float w = (float)fitWeight_[i] / 100.0f;
    sumW += w;
    sumWE += w * e;
  }
  if (sumW <= 0.0f) {
    return 0.0f;
  }
  // Free phase: the onsets are fitted to the best offset of the grid, so a grid
  // that is merely out of phase still scores well (that is the loop's job).
  const float meanErr = sumWE / sumW;
  float fit = 0.0f;
  for (uint8_t i = 0; i < fitCount_; i++) {
    const float d = fabsf(errs[i] - meanErr) / kFitSigma;
    const float score = d < 1.0f ? (1.0f - d * d) : 0.0f;
    fit += ((float)fitWeight_[i] / 100.0f) * score;
  }
  return clampf(fit / sumW, 0.0f, 1.0f);
}

void BeatDetector::onOnset(float strength, float floorW, float peakRaw,
                           uint32_t dtMs) {
  (void)dtMs;

  // A weak blip must not outvote a real hit when voting for the tempo. The
  // linear peak ratio is what carries the accent, because the log compression
  // used for detection deliberately flattens it.
  // Average accent level: the mean of the raw peaks (not a max, which would
  // make every hit look equally strong). The accent is exactly what tells a
  // beat from a subdivision, so the weight has to be relative to it.
  if (refPeak_ <= 0.0f) {
    refPeak_ = peakRaw > 0.0f ? peakRaw : 0.001f;
  }
  refPeak_ += 0.25f * (peakRaw - refPeak_);
  if (refPeak_ < 1e-6f) {
    refPeak_ = 1e-6f;
  }
  const float rel = peakRaw / refPeak_;
  float weight = rel * rel;
  weight = weight > 1.0f ? 1.0f : (weight < 0.05f ? 0.05f : weight);
  (void)strength;
  (void)floorW;

  uint16_t prevOnsetMs = 0;
  const bool havePrev = recentCount_ > 0;
  if (havePrev) {
    const uint8_t idx = (uint8_t)((recentPos_ + kRecentOnsets - 1) % kRecentOnsets);
    prevOnsetMs = recentMs_[idx];
  }

  // Decay the histogram once per onset: a tempo that stops must fade out and
  // leave room for the next one.
  for (uint8_t i = 0; i < kBinCount; i++) {
    hist_[i] = (uint8_t)((float)hist_[i] * kBinDecay);
  }

  uint16_t consecutiveIoi = 0;
  if (havePrev) {
    const uint16_t ioi = (uint16_t)((uint16_t)clockMs_ - prevOnsetMs);
    if (ioi >= kBinLoMs && ioi <= kBinHiMs) {
      addIntervalBins(ioi, weight);
      consecutiveIoi = ioi;
    }
  }

  // A beat is where the accents are. Measuring from the previous ACCENTED onset
  // is what keeps a swing or a syncopated line at its real tempo instead of
  // locking onto the subdivision: 400/200 swing gives 600, not 400.
  if (weight >= kStrongWeight) {
    if (lastStrongValid_) {
      const uint16_t strongIoi =
          (uint16_t)((uint16_t)clockMs_ - lastStrongMs_);
      if (strongIoi >= kBinLoMs && strongIoi <= kBinHiMs &&
          strongIoi != consecutiveIoi) {
        addIntervalBins(strongIoi, kStrongWeightBias);
      }
    }
    lastStrongMs_ = (uint16_t)clockMs_;
    lastStrongValid_ = true;
  }
  recentMs_[recentPos_] = (uint16_t)clockMs_;
  recentPos_ = (uint8_t)((recentPos_ + 1) % kRecentOnsets);
  if (recentCount_ < kRecentOnsets) {
    recentCount_++;
  }

  updateTempo(dtMs);

  fitTime_[fitPos_] = (uint16_t)clockMs_;
  fitWeight_[fitPos_] = (uint8_t)(weight * 100.0f + 0.5f);
  fitPos_ = (uint8_t)((fitPos_ + 1) % kFitOnsets);
  if (fitCount_ < kFitOnsets) {
    fitCount_++;
  }
  fitQuality_ = gridQuality(nullptr);

  if (periodMs_ <= 0.0f) {
    // No period yet: the onset is the beat, there is nothing to compare with.
    fireBeat(false);
    confPct_ = 0;
    return;
  }

  // ---- where does this onset sit relative to the grid? ----
  float err = gridPhase_;
  if (err > 0.5f) {
    err -= 1.0f; // -0.5..0.5, signed distance to the nearest grid beat
  }
  const bool onGrid = fabsf(err) < kPhaseTol;

  if (onGrid) {
    rejectStreak_ = 0;
    sinceAcceptMs_ = 0.0f;
    // The loop runs whether or not the grid is trusted: while unlocked this is
    // what lets an out-of-phase grid converge, without snapping to every onset
    // (snapping would make every onset look on-grid and the streak meaningless).
    gridPhase_ -= kPhaseKp * err;
    if (gridPhase_ < 0.0f) {
      gridPhase_ += 1.0f;
    }
    if (locked_) {
      // Measure the period straight from the onset times over several beats.
      // One frame is 32 ms, so a single interval cannot be more accurate than
      // that; spanning four or five beats divides the error by the same amount.
      // A masked beat only makes the beat index jump, which the division below
      // undoes, so it does not skew the measurement.
      if (!spanRefValid_) {
        spanRefMs_ = (uint16_t)clockMs_;
        spanRefValid_ = true;
      } else {
        const float span = (float)(uint16_t)((uint16_t)clockMs_ - spanRefMs_);
        const int k = (int)(span / periodMs_ + 0.5f);
        if (k >= 2 && k <= 12) {
          const float meas = span / (float)k;
          if (fabsf(meas - periodMs_) < 0.10f * periodMs_) {
            periodMs_ += 0.35f * (meas - periodMs_);
            periodMs_ = clampf(periodMs_, kMinPeriodMs, kMaxPeriodMs);
          }
          // Slide once the window spans 8 beats: long enough to average the
          // one-frame quantisation of the onset times down to ~1 BPM, short
          // enough to follow a real change of tempo.
          if (k >= 8) {
            spanRefMs_ = (uint16_t)clockMs_;
          }
        } else if (k > 12) {
          spanRefMs_ = (uint16_t)clockMs_; // reference far too old to be useful
        }
      }
    }
  } else {
    rejectStreak_++;
  }

  // The lock is decided by how well the recent onsets fit the grid, not by a
  // streak: a grid is real when its beats explain the accents (steady tempo,
  // masked beats, swing) and fake when the onsets are scattered (noise). A
  // streak cannot tell those apart, because half of a swing sits off the beat.
  const bool qualityOk =
      fitCount_ >= kFitMinOnsets && fitQuality_ >= kFitLockQuality;
  if (!locked_ && qualityOk && histPeak_ > kMinHistPeak) {
    locked_ = true;
    sinceAcceptMs_ = 0.0f;
  }

  if (locked_) {
    if (fitQuality_ < kFitUnlockQuality) {
      rejectStreak_++;
    }
    if (rejectStreak_ >= kUnlockStreak) {
      // Repeated off-grid onsets: the grid is wrong (tempo change, or we locked
      // onto a syncopation). Drop it and re-earn it from the onsets.
      locked_ = false;
      rejectStreak_ = 0;
      bpm_ = 0;
      confPct_ = 0;
      fireBeat(false);
    } else if (onGrid && !firedThisBeat_) {
      fireBeat(false);
    }
  } else {
    // Not trusted yet: stay responsive and let every onset fire.
    fireBeat(false);
  }

  confPct_ = locked_ ? (uint8_t)(fitQuality_ * 100.0f + 0.5f) : 0;
}

float BeatDetector::supportAround(float periodMs, float tol) const {
  const float lo = periodMs * (1.0f - tol);
  const float hi = periodMs * (1.0f + tol);
  int first = (int)((lo - kBinLoMs) / kBinMs + 0.5f);
  int last = (int)((hi - kBinLoMs) / kBinMs + 0.5f);
  if (first < 0) {
    first = 0;
  }
  if (last > kBinCount - 1) {
    last = kBinCount - 1;
  }
  float sum = 0.0f;
  for (int i = first; i <= last; i++) {
    sum += (float)hist_[i];
  }
  return sum;
}

void BeatDetector::addIntervalBins(uint16_t ioiMs, float weight) {
  const uint16_t parts[3] = {(uint16_t)ioiMs, (uint16_t)(ioiMs / 2),
                             (uint16_t)(ioiMs / 3)};
  const float weights[3] = {weight, weight * kWeight2, weight * kWeight3};
  for (uint8_t p = 0; p < 3; p++) {
    const uint16_t v = parts[p];
    if (v < kBinLoMs || v > kBinHiMs || (p > 0 && v * (p + 1) != ioiMs)) {
      continue; // only fold exact multiples, else we invent a tempo
    }
    const uint8_t bin = (uint8_t)((v - kBinLoMs) / kBinMs);
    const float add = weights[p] * 40.0f;
    const uint16_t sum = (uint16_t)hist_[bin] + (uint16_t)(add + 0.5f);
    hist_[bin] = (uint8_t)(sum > 255 ? 255 : sum);
  }
}

void BeatDetector::updateTempo(uint32_t dtMs) {
  const uint8_t loBin = (uint8_t)((kMinPeriodMs - kBinLoMs) / kBinMs);
  const uint8_t hiBin = (uint8_t)((kMaxPeriodMs - kBinLoMs) / kBinMs);

  // Score every candidate period, weighted by a mild metrical prior.
  float bestScore = 0.0f;
  int best = -1;
  uint16_t sum = 0;
  uint8_t count = 0;
  for (uint8_t i = loBin; i <= hiBin && i < kBinCount; i++) {
    const float period = (float)(kBinLoMs + i * kBinMs);
    const float z = logf(period / kPriorCentreMs) / kPriorSigma;
    const float score = (float)hist_[i] * expf(-0.5f * z * z);
    sum = (uint16_t)(sum + hist_[i]);
    count++;
    if (score > bestScore) {
      bestScore = score;
      best = (int)i;
    }
  }
  if (best < 0 || bestScore <= 0.0f) {
    histPeak_ = 0.0f;
    (void)dtMs;
    return;
  }

  // Metrical level: if the period that is twice as long is nearly as well
  // supported, the short one is the off-beat, not the beat. Without this a
  // syncopated line locks onto twice the real tempo (the classic octave error).
  {
    const float shortPeriod = (float)(kBinLoMs + best * kBinMs);
    const float longPeriod = shortPeriod * 2.0f;
    if (longPeriod <= kMaxPeriodMs) {
      const float sShort = supportAround(shortPeriod, 0.10f);
      const float sLong = supportAround(longPeriod, 0.10f);
      if (sShort > 0.0f && sLong > kSuperPeriodShare * sShort) {
        // Re-pick the exact bin inside the longer window.
        const int lo = (int)((longPeriod * 0.90f - kBinLoMs) / kBinMs + 0.5f);
        const int hi = (int)((longPeriod * 1.10f - kBinLoMs) / kBinMs + 0.5f);
        int pick = best;
        float pickVal = 0.0f;
        for (int i = lo; i <= hi; i++) {
          if (i < 0 || i >= kBinCount) {
            continue;
          }
          if ((float)hist_[i] > pickVal) {
            pickVal = (float)hist_[i];
            pick = i;
          }
        }
        best = pick;
        bestScore = pickVal;
      }
    }
  }

  float period = (float)(kBinLoMs + best * kBinMs);
  if (best > 0 && best + 1 < kBinCount) {
    const float l = (float)hist_[best - 1];
    const float c = (float)hist_[best];
    const float r = (float)hist_[best + 1];
    const float denom = (l - 2.0f * c + r);
    if (fabsf(denom) > 0.001f) {
      const float shift = 0.5f * (l - r) / denom;
      period += clampf(shift, -0.5f, 0.5f) * (float)kBinMs;
    }
  }
  period = clampf(period, kMinPeriodMs, kMaxPeriodMs);

  // How far the winning bin stands above the average bin in the range.
  const float avg = count > 0 ? (float)sum / (float)count : 0.0f;
  histPeak_ = avg > 0.5f ? bestScore / avg : bestScore;

  if (periodMs_ <= 0.0f || !locked_) {
    // Nothing is trusted yet: the histogram is the only evidence there is.
    if (fabsf(period - periodMs_) > kPeriodJump * periodMs_ || periodMs_ <= 0.0f) {
      periodMs_ = period; // a jump, not an average: follow a new song at once
    } else {
      periodMs_ += kPeriodSmooth * (period - periodMs_);
    }
  } else if (fabsf(period - periodMs_) > kPeriodJump * periodMs_) {
    // Locked, but the histogram now says something clearly different: that is a
    // change of tempo, and averaging towards it would never get there.
    periodMs_ = period;
  }
  // While locked and unchanged, the period belongs to the phase-locked
  // measurement (onset timing over eight beats), not to the 10 ms histogram
  // bins: letting both write it just makes them fight and jitter.
  periodMs_ = clampf(periodMs_, kMinPeriodMs, kMaxPeriodMs);

  if (locked_) {
    bpm_ = (uint16_t)(60000.0f / periodMs_ + 0.5f);
  } else {
    bpm_ = 0;
  }
}

void BeatDetector::fireBeat(bool predicted) {
  pulse_ = 1.0f;
  firedThisBeat_ = true;
  firedThisFrame_ = true;
  firedPredicted_ = predicted;
  sinceFireMs_ = 0;
}
