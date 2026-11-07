#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "config.h"
#include "settings.h"

// Access point + web page used as the remote control. The phone connects
// straight to the board, so no router is involved.
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

  // True once when a fingertip pad was tapped on strip `i`; writes the chosen
  // effect into `effectOut`.
  bool consumeTap(size_t i, uint8_t &effectOut) {
    if (i >= kStripCount || !tapPending_[i]) {
      return false;
    }
    tapPending_[i] = false;
    effectOut = tapEffect_[i];
    return true;
  }

  // Returns 1 when a pad is held/refreshed on strip `i`, 0 when released, or
  // -1 when no hold state change occurred since the last check.
  int8_t consumeHold(size_t i, uint8_t &effectOut) {
    if (i >= kStripCount) {
      return -1;
    }
    const int8_t st = holdState_[i];
    holdState_[i] = -1;
    effectOut = holdEffect_[i];
    return st;
  }

  bool enabled() const { return enabled_; }
  bool setEnabled(bool on);
  String ip() const;
  void setTelemetry(const Telemetry &t) { tele_ = t; }

  // Clears the "saving…" hint once the caller has written to flash.
  void markSaved() { unsaved_ = false; }

  // True while the user is adjusting ledCount (auto-holds for 3.5 s) or has
  // toggled the calibration dot button on for strip `i`.
  bool isCalibrating(size_t i, uint32_t nowMs) const {
    if (i >= kStripCount) {
      return false;
    }
    if (calibPinned_[i]) {
      return true;
    }
    return calibUntilMs_[i] != 0 && (int32_t)(calibUntilMs_[i] - nowMs) > 0;
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
  void applyArg(const char *name, uint8_t *dst);
  void applyArg(const char *name, uint16_t *dst);

  WebServer server_{80};
  Settings *settings_ = nullptr;
  SettingsStore *store_ = nullptr;
  Telemetry tele_;
  uint32_t calibUntilMs_[kStripCount] = {};
  bool calibPinned_[kStripCount] = {};
  bool tapPending_[kStripCount] = {};
  uint8_t tapEffect_[kStripCount] = {};
  int8_t holdState_[kStripCount] = {-1, -1};
  uint8_t holdEffect_[kStripCount] = {};
  bool padDown_[kStripCount] = {};
  bool enabled_ = false;
  bool routesBound_ = false;
  bool changed_ = false;
  bool unsaved_ = false;
};
