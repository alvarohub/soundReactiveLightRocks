#include <M5Unified.h>

#include <math.h>
#include <stdio.h>

#include "audio_fft.h"
#include "audio_task.h"
#include "beat.h"
#include "config.h"
#include "display.h"
#include "layer.h"
#include "motion.h"
#include "settings.h"
#include "strip.h"
#include "telemetry.h"
#include "webui.h"

namespace {

SpectrumAnalyzer analyzer;
AudioTask audio;
AudioSnapshot snap;       // latest analysis copied from the audio task
uint32_t snapSeq = 0;     // last snapshot consumed (onset runs once per block)
uint32_t lastAudioUs = 0; // for the beat detector's dt
MotionSensor motion;
LedStrip strips[kStripCount];

// One engine and one beat detector per LAYER: layers inside the same physical
// strip must be able to follow different rhythms.
LayerEngine engines[kStripCount][kMaxLayersPerStrip];
BeatDetector beats[kStripCount][kMaxLayersPerStrip];
// Result of the last onset test, reused by every rendered frame until a new
// audio block arrives.
BeatInfo beatInfo[kStripCount][kMaxLayersPerStrip];

Settings settings;
SettingsStore store;
WebUi web;
TelemetryStream stream;
Display display;

// Physical strip the hardware buttons currently act on.
uint8_t hwStrip = 0;

uint32_t lastFrameUs = 0;
uint32_t lastStats = 0;
int16_t syncedGainDb = 0x7FFF; // sentinel: force a first sync

// Writing NVS on every slider move would wear the flash and stall the CPU cache
// mid-drag, so changes are saved once they have been quiet for 2 seconds.
constexpr uint32_t kSaveDelayMs = 2000;

// --- Automatic gain control -------------------------------------------------
// Without this the microphone is either deaf or saturated depending on the room,
// and the gain is the one thing that always needs re-tuning on site. The
// controller only ever nudges gainDb by one step and only when there is actually
// something to judge, so a silent room cannot wind it up into a noise amplifier.
constexpr uint32_t kAgcIntervalMs = 400;
constexpr float kAgcRaiseBelow = 0.30f; // peak-hold below this -> too quiet
constexpr float kAgcLowerAbove = 0.90f; // peak-hold above this -> too hot
constexpr float kAgcActAbove = 0.08f;   // below this there is nothing to judge
float agcPeak = 0.0f;
uint32_t agcLastMs = 0;
// Published for the phone, which explains what the AGC is doing. The Telemetry
// struct is built later in loop(), so the controller leaves its verdict here.
int8_t agcActionPub = 0;   // -1 lower, 0 idle, +1 raise, 2 holding
uint8_t agcPeakPctPub = 0; // the peak the last decision was based on

// Render target. The microphone runs at ~31 Hz in its own task; the animation
// and the strip run at this rate instead. It is only a *cap*: the LED transfer
// (~3 ms per 100 LEDs) sets the real floor.
constexpr uint32_t kRenderPeriodUs = 10000; // ~100 fps
bool settingsDirty = false;
uint32_t loopMaxMs = 0;
uint32_t loopStatsMs = 0;
uint8_t teleLoopMaxMs = 0;
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

    // Values come from the snapshot the audio task published: this loop never
    // touches the FFT, so it is free to run at the strip's own rate.
    const float level = snap.layerLevel[i][l];
    const BeatInfo beat = beatInfo[i][l];

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
    lt.beatConf = beats[i][l].confidencePct();
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
    lt.beatConf = beats[i][l].confidencePct();
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

// Fallback for the (unlikely) case where the audio task could not start: do the
// blocking capture inline, exactly like the old single-rate loop.
void fillSnapshotInline(AudioSnapshot &out) {
  out.valid = true;
  out.level = analyzer.level();
  out.levelDb = analyzer.levelDb();
  out.peakHz = analyzer.peakHz();
  const float *bands = analyzer.bands();
  for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
    out.bands[b] = bands[b];
  }
  for (size_t i = 0; i < kStripCount; i++) {
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      const LayerSettings &ls = settings.layer[i][l];
      out.layerLevel[i][l] = analyzer.bandLevel((float)ls.centerHz, (float)ls.widthHz);
      out.layerFlux[i][l] = analyzer.bandFlux((float)ls.centerHz, (float)ls.widthHz);
    }
  }
}

