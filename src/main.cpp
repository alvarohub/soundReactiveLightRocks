#include <M5Unified.h>

#include <math.h>
#include <stdio.h>

#include "audio_fft.h"
#include "blob_mode.h"
#include "config.h"
#include "settings.h"
#include "store.h"
#include "strip.h"
#include "webui.h"

namespace {

// ---- Screen layout (240 x 135 in landscape) ------------------------------
// Vertical positions are derived from the panel height in setup(), so the layout
// also holds if the rotation or the panel differs.
int barTop = 14;
int barBottom = 73;
int bandRowY[kStripCount] = {};
int meterY[kStripCount] = {};
int meterX = 15;
int meterWidth = 222;
int hudY = 111;
int hintY = 123;
int apX = 226;
int bandWidth = 8;
int barWidth = 7;

constexpr int kMeterH = 7;

// M5GFX hands colour values straight to the panel, and this panel writes 16-bit
// RGB565 (see lgfx's TFT_RED = 0xF800), so colours are built in that format.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kGrey = 0x7BEF;
constexpr uint16_t kTrack = 0x2124;
constexpr uint16_t kApOn = rgb565(0, 255, 120);

SpectrumAnalyzer analyzer;
LedStrip strips[kStripCount];
BlobEngine engines[kStripCount];
Settings settings;
SettingsStore store;
WebUi web;

int prevBar[SpectrumAnalyzer::kBands] = {};
int prevMeterQ[kStripCount] = {-1, -1};
int prevThrQ[kStripCount] = {-1, -1};
uint32_t prevMeterColor[kStripCount] = {0xFFFFFFFF, 0xFFFFFFFF};
int prevBandLo[kStripCount] = {-1, -1};
int prevBandHi[kStripCount] = {-1, -1};
int prevBandCtr[kStripCount] = {-1, -1};
uint32_t prevChrome[kStripCount] = {0xFFFFFFFF, 0xFFFFFFFF};
uint32_t lastFrameUs = 0;
uint32_t lastHud = 0;
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

uint16_t stripColor(size_t i) {
  const StripSettings &s = settings.strip[i];
  return s.enabled ? rgb565(s.r, s.g, s.b) : kGrey;
}

// Green at the bottom, red at the top, so a bar's colour depends on its height.
uint16_t barColor(int y) {
  const int span = barBottom - barTop;
  float t = (float)(barBottom - y) / (float)span;
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

  uint8_t r, g;
  if (t < 0.5f) {
    r = (uint8_t)(255.0f * t * 2.0f);
    g = 255;
  } else {
    r = 255;
    g = (uint8_t)(255.0f * (2.0f - t * 2.0f));
  }
  return rgb565(r, g, 0);
}

// Maps a frequency in Hz to the matching X coordinate across the log-spaced
// spectrogram.
int freqToX(float hz) {
  const int maxX = (int)SpectrumAnalyzer::kBands * bandWidth - 2;
  if (hz <= kAudioMinHz) {
    return 0;
  }
  if (hz >= kAudioMaxHz) {
    return maxX;
  }
  const float ratio = kAudioMaxHz / kAudioMinHz;
  const float t = logf(hz / kAudioMinHz) / logf(ratio);
  const int x = (int)(t * (float)maxX + 0.5f);
  return x < 0 ? 0 : (x > maxX ? maxX : x);
}

// Only the difference between frames is painted, which keeps the bars free of
// flicker and keeps the SPI traffic low enough to stay fluid.
void drawSpectrum(const float *bands) {
  const int span = barBottom - barTop;
  for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
    int h = (int)(bands[b] * (float)span + 0.5f);
    if (h > span) {
      h = span;
    }
    const int x = (int)b * bandWidth;
    if (prevBar[b] < 0) {
      M5.Display.fillRect(x, barTop, barWidth, span, kBlack);
      if (h > 0) {
        const int top = barBottom - h;
        M5.Display.fillRect(x, top, barWidth, h, barColor(top));
      }
    } else if (h > prevBar[b]) {
      const int top = barBottom - h;
      M5.Display.fillRect(x, top, barWidth, h - prevBar[b], barColor(top));
    } else if (h < prevBar[b]) {
      M5.Display.fillRect(x, barBottom - prevBar[b], barWidth, prevBar[b] - h,
                          kBlack);
    }
    prevBar[b] = h;
  }
}

