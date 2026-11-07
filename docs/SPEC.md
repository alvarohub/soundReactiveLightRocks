# Sound-Reactive Light Rocks — Specification

> Living document. It describes what the firmware is **meant** to do, not what it
> happens to do today. When behaviour and this file disagree, one of them is a bug.
> Progress against this spec is tracked in [JOURNAL.md](./JOURNAL.md).

Status: **v0.5 — physical strips as containers for up to 4 light layers, five non-exclusive trigger inputs (threshold, beat, pads, accelerometer, gyroscope), five effects including the whip, per-layer intensity, solo preview, per-layer pad view, M5StickS3 + M5StickC Plus2**.

---

## 1. Purpose

Drive two NeoPixel strips so they react to music from the room:

* the strips are embedded in plastic rocks, so only their glow is visible;
* each physical strip can hold **several independent light layers** (up to 4), each
  with its own band, trigger input, effect, colour and intensity weight, all blended
  into the same LEDs;
* each strip can operate either as a **closed ring (Loop)** — light that leaves one
  end comes back at the other — or as a **linear strand (One-shot)** where blobs
  disappear once they travel past the last LED;
* when the music gets loud in a chosen frequency band, **or** a beat is detected in
  it, a **light effect** fires along the strip and fades out;
* the same effects can be played by hand at any time from a **full-screen fingertip
  pad view**, one group of pads per strip.

Everything is tuned from a phone: the board serves its own Wi-Fi access point and a
small web page. Live settings are saved automatically to flash, and three preset
banks let you store and recall favourite setups.

---

## 2. Hardware

| Part | Notes |
|------|-------|
| **M5StickS3** (primary target) | ESP32-S3-PICO-1-N8R8, built-in MEMS mic + ES8311 codec, 135×240 LCD |
| **M5StickC Plus2** (also supported) | ESP32-PICO-V3-02, built-in SPM1423 PDM mic, 135×240 LCD |
| 2 × NeoPixel strip (WS2812B / SK6812, RGB) | Count set per strip in the UI (default 100) |
| External 5 V supply for the strips | 100 LEDs at full white ≈ 6 A. **Never** power high-current strips from the Grove 5 V pin |
| Grove cable | Carries both data lines |

### Pins

| Signal | M5StickS3 | M5StickC Plus2 | Grove wire |
|--------|-----------|----------------|------------|
| Strip 1 data | **G9** | **G32** | yellow |
| Strip 2 data | **G33** → **G10** on StickS3 | **G33** | white |
| Ground | GND | GND | black — must be shared with the strip supply |

*(Correction in table: on M5StickS3, Strip 1 is **G9** and Strip 2 is **G10**; on M5StickC Plus2, Strip 1 is **G32** and Strip 2 is **G33**.)*

Data lines carry 3.3 V logic. A WS2812B powered at 5 V usually accepts 3.3 V data,
but if the strips misbehave, level-shift the data line (e.g. 74HCT125) or drop the
supply to 4.3 V.

Moving a strip to another pin is a one-line edit in
[src/config.h](../src/config.h) (`kStripPins`).

---

## 3. Signal chain

```
built-in mic ──► 512-sample frame @ 16 kHz ──► Hann window ──► real FFT
                                                                    │
                                        ┌───────────────────────────┘
                                        ▼
                        magnitude spectrum, 256 bins (31.25 Hz each)
                                        │
                     ┌──────────────────┴──────────────────┐
                     ▼                                     ▼
       band level for strip 1                band level for strip 2
       (its center/width window)             (its center/width window)
                     │                                     │
                     ▼                                     ▼
              threshold test  ──trigger──►  blob spawned on that strip
                                                   │
                                                   ▼
                                    additive render ──► gamma ──► strip.show()
```

* One audio frame takes 32 ms at 16 kHz, so the whole pipeline — and therefore the
  strip refresh rate — runs at ≈ 31 Hz.
