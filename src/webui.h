#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "config.h"
#include "settings.h"

// Access point + web page used as the remote control. The phone connects
// straight to the board, so no router is involved.
//
// The API addresses a layer as ?strip=<p>&layer=<l>; the physical container
// itself (ledCount, brightness, layer count) uses ?strip=<p> with no layer.
class WebUi {
public:
  static constexpr const char *kSsid = kWifiSsid;
  static constexpr const char *kPassword = kWifiPassword;

  void setup(Settings *settings, SettingsStore *store);
  bool begin();
  void update() {
    if (enabled_) {
      server_.handleClient();
    }
  }

  // True once after a request changed the live settings, so the caller can
  // persist them (debounced, to spare the flash).
  bool consumeChanged() {
    const bool changed = changed_;
    changed_ = false;
    return changed;
  }

  // True once when a pad on strip `p`, layer `l` was tapped; writes the chosen
  // effect into `effectOut`.
  bool consumeTap(size_t p, size_t l, uint8_t &effectOut) {
    if (p >= kStripCount || l >= kMaxLayersPerStrip || !tapPending_[p][l]) {
      return false;
    }
    tapPending_[p][l] = false;
    effectOut = tapEffect_[p][l];
    return true;
  }

  // Returns 1 when a pad is held/refreshed, 0 when released, or -1 when no hold
  // state change occurred since the last check.
  int8_t consumeHold(size_t p, size_t l, uint8_t &effectOut) {
    if (p >= kStripCount || l >= kMaxLayersPerStrip) {
      return -1;
    }
    const int8_t st = holdState_[p][l];
    holdState_[p][l] = -1;
    effectOut = holdEffect_[p][l];
    return st;
  }

  // Solo: only this layer of strip `p` renders. -1 = no solo.
  int8_t soloLayer(size_t p) const {
    return p < kStripCount ? solo_[p] : (int8_t)-1;
  }

  // Layer currently being edited on strip `p`, used to colour the calibration
  // dot and to pre-select the tab after a reload.
  uint8_t editLayer(size_t p) const {
    return p < kStripCount ? edit_[p] : 0;
  }

  bool enabled() const { return enabled_; }
  bool setEnabled(bool on);
  String ip() const;
  void setTelemetry(const Telemetry &t) { tele_ = t; }

  // Clears the "saving…" hint once the caller has written to flash.
  void markSaved() { unsaved_ = false; }

  // True while the user is adjusting ledCount (auto-holds for 3.5 s) or has
  // toggled the calibration dot button on for strip `p`.
  bool isCalibrating(size_t p, uint32_t nowMs) const {
    if (p >= kStripCount) {
      return false;
    }
    if (calibPinned_[p]) {
      return true;
    }
    return calibUntilMs_[p] != 0 && (int32_t)(calibUntilMs_[p] - nowMs) > 0;
  }

private:
  void bindRoutes();
  String stateJson() const;
  void handleRoot();
  void handleState();
  void handleParams();
  void handleTap();
  void handlePreset();
  void handleReset();
  bool readIndex(const char *name, size_t limit, size_t &out);
  void applyArg(const char *name, uint8_t *dst);
  void applyArg(const char *name, uint16_t *dst);

  WebServer server_{80};
  Settings *settings_ = nullptr;
  SettingsStore *store_ = nullptr;
  Telemetry tele_;

  uint32_t calibUntilMs_[kStripCount] = {};
  bool calibPinned_[kStripCount] = {};
  bool tapPending_[kStripCount][kMaxLayersPerStrip] = {};
  uint8_t tapEffect_[kStripCount][kMaxLayersPerStrip] = {};
  int8_t holdState_[kStripCount][kMaxLayersPerStrip] = {};
  uint8_t holdEffect_[kStripCount][kMaxLayersPerStrip] = {};
  bool padDown_[kStripCount][kMaxLayersPerStrip] = {};
  int8_t solo_[kStripCount] = {-1, -1};
  uint8_t edit_[kStripCount] = {};

  bool enabled_ = false;
  bool routesBound_ = false;
  bool changed_ = false;
  bool unsaved_ = false;
};
