#pragma once

#include <Arduino.h>

#include "audio_fft.h"
#include "config.h"
#include "settings.h"

// On-device LCD renderer (240 x 135 landscape on M5StickS3 / M5StickC Plus2).
//
// The screen shows ONE physical strip at a time (the one the side button
// selects): the shared spectrum on top, then one thin row per light layer of
// that strip. A row spans exactly the layer's frequency window on the same axis
// as the bars above it, is filled by how loud that band is, and carries a dot
// that lights while the layer is emitting. Four rows stack, so the whole
// container is legible at a glance. Only changed pixels are repainted.
class Display {
public:
  void begin();

  void showBanner(const char *ssid, const char *password, bool apEnabled,
                  const String &ip);
  void finishBanner(const char *ssid, bool apEnabled, const String &ip);

  void renderFrame(const float *bands, const Settings &settings,
                   const Telemetry &tele, uint32_t nowMs, bool forceHud = false);

  void invalidateHud() { lastHudMs_ = 0; }

  // Which physical strip the hardware buttons act on and the screen shows.
  void setSelectedStrip(uint8_t i);

private:
  void computeLayout();
  int freqToX(float hz) const;
  uint16_t barColor(int y) const;
  // Colour of a physical strip: the first enabled layer's colour, else grey.
  uint16_t stripColor(const Settings &settings, size_t i) const;
  // Index of the first enabled layer of strip i, or 0 when none is on.
  size_t primaryLayer(const Settings &settings, size_t i) const;
  // One-letter badge for the layer's armed triggers.
  static char triggerChar(const LayerSettings &s);

  void drawSpectrum(const float *bands);
  void drawHeader(const Settings &settings, const Telemetry &tele);
  void drawLayerRows(const Settings &settings, const Telemetry &tele);
  void drawHud(const Telemetry &tele, const Settings &settings);
  void drawHint(const char *ssid, bool apEnabled, const String &ip);
  // Forget every cached pixel so the next frame repaints from scratch.
  void invalidateAll();

  // Geometry. The frequency axis starts after the trigger-letter gutter so the
  // spectrum bars and the layer rows use identical x coordinates.
  int axisX0_ = 9;
  int axisX1_ = 218;
  int barTop_ = 13;
  int barBottom_ = 63;
  int rowY0_ = 67;
  int rowH_ = 9;
  int hudY_ = 109;
  int hintY_ = 123;
  int bandWidth_ = 7;
  int barWidth_ = 6;

  int prevBar_[SpectrumAnalyzer::kBands] = {};

  // Per-layer row cache, so a moving level only repaints the pixels that moved.
  int prevLo_[kMaxLayersPerStrip] = {};
  int prevHi_[kMaxLayersPerStrip] = {};
  int prevFill_[kMaxLayersPerStrip] = {};
  int prevCol_[kMaxLayersPerStrip] = {};
  int prevDot_[kMaxLayersPerStrip] = {};
  int prevCh_[kMaxLayersPerStrip] = {};
  uint8_t prevLayers_ = 0xFF;

  char prevHeadLeft_[24] = {};
  char prevHeadRight_[16] = {};
  int prevHeadCol_ = -1;

  uint32_t lastHudMs_ = 0;
  uint8_t selected_ = 0;
};
