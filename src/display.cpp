#include "display.h"

#include <M5Unified.h>
#include <math.h>
#include <stdio.h>

namespace {

constexpr int kRowBarH = 5;  // the thin bar inside a layer row

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

  // The trigger letter of each layer row lives in the left gutter, so the bars
  // and the rows share one frequency axis and line up vertically.
  axisX0_ = 9;
  bandWidth_ = (w - axisX0_ - 4) / (int)SpectrumAnalyzer::kBands;
  if (bandWidth_ < 4) {
    bandWidth_ = 4;
  }
  barWidth_ = bandWidth_ - 1;
  axisX1_ = axisX0_ + bandWidth_ * (int)SpectrumAnalyzer::kBands - 2;

  hintY_ = h - 12;
  hudY_ = hintY_ - 14;
  rowY0_ = hudY_ - 6 - rowH_ * (int)kMaxLayersPerStrip;
  barTop_ = 13;
  barBottom_ = rowY0_ - 5;
}

size_t Display::primaryLayer(const Settings &settings, size_t i) const {
  const size_t count = settings.phys[i].layerCount;
  for (size_t l = 0; l < count && l < kMaxLayersPerStrip; l++) {
    if (settings.layer[i][l].enabled) {
      return l;
    }
  }
  return 0;
}

uint16_t Display::stripColor(const Settings &settings, size_t i) const {
  const size_t l = primaryLayer(settings, i);
  const LayerSettings &s = settings.layer[i][l];
  const bool anyOn = settings.phys[i].layerCount > 0 && s.enabled;
  return anyOn ? rgb565(s.r, s.g, s.b) : kGrey;
}

char Display::triggerChar(const LayerSettings &s) {
  if (!s.enabled) {
    return '-';
  }
  if (s.triggers & kTrigBeat) {
    return 'B';
  }
  if (s.triggers & (kTrigAccel | kTrigGyro)) {
    return 'A';
  }
  if (s.triggers & kTrigThreshold) {
    return 'S';
  }
  if (s.triggers & kTrigTap) {
    return 'M';
  }
  return '?';
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
  if (hz <= kAudioMinHz) {
    return axisX0_;
  }
  if (hz >= kAudioMaxHz) {
    return axisX1_;
  }
  const float ratio = kAudioMaxHz / kAudioMinHz;
  const float t = logf(hz / kAudioMinHz) / logf(ratio);
  const int x = axisX0_ + (int)(t * (float)(axisX1_ - axisX0_) + 0.5f);
  return x < axisX0_ ? axisX0_ : (x > axisX1_ ? axisX1_ : x);
}

