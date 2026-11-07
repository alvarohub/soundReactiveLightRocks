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
2. **Mode 1 — Sound-Triggered Blobs** ([src/blob_mode.h](../src/blob_mode.h), [src/blob_mode.cpp](../src/blob_mode.cpp)):
   - Pool of up to 12 concurrent blobs per strip (dimmest recycled when full).
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
4. **On-Device LCD Display** ([src/main.cpp](../src/main.cpp)):
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
     3. **Triggering conditions** (live level bar with **directly draggable threshold marker**, `Band centre`, `Band width`)
     4. **Display parameters** (**real-time HTML5 Canvas Colour Wheel** + colour swatch square, `Max brightness`)
     5. **Animation parameters** (`Motion`, `Direction`, `Blob length`, `Speed`, `Fade time`, `Retrigger gap` + `Retrig ON / Single`)
     6. **Strip calibration** (`LEDs on this strip 1..1000`, `-10 / ← / → / +10` step buttons, `Calibrate` toggle)
   - **Auto-save + 3 NVS Preset Banks (`Save 1..3` / `Load 1..3`) + Factory Reset** (blob format `kSettingsBlobVersion = 3`, 24 bytes/strip).

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

### Priority 1 — Code Cleanup & Modularisation (Explicitly Requested by User)
The user asked to clean up the codebase structure now that Mode 1 features are
working:
1. **Extract Display logic out of `src/main.cpp` into `src/display.h` & `src/display.cpp`**:
   - Move all screen layout variables, colour helpers (`rgb565`, `barColor`,
     `stripColor`), `freqToX`, `drawSpectrum`, `drawStripChrome`, `drawMeters`,
     `drawHud`, `drawHint`, `showBanner`, `finishBanner`, and `computeLayout` out
     of the anonymous namespace at the top of `src/main.cpp` into a clean
     `Display` class or module.
2. **Consolidate Settings & NVS Storage files**:
   - Currently settings are split across three pairs of files: `src/settings.h`,
     `src/settings_blob.h/.cpp`, and `src/store.h/.cpp`.
   - Merge `settings_blob` and `store` into a single, easy-to-follow
     `src/settings.h` + `src/settings.cpp` (or `src/storage.h/.cpp`) so there is no
     confusion between `StripSettings` serialization and `LedStrip` hardware.
3. **Keep `src/main.cpp` short and clean**:
   - `src/main.cpp` should only contain high-level orchestration in `setup()` and
     `loop()`.

### Priority 2 — Mode 2 Design & Implementation
- `StripSettings::mode` is already persisted in NVS and sent in JSON (`kBlob = 0`).
- When the user specifies Mode 2's visual behaviour, implement it as a peer engine
  rendering into `LedStrip`'s linear buffer (`buf_`).

### Priority 3 — Future Architecture Explorations (Documented in `docs/JOURNAL.md`)
- **Multi-controller phone app / UDP or BLE control**: having multiple M5Sticks act
  as lightweight UDP/OSC or BLE clients controlled simultaneously from one phone UI.
- **Direct audio / spectrum streaming over Wi-Fi UDP**: streaming FFT bands or audio
  from the music source over UDP so the lights react only to the music track and
  ignore ambient party noise (note: ESP32-S3 only has BLE 5.0, not Classic BT A2DP,
  so Wi-Fi UDP is the natural path on M5StickS3).
