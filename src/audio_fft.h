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
  static constexpr size_t kBands = 30;          // log-spaced display bands

  bool begin(uint32_t sampleRateHz);

  // Blocks until one frame has been captured and analysed (~32 ms at 16 kHz).
  // Returns false if the capture path is not running.
  bool update();

  void setGainDb(uint8_t gainDb) { gainDb_ = gainDb; }

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

  // Spectral flux of the last frame: the summed positive change of the
  // spectrum. This is the onset function the beat detector works from.
  float flux() const { return flux_; }

private:
  void computeBandEdges();

  float re_[kSamples] = {};
  float im_[kSamples] = {};
  float mag_[kBins] = {};
  float prevMag_[kBins] = {};
  int16_t pcm_[kSamples] = {};

  float band_[kBands] = {};
  uint16_t bandLo_[kBands] = {};
  uint16_t bandHi_[kBands] = {};

  uint32_t sampleRate_ = 0;
  float level_ = 0.0f;
  float levelDb_ = -60.0f;
  float peakHz_ = 0.0f;
  float flux_ = 0.0f;
  uint8_t gainDb_ = 0;
  bool ready_ = false;
};
