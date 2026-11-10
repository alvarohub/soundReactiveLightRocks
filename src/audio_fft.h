#pragma once

#include <Arduino.h>

#include "config.h"

// Microphone capture + FFT analysis for the M5StickC Plus2 built-in PDM mic.
//
// The capture path goes through M5Unified (M5.Mic), so no pin or I2S setup is
// needed here. Each update() grabs one frame of audio, windows it, runs a real
// FFT and turns the spectrum into log-spaced bands plus a broadband level.
//
// The level is reported unsmoothed: threshold and retrigger timing belong to the
// blob engine, which works in real time rather than per frame.
class SpectrumAnalyzer {
public:
  static constexpr size_t kSamples = kFftSamples;
  static constexpr size_t kBins = kSamples / 2; // usable magnitude bins
  static constexpr size_t kBands = kDisplayBands; // log-spaced display bands

  // Width of the dB window that level()/bandLevel() map onto 0..1. Exposed so
  // callers can undo the microphone gain (level shifts by gainDb / this).
  static constexpr float kLevelRangeDb = 37.0f;

  bool begin(uint32_t sampleRateHz);

  // Blocks until one frame has been captured and analysed (~32 ms at 16 kHz).
  // Returns false if the capture path is not running.
  bool update();

  // Negative values attenuate the analysis (useful in loud rooms).
  void setGainDb(int8_t gainDb) { gainDb_ = gainDb; }

  const float *bands() const { return band_; } // 0..1, smoothed for display
  float level() const { return level_; }       // 0..1, instantaneous, broadband
  float levelDb() const { return levelDb_; }   // dBFS, instantaneous
  float peakHz() const { return peakHz_; }     // dominant frequency

  // Energy inside center +/- width/2 Hz, on the same 0..1 scale as level(), so
  // the same gate and attack/release settings work for any window. A window
  // covering the whole analysis range reads the same as the broadband level.
  float bandLevel(float centerHz, float widthHz) const;

  // Which display bar a frequency falls in, used to draw the band on screen.
  int bandFromHz(float hz) const;

  static float clampLevel(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
  }

  // Spectral flux of the last frame: the summed positive change of the
  // spectrum. bandFlux() restricts this to center +/- width/2 Hz and normalises
  // for window width and mic gain so each strip can run its own beat detector.
  float flux() const { return flux_; }
  float bandFlux(float centerHz, float widthHz) const;

private:
  void computeBandEdges();

  float re_[kSamples] = {};
  float im_[kSamples] = {};
  float mag_[kBins] = {};
  float prevMag_[kBins] = {};
  float rise_[kBins] = {};
  int16_t pcm_[kSamples] = {};

  float band_[kBands] = {};
  uint16_t bandLo_[kBands] = {};
  uint16_t bandHi_[kBands] = {};

  uint32_t sampleRate_ = 0;
  float level_ = 0.0f;
  float levelDb_ = -60.0f;
  float peakHz_ = 0.0f;
  float flux_ = 0.0f;
  int8_t gainDb_ = 0;
  bool ready_ = false;
};
