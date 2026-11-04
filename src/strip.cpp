#include "strip.h"

#include <Arduino.h>
#include <driver/rmt.h>
#include <math.h>
#include <string.h>

namespace {

constexpr float kPi = 3.14159265f;

// 800 kHz WS2812B timings at 40 MHz RMT counter clock (clk_div = 2 -> 25 ns/tick):
//   T0H = 400 ns (16 ticks), T0L = 850 ns (34 ticks)
//   T1H = 800 ns (32 ticks), T1L = 450 ns (18 ticks)
constexpr uint32_t kT0H = 16;
constexpr uint32_t kT0L = 34;
constexpr uint32_t kT1H = 32;
constexpr uint32_t kT1L = 18;

void IRAM_ATTR ws2812RmtAdapter(const void *src, rmt_item32_t *dest,
                                size_t srcSize, size_t wantedNum,
                                size_t *translatedSize, size_t *itemNum) {
  if (src == nullptr || dest == nullptr) {
    *translatedSize = 0;
    *itemNum = 0;
    return;
  }
  const rmt_item32_t bit0 = {{{kT0H, 1, kT0L, 0}}};
  const rmt_item32_t bit1 = {{{kT1H, 1, kT1L, 0}}};
  size_t size = 0;
  size_t num = 0;
  const uint8_t *psrc = static_cast<const uint8_t *>(src);
  rmt_item32_t *pdest = dest;
  while (size < srcSize && num < wantedNum) {
    for (int i = 0; i < 8; i++) {
      pdest->val = (*psrc & (1u << (7 - i))) ? bit1.val : bit0.val;
      num++;
      pdest++;
    }
    size++;
    psrc++;
  }
  *translatedSize = size;
  *itemNum = num;
}

// Only the LED indices within this many steps of the blob centre can be lit;
// used to keep addRing() from walking the whole strip for a short blob.
inline int ceilToInt(float v) {
  const int i = (int)v;
  return v > (float)i ? i + 1 : i;
}

} // namespace

bool LedStrip::begin(int pin, uint16_t count, uint8_t rmtChannel) {
  pin_ = pin;
  channel_ = rmtChannel;

  if (!rmtReady_ && pin_ >= 0) {
    const rmt_channel_t ch = static_cast<rmt_channel_t>(channel_);
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(static_cast<gpio_num_t>(pin_), ch);
    config.clk_div = 2; // 80 MHz APB / 2 = 40 MHz
    if (rmt_config(&config) == ESP_OK &&
        rmt_driver_install(config.channel, 0, 0) == ESP_OK &&
        rmt_translator_init(config.channel, ws2812RmtAdapter) == ESP_OK) {
      rmtReady_ = true;
    } else {
      Serial.printf("[led] RMT init failed on G%d (ch %u)\n", pin_,
                    (unsigned)channel_);
    }
  }

  count_ = 0;
  return setCount(count);
}

bool LedStrip::setCount(uint16_t count) {
  if (count < 1) {
    count = 1;
  }
  if (count > kMaxLedsPerStrip) {
    count = kMaxLedsPerStrip;
  }
  if (count == count_) {
    return ready_;
  }

  // Remember the furthest pixel index that was previously driven so the next
  // show() or showCalibrationDot() blanks any tail LEDs in a single RMT frame
  // (never send two RMT frames back-to-back without a >80 us reset gap).
  if (count_ > maxSentCount_) {
    maxSentCount_ = count_;
  }
  for (uint16_t i = count; i < count_; i++) {
    buf_[i][0] = 0.0f;
    buf_[i][1] = 0.0f;
    buf_[i][2] = 0.0f;
  }

  count_ = count;
  ready_ = rmtReady_ && count_ > 0 && pin_ >= 0;
  dirty_ = true;
  return ready_;
}

void LedStrip::clear() {
  const bool hadLight = peak_ > 0.0f;
  for (uint16_t i = 0; i < count_; i++) {
    buf_[i][0] = 0.0f;
    buf_[i][1] = 0.0f;
    buf_[i][2] = 0.0f;
  }
  peak_ = 0.0f;
  if (hadLight) {
    dirty_ = true; // one more frame is needed to actually blank the strip
  }
}

