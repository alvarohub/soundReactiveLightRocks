# AI & Developer Handoff Guide

> **Read this first** before modifying code. It summarises what has been built and
> tested on hardware, how the multi-board environments work, **critical stability
> invariants that must not be broken**, and the exact next tasks on the roadmap.

Related documents:
- [docs/SPEC.md](./SPEC.md) — Functional specification & parameter tables
- [docs/IDEAS.md](./IDEAS.md) — running list of ideas, libraries and results worth keeping
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
2. **Physical strips as containers of light layers** ([src/layer.h](../src/layer.h), [src/layer.cpp](../src/layer.cpp), [src/beat.h](../src/beat.h), [src/beat.cpp](../src/beat.cpp)):
   - **`PhysicalStripSettings`** (per pin): `ledCount`, `brightnessPct`, `layerCount` (1..4).
   - **`LayerSettings`** (up to 4 per strip): mode, effect, enabled, intensity, band, threshold, beat sensitivity, retrigger, motion, colour.
   - Every layer accumulates into its strip's **linear** buffer; the strip applies gamma and transmits **once** per frame.
   - Per-layer `BeatDetector`, so layers in the same strip can lock to different rhythms.
   - **Solo** (runtime-only) previews a single layer.
   - **Trigger inputs (`triggers`, a bitmask, independent per layer)**: threshold 1, beat 2, tap pads 4, accelerometer 8, gyroscope 16. They are **not exclusive** — a layer fires on whichever armed source happens first. Each layer owns its own `BeatDetector`.
   - **Motion** ([src/motion.h](../src/motion.h), [src/motion.cpp](../src/motion.cpp)): reads the on-board IMU once per frame; `swing` is |a| with gravity removed, `twist` is |ω|.
   - **Effects (`effect`)**: `0` Jet (travelling blob), `1` Flash (whole strip, hold-sustainable), `2` Burst (centre-outward shockwave), `3` Sparkle (glitter shower), `4` Whip (speed from swing strength, colour from swing direction), `5` Glow (slow breathing that needs no trigger; a beat re-phases the wave).
   - **Beat detection**: `SpectrumAnalyzer::bandFlux(centerHz, widthHz)` drives a per-strip `BeatDetector` (spectral-flux onset, adaptive `mean + k·σ` threshold from `beatSensitivityPct`, 180 ms refractory, median-IOI tempo folded to 60–170 BPM).
   - **Pad hold**: `setPadHeld()` sustains Flash/Sparkle and re-fires Jet at the retrigger gap; a 2.5 s keepalive timeout releases a pad if the browser disconnects.
   - **Intensity**: each layer's `intensityPct` scales its contribution in `LayerEngine::render()`.
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
   - One meter per **physical** strip (merged output), labelled `S1`/`B1`/`M1` from the dominant layer's trigger input, plus `.N` when the strip holds N layers. Per-layer detail lives on the phone.
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
     3. **Trigger inputs** (five tickable chips: Threshold / Auto beat / Tap pads / Accelerometer / Gyroscope; live level bar with **directly draggable threshold marker**, beat pulse + BPM + `Beat sensitivity`, `Motion threshold`, `Band centre`, `Band width`)
     4. **Display** (**real-time HTML5 Canvas Colour Wheel** + swatch, `Layer intensity`, `Strip max brightness`)
     5. **Effect & motion** (`Triggered effect` = Jet / Flash / Burst / Sparkle / Whip, `Topology`, `Direction`, `Blob/jet length`, `Speed`, `Fade time`, `Retrigger gap` + `Retrig ON / Single`)
     6. **Strip calibration** (`LEDs on this strip 1..1000`, `-10 / ← / → / +10` step buttons, `Calibrate` toggle)
   - **Full-Screen Effect Pads** (`/api/tap?strip=<p>&layer=<l>&effect=<0..4>&down=<0|1>`): one colour-coded group **per layer**, four multi-touch pads each, press-and-hold support.
   - **Strip/Layer selection**: `Strip 1` / `Strip 2` tabs, then `Layer 1..4` tabs, plus `+ Add layer`, `Remove` and `Solo`.
   - **Auto-save + 3 NVS Preset Banks (`Save 1..3` / `Load 1..3`) + Factory Reset** (blob format `kSettingsBlobVersion = 6`, 210 bytes, with automatic v5/v4/v3 migration).
   - **Hardware buttons**: BtnA selects the next physical strip (marked `>` on the LCD), BtnB cycles the trigger set of that strip's layers, hold A toggles Wi-Fi, hold B saves.

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

