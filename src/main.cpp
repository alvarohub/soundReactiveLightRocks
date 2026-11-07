#include <M5Unified.h>

#include <math.h>
#include <stdio.h>

#include "audio_fft.h"
#include "beat.h"
#include "blob_mode.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "strip.h"
#include "webui.h"

namespace {

SpectrumAnalyzer analyzer;
BeatDetector beats[kStripCount];
LedStrip strips[kStripCount];
BlobEngine engines[kStripCount];
Settings settings;
SettingsStore store;
WebUi web;
Display display;

uint32_t lastFrameUs = 0;
uint32_t lastStats = 0;
uint8_t syncedGainDb = 0xFF;

// Writing NVS on every slider move would wear the flash and stall the CPU cache
// mid-drag, so changes are saved once they have been quiet for 2 seconds.
constexpr uint32_t kSaveDelayMs = 2000;
bool settingsDirty = false;
uint32_t settingsChangedAt = 0;

void noteSettingsChanged() {
  settingsDirty = true;
  settingsChangedAt = millis();
}

void saveNow(bool byButton) {
  settingsDirty = false;
  const bool ok = store.saveLive(settings);
  if (ok) {
    web.markSaved();
  }
  Serial.println(ok ? (byButton ? "[cfg] saved by button" : "[cfg] saved")
                    : "[cfg] save failed");
}

void showBanner() {
  display.showBanner(WebUi::kSsid, WebUi::kPassword, web.enabled(), web.ip());
}

void finishBanner() {
  display.finishBanner(WebUi::kSsid, web.enabled(), web.ip());
}

// One strip's frame: hear its band, advance its trigger input, draw, emit.
void renderStrip(size_t i, uint32_t nowMs, float dtSec, Telemetry &tele) {
  const StripSettings &s = settings.strip[i];
  LedStrip &strip = strips[i];
  BlobEngine &engine = engines[i];

  if (!s.enabled) {
    engine.reset();
    strip.clear();
    strip.show(s.brightnessPct);
    return;
  }

  const float level = analyzer.bandLevel((float)s.centerHz, (float)s.widthHz);
  const uint32_t dtMs = (uint32_t)(dtSec * 1000.0f + 0.5f);
  const BeatInfo beat = beats[i].update(
      analyzer.bandFlux((float)s.centerHz, (float)s.widthHz), dtMs,
      s.beatSensitivityPct);

  uint8_t padEffect = StripSettings::kEffectJet;
  const int8_t hold = web.consumeHold(i, padEffect);
  if (hold >= 0) {
    engine.setPadHeld(hold > 0, padEffect, s);
  }
  if (web.consumeTap(i, padEffect)) {
    engine.triggerEffect(padEffect, s);
  }

  if (web.isCalibrating(i, nowMs)) {
    strip.showCalibrationDot(s.ledCount, s.r, s.g, s.b, s.brightnessPct);
  } else {
    engine.update(level, beat.onBeat, nowMs, dtSec, s);
    strip.clear();
    engine.render(strip, s);
    strip.show(s.brightnessPct);
  }

  tele.strip[i].levelPct = (uint8_t)(level * 100.0f + 0.5f);
  tele.strip[i].brightPct = (uint8_t)(strip.peak() * 100.0f + 0.5f);
  tele.strip[i].blobs = (uint8_t)engine.activeCount();
  tele.strip[i].beatPct = (uint8_t)(beat.pulse * 100.0f + 0.5f);
  tele.strip[i].bpm = beats[i].bpm();
}

void handleButtons() {
  if (M5.BtnA.wasClicked()) {
    // Manual test trigger: the fastest way to judge effect, speed and colour.
    for (size_t i = 0; i < kStripCount; i++) {
      engines[i].trigger(settings.strip[i]);
    }
  }

  if (M5.BtnA.wasHold()) {
    const bool on = web.setEnabled(!web.enabled());
    showBanner();
    delay(on ? 2000 : 800);
    finishBanner();
  }

  if (M5.BtnB.wasClicked()) {
    settings.gainDb = (uint8_t)(settings.gainDb + 6);
    if (settings.gainDb > 18) {
      settings.gainDb = 0;
    }
    noteSettingsChanged();
  }

  if (M5.BtnB.wasHold()) {
    saveNow(true);
  }
}

} // namespace

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = false;       // keep I2S free for the mic
  cfg.external_display_value = 0; // do not probe Grove pins for I2C displays
  cfg.external_imu = false;
  cfg.external_rtc = false;
  M5.begin(cfg);

  Serial.begin(115200);
  display.begin();

  settings = defaultSettings();
  if (!store.begin()) {
    Serial.println("settings will not persist");
  } else if (store.loadLive(settings)) {
    clampSettings(settings);
    Serial.println("loaded saved settings");
  } else {
    Serial.println("no saved settings, using defaults");
  }

  for (size_t i = 0; i < kStripCount; i++) {
    strips[i].begin(kStripPins[i], settings.strip[i].ledCount, (uint8_t)i);
    engines[i].reset();
    beats[i].reset();
  }

  if (!analyzer.begin(kSampleRate)) {
    Serial.println("mic init failed");
  }
  analyzer.setGainDb(settings.gainDb);

  web.setup(&settings, &store);
  if (!web.begin()) {
    Serial.println("web ui failed to start");
  }
  showBanner();
  delay(2800);
  finishBanner();

  Serial.printf("ready: fft %u/%u Hz, ap http://%s\n",
                (unsigned)SpectrumAnalyzer::kSamples, (unsigned)kSampleRate,
                web.ip().c_str());
}

