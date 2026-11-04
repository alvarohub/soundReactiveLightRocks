#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "config.h"
#include "settings.h"
#include "store.h"

struct StripTelemetry {
  uint8_t levelPct = 0;  // band level inside this strip's window
  uint8_t brightPct = 0; // brightest channel of the rendered frame
  uint8_t blobs = 0;     // blobs currently alive
};

// Live values as shown on the phone. Kept as integers so the JSON never depends
// on float printf support.
struct Telemetry {
  uint8_t levelPct = 0; // incoming audio level, broadband
  int16_t dbfs = -99;
  uint16_t peakHz = 0;
  StripTelemetry strip[kStripCount];
};

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
  bool enabled_ = false;
  bool routesBound_ = false;
  bool changed_ = false;
  bool unsaved_ = false;
};