* `bandLevel()` reports RMS amplitude inside `center ± width/2`, mapped to 0…1 on
  the same scale as the broadband level, so a threshold means the same thing for
  any window.
* The FFT backend is chosen at build time (ESP-DSP by default, arduinoFFT
  optional) and the two are numerically interchangeable.

---

## 4. Motion and shape

These rules apply to the **Jet** and **Burst** effects.

### 4.1 Motion (`Loop` vs. `One-shot` & `Forward` / `Reverse` / `Ping-pong`)

* A blob has a position measured in LEDs (floating point) and moves at
  `speed` LEDs per second.
* **Direction (`reverse`)** has three modes per strip:
  - **Forward (`reverse = 0`)**: new blobs spawn at LED `0` and move toward
    `ledCount - 1` (`+speed`).
  - **Reverse (`reverse = 1`)**: new blobs spawn at LED `ledCount - 1` and move
    toward `0` (`-speed`).
  - **Ping-pong (`reverse = 2`)**: new blobs spawn at LED `0`, move toward
    `ledCount - 1`, and bounce back at the ends (`0 ↔ ledCount - 1`).
* **Topology (`loop`)** is configurable per strip (`Loop` / `One-shot`):
  - **Loop (`loop = 1`)**: in Forward/Reverse, a blob wraps seamlessly around the
    ring (`0 ↔ ledCount - 1`). In Ping-pong, it bounces back and forth between
    `0` and `ledCount - 1` until `fadeMs` expires.
  - **One-shot (`loop = 0`)**: in Forward/Reverse, a blob travels once and
    disappears past the far end. In Ping-pong, it completes one out-and-back
    round-trip (`0 → ledCount - 1 → 0`) and disappears past `0`.

### 4.2 Shape

* The blob spans `blobLeds` LEDs, centred on its position.
* Brightness follows a raised-cosine profile across that span: full at the centre,
  zero at the edges. This avoids the hard edges of a square pulse and keeps the
  motion looking smooth at any speed.

### 4.3 Lifetime

* Every blob ages. Amplitude falls linearly from 1 to 0 over `fadeMs`, then the blob
  is removed (or earlier if `loop = 0` and it runs off the end of the strip).
* In Loop mode, `fadeMs` sets how long the ring keeps glowing after the sound
  stops: distance travelled = `speed × fadeMs`.
* Blobs are drawn **additively**.

### 4.4 Colour and output

* Each strip has one colour, chosen on an inline **real-time colour wheel** (with
  a live colour swatch square) in the Web UI. Dragging on the wheel updates the
  strip colour in real time.
* Gamma (`γ = 2.2`) is applied to the blob's brightness envelope rather than
  independently crushing each RGB channel during fade-out, so secondary channels
  keep their exact hue ratio as the blob dims. If overlapping blobs exceed `1.0`,
  RGB channels are scaled proportionally to preserve hue.
* Only `ledCount` LEDs are transmitted per strip — see §7.

---

## 5. Two objects: the physical strip and the light layer

### 5.0 Containers and layers

The thing that glows is **not** the strip. Two separate objects exist:

| Object | Owns | Count |
|--------|------|-------|
| **Physical strip** | GPIO pin, RMT channel, `ledCount`, master `brightnessPct`, one linear float frame buffer | 2 (one per Grove pin) |
| **Light layer** | band, trigger input, effect, colour, motion, `intensityPct` weight | up to **4 per physical strip** |

Layers never talk to each other or to the hardware. Every layer *adds* light into
its strip's shared **linear** buffer (`buf[i] += amp * colour`), and the strip then
gamma-encodes and transmits **once**:

```
PhysicalStrip 1 ── ledCount 120, brightness 90%
   ├── Layer 1  bass   160 Hz ·  threshold · jet     · orange · 100% weight
   ├── Layer 2  highs 4 kHz   ·  auto beat · sparkle · amber  ·  70% weight
   └── Layer 3  mids  800 Hz  ·  manual    · flash   · green  · 100% weight
                          ↓ additive accumulation → one show()
```