void loop() {
  M5.update();
  web.update();

  if (web.consumeChanged()) {
    noteSettingsChanged();
  }
  if (settingsDirty && millis() - settingsChangedAt >= kSaveDelayMs) {
    saveNow(false);
  }

  handleButtons();

  // The gain can be changed from the buttons or from the phone, so keep the
  // analyzer in sync in one place.
  if (syncedGainDb != settings.gainDb) {
    syncedGainDb = settings.gainDb;
    analyzer.setGainDb(syncedGainDb);
    display.invalidateHud();
  }

  // ledCount is a runtime parameter: react to it changing. setCount() is a
  // no-op when the value did not change.
  for (size_t i = 0; i < kStripCount; i++) {
    strips[i].setCount(settings.strip[i].ledCount);
  }

  if (!analyzer.update()) {
    // Even without a mic, pace at ~31 fps so pads and test triggers animate.
    delay(32);
  }
  web.update();

  // The microphone paces the loop, so measure the real frame time instead of
  // assuming a fixed one: motion speed and fade stay accurate.
  const uint32_t nowUs = micros();
  uint32_t dtMs = lastFrameUs == 0 ? 32 : (nowUs - lastFrameUs) / 1000;
  lastFrameUs = nowUs;
  if (dtMs == 0) {
    dtMs = 1;
  } else if (dtMs > 250) {
    dtMs = 250;
  }
  const float dtSec = (float)dtMs * 0.001f;
  const uint32_t nowMs = millis();

  Telemetry tele;
  tele.levelPct = (uint8_t)(analyzer.level() * 100.0f + 0.5f);
  tele.dbfs = (int16_t)lroundf(analyzer.levelDb());
  tele.peakHz = (uint16_t)lroundf(analyzer.peakHz());

  for (size_t i = 0; i < kStripCount; i++) {
    renderStrip(i, nowMs, dtSec, tele);
  }
  web.setTelemetry(tele);

  display.renderFrame(analyzer.bands(), settings, tele, nowMs);

  if (millis() - lastStats >= 2000) {
    lastStats = millis();
    char out[80];
    size_t n = (size_t)snprintf(out, sizeof(out), "lvl %3.0f%% %+4.0f dBFS",
                                analyzer.level() * 100.0f, analyzer.levelDb());
    for (size_t i = 0; i < kStripCount && n + 16 < sizeof(out); i++) {
      n += (size_t)snprintf(out + n, sizeof(out) - n, " | s%u %3.0f%% %ubpm",
                            (unsigned)i + 1, strips[i].peak() * 100.0f,
                            (unsigned)beats[i].bpm());
    }
    Serial.println(out);
  }
}
