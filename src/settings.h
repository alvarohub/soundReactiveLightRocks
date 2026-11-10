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

// Trigger inputs. These are NOT exclusive: a layer can be armed by several at
// once and fires whenever any of them does. Stored as a bitmask.
enum LayerTrigger : uint8_t {
  kTrigThreshold = 1u << 0, // band level rises above thresholdPct
  kTrigBeat      = 1u << 1, // a beat is detected inside this layer's band
  kTrigTap       = 1u << 2, // a fingertip pad (or BtnA) is pressed
  kTrigAccel     = 1u << 3, // the stick is whipped / shaken
  kTrigGyro      = 1u << 4, // the stick is twisted quickly
  kTrigLfo       = 1u << 5, // a free-running metronome ("glow" is a slow one)
};

constexpr uint8_t kAllTriggers = kTrigThreshold | kTrigBeat | kTrigTap |
                                 kTrigAccel | kTrigGyro | kTrigLfo;

struct LayerSettings {
  enum Effect : uint8_t {
    kEffectJet = 0,     // travelling light blob / jet
    kEffectFlash = 1,   // whole-strip flash that fades out
    kEffectBurst = 2,   // shockwave splitting outward from the strip centre
    kEffectSparkle = 3, // glittering shower of sparks
    kEffectWhip = 4, // whip: speed from |accel|, colour from its direction
    // There is no "glow" effect: a glow is a Flash with a long attack and
    // release fired by the LFO trigger. Trigger and effect stay orthogonal.
  };

  enum DirMode : uint8_t {
    kForward = 0,  // 0 -> N-1
    kReverse = 1,  // N-1 -> 0
    kPingPong = 2, // bounces at the ends
  };

  uint8_t triggers = kTrigThreshold; // bitmask of LayerTrigger
  uint8_t effect = kEffectJet;
  uint8_t enabled = 1;
  uint8_t intensityPct = 100; // this layer's weight in the merged output (1..100)
  // Softness: widens this layer's spatial profile, so its edges bleed outward.
  // A layer property because it shapes *this* blob, before anything is merged.
  uint8_t softnessPct = 0;

  // The layer only listens to this window: center +/- width/2.
  uint16_t centerHz = 150;
  uint16_t widthHz = 250;

  uint8_t thresholdPct = 35;       // gate for kTrigThreshold
  uint8_t beatSensitivityPct = 55; // eagerness of kTrigBeat
  // Gate for the motion triggers: |accel| or |gyro| must reach this share of
  // full scale before kTrigAccel / kTrigGyro fires.
  uint8_t motionThresholdPct = 30;

  // Flash envelope. A short attack/release with the LFO at a fast period is a
  // strobe; a long attack/release with a slow LFO is a glow.
  uint16_t attackMs = 20;
  uint16_t releaseMs = 400;

  // Free-running metronome period for kTrigLfo.
  uint16_t lfoPeriodMs = 2000;

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
  int8_t gainDb = 0;  // shared: there is one microphone; negative = attenuate
  uint8_t agc = 0;    // 1 = the firmware keeps gainDb in range by itself
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
  s.triggers = (uint8_t)(s.triggers & kAllTriggers);
  s.effect = clampU8(s.effect, LayerSettings::kEffectJet, LayerSettings::kEffectWhip);
  s.attackMs = clampU16(s.attackMs, 0, 2000);
  s.releaseMs = clampU16(s.releaseMs, 10, 5000);
  s.lfoPeriodMs = clampU16(s.lfoPeriodMs, 100, 5000);
  s.enabled = s.enabled ? 1 : 0;
  s.intensityPct =
      clampU8(s.intensityPct, Settings::kMinIntensityPct, 100);
  s.softnessPct = clampU8(s.softnessPct, 0, 100);

  s.centerHz = clampU16(s.centerHz, Settings::kMinCenterHz, Settings::kMaxCenterHz);
  s.widthHz = clampU16(s.widthHz, Settings::kMinWidthHz, Settings::kMaxWidthHz);

  s.thresholdPct = clampU8(s.thresholdPct, 0, 100);
  s.beatSensitivityPct = clampU8(s.beatSensitivityPct, 0, 100);
  s.motionThresholdPct = clampU8(s.motionThresholdPct, 0, 100);
  s.retrigger = s.retrigger ? 1 : 0;
  s.retriggerMs = clampU16(s.retriggerMs, 0, Settings::kMaxRetriggerMs);

  s.blobLeds = clampU16(s.blobLeds, 1, Settings::kMaxBlobLeds);
  s.speed = clampU16(s.speed, Settings::kMinSpeed, Settings::kMaxSpeed);
  s.fadeMs = clampU16(s.fadeMs, Settings::kMinFadeMs, Settings::kMaxFadeMs);
  s.reverse = clampU8(s.reverse, LayerSettings::kForward, LayerSettings::kPingPong);
  s.loop = s.loop ? 1 : 0;
}

