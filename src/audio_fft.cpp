#include "audio_fft.h"

#include "config.h"
#include "fft_backend.h"

#include <M5Unified.h>
#include <math.h>

namespace {

// Display window: dBFS mapped to 0..1. A full-scale sine reads 0 dBFS, while
// typical speech/music band magnitudes sit between -70 and -20 dBFS.
constexpr float kBandFloorDb = -75.0f;
constexpr float kBandRangeDb = 55.0f;

// Broadband RMS window mapped onto 0..1, which the light engine then shapes.
constexpr float kLevelFloorDb = -55.0f;
constexpr float kLevelFullDb = -18.0f;

// Display-only smoothing for the spectrum bars (~31 frames per second). The
// light output has its own, user-controlled memory instead.
constexpr float kBandAttack = 0.60f;
constexpr float kBandRelease = 0.15f;

constexpr float kFullScale = 32768.0f;

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

inline float smoothBand(float now, float prev) {
  const float a = now > prev ? kBandAttack : kBandRelease;
  return prev + (now - prev) * a;
}

} // namespace

bool SpectrumAnalyzer::begin(uint32_t sampleRateHz) {
  if (!M5.Mic.isEnabled()) {
    return false; // this board has no microphone configured
  }
  sampleRate_ = sampleRateHz;
  if (!M5.Mic.begin()) {
    return false;
  }
  computeBandEdges();

  if (!fftb::begin(kSamples)) {
    return false; // the selected FFT backend could not start
  }

  for (size_t b = 0; b < kBands; b++) {
    band_[b] = 0.0f;
  }
  for (size_t i = 0; i < kBins; i++) {
    prevMag_[i] = 0.0f;
  }
  flux_ = 0.0f;
  level_ = 0.0f;
  levelDb_ = kLevelFloorDb;
  peakHz_ = 0.0f;
  ready_ = true;
  return true;
}

void SpectrumAnalyzer::computeBandEdges() {
  const float binHz = (float)sampleRate_ / (float)kSamples;
  const float ratio = kAudioMaxHz / kAudioMinHz;

  for (size_t b = 0; b < kBands; b++) {
    const float f0 = kAudioMinHz * powf(ratio, (float)b / (float)kBands);
    const float f1 = kAudioMinHz * powf(ratio, (float)(b + 1) / (float)kBands);

    int lo = (int)lroundf(f0 / binHz);
    int hi = (int)lroundf(f1 / binHz);
    if (lo < 1) {
      lo = 1;
    }
    if (hi <= lo) {
      hi = lo + 1;
    }
    if (hi > (int)kBins) {
      hi = (int)kBins;
    }
    bandLo_[b] = (uint16_t)lo;
    bandHi_[b] = (uint16_t)hi; // exclusive
  }
}