Why this matters:

* **Overlaps add as light.** Two beams crossing, or a flash under a jet, sum
  correctly, because gamma is applied once at the output edge and never twice.
* **Hue is preserved.** When the sum exceeds 1.0 the channels are scaled
  proportionally, so a dim secondary channel never collapses to black.
* **Independence is real.** Each layer has its own `BeatDetector`, so the bass
  layer and the vocal layer can lock to different rhythms inside one strip.

`ledCount` and `brightnessPct` belong to the **strip**; everything about *what the
light does* belongs to the **layer**.

### 5.1 Trigger inputs (`triggers`) — a bitmask, NOT exclusive

A layer can be armed by several sources at once and fires on whichever happens
first. The GUI shows them as tickable chips.

| Bit | Value | Source | Gate |
|-----|-------|--------|------|
| `kTrigThreshold` | 1 | band level | `thresholdPct` |
| `kTrigBeat` | 2 | beat detected in this layer's band | `beatSensitivityPct` |
| `kTrigTap` | 4 | fingertip pad (or BtnA test trigger) | — |
| `kTrigAccel` | 8 | the stick is whipped / shaken | `motionThresholdPct` on \|a\| − 1 g |
| `kTrigGyro` | 16 | the stick is twisted quickly | `motionThresholdPct` on \|ω\| |

* `triggers = 0` means the layer is silent: no source can fire it.
* The pad path is gated by `kTrigTap`, so unticking it really does stop pads from
  reaching that layer; BtnA scrolls strips and BtnB cycles the trigger set.
* Motion is read once per frame from the on-board IMU. `swing` is the magnitude of
  the acceleration **with gravity removed**, so a stick lying still reads 0 and a
  whip reads large.

### 5.1b The whip effect (`kEffectWhip`)

Designed for the accelerometer: a blob is launched along the strip and

* **travel speed scales with how hard the stick was swung**
  (`speedScale = 0.35 + 2.4 · swing / 2 g`, so 1× at a gentle flick and ~2.8× at a
  hard whip);
* **travel direction** follows the sign of the horizontal acceleration, so whipping
  left and right sends the light opposite ways;
* **colour comes from the swing direction** (`atan2(ay, ax)` → hue), so each
  direction of a whip has its own colour — the effect reads as a lash of light
  rather than a generic blob.

### 5.2 Effects (`effect`) — one chosen per layer

| Value | Name | Visual |
|-------|------|--------|
| `0` | **Jet** | A travelling blob / jet of light that moves per `speed`, `reverse` and `loop`, and fades over `fadeMs`. |
| `1` | **Flash** | The whole strip lights up in the strip colour and fades out over `fadeMs`. Holding the pad sustains full brightness. |
| `2` | **Burst** | A shockwave that splits from the strip centre outward toward both ends. |
| `3` | **Sparkle** | A glittering shower: a pseudo-random subset of LEDs lights up per frame and decays. Holding the pad keeps the shower alive. |
| `4` | **Whip** | A lash of light whose speed follows the swing strength and whose colour follows the swing direction — see §5.1b. |

### 5.3 Beat detection (per layer)

Standard onset detection, running at the ~31 fps analysis rate:

```
audio frame → FFT → band-limited spectral flux → adaptive threshold → onset
                                             ↓
                          inter-onset intervals → median → BPM
```

* **Onset function**: `bandFlux()` sums only the *positive* frame-to-frame magnitude
  change inside `center ± width/2`, scaled by mic gain and normalised for window
  width, so narrow and wide windows trigger comparably.
* **Threshold**: adaptive, `mean + k·σ` over a ~900 ms window, with `k` from
  `beatSensitivityPct` (3.2σ at 0 %, 0.6σ at 100 %).
* **Refractory**: 180 ms minimum interval (rejects double triggers, caps at 333 BPM).
* **Tempo**: median of the last 8 inter-onset intervals, folded into 60–170 BPM.
* **Pulse**: a raised decay (`τ ≈ 120 ms`) published as `beatPct` for the UI meter.

