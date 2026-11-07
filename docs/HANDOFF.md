# AI & Developer Handoff Guide

> **Read this first** before modifying code. It summarises what has been built and
> tested on hardware, how the multi-board environments work, **critical stability
> invariants that must not be broken**, and the exact next tasks on the roadmap.

Related documents:
- [docs/SPEC.md](./SPEC.md) — Functional specification & parameter tables
- [docs/ARCHITECTURE.md](./ARCHITECTURE.md) — Signal chain, frame loop, & design rationale
- [docs/JOURNAL.md](./JOURNAL.md) — Chronological engineering log & future exploration notes

---

## 1. Current Project Status (Working & Hardware-Verified)

The firmware drives **two independent NeoPixel (WS2812B / SK6812) strips** (up to
**1,000 LEDs per strip**) reacting to room audio via the built-in microphone, tuned
in real time from a mobile Web GUI served over the microcontroller's Wi-Fi SoftAP.

### Supported Hardware Platforms (`platformio.ini`)

Both boards are configured in [platformio.ini](../platformio.ini) with explicit
per-environment build flags for the Grove pins and Wi-Fi SSID/password:

| Environment | Board / MCU | Strip 1 Pin | Strip 2 Pin | Built-in Mic | Default SSID |
|---|---|---|---|---|---|
| `[env:m5sticks3]` *(default)* | **M5StickS3** (`ESP32-S3-PICO-1-N8R8`, custom board JSON in `boards/m5sticks3.json`) | **G9** (`-DSTRIP1_PIN=9`) | **G10** (`-DSTRIP2_PIN=10`) | ES8311 codec + MEMS mic (`M5.Mic`) | `LightRocks-1` |
| `[env:m5stick-c-plus2]` | **M5StickC Plus2** (`ESP32-PICO-V3-02`, custom board JSON in `boards/m5stick-c-plus2.json`) | **G32** (`-DSTRIP1_PIN=32`) | **G33** (`-DSTRIP2_PIN=33`) | SPM1423 PDM mic (`M5.Mic`) | `LightRocks-3` |

*(Note: M5AtomS3 was considered and dropped because the standard AtomS3 has no
built-in microphone.)*

### What is Implemented and Verified

1. **Audio & FFT Pipeline** ([src/audio_fft.h](../src/audio_fft.h), [src/audio_fft.cpp](../src/audio_fft.cpp), [src/fft_backend_espdsp.cpp](../src/fft_backend_espdsp.cpp)):
   - 512-sample frames at 16 kHz (~32 ms/frame, ~31 fps), Hann window, hardware-accelerated ESP-DSP real FFT (`-DFFT_BACKEND_ESP_DSP`).
   - Computes 30 log-spaced display bars (`65 Hz .. 7600 Hz`) plus exact energy inside each strip's `centerHz ± widthHz/2` window (`bandLevel()`).
2. **Per-strip trigger inputs + four effects** ([src/blob_mode.h](../src/blob_mode.h), [src/blob_mode.cpp](../src/blob_mode.cpp), [src/beat.h](../src/beat.h), [src/beat.cpp](../src/beat.cpp)):
   - **Trigger input (`mode`, independent per strip)**: `0` = band threshold, `1` = auto beat (own `BeatDetector` per strip), `2` = manual pads only.
   - **Effects (`effect`)**: `0` Jet (travelling blob), `1` Flash (whole strip, hold-sustainable), `2` Burst (centre-outward shockwave), `3` Sparkle (glitter shower).
   - **Beat detection**: `SpectrumAnalyzer::bandFlux(centerHz, widthHz)` drives a per-strip `BeatDetector` (spectral-flux onset, adaptive `mean + k·σ` threshold from `beatSensitivityPct`, 180 ms refractory, median-IOI tempo folded to 60–170 BPM).
   - **Pad hold**: `setPadHeld()` sustains Flash/Sparkle and re-fires Jet at the retrigger gap; a 2.5 s keepalive timeout releases a pad if the browser disconnects.
   - **Motion (`loop`)**:
     - `Loop (ring)` (`loop = 1`): wraps seamlessly around `0 ↔ ledCount - 1` (or bounces continuously in Ping-pong).
     - `One-shot` (`loop = 0`): disappears once the blob's trailing edge exits the strip.
   - **Direction (`reverse`)**:
     - `Forward (0)`: spawns at `0`, moves `0 → ledCount - 1`.
     - `Reverse (1)`: spawns at `ledCount - 1`, moves `ledCount - 1 → 0`.
     - `Ping-pong (2)`: spawns at `0`, bounces at `ledCount - 1` back toward `0`.
   - **Retrigger (`retrigger` + `retriggerMs`)**:
     - `Retrig ON (1)`: spawns a new blob whenever `level >= thresholdPct` and `retriggerMs` has elapsed.
     - `Single (0)`: blocks any new blob on that strip until all active blobs have finished (`activeCount() == 0`).