bool SpectrumAnalyzer::update() {
  if (!ready_) {
    return false;
  }
  if (!M5.Mic.record(pcm_, kSamples, sampleRate_)) {
    return false;
  }

  // Remove the DC offset, otherwise it leaks into the lowest bands.
  int32_t sum = 0;
  for (size_t i = 0; i < kSamples; i++) {
    sum += pcm_[i];
  }
  const float mean = (float)sum / (float)kSamples;

  float energy = 0.0f;
  for (size_t i = 0; i < kSamples; i++) {
    const float s = (float)pcm_[i] - mean;
    energy += s * s;
    re_[i] = s;
    im_[i] = 0.0f;
  }
  const float rms = sqrtf(energy / (float)kSamples);

  fftb::magnitude(re_, im_, mag_, kSamples);

  // Spectral flux: how much the spectrum grew since the previous frame. Only
  // rises count, so the decay of a note is not mistaken for a new onset.
  float flux = 0.0f;
  for (size_t i = 1; i < kBins; i++) {
    const float rise = mag_[i] - prevMag_[i];
    if (rise > 0.0f) {
      flux += rise;
    }
    prevMag_[i] = mag_[i];
  }
  flux_ = flux;

  const float gain = (float)gainDb_;

  // Log-spaced bands, each represented by its strongest bin.
  for (size_t b = 0; b < kBands; b++) {
    float peak = 0.0f;
    for (size_t i = bandLo_[b]; i < bandHi_[b]; i++) {
      if (mag_[i] > peak) {
        peak = mag_[i];
      }
    }
    const float db = 20.0f * log10f(peak + 1e-9f) + gain;
    const float norm = clamp01((db - kBandFloorDb) / kBandRangeDb);
    band_[b] = smoothBand(norm, band_[b]);
  }

  // Dominant frequency with parabolic interpolation around the peak bin.
  size_t maxIdx = 1;
  for (size_t i = 2; i < kBins; i++) {
    if (mag_[i] > mag_[maxIdx]) {
      maxIdx = i;
    }
  }
  const float binHz = (float)sampleRate_ / (float)kSamples;
  if (mag_[maxIdx] > 1e-4f) {
    float offset = 0.0f;
    if (maxIdx > 1 && maxIdx + 1 < kBins) {
      const float a = mag_[maxIdx - 1];
      const float c = mag_[maxIdx];
      const float d = mag_[maxIdx + 1];
      const float den = a - 2.0f * c + d;
      if (fabsf(den) > 1e-12f) {
        offset = 0.5f * (a - d) / den;
        offset = offset < -0.5f ? -0.5f : (offset > 0.5f ? 0.5f : offset);
      }
    }
    peakHz_ = ((float)maxIdx + offset) * binHz;
  } else {
    peakHz_ = 0.0f;
  }

  levelDb_ = 20.0f * log10f(rms / kFullScale + 1e-9f) + gain;
  level_ = clamp01((levelDb_ - kLevelFloorDb) / (kLevelFullDb - kLevelFloorDb));
  return true;
}

float SpectrumAnalyzer::bandLevel(float centerHz, float widthHz) const {
  if (!ready_ || sampleRate_ == 0) {
    return 0.0f;
  }

  float lo = centerHz - widthHz * 0.5f;
  float hi = centerHz + widthHz * 0.5f;
  if (lo < kAudioMinHz) {
    lo = kAudioMinHz;
  }
  if (hi > kAudioMaxHz) {
    hi = kAudioMaxHz;
  }

  const float binHz = (float)sampleRate_ / (float)kSamples;
  int first = (int)lroundf(lo / binHz);
  int last = (int)lroundf(hi / binHz);
  if (first < 1) {
    first = 1; // bin 0 is DC, already removed but still meaningless here
  }
  if (last > (int)kBins - 1) {
    last = (int)kBins - 1;
  }
  if (last < first) {
    last = first; // narrower than one bin: use the nearest bin
  }

  float sum = 0.0f;
  for (int i = first; i <= last; i++) {
    sum += mag_[i] * mag_[i];
  }

  // Parseval: the one-sided sum of squared magnitudes of a windowed tone is
  // proportional to A^2 * N^2, and mag_ is |X| scaled by magRef = FS * N / 4, so
  // RMS/FS = sqrt(sum / norm). The divisor depends on the window each backend
  // applies, so it comes from the backend: a window covering the whole range then
  // reads exactly like the broadband time-domain level, and gate/attack settings
  // transfer between the two.
  const float rms = sqrtf(sum / fftb::bandPowerNorm());
  const float db = 20.0f * log10f(rms + 1e-9f) + (float)gainDb_;
  return clamp01((db - kLevelFloorDb) / (kLevelFullDb - kLevelFloorDb));
}

int SpectrumAnalyzer::bandFromHz(float hz) const {
  constexpr int kTop = (int)kBands - 1;
  if (hz <= kAudioMinHz) {
    return 0;
  }
  if (hz >= kAudioMaxHz) {
    return kTop;
  }
  const float ratio = kAudioMaxHz / kAudioMinHz;
  const int b = (int)(logf(hz / kAudioMinHz) / logf(ratio) * (float)kBands);
  return b < 0 ? 0 : (b > kTop ? kTop : b);
}
