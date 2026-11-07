# Architecture

How the firmware is put together, and why. [SPEC.md](./SPEC.md) says what it does;
this file says how it is built.

---

## The one idea everything follows

**One audio block = one frame.** The microphone delivers 512 samples at 16 kHz and
blocks for ~32 ms; in that same tick the FFT runs, each strip decides whether to
spawn a blob, the blobs move and fade, the pixels are clocked out and the screen is
repainted. Nothing runs in parallel: no FreeRTOS task of our own, no timer
interrupt, no double buffering between audio and lights.

That single decision is what keeps the structure small. Motion and fading are
computed from the *measured* frame time (`dtMs`), not from an assumption that a
frame takes 32 ms, so the show stays correct even when a frame runs long — but the
code never has to worry about two things touching the pixel buffer at once.

---

## Layers

```
                        ┌───────────────────────────────┐
   built-in mic  ──────►│  SpectrumAnalyzer             │
   (M5Unified, I2S)     │  audio_fft.cpp                │
                        │  mic → window → FFT → magnitudes
                        └───────────────┬───────────────┘
                                        │  bands()            (0..1, for the screen)
                                        │  level()/levelDb()  (0..1, broadband)
                                        │  bandLevel(center, width)  (0..1, per strip)
                                        ▼
                        ┌───────────────────────────────┐
                        │  LayerEngine  (1 per LAYER)   │
                        │  layer.cpp                    │
                        │  owns: effect pool, flash/    │
                        │  sparkle envelopes, trigger   │
                        │  input (threshold / beat)     │
                        └───────────────┬───────────────┘
                                        │  addRing / addFill / addSparkles
                                        │  → adds *linear* intensity into the
                                        │    PHYSICAL strip's shared buffer
                                        ▼
                        ┌───────────────────────────────┐
                        │  LedStrip  (the CONTAINER,    │
                        │  one per pin)                 │
                        │  strip.cpp                    │
                        │  owns: ledCount, linear frame │
                        │  buffer, gamma, RMT output    │
                        └───────────────┬───────────────┘
                                        ▼
                                   G32 / G33
```

The data going down that chain gets simpler at every step: audio → a single
number → a light pattern → bytes on a wire. Each box only knows about the box
below it.

Alongside that signal path runs the **control plane**:

```
        phone ──HTTP──► WebUi ──writes──► Settings ◄──loads/saves── SettingsStore
                          │                  │                        (NVS)
                          │                  └──read by──► main.cpp ──read by──► engines
                          └──reads Telemetry ◄──filled by── main.cpp
```

`Settings` is the single piece of mutable state shared by the phone, the screen and
the light engines. Nobody else keeps a copy of "what the user chose".

---

## One frame, step by step

`loop()` in [main.cpp](../src/main.cpp) is the conductor. In order:

1. **`M5.update()` / `web.update()`** — buttons and HTTP. Cheap, non-blocking.
2. **Debounced save** — if a setting changed more than 800 ms ago, write it to NVS.
3. **Buttons** — test trigger, Wi-Fi toggle, gain cycle, save now.
4. **`analyzer.update()`** — *blocks ~32 ms* capturing one block, then fills
   `bands_`, `mag_`, `level_`, `peakHz_`.
5. **Measure `dtMs`** from `micros()` — this is what motion and fading use.
6. **Per strip**, in a loop of `kStripCount`:
   - if switched off: reset the engine, blank the strip once, skip;
   - `level = analyzer.bandLevel(centerHz, widthHz)`;
   - `engine.update(level, nowMs, dtSec, s)` — move/age blobs, spawn if due;
   - `strip.clear()` → `engine.render(strip, s)` → `strip.show()`.
7. **`web.setTelemetry(tele)`** — one struct of numbers the page can read.
8. **Draw the screen** (`Display::renderFrame`) — spectrum bars, band windows, level meters, HUD. All of it
   incremental: only what changed is repainted.
9. Every 2 s, one line to the serial port.

The strip loop in step 6 is the whole "program" in miniature:

