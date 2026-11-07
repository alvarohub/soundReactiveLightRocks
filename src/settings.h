#pragma once

#include <Preferences.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

// ---------------------------------------------------------------------------
// Two different objects
//
//   PhysicalStripSettings  the hardware container: how many LEDs, how bright,
//                          how many light layers live inside it. One per pin.
//   LayerSettings          one glowing object inside a container: its own band,
//                          trigger input, effect, colour, motion and weight.
//
// Layers never talk to each other. They all accumulate into the same linear
// frame buffer of their physical strip, so overlapping beams add as light and
// gamma is applied once, at the output edge.
// ---------------------------------------------------------------------------

constexpr size_t kMaxLayersPerStrip = 4;

struct LayerSettings {
  enum Mode : uint8_t {
    kThreshold = 0, // fires when the band level rises above thresholdPct
    kBeat = 1,      // fires on detected beats inside this layer's band
    kManual = 2,    // silent until fired from the pads (or BtnA)
  };

  enum Effect : uint8_t {
    kEffectJet = 0,     // travelling light blob / jet
    kEffectFlash = 1,   // whole-strip flash that fades out
    kEffectBurst = 2,   // shockwave splitting outward from the strip centre
    kEffectSparkle = 3, // glittering shower of sparks
  };

  enum DirMode : uint8_t {
    kForward = 0,  // 0 -> N-1
    kReverse = 1,  // N-1 -> 0
    kPingPong = 2, // bounces at the ends
  };

  uint8_t mode = kThreshold;
  uint8_t effect = kEffectJet;
  uint8_t enabled = 1;
  uint8_t intensityPct = 100; // this layer's weight in the merged output (1..100)

  // The layer only listens to this window: center +/- width/2.
  uint16_t centerHz = 150;
  uint16_t widthHz = 250;

  uint8_t thresholdPct = 35;       // used in kThreshold mode
  uint8_t beatSensitivityPct = 55; // used in kBeat mode

  uint8_t retrigger = 1;      // 1 = gap after retriggerMs, 0 = wait until idle
  uint16_t retriggerMs = 300; // minimum gap between effects on this layer

  uint16_t blobLeds = 6;  // length of the light group, in LEDs
  uint16_t speed = 60;    // LEDs per second
  uint16_t fadeMs = 1500; // time for an effect to fade to nothing
  uint8_t reverse = 0;    // 0 = forward, 1 = reverse, 2 = ping-pong
  uint8_t loop = 1;       // 1 = loop around ring, 0 = one-shot

  uint8_t r = 255;
  uint8_t g = 106;
  uint8_t b = 0;
};

struct PhysicalStripSettings {
  uint16_t ledCount = kDefaultLedsPerStrip;
  uint8_t brightnessPct = 100; // master ceiling for this strip (1..100)
  uint8_t layerCount = 1;      // how many of the kMaxLayersPerStrip slots are live
};

// Runtime parameters, shared by the strips, the screen and the web UI. Every
// value is clamped into its documented range, so a bad value coming over HTTP or
// out of flash can never reach the strip driver.
struct Settings {
  static constexpr uint16_t kMinLeds = 1;
  static constexpr uint16_t kMaxLeds = kMaxLedsPerStrip;
  static constexpr uint16_t kMinCenterHz = (uint16_t)kAudioMinHz;
  static constexpr uint16_t kMaxCenterHz = (uint16_t)kAudioMaxHz;
  static constexpr uint16_t kMinWidthHz = 40;
  static constexpr uint16_t kMaxWidthHz = 16000;
  static constexpr uint16_t kMaxRetriggerMs = 2000;
  static constexpr uint16_t kMaxBlobLeds = kMaxLedsPerStrip;
  static constexpr uint16_t kMinSpeed = 1;
  static constexpr uint16_t kMaxSpeed = 500;
  static constexpr uint16_t kMinFadeMs = 50;
  static constexpr uint16_t kMaxFadeMs = 10000;
  static constexpr uint8_t kMaxBrightnessPct = 100;
  static constexpr uint8_t kMinIntensityPct = 1;

  PhysicalStripSettings phys[kStripCount];
  LayerSettings layer[kStripCount][kMaxLayersPerStrip];
  uint8_t gainDb = 0; // shared: there is one microphone
};

inline uint16_t clampU16(int32_t v, uint16_t lo, uint16_t hi) {
  if (v < (int32_t)lo) {
    return lo;
  }
  return v > (int32_t)hi ? hi : (uint16_t)v;
}

inline uint8_t clampU8(int32_t v, uint8_t lo, uint8_t hi) {
  if (v < (int32_t)lo) {
    return lo;
  }
  return v > (int32_t)hi ? hi : (uint8_t)v;
}

inline void clampPhysicalStrip(PhysicalStripSettings &p) {
  p.ledCount = clampU16(p.ledCount, Settings::kMinLeds, Settings::kMaxLeds);
  p.brightnessPct = clampU8(p.brightnessPct, 1, Settings::kMaxBrightnessPct);
  p.layerCount = clampU8(p.layerCount, 1, (uint8_t)kMaxLayersPerStrip);
}

