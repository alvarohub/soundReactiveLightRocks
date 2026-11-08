# Work journal

Chronological log of what was done, why, and what is still open. The
[specification](./SPEC.md) says what the firmware should do; this file says how it
got there.

---

## 2026-11-03 — Project start, mode 1 working

### Context

Requested: a sound-reactive NeoPixel strip setup for two strips embedded in
plastic rocks. Reuse whatever was useful from the earlier
`Katya_Kim/LightSoundReactive` project, but note the hardware difference: a
NeoPixel string is a single-wire, per-LED colour bus, not three PWM channels.

Board was initially stated as "M5StickS3" and corrected to **M5StickC Plus2** —
which is the better fit anyway, because its Grove port exposes exactly G32/G33
and it has the same built-in PDM microphone the previous project already drove.

### Decisions

| Decision | Why |
|----------|-----|
| NeoPixel output via **Adafruit NeoPixel 1.15.5** | Uses the RMT peripheral, claims and releases a channel per refresh, so two strips coexisting is safe. Confirmed in `esp.c` that `updateLength()` is a plain realloc, which makes the LED count a *runtime* parameter without re-initialising hardware. |
| LED count is a **per-strip runtime parameter** (default 100, max 300) | The real count is hidden inside the rocks. Every buffer is sized from `kMaxLedsPerStrip`; the strip object is re-lengthed when the slider is released. |
| Trigger = `level > threshold && now - lastSpawn >= retriggerMs` | Simple, predictable, and covers both cases: a drum hit gives exactly one blob, sustained bass gives a steady stream at the retrigger rate. |
| Blob pool of 12, oldest recycled | A busy passage must stay continuous rather than going silent. Since all blobs age at the same rate, "dimmest" and "oldest" are the same one. |
| Raised-cosine profile, circular distance | No hard edges, and a blob straddling LED 0 is drawn on both ends, so the loop has no visible seam. |
| Linear amplitude fade + gamma 2.2 at output | "Fade time" then means exactly what the slider says, while the gamma keeps the tail visible to the eye instead of cutting off abruptly. |
| Per-strip parameters, not global | Lets the two rocks split the spectrum (bass / mids) out of the box, and each can be recoloured independently. |
| Dropped presets and beat detection from the old project | Not asked for; this project is meant to be simpler. The `mode` field stays in the settings blob so mode 2 needs no storage migration. |
| Defaults: 35 % / 40 % threshold, 300 ms retrigger | On the same 0–1 band-level scale the old project used, music bass sits around 55–80 %, so the previous 4 % gate would have triggered continuously. These values trigger on peaks and can be tuned with the meter + tick. |

### Done

- [x] Project scaffold: `platformio.ini`, custom `m5stick-c-plus2` board
      definition, `.gitignore`.
- [x] Audio pipeline reused as-is: `audio_fft.*`, `fft_backend*` (ESP-DSP default,
      arduinoFFT optional).
- [x] `config.h` / `settings.h`: pins, audio range, parameter bounds, clamping,
      first-run defaults.
- [x] `settings_blob.*` + `store.*`: versioned fixed-layout NVS blob, live slot.
- [x] `strip.*`: ring driver — accumulate linear intensity, gamma LUT, push. Skips
      the RMT transfer entirely when the frame did not change (dark strip).
- [x] `blob_mode.*`: spawn / move / wrap / fade / recycle, plus render.
- [x] `webui.*`: AP `LightRocks`, page, `GET /api/state`,
      `POST /api/params|save|reset`.
- [x] `main.cpp`: layout, spectrum bars, per-strip band markers, level meters with
      threshold tick, HUD, debounced saves, buttons.
- [x] Docs: `SPEC.md`, `README.md`, this journal.

### Verified

- Firmware builds clean for `m5stick-c-plus2` (no warnings), 21 % RAM / 32 % flash.
  The `arduinodft` variant builds too.
- Embedded page checked in a browser against a mock API: sliders update labels and
  the strip title, values survive the 400 ms status poll without fighting the
  finger, tab switching loads the right strip, power toggle updates tab colour,
  save and factory reset round-trip.
- Blob geometry/motion checked numerically on the host (faithful port of
  `addRing` + `update`): at 45 LED/s and a 2000 ms fade the blob travels 0.89 laps
  while its amplitude falls 1.00 → 0, the profile is a clean raised-cosine bump
  about 8 LEDs wide, and a blob sitting on LED 0 lights 97,98,99,0,1,2,3 — no seam.

### Notes for later

- `blobLeds` is the full span of the light group; the two end LEDs sit at zero
  amplitude, so the visibly bright core is about `blobLeds - 2`. Kept that way
  because a hard-edged group looks worse at speed.
- The addRing loop is clamped to one lap, so even `blobLeds = 300` on a 100-LED
  strip cannot blow up the per-frame cost.

### Open / next

- **On-hardware check** — nothing has been run on a real stick or real strips yet.
  First things to confirm: microphone level at gain 0, actual blob speed feel,
  and that the two strips do not disturb each other's RMT timing.
- Level-shifting: WS2812B at 5 V fed with 3.3 V data usually works but is out of
  spec. If the first LED flickers, add a 74HCT125 or drop the supply to ~4.3 V.
- Gamma 2.2 and the default speeds are guesses until seen in the rocks.
- Mode 2 not designed. Current candidate: spectrum/VU running down the strip.

---

## 2026-11-03 — Review round 1 (questions from Tobias)

Questions asked, and where the answers now live:

