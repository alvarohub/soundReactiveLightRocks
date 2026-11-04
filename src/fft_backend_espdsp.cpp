// ESP-DSP backend (Apache-2.0). Only compiled when FFT_BACKEND_ESP_DSP is
// defined; otherwise this file is empty.

#if defined(FFT_BACKEND_ESP_DSP)

#include "fft_backend.h"

#include "config.h"

#include <dsps_fft2r.h>
#include <math.h>

namespace {

constexpr float kTwoPi = 6.28318531f;

// Interleaved complex scratch, sized exactly for the configured FFT (512 points
// = 4 KB). Tying it to kFftSamples means raising the FFT size automatically
// grows the buffer instead of silently overflowing it.
float buf_[2 * kFftSamples];
bool ready_ = false;

} // namespace

namespace fftb {

bool begin(size_t n) {
  if (n != kFftSamples || n < 16 || (n & (n - 1)) != 0) {
    return false;
  }
  ready_ = dsps_fft2r_init_fc32(nullptr, (int)n) == ESP_OK;
  return ready_;
}

void magnitude(float *re, float *im, float *mag, size_t n) {
  (void)im; // ESP-DSP takes the real samples, so the caller's imaginary part is unused
  if (!ready_ || n != kFftSamples) {
    return;
  }

  // Textbook Hann window, applied while packing into the interleaved complex
  // buffer ESP-DSP expects.
  for (size_t i = 0; i < n; i++) {
    const float w = 0.5f - 0.5f * cosf(kTwoPi * (float)i / (float)(n - 1));
    buf_[2 * i] = re[i] * w;
    buf_[2 * i + 1] = 0.0f;
  }

  dsps_fft2r_fc32(buf_, (int)n);  // dispatched to the assembly version on ESP32
  dsps_bit_rev_fc32(buf_, (int)n); // the transform leaves bins in bit-reversed order

  const float ref = magRef(n);
  for (size_t k = 1; k < n / 2; k++) {
    const float rr = buf_[2 * k];
    const float ii = buf_[2 * k + 1];
    mag[k] = sqrtf(rr * rr + ii * ii) / ref;
  }
}

// True Hann (window peak 0.5) gives the textbook Parseval divisor of 3.0.
float bandPowerNorm() { return 3.0f; }

} // namespace fftb

#endif // FFT_BACKEND_ESP_DSP