Because every layer has its own detector, two layers inside the *same* physical
strip can lock to different rhythms — that is where the layered look comes from.

### 5.4 Retrigger

* `retrigger = 1`: a new effect may fire once `retriggerMs` has elapsed since the
  last one **on that layer**.
* `retrigger = 0` (**Single**): nothing new fires while that layer is still busy.

## 6. Solo preview

The GUI has a **Solo** button per layer tab. While solo is active only that layer
renders; every other layer of that physical strip idles. Solo is a preview tool, so
it is **runtime-only** and never written to flash.

---

## 7. Parameters

`ledCount` is per strip because the physical count is not fixed by the firmware —
each build can differ, and it can be re-tuned at any time. Everything is clamped on
the way in, so no value coming over HTTP (or out of flash) can reach the strip
driver out of range.

### Per physical strip (the container)

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| `ledCount` | 1…1000 | 100 | Actual number of LEDs on this strip (shows the calibration dot at `ledCount-1` while tuning) |
| `brightnessPct` | 1…100 | 100 | Master output ceiling for the whole strip, applied after all layers are merged |
| `layerCount` | 1…4 | 1 | How many light layers are live inside this strip |

### Per light layer (the glowing object)

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| `triggers` | bitmask 0…31 | 1 | Any combination of threshold 1, beat 2, tap 4, accel 8, gyro 16 |
| `effect` | 0…4 | 0 | `0` = jet, `1` = flash, `2` = burst, `3` = sparkle, `4` = whip |
| `enabled` | 0/1 | 1 | Layer power. Off = this layer contributes nothing, settings kept |
| `intensityPct` | 1…100 | 100 | This layer's weight in the merged output |
| `centerHz` | 65…7600 | 150 / 1000 | Band centre (strip 1 layer 1 bass, strip 2 layer 1 mids by default) |
| `widthHz` | 40…16000 | 250 / 1400 | Band width; also the beat detector's listening window |
| `thresholdPct` | 0…100 | 35 / 40 | Level that fires the effect in **Threshold** mode (drag the white marker) |
| `beatSensitivityPct` | 0…100 | 55 | Detector eagerness for the beat input |
| `motionThresholdPct` | 0…100 | 30 | Gate for the accelerometer/gyroscope inputs |
| `retrigger` | 0/1 | 1 | `1` = retrigger after `retriggerMs`, `0` = single (wait until idle) |
| `retriggerMs` | 0…2000 | 300 | Minimum gap between effects on this layer |
| `blobLeds` | 1…1000 | 8 / 5 | Length of the light group (jet / burst) |
| `speed` | 1…500 | 45 / 80 | LEDs per second |
| `fadeMs` | 50…10000 | 2000 / 1200 | Time for an effect to fade to nothing |
| `color` | `000000`…`FFFFFF` | `FF6A00` / `0066FF` | Layer colour |
| `reverse` | 0/1/2 | 0 | `0` = forward (`0→N-1`), `1` = reverse (`N-1→0`), `2` = ping-pong |
| `loop` | 0/1 | 1 | `1` = loop / continuous bounce, `0` = one-shot |

Layer 2 of strip 1 ships pre-configured (sparkle, 4 kHz window, 70 % weight) so
"add layer" immediately does something visible.

### Global

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| `gainDb` | 0, 6, 12, 18 | 0 | Microphone gain, hardware quantised |

`ledCount` is a property of the physical strip, so it is sent without a `layer`.
Changing it re-initialises the strip driver: safe but not free, so the UI only sends
it when the slider is released.

---

## 8. Web interface

* The board raises an access point **`LightRocks`** (password `lightrocks`).
* `http://192.168.4.1/` serves a single dark page, layout designed for a phone.
* The content takes **5/6 of the viewport width**, leaving the right **1/6**
  (`16.7vw`) as an empty thumb-scroll gutter so scrolling never moves sliders or
  taps buttons by accident.