void Display::drawSpectrum(const float *bands) {
  const int span = barBottom_ - barTop_;
  for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
    int h = (int)(bands[b] * (float)span + 0.5f);
    if (h > span) {
      h = span;
    }
    const int x = axisX0_ + (int)b * bandWidth_;
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

void Display::drawHeader(const Settings &settings, const Telemetry &tele) {
  const size_t sel = selected_ < kStripCount ? selected_ : 0;
  const uint16_t col = stripColor(settings, sel);

  char left[24];
  char right[16];
  snprintf(left, sizeof(left), "S%u  %uL", (unsigned)sel + 1,
           (unsigned)settings.phys[sel].layerCount);
  snprintf(right, sizeof(right), "out %u%%",
           (unsigned)tele.strip[sel].brightPct);
  if (strcmp(left, prevHeadLeft_) == 0 && strcmp(right, prevHeadRight_) == 0 &&
      (int)col == prevHeadCol_) {
    return;
  }
  snprintf(prevHeadLeft_, sizeof(prevHeadLeft_), "%s", left);
  snprintf(prevHeadRight_, sizeof(prevHeadRight_), "%s", right);
  prevHeadCol_ = (int)col;

  const int w = (int)M5.Display.width();
  M5.Display.fillRect(0, 0, w, barTop_ - 2, kBlack);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(col, kBlack);
  M5.Display.setCursor(2, 1);
  M5.Display.print(left);
  M5.Display.setTextColor(kWhite, kBlack);
  const int rx = w - (int)M5.Display.textWidth(right) - 2;
  M5.Display.setCursor(rx > 60 ? rx : 60, 1);
  M5.Display.print(right);
}

void Display::drawLayerRows(const Settings &settings, const Telemetry &tele) {
  const size_t sel = selected_ < kStripCount ? selected_ : 0;
  const uint8_t count = settings.phys[sel].layerCount;
  const int w = (int)M5.Display.width();

  // A strip switch or a changed layer count invalidates every row.
  if (count != prevLayers_) {
    M5.Display.fillRect(0, rowY0_, w, rowH_ * (int)kMaxLayersPerStrip, kBlack);
    for (size_t l = 0; l < kMaxLayersPerStrip; l++) {
      prevLo_[l] = -1;
      prevHi_[l] = -1;
      prevFill_[l] = -1;
      prevCol_[l] = -1;
      prevDot_[l] = -1;
      prevCh_[l] = -1;
    }
    prevLayers_ = count;
  }

  for (size_t l = 0; l < count && l < kMaxLayersPerStrip; l++) {
    const LayerSettings &s = settings.layer[sel][l];
    const LayerTelemetry &lt = tele.strip[sel].layer[l];
    const int y = rowY0_ + (int)l * rowH_;

    // The row covers exactly the band this layer listens to, on the same axis
    // as the spectrum above, so a layer and its bars are the same width.
    float lo = (float)s.centerHz - (float)s.widthHz * 0.5f;
    float hi = (float)s.centerHz + (float)s.widthHz * 0.5f;
    if (lo < kAudioMinHz) {
      lo = kAudioMinHz;
    }
    if (hi > kAudioMaxHz) {
      hi = kAudioMaxHz;
    }
    const int xLo = freqToX(lo);
    int xHi = freqToX(hi);
    if (xHi < xLo + 3) {
      xHi = xLo + 3;
    }

    const uint16_t col = s.enabled ? rgb565(s.r, s.g, s.b) : kGrey;
    const int span = xHi - xLo + 1;
    const uint8_t lvl = lt.levelPct > 100 ? 100 : lt.levelPct;
    const int fill = (int)((long)span * (long)lvl / 100);
    // The dot is the trigger indicator: on while this layer is actually
    // emitting something, dark when it is quiet or switched off.
    const int dot = (s.enabled && lt.blobs > 0) ? 1 : 0;
    const int ch = triggerChar(s);

    if (xLo != prevLo_[l] || xHi != prevHi_[l] || (int)col != prevCol_[l] ||
        ch != prevCh_[l]) {
      M5.Display.fillRect(0, y, w, rowH_ - 1, kBlack);
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(col, kBlack);
      M5.Display.setCursor(1, y);
      M5.Display.print((char)ch);
      M5.Display.fillRect(xLo, y + 2, span, kRowBarH, kTrack);
      if (fill > 0) {
        M5.Display.fillRect(xLo, y + 2, fill, kRowBarH, col);
      }
      M5.Display.fillRect(w - 6, y + 2, 5, 5, dot ? col : kTrack);
      prevLo_[l] = xLo;
      prevHi_[l] = xHi;
      prevCol_[l] = (int)col;
      prevCh_[l] = ch;
      prevFill_[l] = fill;
      prevDot_[l] = dot;
      continue;
    }

    const int pf = prevFill_[l] < 0 ? 0 : prevFill_[l];
    if (fill != pf) {
      if (fill > pf) {
        M5.Display.fillRect(xLo + pf, y + 2, fill - pf, kRowBarH, col);
      } else {
        M5.Display.fillRect(xLo + fill, y + 2, pf - fill, kRowBarH, kTrack);
      }
      prevFill_[l] = fill;
    }
    if (dot != prevDot_[l]) {
      M5.Display.fillRect(w - 6, y + 2, 5, 5, dot ? col : kTrack);
      prevDot_[l] = dot;
    }
  }
}

void Display::drawHud(const Telemetry &tele, const Settings &settings) {
  // Report the tempo of the first layer that has a beat lock.
  uint16_t bpm = 0;
  for (size_t i = 0; i < kStripCount && bpm == 0; i++) {
    for (size_t l = 0; l < settings.phys[i].layerCount; l++) {
      if (tele.strip[i].layer[l].bpm > 0) {
        bpm = tele.strip[i].layer[l].bpm;
        break;
      }
    }
  }

  char line[48];
  if (bpm > 0) {
    snprintf(line, sizeof(line), "lvl %3u%% %+4ddB %3ubpm %+ddB  ",
             (unsigned)tele.levelPct, (int)tele.dbfs, (unsigned)bpm,
             (int)settings.gainDb);
  } else {
    snprintf(line, sizeof(line), "lvl %3u%% %+4d dB %4u Hz %+ddB",
             (unsigned)tele.levelPct, (int)tele.dbfs, (unsigned)tele.peakHz,
             (int)settings.gainDb);
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
  M5.Display.print("A cycle triggers  hold: wifi");
  M5.Display.setCursor(4, 102);
  M5.Display.print("B next strip      hold: save");
}

void Display::finishBanner(const char *ssid, bool apEnabled, const String &ip) {
  M5.Display.fillScreen(kBlack);
  invalidateAll();
  drawHint(ssid, apEnabled, ip);
  lastHudMs_ = 0;
}

void Display::invalidateAll() {
  for (size_t b = 0; b < SpectrumAnalyzer::kBands; b++) {
    prevBar_[b] = -1;
  }
  // 0xFF can never equal a real layer count, so the row area is cleared and
  // repainted on the next frame.
  prevLayers_ = 0xFF;
  prevHeadLeft_[0] = '\0';
  prevHeadRight_[0] = '\0';
  prevHeadCol_ = -1;
}

void Display::setSelectedStrip(uint8_t i) {
  if (i == selected_) {
    return;
  }
  selected_ = i;
  invalidateAll();
}

void Display::renderFrame(const float *bands, const Settings &settings,
                          const Telemetry &tele, uint32_t nowMs,
                          bool forceHud) {
  M5.Display.startWrite();
  drawHeader(settings, tele);
  drawSpectrum(bands);
  drawLayerRows(settings, tele);
  if (forceHud || lastHudMs_ == 0 || (uint32_t)(nowMs - lastHudMs_) >= 100) {
    lastHudMs_ = nowMs;
    drawHud(tele, settings);
  }
  M5.Display.endWrite();
}
