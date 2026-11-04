#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "settings.h"

// Keeps the live parameters and three preset slots in NVS, so they survive a
// power cycle. Slot indices are 0-based here; the web API uses 1-based numbers
// because they are meant for humans.
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