void runAgc(uint32_t nowMs) {
  if (!settings.agc) {
    agcPeak = 0.0f;
    agcLastMs = nowMs;
    agcActionPub = 0;
    return;
  }
  if (snap.level > agcPeak) {
    agcPeak = snap.level;
  }
  if ((uint32_t)(nowMs - agcLastMs) < kAgcIntervalMs) {
    return;
  }
  agcLastMs = nowMs;
  const float peak = agcPeak;
  agcPeak = 0.0f;
  agcPeakPctPub = (uint8_t)(peak * 100.0f + 0.5f);
  // "Holding" only when there is something to hold: below the action level the
  // AGC is deliberately idle, which is a different message.
  agcActionPub = peak >= kAgcActAbove ? (int8_t)2 : (int8_t)0;

  if (peak < kAgcActAbove) {
    return; // silence: leave the gain alone rather than amplifying the room
  }

  int8_t g = settings.gainDb;
  int8_t action = 0;
  if (peak < kAgcRaiseBelow && g < kMaxGainDb) {
    g = (int8_t)(g + kGainStepDb);
    action = 1;
  } else if (peak > kAgcLowerAbove && g > kMinGainDb) {
    g = (int8_t)(g - kGainStepDb);
    action = -1;
  } else {
    return;
  }
  if (g == settings.gainDb) {
    return;
  }
  settings.gainDb = g;
  agcActionPub = action; // -1 down, +1 up
  clampSettings(settings);
  noteSettingsChanged();
  display.invalidateHud();
}

