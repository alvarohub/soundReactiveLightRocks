#include "telemetry.h"

#include <WiFi.h>
#include <string.h>

namespace {

constexpr uint32_t kTaskStack = 4096;
constexpr UBaseType_t kTaskPriority = 1; // below the audio task and below loop
constexpr uint8_t kFlagAgc = 1u << 0;
constexpr uint8_t kFlagUnsaved = 1u << 1;
constexpr const char *kWsUri = "/ws";

TelemetryStream *gStream = nullptr;

inline void putU16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

} // namespace

bool TelemetryStream::begin(uint16_t port) {
  if (server_ != nullptr) {
    return true;
  }
  gStream = this;
  port_ = port;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port;
  // The control API keeps the default control port; give this instance its own
  // so the two servers can coexist.
  config.ctrl_port = (uint16_t)(port + 1000);
  config.max_open_sockets = kMaxClients + 1;
  config.lru_purge_enable = true;
  config.stack_size = 4096;
  // A phone that vanishes must not hold a slot open.
  config.send_wait_timeout = 2;
  config.recv_wait_timeout = 2;

  if (httpd_start(&server_, &config) != ESP_OK) {
    Serial.println("[ws] listen failed");
    server_ = nullptr;
    return false;
  }

  httpd_uri_t uri{};
  uri.uri = kWsUri;
  uri.method = HTTP_GET;
  uri.handler = &TelemetryStream::wsHandler;
  uri.user_ctx = nullptr;
  uri.is_websocket = true;
  if (httpd_register_uri_handler(server_, &uri) != ESP_OK) {
    Serial.println("[ws] route failed");
    httpd_stop(server_);
    server_ = nullptr;
    return false;
  }

  if (xTaskCreate(&TelemetryStream::taskEntry, "telemetry", kTaskStack, this,
                  kTaskPriority, nullptr) != pdPASS) {
    Serial.println("[ws] task failed");
    httpd_stop(server_);
    server_ = nullptr;
    return false;
  }

  Serial.printf("[ws] http://%s:%u%s\n", WiFi.softAPIP().toString().c_str(),
                (unsigned)port, kWsUri);
  return true;
}

void TelemetryStream::end() {
  if (server_ != nullptr) {
    httpd_stop(server_);
    server_ = nullptr;
  }
  clients_ = 0;
}

esp_err_t TelemetryStream::wsHandler(httpd_req_t *req) {
  TelemetryStream *self = gStream;
  if (self == nullptr || self->server_ == nullptr) {
    return ESP_FAIL;
  }
  const int fd = httpd_req_to_sockfd(req);
  if (fd < 0) {
    return ESP_FAIL;
  }

  if (req->method == HTTP_GET) {
    // The handshake completes when this handler returns ESP_OK.
    if (httpd_ws_get_fd_info(self->server_, fd) !=
        HTTPD_WS_CLIENT_WEBSOCKET) {
      return ESP_FAIL;
    }
    portENTER_CRITICAL(&self->mux_);
    bool known = false;
    for (uint8_t i = 0; i < kMaxClients; i++) {
      if (self->clientsFd_[i] == fd) {
        known = true;
      }
    }
    if (!known) {
      bool placed = false;
      for (uint8_t i = 0; i < kMaxClients && !placed; i++) {
        if (self->clientsFd_[i] <= 0) {
          self->clientsFd_[i] = fd;
          self->clients_ = (uint8_t)(self->clients_ + 1);
          placed = true;
        }
      }
      if (!placed) {
        // Full: reuse the first slot. A stale phone must never lock others out.
        self->clientsFd_[0] = fd;
      }
    }
    const uint8_t n = self->clients_;
    portEXIT_CRITICAL(&self->mux_);
    Serial.printf("[ws] client %d attached (%u total)\n", fd, (unsigned)n);
    return ESP_OK;
  }

  // Post-handshake frames: the browser only sends close/ping, but they have to
  // be consumed or the socket stalls.
  uint8_t buf[64];
  httpd_ws_frame_t frame{};
  frame.payload = buf;
  frame.len = sizeof(buf);
  const esp_err_t err = httpd_ws_recv_frame(req, &frame, sizeof(buf));
  if (err != ESP_OK) {
    self->dropClient(fd);
    return err;
  }
  if (frame.type == HTTPD_WS_TYPE_CLOSE) {
    self->dropClient(fd);
  }
  return ESP_OK;
}

void TelemetryStream::dropClient(int fd) {
  portENTER_CRITICAL(&mux_);
  for (uint8_t i = 0; i < kMaxClients; i++) {
    if (clientsFd_[i] == fd) {
      clientsFd_[i] = 0;
      if (clients_ > 0) {
        clients_ = (uint8_t)(clients_ - 1);
      }
    }
  }
  portEXIT_CRITICAL(&mux_);
}

