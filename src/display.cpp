#include "display.h"

#include <M5Unified.h>
#include <math.h>
#include <stdio.h>

namespace {

constexpr int kMeterH = 7;

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kGrey = 0x7BEF;
constexpr uint16_t kTrack = 0x2124;
constexpr uint16_t kApOn = rgb565(0, 255, 120);

} // namespace

void Display::begin() {
  M5.Display.setRotation(1);
  M5.Power.setLed(0); // red LED shares GPIO19 with the IR emitter on StickC Plus2
  M5.Display.setBrightness(150);
  M5.Display.fillScreen(kBlack);
  computeLayout();
}

void Display::computeLayout() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();

  bandWidth_ = w / (int)SpectrumAnalyzer::kBands;
  barWidth_ = bandWidth_ - 1;

  barTop_ = 14;
  barBottom_ = h - 62;
  for (size_t i = 0; i < kStripCount; i++) {
    bandRowY_[i] = barBottom_ + 3 + (int)i * 3;
    meterY_[i] = barBottom_ + 11 + (int)i * 11;
  }
  meterX_ = 15;
  meterWidth_ = w - meterX_ - 3;
  hudY_ = h - 24;
  hintY_ = h - 12;
}

uint16_t Display::stripColor(const StripSettings &s) const {
  return s.enabled ? rgb565(s.r, s.g, s.b) : kGrey;
}

uint16_t Display::barColor(int y) const {
  const int span = barBottom_ - barTop_;
  float t = (float)(barBottom_ - y) / (float)span;
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

int Display::freqToX(float hz) const {
  const int maxX = (int)SpectrumAnalyzer::kBands * bandWidth_ - 2;
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

void Display::drawSpectrum(const float *bands) {
  const int span = barBottom_ - barTop_;
  for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
    int h = (int)(bands[b] * (float)span + 0.5f);
    if (h > span) {
      h = span;
    }
    const int x = (int)b * bandWidth_;
    if (prevBar_[b] < 0) {
      M5.Display.fillRect(x, barTop_, barWidth_, span, kBlack);
      if (h > 0) {
        const int top = barBottom_ - h;
        M5.Display.fillRect(x, top, barWidth_, h, barColor(top));
      }
    } else if (h > prevBar_[b]) {
      const int top = barBottom_ - h;
      M5.Display.fillRect(x, top, barWidth_, h - prevBar_[b], barColor(top));
    } else if (h < prevBar_[b]) {
      M5.Display.fillRect(x, barBottom_ - prevBar_[b], barWidth_,
                          prevBar_[b] - h, kBlack);
    }
    prevBar_[b] = h;
  }
}

void Display::drawStripChrome(const float *bands, const Settings &settings) {
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

    const uint32_t key = ((uint32_t)s.enabled << 28) |
                         ((uint32_t)(s.mode & 0x0F) << 24) |
                         ((uint32_t)s.r << 16) | ((uint32_t)s.g << 8) |
                         (uint32_t)s.b;
    if (xLo[i] != prevBandLo_[i] || xHi[i] != prevBandHi_[i] ||
        xCtr[i] != prevBandCtr_[i] || key != prevChrome_[i]) {
      if (prevBandLo_[i] >= 0 && prevBandHi_[i] >= prevBandLo_[i]) {
        M5.Display.drawRect(prevBandLo_[i], barTop_,
                            prevBandHi_[i] - prevBandLo_[i] + 1,
                            barBottom_ - barTop_ + 1, kBlack);
      }
      prevBandLo_[i] = xLo[i];
      prevBandHi_[i] = xHi[i];
      prevBandCtr_[i] = xCtr[i];
      prevChrome_[i] = key;
      changed = true;
    }
  }

  if (changed) {
    M5.Display.fillRect(0, barTop_ - 7, M5.Display.width(), 7, kBlack);
    for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
      prevBar_[b] = -1;
    }
    for (size_t i = 0; i < kStripCount; i++) {
      const StripSettings &s = settings.strip[i];
      const char prefix = s.mode == StripSettings::kBeat
                              ? 'B'
                              : (s.mode == StripSettings::kManual ? 'M' : 'S');
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(stripColor(s), kBlack);
      M5.Display.setCursor(2, meterY_[i]);
      M5.Display.printf("%c%u", prefix, (unsigned)i + 1);
    }
  }

  drawSpectrum(bands);

  for (size_t i = 0; i < kStripCount; i++) {
    const StripSettings &s = settings.strip[i];
    const uint16_t col = stripColor(s);
    M5.Display.drawRect(xLo[i], barTop_, xHi[i] - xLo[i] + 1,
                        barBottom_ - barTop_ + 1, col);
    M5.Display.fillTriangle(xCtr[i] - 3, barTop_ - 6, xCtr[i] + 3, barTop_ - 6,
                            xCtr[i], barTop_ - 1, col);
    const int y = bandRowY_[i];
    if (changed) {
      M5.Display.fillRect(0, y, M5.Display.width(), 2, kBlack);
      M5.Display.fillRect(xLo[i], y, xHi[i] - xLo[i] + 1, 2, col);
    }
  }
}