```cpp
const float level = analyzer.bandLevel(centerHz, widthHz);  // hear
engine.update(level, nowMs, dtSec, s);                      // think
strip.clear();
engine.render(strip, s);                                    // draw
strip.show();                                               // emit
```

---

## Containers and layers

Up to **4 `LayerEngine` instances** share one `LedStrip`. The strip loop in
`main.cpp` is now:

```cpp
strip.clear();
for (size_t l = 0; l < layerCount; l++) {
  if (solo >= 0 && l != solo) continue;        // solo preview
  LayerSettings &layer = settings.layer[i][l];
  engine[l].update(levelOf(layer), beatOf(layer).onBeat, nowMs, dtSec, layer, ledCount);
  engine[l].render(strip, layer);              // amp scaled by intensityPct
}
strip.show(brightnessPct);                     // ONE gamma encode + ONE RMT frame
```

Two consequences worth remembering:

* **`ledCount` is a container property**, which is why `LayerEngine::update()` takes
  it as a parameter instead of storing it.
* **`intensityPct` is applied at render time** (the amp is multiplied before it is
  added), so lowering a layer's weight never changes its colour.

## Why the layers are not inside `LedStrip`

It is a deliberate split between *what the light should look like* and *how to get
pixels onto the wire*. Three reasons, in order of how much they matter:

**1. Gamma is not additive.** This is the real constraint. Two overlapping blobs
must be summed as *light*, and gamma must be applied once to the sum. If a blob
painted its own colour bytes, the second blob would be gamma-coded on top of an
already gamma-coded pixel, and overlaps would look wrong (they'd clamp early and
muddy instead of adding up). So the frame buffer has to be linear float, and the
gamma LUT has to live at the *output* edge — which is exactly what `LedStrip` is.
The blob engine can then just do `buf[i] += v * colour`, and it never has to know
that gamma, byte order or RMT exist.

**2. Mode 2 needs the same socket.** `LedStrip` exposes `clear()`,
`addRing(...)`, `show()`. A spectrum/VU mode would render bars into the same
buffer through the same `show()`; nothing in `strip.cpp` would change. If the
blobs lived inside `LedStrip`, adding mode 2 would mean editing the driver, or
building a second driver that duplicates the gamma and RMT work.

**3. The driver has no opinion about music.** `strip.cpp` does not include
`settings.h`, `audio_fft.h` or anything audio-related. It knows pixels, wrapping,
gamma and transmission. `blob_mode.cpp` knows thresholds, speed, fade and colour
and never touches a pin. Each is short enough to hold in your head: ~170 lines and
~110 lines.

The cost of the split is one extra indirection: `main.cpp` has to call three
methods per strip instead of one. That is the price, and it is small.

What the strip class *does* own, because it is genuinely pixel-level:

* the ring geometry (`addRing` wraps distances, always the shortest way round);
* the raised-cosine profile that gives the blob soft edges;
* the gamma LUT and the 8-bit conversion;
* the "nothing changed, don't clock the strip again" optimisation.

---

## `encode` / `decode` — three representations of one thing

`Settings` exists in three forms, and each is owned by exactly one file:

| Form | Where | Owner |
|------|-------|-------|
| `struct Settings`, in RAM | `settings.h` | `main.cpp` holds the one instance |
| 44 bytes in flash | `settings_blob.h/.cpp` | `SettingsStore` |
| JSON on the wire | `webui.cpp` (`stateJson()`, `handleParams()`) | `WebUi` |

`encodeStrip()` / `decodeStrip()` in [settings_blob.cpp](../src/settings_blob.cpp)
are just "one strip's settings ↔ 21 bytes". Nothing to do with the LED hardware.
They convert between representation 1 and representation 2:

```cpp
encodeStrip(const StripSettings &s, uint8_t *p)   // struct  → 21 bytes
decodeStrip(const uint8_t *p, StripSettings &s)   // 21 bytes → struct
```

Why not `memcpy` the struct or use `Preferences::putBytes(key, &s, sizeof s)`?