### Priority 2 — Multi-layer strips ✅ DONE (2026-11-07)
Implemented exactly as planned:
- `PhysicalStripSettings` (container: `ledCount`, `brightnessPct`, `layerCount`) and
  `LayerSettings` (the glowing object) are separate types in [src/settings.h](../src/settings.h).
- `LayerEngine` ([src/layer.h](../src/layer.h), [src/layer.cpp](../src/layer.cpp)) replaced `BlobEngine`; it renders one layer and is scaled by `intensityPct`.
- `main.cpp` does `clear() → for each live layer (respecting solo) update+render → show()`.
- Up to **4 layers** per physical strip; blob v5 with v4/v3 migration.
- GUI: nested Layer tabs, add/remove, solo; pad overlay has one group per layer.

### The beat detector is a tracker, and it has an offline test (2026-11-08)
`beat.cpp` is Arduino-free on purpose, and `test/beat_test.cpp` drives it with
synthetic onsets. Run it after any change:

```
g++ -std=c++17 -O2 test/beat_test.cpp src/beat.cpp -o /tmp/beat_test && /tmp/beat_test
```

It fails (exit 1) on: a tempo more than 4 BPM off, a case that no longer locks,
beats leaking after the music stops, or beat timing worse than two frames. Cases
covered: silence, 60/90/120/128/150 BPM, a 36 dB quieter signal, noise, a masked
beat, weak/missing off-beats, swing, a tempo change, stopping, timing.

Invariants worth keeping:
* the onset function is **normalised by its own running average and log-compressed**,
  so nothing downstream may depend on the absolute flux or the mic gain;
* **an onset must be a rise, not just a local maximum** — that is what noise fails;
* the tempo histogram votes for *exact* halves and thirds only, and accented onsets
  also vote from the previous accent, which is what defeats the octave error;
* **the lock is a grid-fit test, not a streak**: half of a swing sits off the beat,
  so a streak can never lock it, and a streak on noise can lock falsely;
* while locked, the period is measured from onset times over ~8 beats; letting the
  10 ms histogram also write the period makes the two fight and the BPM jitter.

### The state JSON is built one key per call, and format-checked (2026-11-08)
`stateJson()` used to be a couple of long `appendf` calls with 28 positional
arguments. When `softnessPct` was added to the format string the matching argument
was forgotten, so **every field from there on reported its neighbour's value**, the
colour was built from telemetry (so it changed several times a second), and
`thresholdPct` reported `beatSensitivityPct` - which made the threshold control
spring back to the wrong value and look stuck. Nothing warned: the variadic
function had no format attribute.

Two permanent guards:
* `appendf` is declared `__attribute__((format(printf, 4, 5)))` and both envs build
  with `-Wformat -Wformat-security`. Removing that argument again produces
  `format '%u' expects a matching 'unsigned int' argument`.
* every field is emitted as `kvu("key", value)` / `kvd("key", value)` - one key and
  its value on the same line, so a shift is impossible and a swap is visible.

**Lesson:** a hand-written mock hides emitter bugs. The audit that "passed" was
comparing the page against JSON I typed by hand, not against what the firmware
actually formats. The mock is now driven by sentinel values per key (`thresholdPct`
= 42, `softnessPct` = 7, ...), so a page reading the wrong key shows a mismatch.

### Adding a layer must not change the light (2026-11-08)
A new layer used to be a *copy* of the one being edited (rotated colour, 60 %
weight). That made the strip visibly different before the user had asked for
anything, and when the state refresh was missed the page could show one colour
while the strip drew another. `defaultLayerFor(slot)` now gives a **fresh slot**:
`enabled = 0`, plain threshold-driven jet, a palette colour per slot and its own
band (`150 Hz + 250 Hz x slot`). A removed slot is reset the same way, so a stale
layer can never sit enabled in a dead slot.

