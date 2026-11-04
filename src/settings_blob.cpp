#include "settings_blob.h"

namespace {

inline void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

inline uint16_t get16(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

// mode, enabled, ledCount(2), center(2), width(2), threshold, retrigger(2),
// blobLeds(2), speed(2), fade(2), reverse, r, g, b, loop, brightnessPct,
// retriggerFlag
inline void encodeStrip(const StripSettings &s, uint8_t *p) {
  p[0] = s.mode;
  p[1] = s.enabled;
  put16(p + 2, s.ledCount);
  put16(p + 4, s.centerHz);
  put16(p + 6, s.widthHz);
  p[8] = s.thresholdPct;
  put16(p + 9, s.retriggerMs);
  put16(p + 11, s.blobLeds);
  put16(p + 13, s.speed);
  put16(p + 15, s.fadeMs);
  p[17] = s.reverse;
  p[18] = s.r;
  p[19] = s.g;
  p[20] = s.b;
  p[21] = s.loop;
  p[22] = s.brightnessPct;
  p[23] = s.retrigger;
}

inline void decodeStrip(const uint8_t *p, StripSettings &s) {
  s.mode = p[0];
  s.enabled = p[1];
  s.ledCount = get16(p + 2);
  s.centerHz = get16(p + 4);
  s.widthHz = get16(p + 6);
  s.thresholdPct = p[8];
  s.retriggerMs = get16(p + 9);
  s.blobLeds = get16(p + 11);
  s.speed = get16(p + 13);
  s.fadeMs = get16(p + 15);
  s.reverse = p[17];
  s.r = p[18];
  s.g = p[19];
  s.b = p[20];
  s.loop = p[21];
  s.brightnessPct = p[22];
  s.retrigger = p[23];
}

} // namespace

void encodeSettings(const Settings &s, uint8_t *out) {
  out[0] = kSettingsBlobVersion;
  out[1] = s.gainDb;
  for (size_t i = 0; i < kStripCount; i++) {
    encodeStrip(s.strip[i], out + kSettingsHeaderSize + i * kStripBlockSize);
  }
}

bool decodeSettings(const uint8_t *in, size_t len, Settings &out) {
  if (in == nullptr || len != kSettingsBlobSize || in[0] != kSettingsBlobVersion) {
    return false;
  }

  Settings s;
  s.gainDb = in[1];
  for (size_t i = 0; i < kStripCount; i++) {
    decodeStrip(in + kSettingsHeaderSize + i * kStripBlockSize, s.strip[i]);
  }
  clampSettings(s);

  out = s;
  return true;
}