// Draws each strip's frequency band as an empty rectangle overlaid on the
// spectrogram with a small downward-pointing triangle on the upper edge at the
// centre frequency, plus the S1 / S2 meter labels.
void drawStripChrome(const float *bands) {
  bool changed = false;
  int xLo[kStripCount] = {};
  int xHi[kStripCount] = {};
  int xCtr[kStripCount] = {};

  for (size_t i = 0; i < kStripCount; i++) {
    const StripSettings &s = settings.strip[i];
    float lo = (float)s.centerHz - (float)s.widthHz * 0.5f;
    float hi = (float)s.centerHz + (float)s.widthHz * 0.5f;
    if (lo < kAudioMinHz) {
      lo = kAudioMinHz;
    }
    if (hi > kAudioMaxHz) {
      hi = kAudioMaxHz;
    }

    xLo[i] = freqToX(lo);
    xHi[i] = freqToX(hi);
    if (xHi[i] < xLo[i] + 2) {
      xHi[i] = xLo[i] + 2;
    }
    xCtr[i] = freqToX((float)s.centerHz);

    const uint32_t key = ((uint32_t)s.enabled << 24) | ((uint32_t)s.r << 16) |
                         ((uint32_t)s.g << 8) | (uint32_t)s.b;
    if (xLo[i] != prevBandLo[i] || xHi[i] != prevBandHi[i] ||
        xCtr[i] != prevBandCtr[i] || key != prevChrome[i]) {
      if (prevBandLo[i] >= 0 && prevBandHi[i] >= prevBandLo[i]) {
        M5.Display.drawRect(prevBandLo[i], barTop,
                            prevBandHi[i] - prevBandLo[i] + 1,
                            barBottom - barTop + 1, kBlack);
      }
      prevBandLo[i] = xLo[i];
      prevBandHi[i] = xHi[i];
      prevBandCtr[i] = xCtr[i];
      prevChrome[i] = key;
      changed = true;
    }
  }

  if (changed) {
    // Clear the triangle header lane above the spectrogram and mark bars dirty
    // so erased rectangle edges are cleanly repainted.
    M5.Display.fillRect(0, barTop - 7, M5.Display.width(), 7, kBlack);
    for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
      prevBar[b] = -1;
    }
    for (size_t i = 0; i < kStripCount; i++) {
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(stripColor(i), kBlack);
      M5.Display.setCursor(2, meterY[i]);
      M5.Display.printf("S%u", (unsigned)i + 1);
    }
  }

  drawSpectrum(bands);

  for (size_t i = 0; i < kStripCount; i++) {
    const uint16_t col = stripColor(i);
    // Empty rectangle over the spectrogram showing the frequency band window.
    M5.Display.drawRect(xLo[i], barTop, xHi[i] - xLo[i] + 1,
                        barBottom - barTop + 1, col);
    // Small downward-pointing triangle on the top edge marking centerHz.
    M5.Display.fillTriangle(xCtr[i] - 3, barTop - 6, xCtr[i] + 3, barTop - 6,
                            xCtr[i], barTop - 1, col);
    // Keep the horizontal summary line underneath for quick strip distinction.
    const int y = bandRowY[i];
    if (changed) {
      M5.Display.fillRect(0, y, M5.Display.width(), 2, kBlack);
      M5.Display.fillRect(xLo[i], y, xHi[i] - xLo[i] + 1, 2, col);
    }
  }
}

// One row per strip: the band level as a bar, with the trigger threshold marked.
// Quantised to 2% steps so most frames do not touch the display at all.
void drawMeters(const Telemetry &tele) {
  for (size_t i = 0; i < kStripCount; i++) {
    const StripSettings &s = settings.strip[i];
    const int q = tele.strip[i].levelPct / 2;
    const int thrQ = s.thresholdPct / 2;
    const uint32_t key = ((uint32_t)s.enabled << 24) | ((uint32_t)s.r << 16) |
                         ((uint32_t)s.g << 8) | (uint32_t)s.b;
    if (q == prevMeterQ[i] && thrQ == prevThrQ[i] && key == prevMeterColor[i]) {
      continue;
    }
    prevMeterQ[i] = q;
    prevThrQ[i] = thrQ;
    prevMeterColor[i] = key;

    const int y = meterY[i];
    const int fill = meterWidth * q / 50;
    M5.Display.fillRect(meterX, y, meterWidth, kMeterH, kTrack);
    if (fill > 0) {
      M5.Display.fillRect(meterX, y, fill, kMeterH, stripColor(i));
    }
    // Threshold tick, drawn last so it stays visible over the bar.
    const int tick = meterX + meterWidth * thrQ / 50;
    M5.Display.fillRect(tick, y - 1, 2, kMeterH + 2, kWhite);
  }
}

