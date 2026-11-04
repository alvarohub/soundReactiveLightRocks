#include "store.h"

#include "settings_blob.h"

namespace {

constexpr char kNamespace[] = "lightrocks";
constexpr char kLiveKey[] = "live";
constexpr char kPresetKeys[SettingsStore::kPresetCount][8] = {
    "preset0", "preset1", "preset2"};

} // namespace

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