inline void clampLayer(LayerSettings &s) {
  s.mode = clampU8(s.mode, LayerSettings::kThreshold, LayerSettings::kManual);
  s.effect =
      clampU8(s.effect, LayerSettings::kEffectJet, LayerSettings::kEffectSparkle);
  s.enabled = s.enabled ? 1 : 0;
  s.intensityPct =
      clampU8(s.intensityPct, Settings::kMinIntensityPct, 100);

  s.centerHz = clampU16(s.centerHz, Settings::kMinCenterHz, Settings::kMaxCenterHz);
  s.widthHz = clampU16(s.widthHz, Settings::kMinWidthHz, Settings::kMaxWidthHz);

  s.thresholdPct = clampU8(s.thresholdPct, 0, 100);
  s.beatSensitivityPct = clampU8(s.beatSensitivityPct, 0, 100);
  s.retrigger = s.retrigger ? 1 : 0;
  s.retriggerMs = clampU16(s.retriggerMs, 0, Settings::kMaxRetriggerMs);

  s.blobLeds = clampU16(s.blobLeds, 1, Settings::kMaxBlobLeds);
  s.speed = clampU16(s.speed, Settings::kMinSpeed, Settings::kMaxSpeed);
  s.fadeMs = clampU16(s.fadeMs, Settings::kMinFadeMs, Settings::kMaxFadeMs);
  s.reverse = clampU8(s.reverse, LayerSettings::kForward, LayerSettings::kPingPong);
  s.loop = s.loop ? 1 : 0;
}

inline void clampSettings(Settings &s) {
  for (size_t i = 0; i < kStripCount; i++) {
    clampPhysicalStrip(s.phys[i]);
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      clampLayer(s.layer[i][l]);
    }
  }

  // The microphone only offers 0 / 6 / 12 / 18 dB.
  s.gainDb = clampU8(((s.gainDb + 3) / 6) * 6, 0, 18);
}

// First-run values: each strip starts with one layer. Strip 1 takes the bass in
// orange, strip 2 the mids in blue, so a pair of rocks does something
// interesting out of the box.
inline Settings defaultSettings() {
  Settings s;

  LayerSettings &a = s.layer[0][0];
  a.centerHz = 150;
  a.widthHz = 250;
  a.thresholdPct = 35;
  a.retriggerMs = 300;
  a.blobLeds = 8;
  a.speed = 45;
  a.fadeMs = 2000;
  a.r = 255;
  a.g = 106;
  a.b = 0;

  if (kStripCount > 1) {
    LayerSettings &b = s.layer[1][0];
    b.centerHz = 1000;
    b.widthHz = 1400;
    b.thresholdPct = 40;
    b.retriggerMs = 300;
    b.blobLeds = 5;
    b.speed = 80;
    b.fadeMs = 1200;
    b.r = 0;
    b.g = 102;
    b.b = 255;

    // Give strip 1's spare layer a distinct default so "add layer" is useful.
    LayerSettings &a2 = s.layer[0][1];
    a2.centerHz = 4000;
    a2.widthHz = 3000;
    a2.effect = LayerSettings::kEffectSparkle;
    a2.intensityPct = 70;
    a2.r = 255;
    a2.g = 220;
    a2.b = 120;
  }

  for (size_t i = 0; i < kStripCount; i++) {
    s.phys[i].layerCount = 1;
  }

  clampSettings(s);
  return s;
}

// ---------------------------------------------------------------------------
// Live telemetry (built by main.cpp, read by the screen and the web page).
// Integers only, so JSON formatting never depends on float printf support.
// ---------------------------------------------------------------------------

struct LayerTelemetry {
  uint8_t levelPct = 0;  // band level inside this layer's window
  uint8_t brightPct = 0; // peak contribution of this layer
  uint8_t blobs = 0;     // active effects on this layer
  uint8_t beatPct = 0;   // beat pulse envelope
  uint16_t bpm = 0;      // detected tempo in this layer's band (0 = unlocked)
};

struct PhysicalTelemetry {
  uint8_t brightPct = 0; // brightest channel of the merged frame
  LayerTelemetry layer[kMaxLayersPerStrip];
};

struct Telemetry {
  uint8_t levelPct = 0; // broadband input level
  int16_t dbfs = -99;
  uint16_t peakHz = 0;
  PhysicalTelemetry strip[kStripCount];
};

// ---------------------------------------------------------------------------
// Fixed-layout, versioned binary encoding of Settings for NVS flash storage.
// ---------------------------------------------------------------------------

constexpr size_t kLayerBlockSize = 24;
constexpr size_t kPhysBlockSize = 4;
constexpr size_t kSettingsHeaderSize = 2; // version, gainDb
constexpr size_t kSettingsBlobSize =
    kSettingsHeaderSize + kPhysBlockSize * kStripCount +
    kLayerBlockSize * kStripCount * kMaxLayersPerStrip;
constexpr uint8_t kSettingsBlobVersion = 5;

void encodeSettings(const Settings &s, uint8_t *out);
bool decodeSettings(const uint8_t *in, size_t len, Settings &out);

// Keeps the live parameters and three preset slots in NVS so they survive a
// power cycle. Slot indices are 0-based here; the web API uses 1-based numbers.
class SettingsStore {
public:
  static constexpr uint8_t kPresetCount = 3;

  bool begin();

  bool loadLive(Settings &out);
  bool saveLive(const Settings &s);

  bool loadPreset(uint8_t slot, Settings &out);
  bool savePreset(uint8_t slot, const Settings &s);
  bool hasPreset(uint8_t slot) const;

private:
  static const char *presetKey(uint8_t slot);

  Preferences prefs_;
  bool ready_ = false;
  bool hasPreset_[kPresetCount] = {false, false, false};
};
