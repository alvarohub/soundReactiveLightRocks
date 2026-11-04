#pragma once

#include <stddef.h>
#include <stdint.h>

#include "settings.h"

// Fixed-layout, versioned encoding of Settings for non-volatile storage.
//
// Storing the raw struct would silently misread data after a firmware change, so
// a version byte makes an old blob fail cleanly and fall back to defaults.
//
// encodeStrip()/decodeStrip() below deal with ONE strip's settings block (22
// bytes) -- they are serialisation helpers, unrelated to the LedStrip hardware
// class in strip.h.
constexpr size_t kStripBlockSize = 24;
constexpr size_t kSettingsHeaderSize = 2; // version, gainDb
constexpr size_t kSettingsBlobSize = kSettingsHeaderSize + kStripBlockSize * kStripCount;
constexpr uint8_t kSettingsBlobVersion = 3;

void encodeSettings(const Settings &s, uint8_t *out);

// Returns false if the data is the wrong length or an unknown version.
// Accepted values are clamped, since flash contents cannot be trusted.
bool decodeSettings(const uint8_t *in, size_t len, Settings &out);