* **Padding.** A struct's in-memory layout includes alignment padding that the
  compiler may change between versions; the byte layout here is explicit
  (`put16` little-endian at fixed offsets).
* **Versioning.** Byte 0 is a version number. Add a field, bump the version, and
  an old blob is *rejected* — you get the defaults — instead of being misread as a
  different layout. Silent corruption is much worse than a reset.
* **Clamping.** `decodeSettings()` runs `clampSettings()` on whatever came out of
  flash, because flash content cannot be trusted either.

The 21-byte block is laid out as: mode, enabled, ledCount(2), center(2), width(2),
threshold, retrigger(2), blobLeds(2), speed(2), fade(2), reverse, r, g, b. The
header is 2 bytes (version, mic gain), so the whole blob is `2 + 21 × 2 = 44`.

---

## Why there are two FFT implementations

`fft_backend.h` is a three-function interface:

```cpp
bool  begin(size_t n);
void  magnitude(float *re, float *im, float *mag, size_t n);
float bandPowerNorm();
```

and two files implement it:

| File | Library | Licence | Where it comes from |
|------|---------|---------|---------------------|
| `fft_backend_espdsp.cpp` | ESP-DSP | Apache-2.0 | **ships inside the ESP32 Arduino core** — nothing to install |
| `fft_backend_arduinodft.cpp` | arduinoFFT | GPL-3.0 | extra library, portable C++ |

Only one is compiled; `-DFFT_BACKEND_…` in `platformio.ini` selects it.

**You are right that on an ESP32 the second one is redundant.** This pair was
carried over verbatim from the `LightSoundReactive` project, where it existed so
the same behaviour could be demonstrated with and without the Espressif library —
a licensing/portability insurance policy rather than a technical need. ESP-DSP is
the right choice here: it comes with the core, it is the faster of the two, and
`bandPowerNorm()` encodes the small numerical difference between the two windows
so the band levels read identically either way.

The one thing it still buys you: a sanity check. If the FFT ever looks wrong, the
other backend tells you in one build whether the problem is in the analysis code
or in ESP-DSP. If that is not worth 35 lines and one extra env, delete the
`arduinodft` env and `fft_backend_arduinofft.cpp` and nothing else changes.

---

## Where each kind of change goes

| You want to… | Touch |
|--------------|-------|
| add a parameter | `settings.h` (field + clamp + default) → `settings.cpp` (bytes, bump version) → `webui.cpp` (SPEC array + a line in `handleParams`) → SPEC.md table |
| add an effect | `LayerSettings::Effect` in `settings.h` → `LayerEngine::triggerEffect()` + `render()` in `layer.cpp` → pad list `PADEFFS` in `webui.cpp` |
| add a trigger input | `LayerSettings::Mode` → the `shouldTrigger` block at the end of `LayerEngine::update()` → mode buttons in `webui.cpp` |
| change beat behaviour | `beat.cpp` (threshold, refractory, tempo range) or `SpectrumAnalyzer::bandFlux()` for the onset function |
| move a strip to another pin | `kStripPins` in `config.h` |
| change the LED ceiling | `kMaxLedsPerStrip` in `config.h` (buffers are sized from it) |
| change the look of an effect | `addRing()` / `addFill()` / `addSparkles()` in `strip.cpp` (shape) or `update()` in `blob_mode.cpp` (motion/life) |
| change the LCD layout | `display.cpp` (`computeLayout()`, `draw*`) |
| several light layers per strip | already implemented — keep the invariant *one `show()` per physical strip* |

---

## Timing budget

Measured frame is ~32 ms from the microphone plus:

* ~3 ms per strip to clock out 100 pixels over RMT (blocking);
* ~6 ms per strip only when the LED count changes (`updateLength` reallocates);
* display work, kept low by repainting only changed pixels — the meters are even
  quantised to 2 % steps so most frames touch nothing.

That lands around 20–25 fps. It is deliberately not a 60 fps renderer: the audio
cannot give you a new frame faster than 32 ms anyway, and smooth motion comes from
`dtMs`, not from the frame rate.