void LedStrip::addRing(float centerPos, float halfWidth, float amp, uint8_t r,
                       uint8_t g, uint8_t b, bool loop) {
  if (!ready_ || halfWidth <= 0.0f || amp <= 0.0f) {
    return;
  }

  const float n = (float)count_;
  const float cr = (float)r / 255.0f;
  const float cg = (float)g / 255.0f;
  const float cb = (float)b / 255.0f;

  const int centre = (int)floorf(centerPos);
  int span = ceilToInt(halfWidth) + 1;
  // A blob wider than the ring covers it completely; one pass over every LED is
  // enough, and this keeps the loop bounded whatever blobLeds is set to.
  if (span * 2 + 1 > (int)count_) {
    span = (int)count_ / 2 + 1;
  }

  for (int k = centre - span; k <= centre + span; k++) {
    int i = k;
    float d = (float)k - centerPos;

    if (loop) {
      // Shortest way around the ring: |d| never exceeds half a lap.
      while (d > n * 0.5f) {
        d -= n;
      }
      while (d <= -n * 0.5f) {
        d += n;
      }
      i = k % (int)count_;
      if (i < 0) {
        i += (int)count_;
      }
    } else {
      // One-shot mode: do not wrap past either end of the strip.
      if (i < 0 || i >= (int)count_) {
        continue;
      }
    }

    const float t = 1.0f - fabsf(d) / halfWidth;
    if (t <= 0.0f) {
      continue;
    }

    // Raised cosine shaped by kRenderGamma on the intensity envelope: keeps the
    // chosen (r, g, b) hue ratio intact across the entire fade instead of
    // crushing secondary channels toward zero.
    const float raw = (0.5f - 0.5f * cosf(kPi * t)) * amp;
    const float v = powf(raw, kRenderGamma);

    buf_[i][0] += v * cr;
    buf_[i][1] += v * cg;
    buf_[i][2] += v * cb;
    dirty_ = true;

    if (buf_[i][0] > peak_) {
      peak_ = buf_[i][0];
    }
    if (buf_[i][1] > peak_) {
      peak_ = buf_[i][1];
    }
    if (buf_[i][2] > peak_) {
      peak_ = buf_[i][2];
    }
  }
}

void LedStrip::show(uint8_t brightnessPct) {
  if (!ready_) {
    return;
  }
  if (inCalib_) {
    inCalib_ = false;
    dirty_ = true;
  }
  if (brightnessPct != lastBrightnessPct_) {
    lastBrightnessPct_ = brightnessPct;
    if (peak_ > 0.0f) {
      dirty_ = true;
    }
  }
  if (!dirty_) {
    return;
  }

  const uint16_t txLeds = count_ > maxSentCount_ ? count_ : maxSentCount_;
  maxSentCount_ = count_;

  const float scale = (float)brightnessPct * 0.01f;
  for (uint16_t i = 0; i < count_; i++) {
    float pr = buf_[i][0];
    float pg = buf_[i][1];
    float pb = buf_[i][2];
    float m = pr > pg ? pr : pg;
    if (pb > m) {
      m = pb;
    }
    if (m > 1.0f) {
      const float inv = 1.0f / m;
      pr *= inv;
      pg *= inv;
      pb *= inv;
    }
    pr *= scale;
    pg *= scale;
    pb *= scale;
    // WS2812B / SK6812 wire order is GRB.
    grb_[i * 3 + 0] = (uint8_t)(pg * 255.0f + 0.5f);
    grb_[i * 3 + 1] = (uint8_t)(pr * 255.0f + 0.5f);
    grb_[i * 3 + 2] = (uint8_t)(pb * 255.0f + 0.5f);
  }
  if (txLeds > count_) {
    memset(grb_ + (size_t)count_ * 3, 0, (size_t)(txLeds - count_) * 3);
  }
  const rmt_channel_t ch = static_cast<rmt_channel_t>(channel_);
  rmt_write_sample(ch, grb_, (size_t)txLeds * 3, true);
  rmt_wait_tx_done(ch, pdMS_TO_TICKS(100));
  dirty_ = false;
}

void LedStrip::showCalibrationDot(uint16_t ledCount, uint8_t r, uint8_t g,
                                  uint8_t b, uint8_t brightnessPct) {
  if (!rmtReady_) {
    return;
  }
  if (ledCount < 1) {
    ledCount = 1;
  }
  if (ledCount > kMaxLedsPerStrip) {
    ledCount = kMaxLedsPerStrip;
  }

  const uint32_t nowMs = millis();
  if (inCalib_ && !dirty_ && brightnessPct == lastBrightnessPct_ &&
      (uint32_t)(nowMs - lastCalibTxMs_) < 120) {
    return;
  }
  inCalib_ = true;
  lastBrightnessPct_ = brightnessPct;
  lastCalibTxMs_ = nowMs;
  dirty_ = false;
  peak_ = 1.0f;

  // Transmit up to whichever is larger: current ledCount or the highest LED
  // previously lit, so stepping ledCount down blanks the old tip immediately.
  const uint16_t txLeds = ledCount > maxSentCount_ ? ledCount : maxSentCount_;
  maxSentCount_ = ledCount;

  memset(grb_, 0, (size_t)txLeds * 3);
  if (r < 16 && g < 16 && b < 16) {
    r = 255;
    g = 255;
    b = 255;
  }
  const float scale = (float)brightnessPct * 0.01f;
  const uint16_t idx = ledCount - 1;
  grb_[idx * 3 + 0] = (uint8_t)((float)g * scale + 0.5f);
  grb_[idx * 3 + 1] = (uint8_t)((float)r * scale + 0.5f);
  grb_[idx * 3 + 2] = (uint8_t)((float)b * scale + 0.5f);

  const rmt_channel_t ch = static_cast<rmt_channel_t>(channel_);
  rmt_write_sample(ch, grb_, (size_t)txLeds * 3, true);
  rmt_wait_tx_done(ch, pdMS_TO_TICKS(100));
}