| Question | Resolution |
|----------|------------|
| "Why are the blobs not part of the strip class?" | Written up in [ARCHITECTURE.md](./ARCHITECTURE.md#why-the-blobs-are-not-inside-ledstrip). The deciding argument is that **gamma is not additive**: overlapping blobs must be summed as linear light and encoded once, so the frame buffer has to be linear and the gamma LUT has to sit at the output edge. The secondary arguments are that mode 2 can reuse the same `clear/addRing/show` socket, and that the driver stays free of any audio knowledge. |
| "What is encode/decode strip?" | It is `encodeStrip()`/`decodeStrip()` in [settings_blob.cpp](../src/settings_blob.cpp) — one strip's settings ↔ 21 bytes. Nothing to do with the LED hardware; added a comment saying so in [settings_blob.h](../src/settings_blob.h). The three representations of `Settings` (struct / flash bytes / JSON) are tabulated in ARCHITECTURE.md. |
| "Why many FFT implementations? why arduino? this is an ESP32" | Agreed — arduinoFFT is redundant here. It was inherited verbatim from the earlier project, where it existed as a licensing/portability fallback. Documented, and left in place because it is cheap insurance for cross-checking the analysis code; it can be deleted without touching anything else. |
| "I don't understand the architecture" | New [ARCHITECTURE.md](./ARCHITECTURE.md): the one-frame-per-audio-block idea, the layer diagram, a step-by-step walkthrough of `loop()`, who owns which representation of the settings, the timing budget, and a "where each kind of change goes" cheat sheet. README links it. |

Also corrected a claim I had invented: the docs said the LED count is "only known
once the rocks are opened". Tobias never said that — the phrase came from the
project folder name (`soundReactiveLightRocks_TobiasHelp`), and it was stated as
fact when it was an inference. The real reason `ledCount` is a runtime parameter is
simply that it is not fixed by the firmware and varies per build. Both the comment
in [config.h](../src/config.h) and [SPEC.md](./SPEC.md) now say that instead.

The name "Light Rocks" itself turned out to be correct, so it stays.

---

## 2026-11-04 — Hardware test feedback, M5StickS3 target, & UI/motion tweaks

### Context & findings after first hardware test

1. **Loop vs. One-shot mode (`loop` toggle)**:
   - Added per-strip `Loop` / `One-shot` toggle (`StripSettings::loop`).
   - In `Loop` mode (`loop = 1`), blobs wrap around `[0, ledCount)` as a closed ring.
   - In `One-shot` mode (`loop = 0`), blobs do not wrap across the ends; they travel
     along the strip and disappear cleanly once their trailing edge leaves the last LED.

2. **Why `Reverse` did not work properly**:
   - `BlobEngine::spawn()` previously always spawned at `pos = 0.0f`. In reverse,
     the first step moved negative and wrapped to `ledCount - 1` (`99`). Furthermore,
     if `ledCount` (`100`) is larger than the physical number of LEDs on the strip,
     a reversed blob starting at `99` spends its lifetime moving through unpopulated
     indices before ever reaching the physical LEDs.
   - **Fix**: Forward (`reverse = 0`) spawns at `0` and travels `0 → ledCount - 1`.
     Reverse (`reverse = 1`) spawns at `ledCount - 1` and travels `ledCount - 1 → 0`.

3. **Why the colour picker behaved strangely & real-time Canvas Colour Wheel**:
   - Previously, the UI used `<input type="color">` and only sent `/api/params` on
     `change` (when closing the native OS picker), not while dragging.
   - In addition, `LedStrip` was applying `γ = 2.2` independently to each RGB
     channel *after* multiplying by the fading envelope `v`. Raising a secondary
     channel to the 2.2 power during fade-out crushed it toward zero much faster
     than the primary channel (shifting orange toward red, purple toward blue/red).
   - **Fix**:
     - Replaced `<input type="color">` with an inline **HTML5 Canvas Colour Wheel** +
       brightness slider + live colour swatch square that streams colour updates in
       **real time** (`pointermove`, throttled at ~60 ms).
     - Changed `LedStrip::addRing()` / `show()` so gamma (`γ = 2.2`) shapes the
       blob's brightness envelope `v` while preserving the chosen `(R, G, B)` ratio
       faithfully across the entire fade, and scales channels proportionally if
       overlapping blobs exceed `1.0`.

4. **Automatic save + 3 Preset banks + Factory reset**:
   - Removed the redundant "Save to flash" button: live parameter changes are
     always auto-saved to the `live` NVS slot after 800 ms of inactivity and loaded
     automatically on boot.
   - Added **3 preset banks** (`Preset 1`, `Preset 2`, `Preset 3`) in NVS with
     `Save 1..3` and `Load 1..3` buttons in the GUI.
   - Kept `Factory reset` at the bottom, which restores hardcoded C++ defaults
     (`defaultSettings()`) without wiping the 3 saved preset banks.

5. **Web GUI 5/6 width (thumb-scroll gutter)**:
   - Restored the right-hand `1/6` viewport gutter (`padding-right: 16.7vw` with a
     subtle dashed divider) so there is always space on the right side of the phone
     screen to scroll without touching sliders, buttons, or the colour wheel.

6. **Microcontroller platform: M5StickS3 (`G9` / `G10`)**:
   - Note: M5AtomS3 was dropped because the standard AtomS3 has no built-in
     microphone.
   - Added `boards/m5sticks3.json` and `[env:m5sticks3]` (set as default environment
     in `platformio.ini`) targeting the **M5StickS3** (`ESP32-S3-PICO-1-N8R8`,
     ES8311 + MEMS mic supported via `M5Unified`, Grove pins **G9** and **G10**).
   - Kept `[env:m5stick-c-plus2]` (**G32** and **G33**) so either Stick can be
     built and flashed.

7. **Stability fix for Wi-Fi freezes / board resets during real-time colour tuning**:
   - **Root cause 1 — RMT driver install/uninstall thrashing (`esp.c`)**:
     `Adafruit_NeoPixel::show()` on ESP-IDF 4.4 called `rmt_config()`,
     `rmt_driver_install(channel 0)`, and `rmt_driver_uninstall(channel 0)` on
     **every single frame** for both strips (allocating and freeing FreeRTOS ISRs,
     semaphores, and ringbuffers 62 times/sec, and rebinding `channel 0` between
     `G9` and `G10`).
     - **Fix**: Replaced `Adafruit_NeoPixel::show()` in [src/strip.cpp](../src/strip.cpp)
       with two permanent, dedicated ESP-IDF RMT channels (`RMT_CHANNEL_0` for
       Strip 1, `RMT_CHANNEL_1` for Strip 2) initialized **once** in `begin()` and
       using a static `grb_` buffer (zero heap allocations in `loop()` or
       `setCount()`).
   - **Root cause 2 — Overlapping HTTP connection flood during colour-wheel drag**:
     `flush()` in [src/webui.cpp](../src/webui.cpp) was firing a new `fetch()` every
     55 ms without waiting for the previous request to complete, while `poll()` was
     also firing `/api/state` every 400 ms. On the single-socket ESP32 `WebServer`,
     parallel TCP connections quickly exhausted lwIP sockets and blocked
     `handleClient()` until timeout.
     - **Fix**: Added strict single-in-flight (`INFLIGHT`) serialization in JS
       (coalescing rapid wheel moves into `PEND` until the current request returns),
       paused background `/api/state` polls while dragging, changed `handleParams()`
       to return a zero-allocation `204 No Content`, disabled ESP32-S3 Wi-Fi modem
       sleep (`WiFi.setSleep(false)`), enabled `TCP_NODELAY`, and raised the NVS
       auto-save debounce to 2000 ms so flash writes do not stall the CPU mid-drag.
   - **Root cause 3 — M5Unified Grove I2C probe**:
     Set `cfg.external_display_value = 0`, `cfg.external_imu = false`, and
     `cfg.external_rtc = false` in `setup()` so `M5.begin()` never probes `Ex_I2C`
     on the Grove pins (`G9`/`G10`).

8. **Round 2 Controls & UX improvements**:
   - **Per-strip Max Brightness (`brightnessPct`, 1–100%)**: scales all rendered
     output for that strip in `LedStrip::show()`.
   - **Retrigger ON / Single toggle (`retrigger`)**: button right next to the
     `Retrigger gap` slider. When set to `Single` (`retrigger = 0`), no new blob
     can trigger while a blob is already running on that strip.
   - **LED Count Calibration mode + `←` / `→` precision arrows**:
     - Adjusting `ledCount` (via the slider or the `←` / `→` 1-LED step buttons)
       automatically blanks the strip and lights **only LED `ledCount - 1`** for
       3.5 seconds so the physical tip of the strip is directly visible.
     - A `Calibrate` toggle button next to the arrows can also lock that single
       calibration dot on continuously while inspecting the rock.
   - **Grouped Sound & Trigger card + Draggable Threshold on Live Level Meter**:
     - Grouped strip tabs, live strip level meters, mic gain, band centre, band
       width, and retrigger into the top card so you never have to scroll up and
       down while tuning sound response.
     - Made the white threshold marker **directly draggable on each strip's live
       level meter bar**.
   - **Ping-pong direction (`Forward` / `Reverse` / `Ping-pong`)**:
     - Added `kPingPong = 2` (`0 ↔ ledCount - 1`), which bounces continuously while
       fading in `Loop` mode, or does a single out-and-back round-trip in
       `One-shot` mode.
   - **Eliminated `WebServer.cpp:638` log noise**: bound `/favicon.ico` explicitly
     so browser icon probes do not trigger `request handler not found` logs.

9. **Round 3 — 1000-LED support, Calibration fix, Spectrogram band box, & UI card grouping**:
   - **Raised `kMaxLedsPerStrip` from 300 to 1000 LEDs** (`src/config.h`):
     - Static buffer cost for 2 × 1000 LEDs is only ~30 KB (total RAM 27.6% of
       internal SRAM).
     - Per-frame cost scales only with active `ledCount` (for `clear()`/`show()`)
       and `blobLeds` (for `addRing()`), never with the 1000-LED ceiling.
   - **Fixed Calibration dot bug (`src/strip.cpp`, `src/webui.cpp`)**:
     - Previously, `setCount()` transmitted an RMT frame of zeros when decreasing
       `ledCount` and `showCalibrationDot()` immediately transmitted a second RMT
       frame in the same millisecond without a `>80 µs` WS2812B reset pulse,
       concatenating the two frames and lighting the wrong pixel.
     - Now `setCount()` never transmits directly; `maxSentCount_` tracks the
       furthest driven pixel and `show()` / `showCalibrationDot()` blanks any tail
       pixels in a **single** RMT frame.
     - Calibration mode now **automatically exits** whenever you touch any other
       slider/wheel, switch strips, load a preset, or press Factory Reset, so you
       can never get stuck in calibration mode. Added `-10` / `+10` step buttons
       alongside `← -1` and `+1 →`.
   - **Spectrogram Band Rectangle + Centre Frequency Triangle (`src/main.cpp`)**:
     - Each strip's frequency window (`centerHz ± widthHz/2`) is now drawn as an
       **empty rectangle over the spectrogram**, with a **downward-pointing
       triangle on the top edge** marking `centerHz`.
   - **Clean 6-box Web GUI Grouping (`src/webui.cpp`)**:
     1. **Microphone (global)** — gain + broadband stats.
     2. **Select strip & Power** — `Strip 1` / `Strip 2` tabs, strip title banner,
        and power switch above all per-strip cards.
     3. **Triggering conditions** — live band level & draggable threshold bar,
        `Band centre`, and `Band width`.
     4. **Display parameters** — real-time colour wheel + swatch square, and
        `Max brightness`.
     5. **Animation parameters** — `Motion`, `Direction`, `Blob length`, `Speed`,
        `Fade time`, and `Retrigger gap` + `Retrig ON / Single` toggle.
     6. **Strip calibration** — separate card for `LEDs on this strip (1..1000)`,
        `-10 / ← / → / +10` step buttons, and `Calibrate / Exit calib`.

10. **Round 4 — Multi-module Wi-Fi SSIDs, per-env pins in `platformio.ini`, & persistent LCD identity**:
   - Moved `WIFI_SSID`, `WIFI_PASSWORD`, `STRIP1_PIN`, and `STRIP2_PIN` into
     `platformio.ini` per environment (`m5sticks3` uses `G9/G10`, `m5stick-c-plus2`
     uses `G32/G33`) with safe fallbacks in `src/config.h`.
   - Added persistent display of the module's `WIFI_SSID` and IP address on the
     bottom line of the LCD (`src/main.cpp`) as well as in the Web UI header so
     multiple programmed modules are immediately distinguishable.
   - Created [docs/HANDOFF.md](./HANDOFF.md) summarizing the full system state,
     critical RTOS/RMT/HTTP stability invariants, and the next roadmap items for
     handoff.

---

## 2026-11-07 — Pad effects, per-strip beat detection, and the modularisation pass

### Requested

1. **Fingertip pads** to trigger effects from the web GUI, like the sister project,
   but more interesting than a single colour: at least a colour flash, plus light
   jets and other effects.
2. **Proper beat detection mode**, again inspired by `Katya_Kim/LightSoundReactive`.
3. The user's key clarification: **each strip must be controlled independently**,
   and **beat detection is an *input*, not a mode** — a strip is activated either by
   the automatic beat input or by the level threshold.
4. The pad view should be a **full screen**, divided in two groups, one per strip.
5. Also finish the `HANDOFF.md` Priority 1 cleanup while touching everything.

### Decisions

| Decision | Why |
|----------|-----|
| `mode` becomes the **trigger input** (`kThreshold` / `kBeat` / `kManual`) | Exactly what was asked: threshold and beat are alternative ways to fire the same effect pipeline. `kManual` keeps the "silent until tapped" behaviour of the sister project. |
| New `effect` field (`kEffectJet` / `kEffectFlash` / `kEffectBurst` / `kEffectSparkle`) | Separates *what is drawn* from *what fires it*, so any input can drive any effect. |
| **One `BeatDetector` per strip**, fed by `bandFlux(centerHz, widthHz)` | Gives real independence: the bass rock can lock to 124 BPM while the mid rock follows a different rhythm. A single broadband detector could not do that. |
| `bandFlux()` normalises by band width and mic gain | A narrow window otherwise reads a much smaller flux than a wide one, so sensitivity meant something different per strip. |
| Flash and Sparkle are **envelope-only** effects (no position) | They reuse `addFill()` / `addSparkles()` on the same linear buffer, so overlapping with jets still adds correctly. |
| Pad **hold** semantics: sustain Flash/Sparkle, re-fire Jet at `retriggerMs` | Holding a "flash" should keep the light on, while holding a "jet" should stream jets; a 2.5 s keepalive timeout releases the pad if the browser dies. |
| `Display`, `settings.cpp`, `beat.*` extracted | Completes the Priority 1 cleanup; `main.cpp` is now orchestration only. |
| Blob version bumped to **v4 (26 bytes/strip)** with a **v3 migration path** | Existing hardware keeps its tuned presets instead of silently reverting to defaults. |

### Done

- [x] `src/settings.h` / `src/settings.cpp`: merged `settings_blob.*` + `store.*`; added `mode`, `effect`, `beatSensitivityPct`; v4 blob + v3 migration.
- [x] `src/beat.h` / `src/beat.cpp`: per-strip onset + tempo detector (ported and adapted).
- [x] `src/audio_fft.*`: `rise_[]` buffer + `bandFlux(center, width)`.
- [x] `src/strip.*`: `addFill()` (flash) and `addSparkles()` (deterministic per-LED hash shower).
- [x] `src/blob_mode.*`: effect pool + flash/sparkle envelopes + burst spawn + pad hold + trigger-input evaluation.
- [x] `src/display.h` / `src/display.cpp`: full LCD renderer, meter labels showing `S`/`B`/`M`.
- [x] `src/webui.*`: trigger-input selector, effect selector, beat sensitivity, `/api/tap`, full-screen pad overlay.
- [x] `src/main.cpp`: orchestration only.
- [x] Docs: SPEC, HANDOFF, this entry.

### Verified

- `pio run` (m5sticks3) and `pio run -e m5stick-c-plus2`: **both SUCCESS**, zero
  warnings in project sources. RAM 28 %, Flash 32 % on S3.
- Embedded page extracted and `node --check`ed: **JS syntax valid**, HTML tags balanced.
- Page exercised against a mock API in Chromium: all cards render, strip title and
  trigger hints update on mode switch, the full-screen pad view shows two colour-coded
  groups of four pads with live status, and tapping a pad issues
  `POST /api/tap?strip=0&effect=0&down=1` followed by `down=0`.
- Fixed a real bug found this way: `&amp;` inside `textContent` rendered literally.

### Open / next

- Not yet run on hardware: confirm beat lock on real music, and that Flash/Sparkle
  hold feels right on the rocks.
- **Next architectural step (user request)**: physical strips become containers for
  multiple *light layers* ("virtual strips"), each with its own band, trigger input,
  colour and intensity weight, all accumulating into one linear buffer. See
  `HANDOFF.md` Priority 2 for the planned shape.


## 2026-11-07 (later) — Physical strips become containers of light layers

### The insight

The user reframed the model after the pad work landed:

> "the real object is not the physical strip... one physical strip is a kind of
> container that can contain one, two, three more strips, meaning this object with
> all its functions for the lights."

So a **physical strip** is hardware (pin, RMT channel, LED count, one frame buffer)
and a **light layer** is the thing that glows (band, trigger input, effect, colour,
weight). Several layers share one strip and blend in the same buffer.

### Why the codebase was already shaped for this

`LedStrip::buf_` is **linear float** RGB and `addRing` / `addFill` / `addSparkles`
all *accumulate*. Gamma is applied once, in `show()`. That was the original
argument for keeping blobs out of the driver (ARCHITECTURE.md), and it means layers
merge correctly for free: overlaps add as light, hue is preserved, and >1.0 is
scaled proportionally rather than clamped per channel.

### Decisions

| Decision | Why |
|----------|-----|
| `PhysicalStripSettings` vs `LayerSettings` as separate types | Makes the two roles explicit in the type system instead of overloading one struct. |
| Max **4 layers** per strip | Enough for "several beams"; keeps the blob 202 bytes and the engine array ~2 KB. |
| `ledCount` passed *into* the engine instead of stored | It belongs to the container, not the layer. The engine needs it only to compute spawn positions. |
| Per-layer `intensityPct` | Lets layers be balanced without touching their colours or brightness. |
| `solo` is runtime-only | It is a preview tool, not a saved property. Kept out of the blob deliberately. |
| LCD shows one row + one merged meter per physical strip | 8 rows of per-layer detail does not fit 135 px legibly; the phone has the room. |
| `LayerEngine` (files renamed from `blob_mode.*` to `layer.*`) | The name now matches the object. |
| Add/remove layer re-syncs state **after** the params POST completes | Keeps the strict single-in-flight HTTP rule; a follow-up fetch hooks onto the flush completion. |

### Done

- [x] `settings.h/.cpp`: container + layer types, blob **v5** (202 bytes) with v4/v3 migration.
- [x] `layer.h/.cpp`: `LayerEngine` (renamed, intensity-aware, `ledCount` passed in).
- [x] `main.cpp`: per-layer engines and beat detectors, solo support, `clear → render all → show`.
- [x] `display.h/.cpp`: rewritten for containers (dominant-layer band, merged meter, layer-count hint).
- [x] `webui.h/.cpp`: `?strip=&layer=` addressing, `layerCount`/`solo` on the container, nested layer tabs, add/remove/solo, pad overlay **one group per layer**.
- [x] Docs: SPEC (§5 rewritten, §7 split into container/layer tables, §6 solo, API rows), HANDOFF, this entry.

### Verified

- `pio run` and `pio run -e m5stick-c-plus2`: **both SUCCESS**, no warnings in project
  sources. RAM 28.4 % / 28.8 %, Flash 32.3 % / 33.7 %.
- Page JS `node --check`ed; tags balanced.
- Browser test against a stateful mock: strip/layer tabs, layer switch updates title /
  band / colour / intensity, **+ Add layer** grows to 3 and auto-selects the new
  layer, **Solo** toggles to `Solo ON`, pad overlay shows one colour-coded group per
  layer, and a pad tap issues `POST /api/tap?strip=0&layer=1&effect=0&down=1`.
- Two rendered bugs found and fixed this way: `&middot;` used inside `textContent`,
  and the strip tab showing "on" regardless of layer state.

### Open / next

- **On hardware**: beat lock on real music, hold feel, and current draw with several
  layers at full white on one 5 V supply.
- Ideas this unlocks: different `fadeMs` per layer to fake reverb, one beat layer plus
  one threshold layer for kick + snare, a slow "bed" flash layer under fast jets.

## 2026-11-07 (evening) — Blending fix, non-exclusive triggers, motion & whip

### 1. Colour blending was broken — two real bugs

Reported as "colour blending is not working very well". Diagnosis in `strip.cpp`:

| Bug | Effect |
|-----|--------|
| Gamma applied **per effect** inside `addRing` / `addFill` / `addSparkles`, before accumulation | A 50 %-amplitude blob contributed 22 % of the light, so overlaps did not sum like light; two layers at 0.7 summed as 0.46+0.46 rather than 1.4. |
| The layer `intensityPct` was folded into `amp` and therefore went through the same `^2.2` | The intensity slider could not balance layers by eye: 50 % showed as ~10 % of the light. |

Fix: layers now accumulate **linear** light; `show()` applies gamma **once**, to the
merged maximum, then scales the whole RGB vector so the hue ratio survives. The
weight is compensated by `1/γ` so the slider is perceptually linear.

I first tried a "soft knee" above 1.0 — and the numeric check showed it was a
*regression* (0.973 at m = 1.2 where the old code gave 1.0, i.e. dimmer). Reverted.
Worth recording: the knee idea looked obviously right and was wrong; measuring saved it.

Verified numerically: a single blob is bit-identical to before (0.4563 at amp 0.7),
a doubled same-colour layer goes 0.218 → 1.0, and intensity 100/75/50/25 % now maps
to 0.456/0.242/0.099/0.022 consistently.

**Invariant:** gamma lives only in `LedStrip::show()`.

### 2. Trigger inputs are now non-exclusive

`StripSettings::mode` (one enum) became `LayerSettings::triggers`, a bitmask:

```
1 threshold · 2 beat · 4 tap pads · 8 accelerometer · 16 gyroscope
```

A layer fires on whichever armed source happens first. The GUI shows tickable chips
(`✓ Threshold`). Pads are gated by the tap bit, so unticking it really removes them
as a source. BtnA scrolls physical strips (`>` on the LCD), BtnB cycles the trigger
set of the selected strip's layers; gain is now web-only, Wi-Fi toggle and save moved
to the long presses.

### 3. Accelerometer, gyroscope and the whip

New [src/motion.h](../src/motion.h) / [src/motion.cpp](../src/motion.cpp): reads the
internal IMU once per frame. `swing` is `| |a| − 1 g |`, so a stick at rest reads 0
and a whip reads large; `twist` is `|ω|`.

New `kEffectWhip` (as requested):

* speed scales with swing strength — `0.35 + 2.4·swing/2 g`, i.e. ~2.8× faster for a
  hard lash than for a gentle one;
* direction follows the sign of the horizontal acceleration;
* **colour comes from the swing direction** (`atan2(ay, ax)` → hue), so whipping one
  way gives one colour and the other way gives another.

Blobs gained an optional per-blob colour (`custom`, `cr/cg/cb`) and `speedScale` so
the whip can differ from the layer's own colour and speed.

### Storage

Blob **v6** (26-byte layer block, 210 bytes total). v5/v4/v3 migrate automatically;
an old `mode` enum becomes the matching trigger bit by hand — it is deliberately *not*
a blind byte copy, because the old byte was an exclusive choice.

### Verified

- Both envs build, no warnings. RAM 28.9 % / 29.3 %, flash 32.5 % / 33.9 %.
- Blending maths checked on the host (old vs new, table above).
- Page JS `node --check`ed; browser test against a stateful mock: five trigger chips
  render with ticks, ticking Accelerometer sent `triggers=11` and revealed the Motion
  threshold slider, switching to the whip layer showed `accel · whip` with
  `Motion threshold 25%`, Solo toggled, and the button hint updated.
- Found a silent patch failure this way: the trigger-card markup replacement had not
  applied, so `#trigchips` was null and half the cards never built. Replaced it by
  line number instead of string matching.

### Open

- Hardware: confirm the IMU is enabled on both boards and tune `motionThresholdPct`
  for real whips; check that a whip does not fire continuously while the stick is
  carried around (retrigger gap is the only current guard).

## 2026-11-07 (night) — Beat silence gate & negative mic gain

### Reported

1. "Beat detection is a little weird. In any case, when there is silence, it should
   not trigger."
2. "I also need the mic gain to be NEGATIVE (it is too sensitive in noisy places)."

### Beat detection in silence — root cause

My port had *lowered* the absolute onset floor from the sister project's `0.02` to
`0.008`, and nothing looked at the actual signal level. The adaptive threshold is
`mean + k·σ` over a ~900 ms window, so in a quiet room `mean → 0`, `var → 0` and the
threshold sits at ~0: any noise excursion above `0.008` counted as a beat. Worse,
`bandFlux()` multiplied by the mic gain and by `sqrt(255/bandBins)`, so a narrow
layer amplified noise up to ~8×.

Fixes:
* floor restored to `0.02`;
* **silence gate** — `BeatDetector::update()` now takes the band level and refuses
  to fire below `0.06`, also dropping the tempo lock immediately;
* `bandFlux()` no longer scaled by the mic gain, so attenuation does not deafen it.

Verified by compiling `beat.cpp` on the host (it has no Arduino dependency) and
running four cases:

| case | before | after |
|------|--------|-------|
| silence, level 0.01 | fired | **0 beats** |
| very quiet, level 0.03 | fired | **0 beats** |
| 120 BPM clicks | ok | 42 beats in 22 s, **117 BPM** |
| sustained loud tone | — | **0 beats** (no onset, so no beat — correct) |

### Negative microphone gain

`gainDb` was `uint8_t 0..18` with a comment about the hardware only offering
0/6/12/18 dB — but it was always a **software** gain applied in `audio_fft.cpp`, so
negative values cost nothing. It is now `int8_t`, −24…+18 dB in 6 dB steps.

One trap: quantising with `(g + step/2) / step * step` truncates towards zero, so
−24 became −18. Fixed by rounding by hand with the sign handled explicitly, and
checked the mapping for every step from −30 to +24.

The blob still stores the gain in one byte; it is cast through `uint8_t`/`int8_t`
explicitly so the sign survives as two's complement.

### Verified

- Both envs build, no warnings. RAM 28.9 % / 29.3 %.
- Host tests for the beat detector (table above) and for the gain quantiser.
- Browser: gain slider is `min=-24 max=18 step=6`, label reads `-24 dB`, the hint
  explains attenuation, and moving it sends `gainDb=-24` to the board.

### Open

- `kSilenceLevel` (0.06) and `kMinStrength` (0.02) are reasoned, not measured.
  On hardware they should be checked at gain 0 and at −24 dB in a quiet room.
- The threshold trigger has no equivalent silence gate; if the user reports false
  threshold fires in silence, the same idea applies there.

## 2026-11-08 — Pad layout rework & beat gate vs. mic gain

### Reported by the user

1. After uploading, "I don't see the auto-beat trigger".
2. "What happened with the glow mode?"
3. The Live Effect Pads window cannot be scrolled and has no right-side margin.
4. Make the pad buttons smaller and group layers by strip, two columns (A left,
   B right); do not show layers that were not added.

### Found: the silence gate fought the new negative gain

This is almost certainly (1). The gate I added yesterday compares the **band level**,
and `bandLevel()` includes the microphone gain. With gain at −12 or −24 dB the level
drops by up to 0.65 of the 0..1 range, so it can sit below the 0.06 gate even while
music is playing — the beat detector then never fires at all, which reads exactly as
"the auto-beat trigger is not there".

Fix: the gate now receives the level **with the gain removed**
(`level − gainDb / kLevelRangeDb`), so gain attenuates what you hear without
disabling detection. `SpectrumAnalyzer::kLevelRangeDb` and a `clampLevel()` helper
were exposed for this.

### Also hardened

`stateJson()` had a 3400-byte buffer with a measured worst case of 3062 — a silent
truncation there would produce invalid JSON and a half-built page, which is another
way for the beat chip to "disappear". Raised to 4096 and made **static** rather than
stack-allocated.

### Pad overlay rework

* scrolls now (`touch-action:pan-y`, `overflow-y:auto`, momentum scrolling);
* keeps the same 1/6 viewport thumb gutter as the control page (dashed divider);
* layout is a two-column grid: **Strip A | Strip B**, each column stacking its own
  layers, with a `Strip X · N layers` header;
* pads are 42 px tall instead of 60, smaller type, tighter 2x2 grid;
* only added layers are rendered (`l < layerCount`), so a one-layer strip shows one
  block, not four.

### Verified

- Both envs build, no warnings. RAM 30.1 % / 30.5 % (the static JSON buffer accounts
  for most of the increase from 29.3 %).
- Browser: overlay `scrollHeight 499 > clientHeight 445`, `overflow-y:auto`,
  `padding-right 91.8px`, `touch-action:pan-y`, pad height 42 px, columns
  `Strip A: 3 groups` and `Strip B: 1 group`.

### Open

- Whether "glow mode" means a *new* effect (the sister project had a Glow mode; this
  firmware never did) — asked the user.
- The threshold trigger still has no silence gate.

## 2026-11-08 (later) — Missing chip, Glow effect

### "The auto-beat chip is missing entirely"

Confirmed with the user: not "beats never fire" but *the chip itself was absent*.
The five chips were built from JavaScript (`$('trigchips').innerHTML = CHIPS.map(...)`),
so anything that threw earlier in `build()` left the row empty — visually identical
to a missing trigger, and impossible to reproduce in a mock where everything runs.

Changes:
* the chips are now **static markup**; the script only attaches handlers and toggles
  the `on` class. This removes the failure mode entirely rather than chasing it.
* found and fixed a **duplicate id** in the process: the beat chip and the beat
  panel had both been `trig_beat` (the panel is now `beatpanel`). A duplicate id is
  exactly the kind of thing that makes `$('trig_beat')` return the wrong element.
* the header now shows `kFirmwareVersion` (`v0.7`) and `/api/state` carries `"ver"`,
  so an upload that did not take is obvious instead of suspected.
* `<meta http-equiv=cache-control content=no-store>` as belt and braces.

### Glow effect (confirmed request)

The sister project's Glow mode had no equivalent here, so `kEffectGlow = 5`:

* a raised cosine over `glowPeriodMs` (200–5000 ms) between off and full;
* it advances its **own** phase, so the layer breathes with no trigger at all —
  that is what "mic independent" means;
* a trigger adds a short boost on top;
* if the **beat** input is armed, each beat snaps the wave to its peak
  (`phase = 0.5`), so the breathing lands with the music instead of drifting.

Storage: instead of growing the blob, byte 25 of the layer block — until now the
spare byte — carries `glowPeriodMs / 20`. Version bumped to **v7**; a v6 blob has 0
there and is mapped to the 2 s default.

### Verified

- Static chips render without JS ever touching `innerHTML`; a duplicate-id scan over
  the generated page reports none.
- Browser: header shows `· v0.7`, all five chips present, Glow appears in the effect
  list, the Glow layer shows `Glow period 3000 ms` while a Jet layer hides it, and the
  hint correctly reads "glow breathes on its own — no trigger needed".
- Both envs build with no warnings.

## 2026-11-08 (later still) — Why big blobs look slow, and sub-frame motion blur

### Two observations, one real defect

1. "The larger the blob, the slower it moves for the same speed (LEDs/sec)."
2. "When the speed is large, the blob seems to jump between positions, not even
   overlapping."

**#1 is not a bug.** `b.pos` advances at exactly `speed` LEDs/s whatever
`blobLeds` is. But at 31 fps a blob at 45 LED/s moves 1.44 LEDs per frame: that is
18 % of an 8-LED blob's width and 2.4 % of a 60-LED blob's. Same absolute rate, very
different relative motion, so the wide one reads as crawling. It also fades over
`fadeMs` before it has visibly travelled its own length. Kept the semantics and
documented them; noted "blob lengths per second" as an alternative unit in IDEAS.md.

**#2 is temporal aliasing.** At 500 LED/s the blob advances ~16 LEDs per frame. With
an 8-LED blob, consecutive frames do not touch, so it strobes.

Fix: `LayerEngine::drawBlob()` sweeps the path between `prevPos` and `pos`:

* travel measured the shortest way round a ring, plain difference otherwise;
* `ghosts = 1 + |travel| / 0.6`, capped at 12;
* each ghost carries `amp / ghosts`, so the light is spread over the streak and a
  fast blob does not get brighter;
* a blob moving < 0.6 LED per frame still draws exactly one copy, so slow motion is
  unchanged.

Verified on the host that ghost spacing stays below the blob width for every
speed/blob combination in range (2.02 LEDs at 500 LED/s for the worst case, against
a 4-LED blob), i.e. the streak is continuous rather than dotted.

### Continuous positions

The user suggested a continuous blob position with distance-based spatial decay at
render time. That is already the design: `Blob::pos` is a float, and `addRing()`
computes `d = ledIndex - centerPos` per LED and shapes a raised cosine on `|d|`.
What was missing was sampling the *path*, not the position — now added.

### BLE vs Wi-Fi

Answered in detail in [IDEAS.md](./IDEAS.md): the ESP32-S3 has **BLE only** (no
Classic BT), so A2DP audio is impossible on the StickS3; Wi-Fi AP + HTTP remains the
only zero-install calibration path (iOS Safari has no Web Bluetooth); the strong case
for BLE here is **BLE MIDI** as a *separate* performance input. Wi-Fi and Bluetooth
share the radio and cannot both run at full duty.

### New

[docs/IDEAS.md](./IDEAS.md) — running list of ideas, libraries and results, as
requested, so nothing is lost between sessions.

## 2026-11-08 (evening) — Ghosts, bleed, and Glow becomes an LFO trigger

### The "weird little disconnected" blobs

My own regression. The motion-blur ghosts used a **fixed 0.6-LED step**, so with a
1-LED blob (`halfWidth = 0.5`) every ghost landed on a different LED: a dotted line,
which is exactly what the user described at low speed with one LED.

Fix: the step is now `halfWidth / 2`, so neighbouring profiles always overlap. If the
blob is so short that even that looks dotted, the answer is spatial, not temporal.

### Bleed / blur (user's suggestion, and the right one)

Per-strip `bleedPct` applies a `[1,2,1]/4` convolution to the **merged** frame just
before gamma, repeated up to three times at 100 %. Implemented **in place** with a
rolling window (prev/cur/next plus one saved pixel for the ring wrap) so it costs a
handful of floats instead of a second 12 KB frame buffer — the first version did
allocate one and pushed RAM from 30 % to 37.6 %.

### Fade shape

With gamma at the output, decaying the light amplitude linearly displays as
`amp^2.2`: a fast drop and then a long dim tail, so `fadeMs` did not mean what the
slider said. `Blob::life` now ages linearly and `amp = life^(1/2.2)`, making
displayed brightness fall linearly with time (0.25/0.50/0.75 of the fade now shows
0.75/0.50/0.25 instead of 0.53/0.22/0.05).

### Glow is a trigger, not an effect

Adopted the user's framing exactly: "glow mode is in fact a trigger... a metronome".
* `kEffectGlow` is gone.
* `kTrigLfo` (bit 32) is a free-running metronome with `lfoPeriodMs`; it bypasses the
  retrigger gate because a clock should not be swallowed by a gap.
* `Flash` gained `attackMs` and `releaseMs`. Long + slow LFO = glow; short + fast LFO
  = strobe. Trigger and effect stay orthogonal.
* Blob v8 migrates a stored `effect == 5` into `Flash + LFO` with
  `attack = period/4`, `release = period/2`, so a saved preset keeps its look.

### UI

Strip-level controls moved out of the layers section into the strip card, directly
under the Strip 1 / Strip 2 buttons: **Strip brightness**, then **Bleed / blur**, then
the layer list — as requested. The Display card is now only the layer colour and
intensity.

### Refresh rate: measured budget, honest answer

30 µs/LED means 3 ms for 100 LEDs, 9 ms for 300, 30 ms for 1000 — so the strip is
*not* the bottleneck. The blocking mic capture (512/16 kHz = 32 ms) fixes the loop at
~31 fps regardless of speed. Going faster needs the audio capture moved to its own
task with a ring buffer; that is now the top item in IDEAS.md rather than something
motion blur is papering over.

## 2026-11-08 (night) — Audio and rendering finally decoupled; AGC

### The right question

"Why does audio capture have anything to do with display rendering?" It should not,
and it was my mistake. I checked the library before answering:

* `M5.Mic` already configures I2S/PDM with **DMA** (`dma_buf_count`, `dma_buf_len`)
  and runs its **own capture task** — exactly the continuous DMA the user assumed;
* but `record()` is a *blocking* API: it queues a request and waits on a semaphore
  until the library task has filled the buffer.

So the hardware was fine; the firmware chose to wait. That came from the sister
project's "one audio block = one frame" simplification, which was tidy but coupled
two independent rates.

### The split

New [src/audio_task.h](../src/audio_task.h) / [audio_task.cpp](../src/audio_task.cpp):

* a FreeRTOS task on **core 0** (Wi-Fi core), priority 3, 6 KB stack, owns
  `SpectrumAnalyzer` exclusively;
* it applies `gainDb` itself (so nobody else may call `setGainDb`), captures, runs the
  FFT, and publishes an `AudioSnapshot`: broadband level/dBFS/peak, the 30 display
  bands, and per layer its band level and spectral flux;
* the render loop copies the snapshot under a spinlock and renders at up to **~100
  fps**, advancing animation from its own clock;
* onset detection runs only when `snapshot.seq` changes, so each flux sample is
  consumed once — otherwise the same sample would be tested 3× per block;
* if the task cannot start, the loop falls back to the old inline path.

This is what makes `speed` above ~30 LED/s mean anything: at 31 fps a 500 LED/s blob
had to jump 16 LEDs per frame.

### Automatic gain control

Requested for noisy rooms. Every 400 ms, from the peak broadband level of the window:

| peak | action |
|------|--------|
| < 0.08 | nothing (silence must not be amplified) |
| < 0.30 | +3 dB |
| 0.30–0.90 | nothing |
| > 0.90 | −3 dB |

Gain steps were narrowed from 6 dB to **3 dB** so the controller settles without
audible pumping. Verified by simulation: silence and very quiet stay put, a quiet
room walks up to +9 dB (level 0.38), loud walks down to −3 dB, very loud to −18 dB
(level 0.86), and a source sitting on a boundary does not oscillate.

Moving the gain slider turns AGC off — that is the manual override.

UI: an `AGC` toggle sits on the gain slider's row, matching the retrigger control.
The slider is disabled while AGC is on.

Blob **v9** (header grew by the `agc` byte), v8 migrates.

## 2026-11-08 (late) — Saved layers vanished after flashing; softness moves to the layer

### The bug: every legacy migration path was off by one byte

Symptom: after flashing, the GUI showed **one** layer although four were saved and
reachable before.

Cause: when `agc` was added, `kSettingsHeaderSize` grew from 2 to 3 — and all the
legacy branches compared the incoming length against that *current* constant:

```cpp
len == kSettingsHeaderSize + ...      // 3 + ..., but an old blob is 2 + ...
```

So a v7 blob (218 bytes) was checked against 219, a v6 blob against 271, and so on.
Every old blob failed to decode, `decodeSettings()` returned false and the firmware
used `defaultSettings()` — one layer, nothing in NVS reachable.

Fixed with explicit legacy constants (`kHeaderV8 = 2`, `kPhysBlockV8 = 4`,
`kLayerBlockV7 = 26`, `kLayerBlockV5 = 24`) and, while there, the v6 branch was
decoding with the *current* block decoders (wrong layout) — rewritten to read the v7
layout properly.

Verified with a host test that builds synthetic v6/v7/v8/v9 blobs and decodes them:

| blob | result |
|------|--------|
| v7 (218 B) | ok, layerCount 4 |
| v6 (218 B) | ok, layerCount 4 |
| v8 (270 B) | ok, layerCount 4 |
| v9 (271 B) | ok, layerCount 4, strip bleed → layer softness |

**Lesson:** a decoder for version N must never use the current version's size
constants. Sizes are part of the format, so they have to be frozen per version.

### Softness belongs to the layer

"That's not about blending layers inside a strip, it's the blur, right?" — correct,
and the strip-level `bleed()` was the wrong primitive: a convolution on the merged
frame cannot know whose edge it is softening, and on an already smooth blob it was
barely visible.

Now `LayerSettings::softnessPct` widens that layer's profile
(`halfWidth × (1 + softness)`), so at 100 % a blob is twice as wide and its edges
carry light outward. `LedStrip::bleed()` is deleted. The control moved from the strip
card into the layer's Display card, and a v9 strip bleed is carried into the layer
softness on load.

### Verified

- Host round trip and four migration cases (table above).
- Browser: 3 saved layers restored as 3 tabs, strip card is
  `brightness → layers`, Display card shows `intensity + softness 65%`, AGC ON.
- Both envs build clean.

## Roadmap / Next Up (DO NOT FORGET)

- [ ] **Hardware verification on M5StickS3**:
  - Verify NeoPixel colour byte order (`GRB` vs `RGB`) on the actual strips with the
    new real-time colour wheel.
  - Tune actual `ledCount` for each rock strip.
- [x] **Code Architecture Cleanup & Refactoring** — **DONE 2026-11-07**:
  - `display.h` / `display.cpp` now own the whole LCD renderer.
  - `settings.h` / `settings.cpp` merged `settings_blob.*` + `store.*`.
  - `main.cpp` reduced to orchestration (`setup`, `loop`, `renderStrip`, `handleButtons`).
- [x] **Multi-layer strips** — **DONE 2026-11-07**: physical strips are containers for
  up to 4 independent light layers (blob v5, nested Layer tabs, solo, per-layer pads).

---

## Future Exploration — Dedicated Phone App & Multi-Device Control

**Idea raised (2026-11-04):** Instead of each ESP32 running its own Wi-Fi SoftAP
and HTTP web server, use a dedicated app on the phone (or a shared Wi-Fi / BLE
transport) where multiple microcontrollers act as lightweight clients.

### Why this is compelling (especially for an installation with multiple rocks)

1. **One screen controls the whole room (`1 → N` instead of `1 → 1`)**:
   - With SoftAP, each ESP32 is its own isolated Wi-Fi network (`LightRocks-1`,
     `LightRocks-2`, …). To tweak 5 controllers, you would have to disconnect and
     reconnect Wi-Fi 5 times.
   - If the microcontrollers join a shared network (either the phone's hotspot, a
     small exhibition router, or via BLE), a single phone UI can control **all
     nodes at once** ("set all rocks to orange / 40% threshold") or individually.
2. **Tiny, connectionless packets instead of TCP + HTTP**:
   - Notice that serving the HTML page itself only happens once when you open the
     tab — the real load on the ESP32 comes from **TCP socket setup/teardown** for
     every slider drag and status poll.
   - With a dedicated app (or even a local HTML file / PWA on the phone), the ESP32
     only needs a **single UDP socket (or OSC / WebSocket / BLE GATT)** receiving
     tiny 10–24 byte binary or OSC packets (`0` TCP handshake, `0` HTTP headers,
     `0` socket exhaustion, sub-millisecond latency).
   - UDP broadcast (`255.255.255.255`) also lets one slider move on the phone
     update 10 ESP32s simultaneously in a single packet.
3. **Three practical ways to do it (from lightest to richest)**:
   - **Option A — BLE (Bluetooth Low Energy) + Web Bluetooth / Phone App**:
     - Phone stays on its normal Wi-Fi/5G; no Wi-Fi switching at all.
     - Very low power and rock-solid on ESP32-S3 (`NimBLE`), exposes a small GATT
       characteristic for the 46-byte `Settings` struct + 8-byte `Telemetry`
       notification.
   - **Option B — Wi-Fi Station + UDP / OSC (Open Sound Control)**:
     - All M5Sticks connect as clients to a phone hotspot or exhibition Wi-Fi and
       advertise via mDNS (`rock-1.local`, `rock-2.local`).
     - Standard tools like **TouchOSC** or a tiny custom app/page send UDP packets
       directly — zero HTTP server needed on the ESP32, and trivially scales to
       dozens of rocks.
   - **Option C — Keep the current Web UI, swap only the transport to WebSocket**:
     - If we still want "zero app install" for a single controller, replacing the
       repeated `POST /api/params` + `GET /api/state` HTTP fetches with **one
       persistent WebSocket** keeps the browser UI while eliminating 99% of the
       TCP overhead.

---

## Future Exploration — Streaming Audio Directly (Bluetooth A2DP vs. Wi-Fi Audio)

**Idea raised (2026-11-04):** Connect the device as a Bluetooth audio sink (A2DP)
so the lights react strictly to the streamed music signal rather than ambient party
noise in the room — and whether the new **M5StickS3** solves the infamous
Wi-Fi + Bluetooth coexistence nightmare from the older M5StickC Plus2.

### Technical reality check: Original ESP32 vs. ESP32-S3

1. **Why Wi-Fi + Bluetooth A2DP failed on M5StickC Plus2 (original ESP32)**:
   - On the original ESP32, Bluetooth Classic (BR/EDR, which A2DP uses) and Wi-Fi
     share a single 2.4 GHz radio front-end and antenna via time-division
     multiplexing. Continuous A2DP audio packets (~300 kbps) and Wi-Fi SoftAP
     beacons/TCP traffic fight for airtime, causing severe audio stuttering and
     Wi-Fi drops. Worse, enabling both the Bluedroid Classic BT stack and the
     Wi-Fi stack consumes >180 KB of internal SRAM, starving the heap.
2. **Important hardware catch on the new M5StickS3 (`ESP32-S3`)**:
   - **Espressif removed Bluetooth Classic (BR/EDR) from the ESP32-S3 silicon!**
   - The ESP32-S3 only has **Bluetooth Low Energy (BLE 5.0)**. Because standard
     Bluetooth speakers use **Bluetooth Classic A2DP**, the `ESP32-A2DP` library
     cannot run on an ESP32-S3 at all (it only compiles for the original ESP32).
   - *(Note: BLE 5.0 DOES coexist much better with Wi-Fi on ESP32-S3 than Classic
     BT ever did on the old ESP32 — so BLE control + Wi-Fi works fine — but BLE
     cannot act as a standard Classic A2DP Bluetooth speaker.)*
3. **Practical ways to get clean, ambient-noise-free music into the rocks**:
   - **Approach 1 — Stream Audio or FFT Bands over Wi-Fi UDP (Best for M5StickS3)**:
     - Instead of Bluetooth A2DP, a tiny sender script/app on the laptop or phone
       playing the music computes the FFT (or captures loopback audio) and
       broadcasts 60 fps UDP packets over Wi-Fi to all M5StickS3 rocks in the room.
     - Uses **only Wi-Fi** (zero Bluetooth coexistence issues!), works on
       M5StickS3, synchronises any number of rocks simultaneously, and is 100%
       immune to people talking or clapping in the room.
   - **Approach 2 — Dedicated A2DP Bridge Node**:
     - Use one old M5StickC Plus2 (original ESP32, with its Wi-Fi turned OFF) as a
       Bluetooth A2DP receiver that runs the FFT and sends the band levels over
       UART/I2C or ESP-NOW to the LED controllers.
   - **Approach 3 — Tiny External BT Audio Receiver Module**:
     - A miniature $2 Bluetooth audio receiver board (or line-in splitter from the
       DJ/speaker mixer) feeding an analog/I2S input on the microcontroller, or
       switching the M5StickS3's ES8311 codec / ADC to line-in.



