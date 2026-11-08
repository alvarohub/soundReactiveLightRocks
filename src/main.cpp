#include <M5Unified.h>

#include <math.h>
#include <stdio.h>

#include "audio_fft.h"
#include "beat.h"
#include "config.h"
#include "display.h"
#include "layer.h"
#include "motion.h"
#include "settings.h"
#include "strip.h"
#include "webui.h"

namespace {

SpectrumAnalyzer analyzer;
MotionSensor motion;
LedStrip strips[kStripCount];

// One engine and one beat detector per LAYER: layers inside the same physical
// strip must be able to follow different rhythms.
LayerEngine engines[kStripCount][kMaxLayersPerStrip];
BeatDetector beats[kStripCount][kMaxLayersPerStrip];

Settings settings;
SettingsStore store;
WebUi web;
Display display;

// Physical strip the hardware buttons currently act on.
uint8_t hwStrip = 0;

uint32_t lastFrameUs = 0;
uint32_t lastStats = 0;
int16_t syncedGainDb = 0x7FFF; // sentinel: force a first sync

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

// One physical strip's frame: every live layer hears its own band, advances its
// own trigger input, and draws into the strip's shared linear buffer. A single
// show() then gamma-encodes and clocks the merged result out.
void renderStrip(size_t i, uint32_t nowMs, float dtSec, Telemetry &tele) {
  const PhysicalStripSettings &ps = settings.phys[i];
  LedStrip &strip = strips[i];
  const size_t count = ps.layerCount < 1 ? 1 : ps.layerCount;
  const int8_t solo = web.soloLayer(i);
  const uint32_t dtMs = (uint32_t)(dtSec * 1000.0f + 0.5f);

  strip.clear();

  for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
    LayerSettings &s = settings.layer[i][l];
    LayerEngine &engine = engines[i][l];
    LayerTelemetry &lt = tele.strip[i].layer[l];

    if (l >= count || solo >= 0) {
      // Not part of this strip, or muted by solo: keep it quiet and idle.
      engine.reset();
      lt = LayerTelemetry();
      continue;
    }

    const float level =
        analyzer.bandLevel((float)s.centerHz, (float)s.widthHz);
    // Silence gate input: the band level with the microphone gain removed, so a
    // negative gain attenuates what you hear without also disabling the beat
    // detector. (The raw level would fall below the gate and beats would never
    // fire, which is exactly how this surfaced.)
    const float gateLevel =
        analyzer.clampLevel(level - (float)settings.gainDb /
                                        SpectrumAnalyzer::kLevelRangeDb);
    const BeatInfo beat = beats[i][l].update(
        analyzer.bandFlux((float)s.centerHz, (float)s.widthHz), gateLevel, dtMs,
        s.beatSensitivityPct);

    // Pads only reach a layer that has the tap trigger armed, so the
    // checkboxes really do decide what can fire a layer.
    const bool tapsArmed = (s.triggers & kTrigTap) != 0;

    uint8_t padEffect = LayerSettings::kEffectJet;
    const int8_t hold = web.consumeHold(i, l, padEffect);
    if (hold >= 0) {
      engine.setPadHeld(hold > 0, tapsArmed ? padEffect : (uint8_t)0xFF);
    }
    if (web.consumeTap(i, l, padEffect) && tapsArmed) {
      engine.triggerEffect(padEffect, s, ps.ledCount);
    }

    if (!s.enabled) {
      engine.reset();
      lt.levelPct = (uint8_t)(level * 100.0f + 0.5f);
      lt.bpm = beats[i][l].bpm();
      continue;
    }

    engine.update(level, beat.onBeat, motion.state(), nowMs, dtSec, s,
                  ps.ledCount);
    engine.render(strip, s);

    lt.levelPct = (uint8_t)(level * 100.0f + 0.5f);
    lt.brightPct = (uint8_t)(engine.peak() * 100.0f + 0.5f);
    lt.blobs = (uint8_t)engine.activeCount();
    lt.beatPct = (uint8_t)(beat.pulse * 100.0f + 0.5f);
    lt.bpm = beats[i][l].bpm();
  }

  // Spatial bleed is applied to the merged frame, just before gamma: it is a
  // property of the physical strip, not of any one layer.
  if (ps.bleedPct > 0 && !web.isCalibrating(i, nowMs)) {
    strip.bleed((float)ps.bleedPct / 100.0f, true);
  }

  // Calibration shows only the physical tip, so it replaces the merged frame.
  if (web.isCalibrating(i, nowMs)) {
    const LayerSettings &s = settings.layer[i][web.editLayer(i)];
    strip.showCalibrationDot(ps.ledCount, s.r, s.g, s.b, ps.brightnessPct);
  } else {
    strip.show(ps.brightnessPct);
  }
  tele.strip[i].brightPct = (uint8_t)(strip.peak() * 100.0f + 0.5f);
}

// Cycles one layer's armed triggers through useful combinations, from the
// simplest arming to "everything", so a few presses always reach the wanted set.
uint8_t nextTriggerSet(uint8_t t) {
  static const uint8_t kCycle[] = {
      kTrigThreshold,
      kTrigBeat,
      kTrigTap,
      kTrigAccel,
      kTrigGyro,
      (uint8_t)(kTrigThreshold | kTrigBeat),
      (uint8_t)(kTrigAccel | kTrigGyro),
      kAllTriggers,
  };
  for (size_t i = 0; i < sizeof(kCycle); i++) {
    if (t == kCycle[i]) {
      return kCycle[(i + 1) % sizeof(kCycle)];
    }
  }
  return kCycle[0];
}

void handleButtons() {
  if (M5.BtnA.wasClicked()) {
    // Scroll the physically selected strip; BtnB then acts on it.
    hwStrip = (uint8_t)((hwStrip + 1) % kStripCount);
    display.setSelectedStrip(hwStrip);
    Serial.printf("[btn] strip %u selected\n", (unsigned)hwStrip + 1);
  }

  if (M5.BtnA.wasHold()) {
    const bool on = web.setEnabled(!web.enabled());
    showBanner();
    delay(on ? 2000 : 800);
    finishBanner();
  }

  if (M5.BtnB.wasClicked()) {
    // Cycle the trigger mode of every live layer on the selected strip.
    bool any = false;
    for (size_t l = 0; l < settings.phys[hwStrip].layerCount; l++) {
      settings.layer[hwStrip][l].triggers =
          nextTriggerSet(settings.layer[hwStrip][l].triggers);
      any = true;
    }
    if (any) {
      noteSettingsChanged();
      Serial.printf("[btn] strip %u triggers -> 0x%02X\n",
                    (unsigned)hwStrip + 1,
                    (unsigned)settings.layer[hwStrip][0].triggers);
    }
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
    strips[i].begin(kStripPins[i], settings.phys[i].ledCount, (uint8_t)i);
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      engines[i][l].reset();
      beats[i][l].reset();
    }
  }

  motion.begin();

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
    strips[i].setCount(settings.phys[i].ledCount);
  }

  motion.update();

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
    for (size_t i = 0; i < kStripCount && n + 14 < sizeof(out); i++) {
      n += (size_t)snprintf(out + n, sizeof(out) - n, " | p%u %uL %3.0f%%",
                            (unsigned)i + 1,
                            (unsigned)settings.phys[i].layerCount,
                            strips[i].peak() * 100.0f);
    }
    Serial.println(out);
  }
}