void handleButtons() {
  // BtnA (front) works on what the screen shows, BtnB (side) changes which
  // physical strip the screen and the pads are working on.
  if (M5.BtnB.wasClicked()) {
    hwStrip = (uint8_t)((hwStrip + 1) % kStripCount);
    display.setSelectedStrip(hwStrip);
    Serial.printf("[btn] strip %u selected\n", (unsigned)hwStrip + 1);
  }

  if (M5.BtnA.wasClicked()) {
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

  if (M5.BtnA.wasHold()) {
    const bool on = web.setEnabled(!web.enabled());
    if (on) {
      stream.begin();
    } else {
      stream.end();
    }
    showBanner();
    delay(on ? 2000 : 800);
    finishBanner();
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
  // Capture runs in its own task from here on: the render loop must never wait
  // for a microphone block again.
  audio.begin(&analyzer, &settings);

  web.setup(&settings, &store);
  web.setStream(&stream);
  if (!web.begin()) {
    Serial.println("web ui failed to start");
  }
  // The socket lives with the access point: no AP, nothing to stream to.
  if (web.enabled() && !stream.begin()) {
    Serial.println("telemetry stream failed to start");
  }
  showBanner();
  delay(2800);
  finishBanner();

  Serial.printf("ready: fft %u/%u Hz, ap http://%s\n",
                (unsigned)SpectrumAnalyzer::kSamples, (unsigned)kSampleRate,
                web.ip().c_str());
}

void loop() {
  const uint32_t nowUs = micros();
  M5.update();
  web.update();

  if (web.consumeChanged()) {
    noteSettingsChanged();
  }
  if (settingsDirty && millis() - settingsChangedAt >= kSaveDelayMs) {
    saveNow(false);
  }

  handleButtons();

  // The audio task applies the gain itself (it is the only task allowed to
  // touch the analyser); the render side only needs to refresh the HUD.
  if (syncedGainDb != settings.gainDb) {
    syncedGainDb = settings.gainDb;
    display.invalidateHud();
  }

  // ledCount is a runtime parameter: react to it changing. setCount() is a
  // no-op when the value did not change.
  for (size_t i = 0; i < kStripCount; i++) {
    strips[i].setCount(settings.phys[i].ledCount);
  }

  motion.update();

  // Pull the newest analysis. Only a *new* audio block advances the onset
  // detectors, so each flux sample is consumed exactly once.
  bool fresh = audio.read(snap, snapSeq);
  if (!audio.running()) {
    if (analyzer.update()) {
      fillSnapshotInline(snap);
      snap.seq++;
      snapSeq = snap.seq;
      fresh = true;
    } else {
      delay(5);
    }
  }

  if (fresh && snap.valid) {
    uint32_t audioDtMs = lastAudioUs ? (nowUs - lastAudioUs) / 1000 : 32;
    lastAudioUs = nowUs;
    if (audioDtMs == 0) {
      audioDtMs = 1;
    } else if (audioDtMs > 250) {
      audioDtMs = 250;
    }
    for (size_t i = 0; i < kStripCount; i++) {
      for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
        const LayerSettings &ls = settings.layer[i][l];
        // Silence gate input with the gain removed, so attenuating the
        // microphone does not also deafen the beat detector.
        const float gate = SpectrumAnalyzer::clampLevel(
            snap.layerLevel[i][l] -
            (float)settings.gainDb / SpectrumAnalyzer::kLevelRangeDb);
        beatInfo[i][l] =
            beats[i][l].update(snap.layerFlux[i][l], gate, audioDtMs,
                               ls.beatSensitivityPct);
      }
    }
  }

  runAgc(millis());

  // Animation runs on the render clock, not the audio clock.
  const uint32_t rawDtMs = lastFrameUs == 0 ? 10 : (nowUs - lastFrameUs) / 1000;
  lastFrameUs = nowUs;
  // Track the worst frame gap *before* clamping: a stall is what the phone needs
  // to see, and clamping is exactly what hides it from the animation.
  if (rawDtMs > loopMaxMs) {
    loopMaxMs = rawDtMs;
  }
  uint32_t dtMs = rawDtMs;
  if (dtMs == 0) {
    dtMs = 1;
  } else if (dtMs > 250) {
    dtMs = 250;
  }
  const float dtSec = (float)dtMs * 0.001f;
  const uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - loopStatsMs) >= 1000) {
    loopStatsMs = nowMs;
    teleLoopMaxMs = (uint8_t)(loopMaxMs > 255 ? 255 : loopMaxMs);
    loopMaxMs = 0;
  }

  Telemetry tele;
  tele.levelPct = (uint8_t)(snap.level * 100.0f + 0.5f);
  tele.dbfs = (int16_t)lroundf(snap.levelDb);
  tele.peakHz = (uint16_t)lroundf(snap.peakHz);
  tele.agcAction = agcActionPub;
  tele.agcPeakPct = agcPeakPctPub;
  tele.unsaved = web.unsaved() ? 1 : 0;
  tele.loopMaxMs = teleLoopMaxMs;
  for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
    tele.spec[b] = (uint8_t)(snap.bands[b] * 100.0f + 0.5f);
  }

  for (size_t i = 0; i < kStripCount; i++) {
    renderStrip(i, nowMs, dtSec, tele);
  }
  web.setTelemetry(tele);
  // One small copy per frame; the socket's own task does the sending, so a
  // phone that stops reading can never pace the LEDs.
  stream.publish(tele, settings);

  display.renderFrame(snap.bands, settings, tele, nowMs);

  if (millis() - lastStats >= 2000) {
    lastStats = millis();
    char out[80];
    size_t n = (size_t)snprintf(out, sizeof(out), "lvl %3.0f%% %+4.0f dBFS",
                                snap.level * 100.0f, snap.levelDb);
    for (size_t i = 0; i < kStripCount && n + 14 < sizeof(out); i++) {
      n += (size_t)snprintf(out + n, sizeof(out) - n, " | p%u %uL %3.0f%%",
                            (unsigned)i + 1,
                            (unsigned)settings.phys[i].layerCount,
                            strips[i].peak() * 100.0f);
    }
    Serial.println(out);
  }

  // Top the frame up to the period rather than busy-waiting, so Wi-Fi and the
  // audio task get CPU.
  const uint32_t elapsedUs = micros() - nowUs;
  if (elapsedUs + 1500 < kRenderPeriodUs) {
    delay((kRenderPeriodUs - elapsedUs) / 1000);
  }
}