### The LCD shows one strip at a time, one row per layer (2026-11-08)
The old screen drew a band rectangle and a merged meter for *both* strips, which
said nothing about the layers inside. Now `BtnB` selects a strip and the screen
switches to it: header (`S<n> <n>L` + `out <n>%`), the shared spectrum, then one
row per layer. The trick that makes it readable is that the spectrum and the rows
**share one x axis** (`axisX0_..axisX1_`, with a gutter on the left for the
trigger letter), so a row covers exactly the bars of the band it listens to.

Rows repaint incrementally like the bars: geometry/colour changes redraw the row,
a level change repaints only the delta (and repaints the track, not black, when
the level falls), and the trigger dot only when it flips.

### Presets: slots you can read, destructive actions that ask twice (2026-11-10)
Six big `Save n` / `Load n` buttons were unreadable: nothing said whether a slot
held anything, a click on Save gave no confirmation, and an empty slot's Load looked
enabled because it carried the green "alt" style.

The pattern now (the usual one for destructive hardware/phone actions):
* **a slot chip per slot**, showing what the slot actually holds - the colours of
  its layers and the layer count - so "used or free" needs no guessing;
* tap a chip to **select**, then one of two verbs: `Recall` or `Save here`;
* `Recall` on an empty slot is **disabled**, not merely dimmed;
* Save over a used slot and Recall both **ask twice** ("Tap again: overwrite"),
  arming for 4 s and then reverting;
* every outcome is **reported**: `Saved to slot 1`, `Recalled slot 2`, `Slot 3 is
  empty`, or `the board refused that request`.

Firmware side: `SettingsStore::presetSummary()` decodes a slot just enough to
describe it (layer count + up to four layer colours), `WebUi::refreshPresets()` is
called at boot and after every preset/reset action, and `/api/state` carries
`presets:[{has,layers,cols}]`. A slot saved while the page is open now updates
immediately instead of only after a reboot.

**Also fixed:** `post()` used to treat any response as success, so the toast always
said "saved". It now returns null on a non-2xx or a failed fetch, and only then
does the caller report anything.

**Gotcha, again:** the slot chips are rebuilt only when their signature changes.
Rebuilding the `innerHTML` on every state read detaches the node under the user's
finger and the tap is lost - the same failure as the layer tabs.