void Display::drawMeters(const Telemetry &tele, const Settings &settings) {
  for (size_t i = 0; i < kStripCount; i++) {
    const StripSettings &s = settings.strip[i];
    uint8_t pct = tele.strip[i].levelPct;
    if (s.mode == StripSettings::kBeat) {
      pct = tele.strip[i].beatPct > pct ? tele.strip[i].beatPct : pct;
    } else if (s.mode == StripSettings::kManual) {
      pct = tele.strip[i].brightPct;
    }
    const int q = pct / 2;
    const int thrQ =
        (s.mode == StripSettings::kThreshold) ? (s.thresholdPct / 2) : -1;
    const uint32_t key = ((uint32_t)s.enabled << 28) |
                         ((uint32_t)(s.mode & 0x0F) << 24) |
                         ((uint32_t)s.r << 16) | ((uint32_t)s.g << 8) |
                         (uint32_t)s.b;
    if (q == prevMeterQ_[i] && thrQ == prevThrQ_[i] &&
        key == prevMeterColor_[i]) {
      continue;
    }
    prevMeterQ_[i] = q;
    prevThrQ_[i] = thrQ;
    prevMeterColor_[i] = key;

    const int y = meterY_[i];
    const int fill = meterWidth_ * q / 50;
    M5.Display.fillRect(meterX_, y, meterWidth_, kMeterH, kTrack);
    if (fill > 0) {
      M5.Display.fillRect(meterX_, y, fill, kMeterH, stripColor(s));
    }
    if (thrQ >= 0) {
      const int tick = meterX_ + meterWidth_ * thrQ / 50;
      M5.Display.fillRect(tick, y - 1, 2, kMeterH + 2, kWhite);
    }
  }
}

void Display::drawHud(const Telemetry &tele, const Settings &settings) {
  uint16_t bpm = 0;
  for (size_t i = 0; i < kStripCount; i++) {
    if (settings.strip[i].mode == StripSettings::kBeat &&
        tele.strip[i].bpm > 0) {
      bpm = tele.strip[i].bpm;
      break;
    }
  }
  if (bpm == 0) {
    for (size_t i = 0; i < kStripCount; i++) {
      if (tele.strip[i].bpm > 0) {
        bpm = tele.strip[i].bpm;
        break;
      }
    }
  }

  char line[48];
  if (bpm > 0) {
    snprintf(line, sizeof(line), "lvl %3u%% %+4ddB %3ubpm +%udB   ",
             (unsigned)tele.levelPct, (int)tele.dbfs, (unsigned)bpm,
             (unsigned)settings.gainDb);
  } else {
    snprintf(line, sizeof(line), "lvl %3u%% %+4d dB %4u Hz  +%udB",
             (unsigned)tele.levelPct, (int)tele.dbfs, (unsigned)tele.peakHz,
             (unsigned)settings.gainDb);
  }

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(kWhite, kBlack);
  M5.Display.setCursor(2, hudY_);
  M5.Display.print(line);
}

void Display::drawHint(const char *ssid, bool apEnabled, const String &ip) {
  M5.Display.fillRect(0, hintY_, M5.Display.width(), 10, kBlack);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(apEnabled ? kApOn : kGrey, kBlack);
  M5.Display.setCursor(2, hintY_);
  M5.Display.print(ssid);

  M5.Display.setTextColor(kGrey, kBlack);
  if (apEnabled) {
    const int ipX = M5.Display.width() - (int)ip.length() * 6 - 2;
    M5.Display.setCursor(ipX > 100 ? ipX : 100, hintY_);
    M5.Display.print(ip);
  } else {
    M5.Display.setCursor(M5.Display.width() - 40, hintY_);
    M5.Display.print("AP off");
  }
}

void Display::showBanner(const char *ssid, const char *password,
                         bool apEnabled, const String &ip) {
  M5.Display.fillScreen(kBlack);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(kWhite, kBlack);
  M5.Display.setCursor(4, 6);
  M5.Display.print(apEnabled ? "Connect to Wi-Fi:" : "Access point off");

  M5.Display.setTextColor(apEnabled ? kApOn : kGrey, kBlack);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(4, 18);
  M5.Display.print(ssid);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(kWhite, kBlack);
  if (apEnabled) {
    M5.Display.setCursor(4, 42);
    M5.Display.printf("password  %s", password);
    M5.Display.setCursor(4, 54);
    M5.Display.printf("open      http://%s", ip.c_str());
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

void Display::finishBanner(const char *ssid, bool apEnabled, const String &ip) {
  M5.Display.fillScreen(kBlack);
  for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
    prevBar_[b] = 0;
  }
  for (size_t i = 0; i < kStripCount; i++) {
    prevMeterQ_[i] = -1;
    prevThrQ_[i] = -1;
    prevMeterColor_[i] = 0xFFFFFFFF;
    prevBandLo_[i] = -1;
    prevBandHi_[i] = -1;
    prevBandCtr_[i] = -1;
    prevChrome_[i] = 0xFFFFFFFF;
  }
  drawHint(ssid, apEnabled, ip);
  lastHudMs_ = 0;
}

void Display::renderFrame(const float *bands, const Settings &settings,
                          const Telemetry &tele, uint32_t nowMs,
                          bool forceHud) {
  M5.Display.startWrite();
  drawStripChrome(bands, settings);
  drawMeters(tele, settings);
  if (forceHud || lastHudMs_ == 0 || (uint32_t)(nowMs - lastHudMs_) >= 100) {
    lastHudMs_ = nowMs;
    drawHud(tele, settings);
  }
  M5.Display.endWrite();
}
