#include "settings.h"

#include <Arduino.h>

namespace {

constexpr char kNamespace[] = "lightrocks";
constexpr char kLiveKey[] = "live";
constexpr char kPresetKeys[SettingsStore::kPresetCount][8] = {
    "preset0", "preset1", "preset2"};

// Older layouts, kept so presets written by earlier firmware still load: their
// single "strip" becomes layer 0 of that physical strip.
constexpr uint8_t kBlobVersionV5 = 5;
constexpr uint8_t kBlobVersionV4 = 4;
constexpr uint8_t kBlobVersionV3 = 3;
constexpr size_t kStripBlockV4 = 26;
constexpr size_t kStripBlockV3 = 24;

inline void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

inline uint16_t get16(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

// Physical block (4 bytes): ledCount(2), brightnessPct, layerCount.
inline void encodePhys(const PhysicalStripSettings &p, uint8_t *b) {
  put16(b + 0, p.ledCount);
  b[2] = p.brightnessPct;
  b[3] = p.layerCount;
}

inline void decodePhys(const uint8_t *b, PhysicalStripSettings &p) {
  p.ledCount = get16(b + 0);
  p.brightnessPct = b[2];
  p.layerCount = b[3];
}

// Layer block (26 bytes): triggers, effect, enabled, intensityPct, centerHz(2),
// widthHz(2), thresholdPct, beatSensitivityPct, retriggerMs(2), blobLeds(2),
// speed(2), fadeMs(2), reverse, loop, retrigger, r, g, b,
// motionThresholdPct, spare.
inline void encodeLayer(const LayerSettings &s, uint8_t *p) {
  p[0] = s.triggers;
  p[1] = s.effect;
  p[2] = s.enabled;
  p[3] = s.intensityPct;
  put16(p + 4, s.centerHz);
  put16(p + 6, s.widthHz);
  p[8] = s.thresholdPct;
  p[9] = s.beatSensitivityPct;
  put16(p + 10, s.retriggerMs);
  put16(p + 12, s.blobLeds);
  put16(p + 14, s.speed);
  put16(p + 16, s.fadeMs);
  p[18] = s.reverse;
  p[19] = s.loop;
  p[20] = s.retrigger;
  p[21] = s.r;
  p[22] = s.g;
  p[23] = s.b;
  p[24] = s.motionThresholdPct;
  p[25] = 0; // spare, keeps the block a round 26 bytes
}

inline void decodeLayer(const uint8_t *p, LayerSettings &s) {
  s.triggers = p[0];
  s.effect = p[1];
  s.enabled = p[2];
  s.intensityPct = p[3];
  s.centerHz = get16(p + 4);
  s.widthHz = get16(p + 6);
  s.thresholdPct = p[8];
  s.beatSensitivityPct = p[9];
  s.retriggerMs = get16(p + 10);
  s.blobLeds = get16(p + 12);
  s.speed = get16(p + 14);
  s.fadeMs = get16(p + 16);
  s.reverse = p[18];
  s.loop = p[19];
  s.retrigger = p[20];
  s.r = p[21];
  s.g = p[22];
  s.b = p[23];
  s.motionThresholdPct = p[24];
}

// A single old `mode` enum becomes the matching trigger bit.
inline uint8_t triggersFromMode(uint8_t mode) {
  switch (mode) {
  case 1:
    return kTrigBeat;
  case 2:
    return kTrigTap; // older "manual pads" mode
  default:
    return kTrigThreshold;
  }
}

// v4 strip block -> physical container + layer 0.
inline void decodeStripV4(const uint8_t *p, PhysicalStripSettings &phys,
                          LayerSettings &l) {
  l.triggers = triggersFromMode(p[0]);
  l.enabled = p[1];
  phys.ledCount = get16(p + 2);
  l.centerHz = get16(p + 4);
  l.widthHz = get16(p + 6);
  l.thresholdPct = p[8];
  l.retriggerMs = get16(p + 9);
  l.blobLeds = get16(p + 11);
  l.speed = get16(p + 13);
  l.fadeMs = get16(p + 15);
  l.reverse = p[17];
  l.r = p[18];
  l.g = p[19];
  l.b = p[20];
  l.loop = p[21];
  phys.brightnessPct = p[22];
  l.retrigger = p[23];
  l.beatSensitivityPct = p[24];
  l.effect = p[25];
  phys.layerCount = 1;
  l.intensityPct = 100;
  l.motionThresholdPct = 30;
}

// v3 strip block -> same, but without beatSensitivityPct / effect.
inline void decodeStripV3(const uint8_t *p, PhysicalStripSettings &phys,
                          LayerSettings &l) {
  decodeStripV4(p, phys, l); // identical up to byte 23
  l.beatSensitivityPct = 55;
  l.effect = LayerSettings::kEffectJet;
}

} // namespace

void encodeSettings(const Settings &s, uint8_t *out) {
  out[0] = kSettingsBlobVersion;
  out[1] = s.gainDb;
  size_t n = kSettingsHeaderSize;

  for (size_t i = 0; i < kStripCount; i++) {
    encodePhys(s.phys[i], out + n);
    n += kPhysBlockSize;
  }
  for (size_t i = 0; i < kStripCount; i++) {
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      encodeLayer(s.layer[i][l], out + n);
      n += kLayerBlockSize;
    }
  }
}