// A brand new layer slot: OFF, with a plain threshold-driven jet in a colour
// that differs per slot. Adding a layer must never change what is on the strip
// until the user switches it on, and the four slots have to be tellable apart at
// a glance in the tab row and on the LCD.
inline LayerSettings defaultLayerFor(uint8_t slot) {
  static const uint8_t kPalette[kMaxLayersPerStrip][3] = {
      {255, 106, 0}, {0, 170, 255}, {80, 255, 130}, {255, 90, 200}};
  LayerSettings s;
  s.enabled = 0;
  const uint8_t i = slot < kMaxLayersPerStrip ? slot : 0;
  s.r = kPalette[i][0];
  s.g = kPalette[i][1];
  s.b = kPalette[i][2];
  // Each slot listens a little higher up, so stacking two layers immediately
  // gives two different bands instead of the same one twice.
  s.centerHz = (uint16_t)(150u + 250u * i);
  s.widthHz = 250;
  clampLayer(s);
  return s;
}

inline void clampSettings(Settings &s) {
  for (size_t i = 0; i < kStripCount; i++) {
    clampPhysicalStrip(s.phys[i]);
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      clampLayer(s.layer[i][l]);
    }
  }

  // Quantised to the 6 dB step so the slider and the stored value agree, and
  // clamped into a range that includes attenuation: in a noisy room a negative
  // gain is the useful setting.
  int g = (int)s.gainDb;
  if (g < (int)kMinGainDb) {
    g = (int)kMinGainDb;
  }
  if (g > (int)kMaxGainDb) {
    g = (int)kMaxGainDb;
  }
  // Round to the nearest step. Done by hand rather than with (g + step/2) /
  // step because C division truncates towards zero, which would map -24 to -18.
  const int step = (int)kGainStepDb;
  g = (g >= 0) ? ((g + step / 2) / step) * step
               : -(((-g + step / 2) / step) * step);
  if (g < (int)kMinGainDb) {
    g = (int)kMinGainDb;
  }
  if (g > (int)kMaxGainDb) {
    g = (int)kMaxGainDb;
  }
  s.gainDb = (int8_t)g;
  s.agc = s.agc ? 1 : 0;
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
  uint8_t beatConf = 0;  // how well the onsets fit that tempo grid, 0..100
};

struct PhysicalTelemetry {
  uint8_t brightPct = 0; // brightest channel of the merged frame
  LayerTelemetry layer[kMaxLayersPerStrip];
};

struct Telemetry {
  uint8_t levelPct = 0; // broadband input level
  // The 30 log-spaced display bands, 0..100. Sent so the phone can draw where the
  // layers sit on the spectrum; it is one byte per band, not an FFT stream.
  uint8_t spec[kDisplayBands] = {};
  int16_t dbfs = -99;
  uint16_t peakHz = 0;
  // What the automatic gain control is doing, so the phone can explain it:
  // -1 lowering, 0 holding (or idle), +1 raising. agcPeakPct is the peak the
  // decision was based on, in the same 0..100 scale as levelPct.
  int8_t agcAction = 0;
  uint8_t agcPeakPct = 0;
  uint8_t unsaved = 0; // settings differ from what is in flash
  PhysicalTelemetry strip[kStripCount];
};

// ---------------------------------------------------------------------------
// Fixed-layout, versioned binary encoding of Settings for NVS flash storage.
// ---------------------------------------------------------------------------

constexpr size_t kLayerBlockSize = 32;
constexpr size_t kPhysBlockSize = 6;
constexpr size_t kSettingsHeaderSize = 3; // version, gainDb, agc
constexpr size_t kSettingsBlobSize =
    kSettingsHeaderSize + kPhysBlockSize * kStripCount +
    kLayerBlockSize * kStripCount * kMaxLayersPerStrip;
constexpr uint8_t kSettingsBlobVersion = 10;

void encodeSettings(const Settings &s, uint8_t *out);
bool decodeSettings(const uint8_t *in, size_t len, Settings &out);

// Keeps the live parameters and three preset slots in NVS so they survive a
// power cycle. Slot indices are 0-based here; the web API uses 1-based numbers.
// What a preset slot holds, so the page can show it instead of a bare number.
struct PresetSummary {
  static constexpr uint8_t kMaxColors = kMaxLayersPerStrip;
  bool valid = false;
  uint8_t layers = 0; // live layers across every strip
  uint8_t colorCount = 0;
  // One colour per live layer (the first few), so a slot can be previewed.
  uint8_t r[kMaxColors] = {};
  uint8_t g[kMaxColors] = {};
  uint8_t b[kMaxColors] = {};
};

class SettingsStore {
public:
  static constexpr uint8_t kPresetCount = 3;

  bool begin();

  bool loadLive(Settings &out);
  bool saveLive(const Settings &s);

  bool loadPreset(uint8_t slot, Settings &out);
  bool savePreset(uint8_t slot, const Settings &s);
  bool hasPreset(uint8_t slot) const;
  // Decodes a slot just enough to describe it. Leaves the live settings alone.
  bool presetSummary(uint8_t slot, PresetSummary &out);

private:
  static const char *presetKey(uint8_t slot);

  Preferences prefs_;
  bool ready_ = false;
  bool hasPreset_[kPresetCount] = {false, false, false};
};