### Writes are queued AND reconciled; the page never assumes one landed (2026-11-10)
Dropping the lost-socket wedge was not enough. A write could still be lost and the
next state read would quietly put the board's old value back — the threshold marker
was the visible casualty ("cannot move the threshold, other things work, reload
fixes it"). Fire-and-forget is simply the wrong model for a link that loses writes.

* every write is recorded in `DES` (scope = strip + layer + key) *and* queued in `Q`
* one request in flight, with an `AbortController` deadline; failures back off
  exponentially (150 ms → 2 s, 10 tries ≈ 10 s) so an unreachable board is retried
  over seconds instead of burning every attempt in a burst
* every `/api/state` read is truth: `reconcile()` deletes a desire the board already
  satisfies and re-queues one it does not
* `fillInputs()` **skips any control whose write is still in flight**, so a stale
  read can never overwrite what was just set
* `INFLIGHT` has a 3.5 s watchdog, and the drag flags (`WDRAG`/`TDRAG`/`DRAGGING`)
  are cleared by a window-level `pointerup`/`pointercancel`/`blur` plus a 4 s
  watchdog. A latched drag used to stop polling until a reload.

**Test it with a mock that drops writes** (`/__drop?rate=0.5`): interleave threshold
drags, sliders and chips, then check page == board. A mock that answers every
request hides exactly this class of bug. Also make the mock HTTP/1.0: a keep-alive
mock leaves the browser waiting and looks like a firmware stall.

### The page must survive a dropped socket (2026-11-08)
The board's web server serves **one socket at a time**, but the page could open
two: `poll()` only checked `INFLIGHT` and `flush()` only checked `INFLIGHT`, so a
600 ms state read could overlap a slider POST. When the board dropped the second
socket, that `fetch` never settled — there was no timeout — so `INFLIGHT` stayed
true and `flush()` returned early *forever*. Nothing was sent again until the page
was reloaded, while the LEDs kept running: "sometimes I cannot change a value".

Rules now:
* **One socket.** `busySocket()` = `INFLIGHT || POLL_BUSY`; `flush()` waits for a
  poll to finish, `poll()` waits for a POST. Both flags are cleared on every exit
  path, including a rejected fetch.
* **Every request has a deadline** (`abortAfter`, 2.5 s for state/params, 4 s for
  presets). A hung request is abandoned, not waited on.
* **A failed POST is retried once** with the same value, so a swallowed edit is not
  silently lost.

### The GUI mirrors the board; it does not remember what it last sent (2026-11-08)
`apply()` now refills every control whenever the page is idle (`!editing()` and
~900 ms since the last user action), not only when the layer structure changes.
That makes drift self-healing within one poll: a dropped POST, a change made from
the other phone, AGC moving the gain, or the board's own buttons all end up on
screen instead of being resurfaced on the next unrelated edit.

`editing()` = wheel drag || threshold drag || a range input is being dragged. The
drag flags are cleared by **window-level** `pointerup`/`pointercancel`/`blur`, not
by the element's own handler — a finger that leaves the bar used to latch the flag
and silently stop polling until reload.

### Readouts must not reflow (2026-11-08)
`lvl 7%` → `lvl 100% · 174 BPM · beat 100% · 12 active` used to wrap the row label
and shove the meter and every card below it up and down several times a second.
Live lines (`#mt0`, `.ptitle small`, the pad group headers) are `nowrap` with
`text-overflow:ellipsis`, so the height is constant. Verified by measuring the row
height and the meter's `top` at four different text lengths — identical.

### The layer tab row is derived from state, never from button handlers (2026-11-08)
`buildLayerTabs()` used to be called only from `fillInputs()`, which `apply()` ran
once (a `SEEDED` guard) and the add/remove buttons ran by hand. But `/api/state` is
polled every 600 ms, so anything that changes `layerCount` outside those two buttons
— a preset load, a factory reset, the other phone, the board — left the tabs
describing a structure that no longer existed. The count text said `1 of 4` with no
tab to press.

`apply()` now compares `structSig(s)` (the per-strip layer counts) and refills the
inputs whenever it changes, and `buildLayerTabs()` writes the count label itself, so
the two can never disagree.

**Rule:** anything that draws a *structure* (tabs, pad groups) must be rebuilt from
`apply()`, not from the handler that happened to cause the change. Handlers are not
the only source of state.

### Layer creation and deletion are explicit board actions (2026-11-08)
`layerCount` arithmetic from the page could not say *which* slot to remove, so
"Remove" always truncated the last layer while the user was looking at another one.
The page now sends `layerAdd=<source>` / `layerRemove=<index>`, and the board
duplicates or compacts accordingly (`WebUi::addLayer` / `removeLayer`). `solo_` and
`edit_` are re-based because they are indices.

### Legacy blob checks must use the OLD header size (2026-11-08)
Every migration path compared `len` against `kSettingsHeaderSize`, which grew from 2
to 3 when `agc` was added. That made every legacy path one byte too long, so old
blobs silently failed to decode and the firmware fell back to defaults — the user's
four saved layers became one, uneditable. The legacy branches now use `kHeaderV8`
(2) plus the historical block sizes (`kPhysBlockV8`, `kLayerBlockV7`,
`kLayerBlockV5`).

**Rule:** never use a `k…` size constant from the *current* layout inside a decoder
for an *older* version. Keep explicit legacy constants, and exercise each path with a
synthetic blob (there is a host test for v6/v7/v8/v9 in the journal entry).

### Softness is a layer property, not a strip one (2026-11-08)
The strip-level `bleed()` (a `[1,2,1]` convolution over the merged frame) is gone. It
could not know whose edge it was softening, and on an already-smooth blob it was
barely visible. `LayerSettings::softnessPct` widens that layer's profile
(`halfWidth × (1 + softness)`), which is visible and is what "blur the borders of the
blob" actually means.

### Audio capture runs in its own task (2026-11-08)
`M5.Mic` is I2S/PDM with DMA and a library-side capture task, but `record()` blocks
until a block is ready. Calling it from `loop()` therefore pinned the whole firmware
to ~31 fps. `AudioTask` now owns that path and publishes an `AudioSnapshot`; the
render loop runs at up to ~100 fps and never touches the FFT.

Rules:
* only the audio task may touch `SpectrumAnalyzer`;
* onset detection must run on a **new snapshot only** (guarded by `seq`), or the same
  flux sample would be evaluated several times per block;
* animation timing uses the **render** clock, not the audio clock.

### Ghost spacing must scale with the blob (2026-11-08)
The first motion-blur implementation used a fixed 0.6-LED ghost step, which put each
ghost on a different LED for a 1-LED blob and produced a dotted line. The step is now
`halfWidth / 2`, so neighbouring profiles always overlap. Keep it relative.

### Fade is perceptual, gamma stays at the output (2026-11-08)
`Blob::life` ages linearly and `amp = life^(1/2.2)`. Do not go back to decaying the
amplitude directly: with gamma at the output that displays as `amp^2.2` (fast drop,
long dim tail) and `fadeMs` stops meaning what it says.

### Glow is a trigger, not an effect (2026-11-08)
There is no `kEffectGlow`. `kTrigLfo` is a free-running metronome, and `Flash` has
`attackMs`/`releaseMs`: long ones give the old glow look, short ones give a strobe.
Blob v8 migrates a saved `effect == 5` to `Flash + LFO` with matching timings.

### Fast blobs must be swept, not sampled (2026-11-08)
At ~31 fps a blob at 500 LED/s moves ~16 LEDs per frame, so a small blob appeared to
teleport. `LayerEngine::drawBlob()` now draws up to 12 ghosts along the path covered
since `prevPos`, each with `amp / ghosts`. Keep it that way: a single sample per frame
aliases badly at high speed.

Also note `speed` is deliberately **absolute** (LEDs/s) and does not scale with
`blobLeds` — see SPEC §4.2b before "fixing" that.

### The trigger chips must stay in static markup (2026-11-08)
The five trigger chips were generated from JavaScript. When the user reported "I
don't see the auto-beat trigger", the simplest explanation was that any earlier
error left the row empty — indistinguishable from a missing trigger. They are now
plain markup in `kPage`, and the JS only wires handlers and toggles classes.

The header also prints `kFirmwareVersion` (from `config.h`) and `/api/state` carries
it as `"ver"`, so "did the upload take?" is answerable at a glance. Bump that string
whenever behaviour changes.

### Beat detection in silence (fixed 2026-11-07)
The port had lowered the absolute flux floor to `0.008` (the sister project used
`0.02`) and nothing gated on actual level, so once the adaptive threshold decayed
towards zero in a quiet room, room noise registered as beats.

Two guards now:
1. `kMinStrength` restored to `0.02`.
2. A **silence gate**: `BeatDetector::update()` takes the band level and refuses to
   fire below `kSilenceLevel = 0.06`, also clearing the tempo lock at once.

`SpectrumAnalyzer::bandFlux()` is no longer multiplied by the mic gain, so setting a
negative gain (loud rooms) does not also deafen the beat detector.

### Colour blending (fixed 2026-11-07)
Two real defects were found and fixed in `LedStrip`:
1. **Gamma was applied per effect before accumulation**, so a 50 %-amplitude blob
   contributed 22 % of the light and overlaps did not sum like light.
2. **The layer weight went through the same exponent**, so the intensity slider
   could not be used to balance layers by eye.

Now layers accumulate **linear** light and `show()` applies gamma **once** to the
merged maximum, scaling the whole vector so hue is preserved. `intensityPct` is
compensated by `1/γ` in `LayerEngine::render()` so 50 % means half the visible
brightness. A single blob looks identical to before (verified numerically);
overlapping same-colour layers are now markedly brighter instead of being divided
back down.

**Invariant to keep:** never apply gamma in `addRing` / `addFill` / `addSparkles`.

### Remaining work after this
- **On-hardware verification** is still the top item: beat lock on real music, hold
  feel, and whether 4 layers on one strip at 100 % brightness is too much current
  (see the 5 V supply note in SPEC §2).

### Priority 3 — Future Architecture Explorations (Documented in `docs/JOURNAL.md`)
- **Multi-controller phone app / UDP or BLE control**: having multiple M5Sticks act
  as lightweight UDP/OSC or BLE clients controlled simultaneously from one phone UI.
- **Direct audio / spectrum streaming over Wi-Fi UDP**: streaming FFT bands or audio
  from the music source over UDP so the lights react only to the music track and
  ignore ambient party noise (note: ESP32-S3 only has BLE 5.0, not Classic BT A2DP,
  so Wi-Fi UDP is the natural path on M5StickS3).
