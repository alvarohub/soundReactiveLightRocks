#include "settings.h"

#include <Arduino.h>

namespace {

constexpr char kNamespace[] = "lightrocks";
constexpr char kLiveKey[] = "live";
constexpr char kPresetKeys[SettingsStore::kPresetCount][8] = {
    "preset0", "preset1", "preset2"};

// Legacy v3 layout constants so presets saved before v4 upgrade cleanly.
constexpr uint8_t kSettingsBlobVersionV3 = 3;
constexpr size_t kStripBlockSizeV3 = 24;
constexpr size_t kSettingsBlobSizeV3 =
    kSettingsHeaderSize + kStripBlockSizeV3 * kStripCount;

inline void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

inline uint16_t get16(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

// One strip's settings <-> 26-byte block (v4):
//   0:mode, 1:enabled, 2..3:ledCount, 4..5:centerHz, 6..7:widthHz,
//   8:thresholdPct, 9..10:retriggerMs, 11..12:blobLeds, 13..14:speed,
//   15..16:fadeMs, 17:reverse, 18:r, 19:g, 20:b, 21:loop, 22:brightnessPct,
//   23:retrigger, 24:beatSensitivityPct, 25:effect
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
  p[24] = s.beatSensitivityPct;
  p[25] = s.effect;
}

inline void decodeStripV3(const uint8_t *p, StripSettings &s) {
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
  s.beatSensitivityPct = 55;
  s.effect = StripSettings::kEffectJet;
}

inline void decodeStripV4(const uint8_t *p, StripSettings &s) {
  decodeStripV3(p, s);
  s.beatSensitivityPct = p[24];
  s.effect = p[25];
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
  if (in == nullptr || len < kSettingsHeaderSize) {
    return false;
  }

  Settings s;
  s.gainDb = in[1];

  if (in[0] == kSettingsBlobVersion && len == kSettingsBlobSize) {
    for (size_t i = 0; i < kStripCount; i++) {
      decodeStripV4(in + kSettingsHeaderSize + i * kStripBlockSize, s.strip[i]);
    }
  } else if (in[0] == kSettingsBlobVersionV3 && len == kSettingsBlobSizeV3) {
    for (size_t i = 0; i < kStripCount; i++) {
      decodeStripV3(in + kSettingsHeaderSize + i * kStripBlockSizeV3,
                    s.strip[i]);
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
