#include "audio_task.h"

namespace {
// Stack has to hold the analyser's locals; its FFT buffers are members, not
// stack, so this is comfortable.
constexpr uint32_t kAudioTaskStack = 6144;
constexpr UBaseType_t kAudioTaskPriority = 3; // above loopTask (1), below Wi-Fi
// Core 0 runs the Wi-Fi stack; capture mostly sleeps in the I2S queue, and
// keeping it off core 1 leaves the render loop uninterrupted.
constexpr BaseType_t kAudioTaskCore = 0;
} // namespace

bool AudioTask::begin(SpectrumAnalyzer *analyzer, const Settings *settings) {
  analyzer_ = analyzer;
  settings_ = settings;
  if (handle_ != nullptr) {
    return true;
  }
  const BaseType_t ok = xTaskCreatePinnedToCore(
      &AudioTask::taskEntry, "audio", kAudioTaskStack, this, kAudioTaskPriority,
      &handle_, kAudioTaskCore);
  if (ok != pdPASS) {
    handle_ = nullptr;
    Serial.println("[audio] task start failed, falling back to inline capture");
    return false;
  }
  Serial.printf("[audio] task on core %d, prio %u\n", (int)kAudioTaskCore,
                (unsigned)kAudioTaskPriority);
  return true;
}

void AudioTask::taskEntry(void *arg) {
  static_cast<AudioTask *>(arg)->run();
}

void AudioTask::run() {
  // The microphone gain is a software setting inside the analyser, so it is
  // re-applied here rather than from the render loop: only this task may touch
  // the analyser.
  int8_t appliedGain = 127;

  for (;;) {
    if (settings_ != nullptr && appliedGain != settings_->gainDb) {
      appliedGain = settings_->gainDb;
      analyzer_->setGainDb(appliedGain);
    }

    if (!analyzer_->update()) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    AudioSnapshot s;
    s.valid = true;
    s.level = analyzer_->level();
    s.levelDb = analyzer_->levelDb();
    s.peakHz = analyzer_->peakHz();
    const float *bands = analyzer_->bands();
    for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
      s.bands[b] = bands[b];
    }

    // Pre-compute what each layer needs, so the render loop never reads the FFT.
    if (settings_ != nullptr) {
      for (size_t i = 0; i < kStripCount; i++) {
        for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
          const LayerSettings &ls = settings_->layer[i][l];
          const float c = (float)ls.centerHz;
          const float w = (float)ls.widthHz;
          s.layerLevel[i][l] = analyzer_->bandLevel(c, w);
          s.layerFlux[i][l] = analyzer_->bandFlux(c, w);
        }
      }
    }

    portENTER_CRITICAL(&mux_);
    const uint32_t next = shared_.seq + 1;
    shared_ = s;
    shared_.seq = next;
    portEXIT_CRITICAL(&mux_);
  }
}

bool AudioTask::read(AudioSnapshot &out, uint32_t &seenSeq) {
  bool fresh = false;
  portENTER_CRITICAL(&mux_);
  if (shared_.valid && shared_.seq != seenSeq) {
    seenSeq = shared_.seq;
    out = shared_;
    fresh = true;
  }
  portEXIT_CRITICAL(&mux_);
  return fresh;
}
