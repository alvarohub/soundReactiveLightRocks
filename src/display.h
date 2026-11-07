#pragma once

#include <Arduino.h>

#include "audio_fft.h"
#include "config.h"
#include "settings.h"

// On-device LCD renderer (240 x 135 landscape on M5StickS3 / M5StickC Plus2).
//
// Owns all screen layout coordinates and dirty-rectangle tracking so only
// changed pixels are sent over SPI each frame.
class Display {
public:
  void begin();

  void showBanner(const char *ssid, const char *password, bool apEnabled,
                  const String &ip);
  void finishBanner(const char *ssid, bool apEnabled, const String &ip);

  void renderFrame(const float *bands, const Settings &settings,
                   const Telemetry &tele, uint32_t nowMs, bool forceHud = false);

  void invalidateHud() { lastHudMs_ = 0; }

private:
  void computeLayout();
  int freqToX(float hz) const;
  uint16_t barColor(int y) const;
  uint16_t stripColor(const StripSettings &s) const;

  void drawSpectrum(const float *bands);
  void drawStripChrome(const float *bands, const Settings &settings);
  void drawMeters(const Telemetry &tele, const Settings &settings);
  void drawHud(const Telemetry &tele, const Settings &settings);
  void drawHint(const char *ssid, bool apEnabled, const String &ip);

  int barTop_ = 14;
  int barBottom_ = 73;
  int bandRowY_[kStripCount] = {};
  int meterY_[kStripCount] = {};
  int meterX_ = 15;
  int meterWidth_ = 222;
  int hudY_ = 111;
  int hintY_ = 123;
  int bandWidth_ = 8;
  int barWidth_ = 7;

  int prevBar_[SpectrumAnalyzer::kBands] = {};
  int prevMeterQ_[kStripCount] = {};
  int prevThrQ_[kStripCount] = {};
  uint32_t prevMeterColor_[kStripCount] = {};
  int prevBandLo_[kStripCount] = {};
  int prevBandHi_[kStripCount] = {};
  int prevBandCtr_[kStripCount] = {};
  uint32_t prevChrome_[kStripCount] = {};
  uint32_t lastHudMs_ = 0;
};
