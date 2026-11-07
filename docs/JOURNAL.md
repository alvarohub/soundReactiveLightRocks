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


## Roadmap / Next Up (DO NOT FORGET)

- [ ] **Hardware verification on M5StickS3**:
  - Verify NeoPixel colour byte order (`GRB` vs `RGB`) on the actual strips with the
    new real-time colour wheel.
  - Tune actual `ledCount` for each rock strip.
- [x] **Code Architecture Cleanup & Refactoring** — **DONE 2026-11-07**:
  - `display.h` / `display.cpp` now own the whole LCD renderer.
  - `settings.h` / `settings.cpp` merged `settings_blob.*` + `store.*`.
  - `main.cpp` reduced to orchestration (`setup`, `loop`, `renderStrip`, `handleButtons`).
- [ ] **Multi-layer strips ("virtual strips")** — physical strips become containers
  for several independent light layers; see `HANDOFF.md` Priority 2.

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