// A background colour is given to every glyph, so the text clears itself and the
// HUD can be refreshed without repainting the whole line.
void drawHud(const Telemetry &tele) {
  char line[48];
  snprintf(line, sizeof(line), "lvl %3u%% %+4d dB %4u Hz", (unsigned)tele.levelPct,
           (int)tele.dbfs, (unsigned)tele.peakHz);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(kWhite, kBlack);
  M5.Display.setCursor(2, hudY);
  M5.Display.print(line);

  M5.Display.setTextColor(web.enabled() ? kApOn : kGrey, kBlack);
  M5.Display.setCursor(apX, hudY);
  M5.Display.print("AP");
}

void drawHint() {
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(kGrey, kBlack);
  M5.Display.setCursor(2, hintY);
  M5.Display.print("A test/hold wifi   B gain/save");
}

// Shown at boot (and when the access point is toggled), because the phone needs
// the network name and the password.
void showBanner() {
  const bool on = web.enabled();

  M5.Display.fillScreen(kBlack);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(kWhite, kBlack);
  M5.Display.setCursor(4, 6);
  M5.Display.print(on ? "Connect to Wi-Fi:" : "Access point off");

  M5.Display.setTextColor(on ? kApOn : kGrey, kBlack);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(4, 18);
  M5.Display.print(on ? WebUi::kSsid : "---");

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(kWhite, kBlack);
  if (on) {
    M5.Display.setCursor(4, 42);
    M5.Display.printf("password  %s", WebUi::kPassword);
    M5.Display.setCursor(4, 54);
    M5.Display.printf("open      http://%s", web.ip().c_str());
  } else {
    M5.Display.setCursor(4, 42);
    M5.Display.print("hold A to switch it");
    M5.Display.setCursor(4, 54);
    M5.Display.print("back on");
  }

  M5.Display.setTextColor(kGrey, kBlack);
  M5.Display.setCursor(4, 78);
  M5.Display.printf("strip 1 on G%d, strip 2 on G%d", kStripPins[0],
                    kStripPins[kStripCount - 1]);
  M5.Display.setCursor(4, 90);
  M5.Display.print("A test trigger  hold: wifi");
  M5.Display.setCursor(4, 102);
  M5.Display.print("B mic gain      hold: save");
}

void finishBanner() {
  M5.Display.fillScreen(kBlack);
  for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
    prevBar[b] = 0;
  }
  for (size_t i = 0; i < kStripCount; i++) {
    prevMeterQ[i] = -1;
    prevThrQ[i] = -1;
    prevMeterColor[i] = 0xFFFFFFFF;
    prevBandLo[i] = -1;
    prevBandHi[i] = -1;
    prevBandCtr[i] = -1;
    prevChrome[i] = 0xFFFFFFFF;
  }
  drawHint();
  lastHud = 0;
  lastFrameUs = 0; // don't count the banner pause as a frame interval
}