3. **NeoPixel RMT Driver & Calibration Dot** ([src/strip.h](../src/strip.h), [src/strip.cpp](../src/strip.cpp)):
   - Static buffers for up to `kMaxLedsPerStrip = 1000` LEDs per strip (~30 KB total RAM).
   - Raised-cosine spatial blob profile with gamma (`γ = 2.2`) applied to the blob brightness envelope (preserving RGB hue ratios during fade-out) and per-strip `brightnessPct` (`1..100%`) scaling.
   - **LED Count Calibration mode**: adjusting `ledCount` (or toggling `Calibrate`) blanks the strip and lights only LED `ledCount - 1` so the physical end of the strip can be found visually. Automatically exits after 3.5 s or as soon as any other control is touched.
4. **On-Device LCD Display** ([src/display.h](../src/display.h), [src/display.cpp](../src/display.cpp)):
   - All screen layout, colour helpers, spectrum bars, band rectangles, level meters, HUD and boot banner now live in a `Display` class; [src/main.cpp](../src/main.cpp) only orchestrates.
   - Per-strip meter labels show the trigger input: `S1` (threshold), `B1` (auto beat), `M1` (manual pads).
   - Real-time 30-band colour spectrogram.
   - Overlaid **empty coloured rectangle** on the spectrogram showing each strip's `[loHz, hiHz]` band window, with a **downward-pointing triangle on the top edge** marking `centerHz`.
   - Per-strip level meters (`S1`, `S2`) with white threshold ticks.
   - Bottom lines showing audio telemetry (`lvl`, `dBFS`, `peakHz`, `+gainDb`) and the **persistent module SSID + IP address** (`LightRocks-1   192.168.4.1`).
5. **Mobile Web GUI & NVS Presets** ([src/webui.h](../src/webui.h), [src/webui.cpp](../src/webui.cpp), [src/store.h](../src/store.h), [src/store.cpp](../src/store.cpp), [src/settings_blob.cpp](../src/settings_blob.cpp)):
   - Served at `http://192.168.4.1/`.
   - 5/6 viewport width with a 1/6 right-hand thumb-scroll gutter (`padding-right: 16.7vw`).
   - Organized into 6 logical cards:
     1. **Microphone (global)** (gain + broadband stats)
     2. **Select strip to edit** (`Strip 1` / `Strip 2` tabs + title + Power switch)
     3. **Triggering conditions** (`Trigger input` = Threshold / Auto Beat / Manual pads, live level bar with **directly draggable threshold marker** in threshold mode, beat pulse + BPM + `Beat sensitivity` in beat mode, `Band centre`, `Band width`)
     4. **Display parameters** (**real-time HTML5 Canvas Colour Wheel** + colour swatch square, `Max brightness`)
     5. **Animation parameters** (`Triggered effect` = Jet / Flash / Burst / Sparkle, `Motion`, `Direction`, `Blob/Jet length`, `Speed`, `Fade time`, `Retrigger gap` + `Retrig ON / Single`)
     6. **Strip calibration** (`LEDs on this strip 1..1000`, `-10 / ← / → / +10` step buttons, `Calibrate` toggle)
   - **Full-Screen Effect Pads** (`/api/tap?strip=<i>&effect=<0..3>&down=<0|1>`): an overlay split into one colour-coded group per strip, four multi-touch pads each, press-and-hold support.
   - **Auto-save + 3 NVS Preset Banks (`Save 1..3` / `Load 1..3`) + Factory Reset** (blob format `kSettingsBlobVersion = 4`, 26 bytes/strip, with automatic v3 migration).

---

## 2. CRITICAL Stability Invariants (DO NOT REGRESS)

Several subtle hardware/RTOS bugs were diagnosed and fixed on the M5StickS3 and
M5StickC Plus2. **Do not undo these patterns:**