* **Strip 1 / Strip 2** tabs select the physical container; **Layer 1..4** tabs
  inside it select which layer the cards below edit.
* **+ Add layer** / **Remove** change how many layers that strip holds; **Solo**
  previews one layer alone.
* An inline **Canvas Colour Wheel** + brightness slider + current colour square
  streams colour changes in real time while dragging.
* **Trigger inputs** are five tickable chips (`Threshold`, `Auto beat`, `Tap pads`,
  `Accelerometer`, `Gyroscope`). Several can be armed at once; only the relevant
  controls below are shown (level bar + marker, beat sensitivity, motion
  threshold).
* A **Triggered effect** selector chooses among `Jet`, `Flash`, `Burst`, `Sparkle`,
  `Whip`.
* **Layer intensity** scales one layer against the others; **Strip max brightness**
  scales the merged result.
* **Full-Screen Effect Pads**: a dedicated overlay with **one group per layer**
  (strip + layer in the heading), each with the four effect pads. Pads are
  multi-touch and support press-and-hold (Flash and Sparkle sustain; Jet re-fires at
  the retrigger gap). Each group shows that layer's live trigger state and output.
* Live readouts: broadband level, dBFS, dominant frequency, per-strip beat pulse /
  BPM, active effect count, and a bar showing the level against the threshold.

### API

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | The control page |
| GET | `/api/state` | Full state + telemetry + preset availability as JSON |
| POST | `/api/params?strip=<p>&<key>=<value>` | Change the **physical** strip (`ledCount`, `brightnessPct`, `layerCount`, `solo`) |
| POST | `/api/params?strip=<p>&layer=<l>&<key>=<value>` | Change one **layer** (204, no body) |
| POST | `/api/params?gainDb=<n>` | Microphone gain |
| POST | `/api/tap?strip=<p>&layer=<l>&effect=<0..3>&down=<0\|1>` | Fire an effect from a pad; `down=1` also starts/refreshes the hold |
| POST | `/api/preset?slot=<1\|2\|3>&action=<save\|load>` | Save current state to preset bank 1–3, or load a bank into live state |
| POST | `/api/reset` | Factory reset (restores hardcoded code defaults, keeps presets) |

Unknown arguments are ignored; out-of-range values are clamped.

---

## 9. Persistence & Presets

* All live parameters are written automatically to NVS (`Preferences`, namespace
  `lightrocks`, key `live`).
* Writes are **debounced** (2 s after the last change) so dragging sliders or
  the colour wheel does not wear flash. On power-up, `live` is loaded automatically.
* **Three preset banks** (`preset0`, `preset1`, `preset2`) can be saved and loaded
  from the UI. Loading a preset overwrites the current live parameters (and saves
  them to `live`).
* **Factory reset** restores the hardcoded defaults from `defaultSettings()` in
  code (never overwritten by user saves) without deleting the 3 preset banks.
* The settings blob is versioned (`v6`: 4-byte physical block + 26-byte layer block
  × 4 layers per strip, 210 bytes total) and fixed-layout.
  A `v5`, `v4` or `v3` blob is migrated automatically on load; an old single
  `mode` enum becomes the matching trigger bit. Any other version falls back to
  defaults.

---

## 10. Display and buttons

| Control | Action |
|---------|--------|
| Screen | Spectrum bars, the dominant layer's band per strip, one merged output meter per strip (`S1`/`B1`/`M1`, plus `.N` when the strip holds N layers), AP status |
| BtnA click | Select the next physical strip (shown with `>` on the LCD) |
| BtnA hold | Toggle the access point |
| BtnB click | Cycle the trigger set of every live layer on the selected strip |
| BtnB hold | Save settings to flash |
| Boot | 3 s banner with SSID, password and URL |

---

## 11. Non-goals (for now)

* No station mode / no router connection.
* No OTA updates.
* No per-blob colour variation, no hue animation along the strip.
* No audio recording or playback — the microphone is read-only, one frame at a time.
