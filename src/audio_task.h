#pragma once

#include <Arduino.h>

#include "audio_fft.h"
#include "config.h"
#include "settings.h"

// Latest analysis, published by the audio task and copied by the render loop.
//
// The microphone pipeline (I2S DMA -> record() -> FFT) runs at ~31 Hz, while the
// strip wants to be rendered far more often. They are independent rates, so the
// capture side publishes a snapshot and the render side reads whatever is current.
struct AudioSnapshot {
  uint32_t seq = 0;    // incremented once per analysed block
  bool valid = false;
  float level = 0.0f;  // broadband, 0..1
  float levelDb = -60.0f;
  float peakHz = 0.0f;
  float bands[SpectrumAnalyzer::kBands] = {};
  // Per layer, so the render loop does not have to touch the FFT at all.
  float layerLevel[kStripCount][kMaxLayersPerStrip] = {};
  float layerFlux[kStripCount][kMaxLayersPerStrip] = {};
};

// Owns the blocking microphone path in its own FreeRTOS task, so that
// SpectrumAnalyzer::update() never paces the animation.
class AudioTask {
public:
  // Starts the task. `settings` is read (never written) to know each layer's
  // frequency window. If the task cannot start, `running()` stays false and the
  // caller can fall back to calling the analyser inline.
  bool begin(SpectrumAnalyzer *analyzer, const Settings *settings);
  bool running() const { return handle_ != nullptr; }

  // Copies the latest snapshot. Returns false when nothing new has been
  // published since `seenSeq`, so the caller can run onset detection exactly
  // once per audio block instead of once per rendered frame.
  bool read(AudioSnapshot &out, uint32_t &seenSeq);

private:
  static void taskEntry(void *arg);
  void run();

  SpectrumAnalyzer *analyzer_ = nullptr;
  const Settings *settings_ = nullptr;
  TaskHandle_t handle_ = nullptr;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  AudioSnapshot shared_;
};
