// arduinoFFT backend (GPL-3.0). Only compiled when FFT_BACKEND_ARDUINO_FFT is
// defined; otherwise this file is empty.

#if defined(FFT_BACKEND_ARDUINO_FFT)

#include "fft_backend.h"

#include <arduinoFFT.h>
#include <math.h>

namespace fftb {

bool begin(size_t) { return true; }

void magnitude(float *re, float *im, float *mag, size_t n) {
  ArduinoFFT<float> fft(re, im, (uint_fast16_t)n, 0.0f);
  fft.windowing(FFTWindow::Hann, FFTDirection::Forward);
  fft.compute(FFTDirection::Forward);

  const float ref = magRef(n);
  for (size_t i = 1; i < n / 2; i++) {
    mag[i] = sqrtf(re[i] * re[i] + im[i] * im[i]) / ref;
  }
}

// arduinoFFT's "Hann" is 0.54 * (1 - cos), i.e. 1.08x the textbook window, so its
// magnitudes come out 1.08x larger and the Parseval divisor is 3.49 rather than
// the textbook 3.0. Keeping the constant here means bandLevel() reads the same
// with either backend.
float bandPowerNorm() { return 3.49f; }

} // namespace fftb

#endif // FFT_BACKEND_ARDUINO_FFT