bool TelemetryStream::stillConnected(int fd) const {
  if (server_ == nullptr || fd <= 0) {
    return false;
  }
  return httpd_ws_get_fd_info(server_, fd) == HTTPD_WS_CLIENT_WEBSOCKET;
}

void TelemetryStream::publish(const Telemetry &tele, const Settings &settings) {
  portENTER_CRITICAL(&mux_);
  flags_ = (uint8_t)((settings.agc ? kFlagAgc : 0) |
                     (tele.unsaved ? kFlagUnsaved : 0));
  levelPct_ = tele.levelPct;
  dbfs_ = (int8_t)tele.dbfs;
  peakHz_ = tele.peakHz;
  gainDb_ = settings.gainDb;
  agcPeakPct_ = tele.agcPeakPct;
  agcAction_ = tele.agcAction;
  for (size_t i = 0; i < kStripCount; i++) {
    stripBright_[i] = tele.strip[i].brightPct;
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      const LayerTelemetry &lt = tele.strip[i].layer[l];
      layerLevel_[i][l] = lt.levelPct;
      layerBeat_[i][l] = lt.beatPct;
      layerBpm_[i][l] = lt.bpm;
      layerConf_[i][l] = lt.beatConf;
      layerBlobs_[i][l] = lt.blobs;
    }
  }
  for (size_t b = 0; b < kDisplayBands; b++) {
    spec_[b] = tele.spec[b];
  }
  loopMaxMs_ = tele.loopMaxMs;
  portEXIT_CRITICAL(&mux_);
}

size_t TelemetryStream::buildFrame(uint8_t *out, size_t cap) const {
  if (out == nullptr || cap < kFrameSize) {
    return 0;
  }
  portMUX_TYPE *mux = const_cast<portMUX_TYPE *>(&mux_);
  portENTER_CRITICAL(mux);
  size_t n = 0;
  out[n++] = kFrameVersion;
  out[n++] = flags_;
  putU16(out + n, seq_);
  n += 2;
  out[n++] = levelPct_;
  out[n++] = (uint8_t)dbfs_;
  putU16(out + n, peakHz_);
  n += 2;
  out[n++] = (uint8_t)gainDb_;
  out[n++] = agcPeakPct_;
  out[n++] = (uint8_t)agcAction_;
  for (size_t i = 0; i < kStripCount; i++) {
    out[n++] = stripBright_[i];
  }
  for (size_t i = 0; i < kStripCount; i++) {
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      out[n++] = layerLevel_[i][l];
    }
  }
  for (size_t i = 0; i < kStripCount; i++) {
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      out[n++] = layerBeat_[i][l];
    }
  }
  for (size_t i = 0; i < kStripCount; i++) {
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      putU16(out + n, layerBpm_[i][l]);
      n += 2;
    }
  }
  for (size_t i = 0; i < kStripCount; i++) {
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      out[n++] = layerConf_[i][l];
    }
  }
  for (size_t i = 0; i < kStripCount; i++) {
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      out[n++] = layerBlobs_[i][l];
    }
  }
  for (size_t b = 0; b < kDisplayBands; b++) {
    out[n++] = spec_[b];
  }
  out[n++] = loopMaxMs_;
  portEXIT_CRITICAL(mux);
  return n;
}

void TelemetryStream::taskEntry(void *arg) {
  static_cast<TelemetryStream *>(arg)->taskLoop();
}

void TelemetryStream::taskLoop() {
  uint32_t last = 0;
  uint8_t frame[kFrameSize];
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(10));
    if (clients_ == 0) {
      continue; // nobody listening: no work at all
    }
    const uint32_t now = millis();
    if ((uint32_t)(now - last) < kPeriodMs) {
      continue;
    }
    last = now;

    const size_t len = buildFrame(frame, sizeof(frame));
    if (len == 0) {
      continue;
    }
    {
      portENTER_CRITICAL(&mux_);
      seq_++;
      portEXIT_CRITICAL(&mux_);
    }

    for (uint8_t i = 0; i < kMaxClients; i++) {
      portENTER_CRITICAL(&mux_);
      const int fd = clientsFd_[i];
      portEXIT_CRITICAL(&mux_);
      if (fd <= 0) {
        continue;
      }
      if (!stillConnected(fd)) {
        dropClient(fd);
        continue;
      }
      httpd_ws_frame_t wsFrame{};
      wsFrame.type = HTTPD_WS_TYPE_BINARY;
      wsFrame.payload = frame;
      wsFrame.len = len;
      wsFrame.final = true;
      // Bounded by send_wait_timeout, so a phone that stopped reading costs at
      // most two seconds here - never the render loop, which runs in another
      // task and only ever writes the snapshot.
      if (httpd_ws_send_frame_async(server_, fd, &wsFrame) != ESP_OK) {
        dropClient(fd);
      }
    }
  }
}
