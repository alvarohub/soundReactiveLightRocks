#pragma once

#include <Arduino.h>
#include <esp_http_server.h>

#include "config.h"
#include "settings.h"

// Live telemetry pushed to the phone at ~25 Hz over a WebSocket, on its own port
// so it never competes with the control API.
//
// The control page asks for a full settings document every couple of seconds;
// levels, however, change 30 times a second, and polling them was what made the
// UI trail reality by about a second. This pushes one small binary frame
// instead: no JSON, no parsing, no request per frame.
//
// The frame layout is fixed and versioned; the page decodes it by offset. Keep
// the two in step (see kFrameSize and docs/SPEC.md).
//
//   off size field
//    0    1  version (kFrameVersion)
//    1    1  flags: bit0 agc, bit1 unsaved
//    2    2  seq, wraps
//    4    1  broadband level, %
//    5    1  dBFS, signed
//    6    2  peak frequency, Hz
//    8    1  mic gain, dB, signed
//    9    1  AGC peak, %
//   10    1  AGC action: -1 lower, 0 idle, 1 raise, 2 holding
//   11    2  per-strip output brightness, %
//   13  2*4  per (strip, layer): level, %
//   21  2*4  per (strip, layer): beat pulse, %
//   29  2*4*2 per (strip, layer): detected BPM
//   45  2*4  per (strip, layer): beat confidence, %
//   53  2*4  per (strip, layer): active effects, count
//   61   30  spectrum bands, %
//   91    1  worst render-loop frame gap in the last second, ms
// = 92 bytes
class TelemetryStream {
public:
  static constexpr uint8_t kFrameVersion = 1;
  static constexpr size_t kFrameSize = 92;
  static constexpr uint16_t kDefaultPort = 81;
  static constexpr uint8_t kMaxClients = 3;
  // 40 ms: faster than the ~31 Hz analysis rate would only repeat numbers, and
  // slower than the render loop so publishing never paces the LEDs.
  static constexpr uint32_t kPeriodMs = 40;

  bool begin(uint16_t port = kDefaultPort);
  void end();
  bool running() const { return server_ != nullptr; }
  uint16_t port() const { return port_; }
  uint8_t clientCount() const { return clients_; }

  // Copies what the stream needs. Cheap enough to call every frame: no
  // allocation, no formatting.
  void publish(const Telemetry &tele, const Settings &settings);

  // The same frame the socket sends, for the HTTP fallback endpoint.
  size_t buildFrame(uint8_t *out, size_t cap) const;

private:
  static void taskEntry(void *arg);
  void taskLoop();
  void sendFrame();
  static esp_err_t wsHandler(httpd_req_t *req);
  void dropClient(int fd);
  bool stillConnected(int fd) const;

  httpd_handle_t server_ = nullptr;
  uint16_t port_ = kDefaultPort;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

  // Snapshot, written by publish() and read by the task.
  uint8_t flags_ = 0;
  uint8_t levelPct_ = 0;
  int8_t dbfs_ = -99;
  uint16_t peakHz_ = 0;
  int8_t gainDb_ = 0;
  uint8_t agcPeakPct_ = 0;
  int8_t agcAction_ = 0;
  uint8_t stripBright_[kStripCount] = {};
  uint8_t layerLevel_[kStripCount][kMaxLayersPerStrip] = {};
  uint8_t layerBeat_[kStripCount][kMaxLayersPerStrip] = {};
  uint16_t layerBpm_[kStripCount][kMaxLayersPerStrip] = {};
  uint8_t layerConf_[kStripCount][kMaxLayersPerStrip] = {};
  uint8_t layerBlobs_[kStripCount][kMaxLayersPerStrip] = {};
  uint8_t spec_[kDisplayBands] = {};
  uint8_t loopMaxMs_ = 0;

  int clientsFd_[kMaxClients] = {};
  volatile uint8_t clients_ = 0;
  uint16_t seq_ = 0;
};