bool decodeSettings(const uint8_t *in, size_t len, Settings &out) {
  if (in == nullptr || len < kSettingsHeaderSize) {
    return false;
  }

  Settings s;
  s.gainDb = in[1];

  if (in[0] == kSettingsBlobVersion && len == kSettingsBlobSize) {
    size_t n = kSettingsHeaderSize;
    for (size_t i = 0; i < kStripCount; i++) {
      decodePhys(in + n, s.phys[i]);
      n += kPhysBlockSize;
    }
    for (size_t i = 0; i < kStripCount; i++) {
      for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
        decodeLayer(in + n, s.layer[i][l]);
        n += kLayerBlockSize;
      }
    }
  } else if (in[0] == kBlobVersionV5 &&
             len == kSettingsHeaderSize + kPhysBlockSize * kStripCount +
                        (size_t)24 * kStripCount * kMaxLayersPerStrip) {
    size_t n = kSettingsHeaderSize;
    for (size_t i = 0; i < kStripCount; i++) {
      decodePhys(in + n, s.phys[i]);
      n += kPhysBlockSize;
    }
    for (size_t i = 0; i < kStripCount; i++) {
      for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
        // v5 layer block is 24 bytes and byte 0 is the old mode enum.
        const uint8_t *b = in + n;
        LayerSettings &d = s.layer[i][l];
        d.triggers = triggersFromMode(b[0]);
        d.effect = b[1];
        d.enabled = b[2];
        d.intensityPct = b[3];
        d.centerHz = get16(b + 4);
        d.widthHz = get16(b + 6);
        d.thresholdPct = b[8];
        d.beatSensitivityPct = b[9];
        d.retriggerMs = get16(b + 10);
        d.blobLeds = get16(b + 12);
        d.speed = get16(b + 14);
        d.fadeMs = get16(b + 16);
        d.reverse = b[18];
        d.loop = b[19];
        d.retrigger = b[20];
        d.r = b[21];
        d.g = b[22];
        d.b = b[23];
        d.motionThresholdPct = 30;
        n += 24;
      }
    }
  } else if (in[0] == kBlobVersionV4 &&
             len == kSettingsHeaderSize + kStripBlockV4 * kStripCount) {
    for (size_t i = 0; i < kStripCount; i++) {
      decodeStripV4(in + kSettingsHeaderSize + i * kStripBlockV4, s.phys[i],
                    s.layer[i][0]);
    }
  } else if (in[0] == kBlobVersionV3 &&
             len == kSettingsHeaderSize + kStripBlockV3 * kStripCount) {
    for (size_t i = 0; i < kStripCount; i++) {
      decodeStripV3(in + kSettingsHeaderSize + i * kStripBlockV3, s.phys[i],
                    s.layer[i][0]);
    }
  } else {
    return false;
  }

  clampSettings(s);
  out = s;
  return true;
}

const char *SettingsStore::presetKey(uint8_t slot) {
  return slot < kPresetCount ? kPresetKeys[slot] : nullptr;
}

bool SettingsStore::begin() {
  ready_ = prefs_.begin(kNamespace, false);
  if (!ready_) {
    Serial.println("[cfg] NVS unavailable");
    return false;
  }
  for (uint8_t i = 0; i < kPresetCount; i++) {
    uint8_t buf[kSettingsBlobSize];
    Settings tmp;
    const size_t len = prefs_.getBytes(kPresetKeys[i], buf, sizeof(buf));
    hasPreset_[i] = decodeSettings(buf, len, tmp);
  }
  return true;
}

bool SettingsStore::loadLive(Settings &out) {
  if (!ready_) {
    return false;
  }
  uint8_t buf[kSettingsBlobSize];
  const size_t len = prefs_.getBytes(kLiveKey, buf, sizeof(buf));
  return decodeSettings(buf, len, out);
}

bool SettingsStore::saveLive(const Settings &s) {
  if (!ready_) {
    return false;
  }
  uint8_t buf[kSettingsBlobSize];
  encodeSettings(s, buf);
  return prefs_.putBytes(kLiveKey, buf, sizeof(buf)) == sizeof(buf);
}

bool SettingsStore::loadPreset(uint8_t slot, Settings &out) {
  if (!ready_ || slot >= kPresetCount) {
    return false;
  }
  uint8_t buf[kSettingsBlobSize];
  const size_t len = prefs_.getBytes(presetKey(slot), buf, sizeof(buf));
  return decodeSettings(buf, len, out);
}

bool SettingsStore::savePreset(uint8_t slot, const Settings &s) {
  if (!ready_ || slot >= kPresetCount) {
    return false;
  }
  uint8_t buf[kSettingsBlobSize];
  encodeSettings(s, buf);
  const bool ok =
      prefs_.putBytes(presetKey(slot), buf, sizeof(buf)) == sizeof(buf);
  if (ok) {
    hasPreset_[slot] = true;
  }
  return ok;
}

bool SettingsStore::hasPreset(uint8_t slot) const {
  return slot < kPresetCount && hasPreset_[slot];
}
