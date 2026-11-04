#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"

// Everything one strip can be told to do. Each strip owns a full set, so the two
// rocks can react to different bands, with different colours and speeds.
struct StripSettings {
  enum Mode : uint8_t {
    kBlob = 0, // a light group orbits the ring and fades out
  };

  enum DirMode : uint8_t {
    kForward = 0,  // 0 -> ledCount - 1
    kReverse = 1,  // ledCount - 1 -> 0
    kPingPong = 2, // bounces at the ends
  };

  uint8_t mode = kBlob;
  uint8_t enabled = 1;                      // 0 switches the strip off, keeping the rest
  uint8_t brightnessPct = 100;              // master brightness ceiling for this strip (1..100%)
  uint16_t ledCount = kDefaultLedsPerStrip; // how many LEDs this strip really has

  // The strip only listens to this window: center +/- width/2.
  uint16_t centerHz = 150;
  uint16_t widthHz = 250;

  // A blob is born when the band level rises above this.
  uint8_t thresholdPct = 12;
  uint8_t retrigger = 1;      // 1 = retrigger after retriggerMs, 0 = wait until strip is empty
  uint16_t retriggerMs = 250; // minimum gap between blobs on this strip

  uint16_t blobLeds = 6;  // length of the light group, in LEDs
  uint16_t speed = 60;    // LEDs per second along the ring
  uint16_t fadeMs = 1500; // time for a blob to fade to nothing
  uint8_t reverse = 0;    // 0 = forward, 1 = reverse, 2 = ping-pong
  uint8_t loop = 1;       // 1 = loop around ring, 0 = one-shot (disappear at end)

  uint8_t r = 255;
  uint8_t g = 106;
  uint8_t b = 0;
};

// Runtime parameters, shared by the strips, the screen and the web UI. All of
// them are clamped into their documented range, so a bad value coming over HTTP
// or out of flash can never reach the strip driver.
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

  StripSettings strip[kStripCount];
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

inline void clampStripSettings(StripSettings &s) {
  s.mode = clampU8(s.mode, StripSettings::kBlob, StripSettings::kBlob);
  s.enabled = s.enabled ? 1 : 0;
  s.brightnessPct = clampU8(s.brightnessPct, 1, 100);

  s.ledCount = clampU16(s.ledCount, Settings::kMinLeds, Settings::kMaxLeds);
  s.centerHz = clampU16(s.centerHz, Settings::kMinCenterHz, Settings::kMaxCenterHz);
  s.widthHz = clampU16(s.widthHz, Settings::kMinWidthHz, Settings::kMaxWidthHz);

  s.thresholdPct = clampU8(s.thresholdPct, 0, 100);
  s.retrigger = s.retrigger ? 1 : 0;
  s.retriggerMs = clampU16(s.retriggerMs, 0, Settings::kMaxRetriggerMs);

  s.blobLeds = clampU16(s.blobLeds, 1, Settings::kMaxBlobLeds);
  s.speed = clampU16(s.speed, Settings::kMinSpeed, Settings::kMaxSpeed);
  s.fadeMs = clampU16(s.fadeMs, Settings::kMinFadeMs, Settings::kMaxFadeMs);
  s.reverse = clampU8(s.reverse, StripSettings::kForward, StripSettings::kPingPong);
  s.loop = s.loop ? 1 : 0;
}

inline void clampSettings(Settings &s) {
  for (size_t i = 0; i < kStripCount; i++) {
    clampStripSettings(s.strip[i]);
  }

  // The microphone only offers 0 / 6 / 12 / 18 dB.
  s.gainDb = clampU8(((s.gainDb + 3) / 6) * 6, 0, 18);
}

// First-run values: strip 1 takes the bass, strip 2 the mids, so a pair of rocks
// does something interesting out of the box.
inline Settings defaultSettings() {
  Settings s;

  StripSettings &a = s.strip[0];
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
    StripSettings &b = s.strip[1];
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
  }

  clampSettings(s);
  return s;
}