1. **DO NOT call `Adafruit_NeoPixel::show()` on ESP32 / ESP32-S3**:
   - In Arduino-ESP32 (IDF 4.4), `Adafruit_NeoPixel`'s `espShow()` calls
     `rmt_config()`, `rmt_driver_install()`, and `rmt_driver_uninstall()` on
     **every single frame**, allocating/freeing FreeRTOS ISRs and semaphores 60
     times/sec and thrashing RMT channel 0 between the two pins. Under Wi-Fi load,
     this crashes/resets the board.
   - Instead, `LedStrip` in [src/strip.cpp](../src/strip.cpp) binds two permanent
     ESP-IDF RMT channels (`RMT_CHANNEL_0` for Strip 1, `RMT_CHANNEL_1` for
     Strip 2) **once** in `begin()` and clocks `grb_` out via `rmt_write_sample()`
     with zero heap allocations.
2. **Never send two RMT frames back-to-back on the same strip without a reset pause**:
   - WS2812B requires `>80 µs` of low line to latch a frame. `LedStrip::setCount()`
     must **not** call `rmt_write_sample()` directly; instead it updates
     `maxSentCount_` so the single `show()` or `showCalibrationDot()` call per
     frame blanks any old tail LEDs in one RMT transfer.
3. **Keep HTTP parameter updates Single-In-Flight (`INFLIGHT`) with `204 No Content`**:
   - The ESP32 `WebServer` is single-threaded. In [src/webui.cpp](../src/webui.cpp),
     the browser JS serialises `/api/params` requests so at most **one** request is
     in flight at a time, pauses `/api/state` polling while dragging, and
     `handleParams()` returns an empty `204 No Content` (zero `String` heap
     allocation).
4. **Keep `cfg.internal_spk = false` and `cfg.external_display_value = 0` in `setup()`**:
   - On M5StickS3, disabling the speaker keeps the ES8311 I2S bus dedicated to the
     microphone, and disabling external I2C display/IMU/RTC probing prevents
     `M5.begin()` from toggling the Grove pins (`G9`/`G10`).

---

## 3. Next Tasks on the Roadmap (Priority Order)

### Priority 1 — Code Cleanup & Modularisation ✅ DONE (2026-11-07)
Completed:
1. **Display extracted** into [src/display.h](../src/display.h) / [src/display.cpp](../src/display.cpp) as a `Display` class (layout, spectrum, band rectangles, meters, HUD, banner).
2. **Settings + storage consolidated** into [src/settings.h](../src/settings.h) / [src/settings.cpp](../src/settings.cpp). `settings_blob.*` and `store.*` are gone; `SettingsStore` (NVS), the versioned blob encoder/decoder and the v3→v4 migration live there.
3. **`src/main.cpp` is now orchestration only** (~250 lines): `setup()`, `loop()`, `renderStrip()`, `handleButtons()`.

### Priority 2 — Multi-layer strips ("virtual strips" inside one physical strip)
The user's insight (2026-11-07): **the object that glows is not the physical strip.**
A physical strip is a *container*; several independent light objects ("layers" /
"virtual strips") should live inside it, each with its own frequency band, trigger
input, colour, speed, and intensity weight. They all accumulate into the same
linear frame buffer, so overlapping beams blend additively and gamma is applied
once at the output edge — the exact reason `LedStrip::buf_` is linear float.

Planned shape:
- `PhysicalStrip` (was `LedStrip`): GPIO/RMT channel, `ledCount`, linear `buf_`, one `show()`.
- `LightLayer`: its own `StripSettings`-like config + `BlobEngine` + `BeatDetector`, with an `intensityPct` weight.
- `PhysicalStrip::render(layers, n)`: loop layers → `engine.render(strip)` → single `show()`.
- Persistence: per-physical-strip layer count + a fixed `kMaxLayersPerStrip` array in the blob (bump to v5).
- GUI: the strip tab gains an "Layers" section — add/remove up to N layers, each with its own sub-panel.

### Priority 3 — Future Architecture Explorations (Documented in `docs/JOURNAL.md`)
- **Multi-controller phone app / UDP or BLE control**: having multiple M5Sticks act
  as lightweight UDP/OSC or BLE clients controlled simultaneously from one phone UI.
- **Direct audio / spectrum streaming over Wi-Fi UDP**: streaming FFT bands or audio
  from the music source over UDP so the lights react only to the music track and
  ignore ambient party noise (note: ESP32-S3 only has BLE 5.0, not Classic BT A2DP,
  so Wi-Fi UDP is the natural path on M5StickS3).