void computeLayout() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();

  bandWidth = w / SpectrumAnalyzer::kBands;
  barWidth = bandWidth - 1;

  barTop = 14;
  barBottom = h - 62;
  for (size_t i = 0; i < kStripCount; i++) {
    bandRowY[i] = barBottom + 3 + (int)i * 3;
    meterY[i] = barBottom + 11 + (int)i * 11;
  }
  meterX = 15;
  meterWidth = w - meterX - 3;
  hudY = h - 24;
  hintY = h - 12;
  apX = w - 16;
}

} // namespace

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = false;        // keep speaker off so I2S pins stay free for the mic
  cfg.external_display_value = 0;  // do not probe Grove pins (G9/G10 or G32/G33) for I2C displays
  cfg.external_imu = false;
  cfg.external_rtc = false;
  M5.begin(cfg);

  Serial.begin(115200);

  M5.Display.setRotation(1);
  M5.Power.setLed(0); // red LED shares GPIO19 with the IR emitter
  M5.Display.setBrightness(150);
  M5.Display.fillScreen(kBlack);

  computeLayout();

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
  }

  if (!analyzer.begin(kSampleRate)) {
    M5.Display.setTextColor(kWhite, kBlack);
    M5.Display.setCursor(4, 66);
    M5.Display.print("mic init failed");
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
  for (size_t i = 0; i < kStripCount; i++) {
    Serial.printf("  strip %u: G%d, %u LEDs\n", (unsigned)i + 1, strips[i].pin(),
                  (unsigned)strips[i].count());
  }
}

void loop() {
  M5.update();
  web.update();

  if (web.consumeChanged()) {
    noteSettingsChanged();
  }
  if (settingsDirty && millis() - settingsChangedAt >= kSaveDelayMs) {
    settingsDirty = false;
    const bool ok = store.saveLive(settings);
    if (ok) {
      web.markSaved();
    }
    Serial.println(ok ? "[cfg] saved" : "[cfg] save failed");
  }

  if (M5.BtnA.wasClicked()) {
    // Manual trigger: the fastest way to judge speed, fade and colour without
    // having to make a noise.
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
    settingsDirty = false;
    const bool ok = store.saveLive(settings);
    if (ok) {
      web.markSaved();
    }
    Serial.println(ok ? "[cfg] saved by button" : "[cfg] save failed");
  }

  // The gain can be changed from the buttons or from the phone, so keep the
  // analyzer in sync in one place.
  if (syncedGainDb != settings.gainDb) {
    syncedGainDb = settings.gainDb;
    analyzer.setGainDb(syncedGainDb);
    lastHud = 0;
  }

  // ledCount is a runtime parameter, so react to it changing rather than assuming
  // it at boot. setCount() is a no-op when the value did not change.
  for (size_t i = 0; i < kStripCount; i++) {
    strips[i].setCount(settings.strip[i].ledCount);
  }

  if (!analyzer.update()) {
    // Even if the mic is unavailable, pace at ~31 fps so manual triggers and
    // the web UI still animate the strips.
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
    const StripSettings &s = settings.strip[i];
    LedStrip &strip = strips[i];
    BlobEngine &engine = engines[i];

    if (!s.enabled) {
      engine.reset();
      strip.clear(); // blanks the strip once, then stays quiet
      strip.show(s.brightnessPct);
      continue;
    }

    // Each strip only listens to its own window.
    const float level = analyzer.bandLevel((float)s.centerHz, (float)s.widthHz);
    if (web.isCalibrating(i, nowMs)) {
      strip.showCalibrationDot(s.ledCount, s.r, s.g, s.b, s.brightnessPct);
    } else {
      engine.update(level, nowMs, dtSec, s);
      strip.clear();
      engine.render(strip, s);
      strip.show(s.brightnessPct);
    }

    tele.strip[i].levelPct = (uint8_t)(level * 100.0f + 0.5f);
    tele.strip[i].brightPct = (uint8_t)(strip.peak() * 100.0f + 0.5f);
    tele.strip[i].blobs = (uint8_t)engine.activeCount();
  }
  web.setTelemetry(tele);

  M5.Display.startWrite();
  drawStripChrome(analyzer.bands());
  drawMeters(tele);
  if (millis() - lastHud >= 100) {
    lastHud = millis();
    drawHud(tele);
  }
  M5.Display.endWrite();

  if (millis() - lastStats >= 2000) {
    lastStats = millis();
    char out[64];
    size_t n = (size_t)snprintf(out, sizeof(out), "lvl %3.0f%% %+4.0f dBFS",
                                analyzer.level() * 100.0f, analyzer.levelDb());
    for (size_t i = 0; i < kStripCount && n + 12 < sizeof(out); i++) {
      n += (size_t)snprintf(out + n, sizeof(out) - n, " | s%u %3.0f%% %ub",
                            (unsigned)i + 1, strips[i].peak() * 100.0f,
                            (unsigned)engines[i].activeCount());
    }
    Serial.println(out);
  }
}
