#pragma once

#include <stddef.h>

// Compile-time choice of FFT implementation.
//
//   -DFFT_BACKEND_ESP_DSP      ESP-DSP (Apache-2.0). Ships inside Arduino-ESP32,
//                              no extra dependency, uses the optimised assembly
//                              routine on ESP32. This is the default.
//   -DFFT_BACKEND_ARDUINO_FFT  arduinoFFT (GPL-3.0). Needs the extra library, and
//                              makes the resulting *binary* GPL-3.0.
//
// The two are meant to be numerically interchangeable: the same audio frame
// produces the same magnitudes either way, so switching only changes speed.

#if defined(FFT_BACKEND_ESP_DSP)
#define FFT_BACKEND_NAME "ESP-DSP (Apache-2.0)"
#elif defined(FFT_BACKEND_ARDUINO_FFT)
#define FFT_BACKEND_NAME "arduinoFFT (GPL-3.0)"
#else
#error "Choose an FFT backend: -DFFT_BACKEND_ESP_DSP or -DFFT_BACKEND_ARDUINO_FFT"
#endif

namespace fftb {

// Full-scale value of a 16-bit sample.
constexpr float kFullScaleSample = 32768.0f;

// Normalises a windowed magnitude so a full-scale sine reads ~1.0 in its peak
// bin: a raised-cosine window turns amplitude A into |X| = A * N * W / 2, where
// W is the window peak (0.5 for Hann), hence the /4 with W = 0.5.
inline float magRef(size_t n) { return kFullScaleSample * (float)n / 4.0f; }

// One-time setup. Returns false if the backend cannot run at this size.
bool begin(size_t n);

// Fills mag[k] with |X[k]| for k = 1..n/2-1 from n real samples.
// `re` and `im` are scratch owned by the caller; the backend may overwrite them.
void magnitude(float *re, float *im, float *mag, size_t n);

// Divisor that turns the summed squared magnitudes of one window into an RMS.
// It depends on the window the backend applies, so it belongs to the backend.
float bandPowerNorm();

} // namespace fftb
