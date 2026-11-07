# Sound-Reactive Light Rocks — Specification

> Living document. It describes what the firmware is **meant** to do, not what it
> happens to do today. When behaviour and this file disagree, one of them is a bug.
> Progress against this spec is tracked in [JOURNAL.md](./JOURNAL.md).

Status: **v0.3 — per-strip trigger inputs (threshold / auto beat / manual pads), four effects (jet, flash, burst, sparkle), full-screen fingertip pad view, M5StickS3 + M5StickC Plus2**.

---

## 1. Purpose

Drive two NeoPixel strips so they react to music from the room:

* the strips are embedded in plastic rocks, so only their glow is visible;
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

## 5. Trigger inputs and effects

### 5.1 Trigger input (`mode`) — chosen independently per strip

| Value | Name | Behaviour |
|-------|------|-----------|
| `0` | **Threshold** | The effect fires when the strip's band level reaches `thresholdPct`. |
| `1` | **Auto beat** | The effect fires on detected beats *inside this strip's frequency window*. Each strip owns its own `BeatDetector`, fed by `bandFlux(centerHz, widthHz)`. |
| `2` | **Manual (pads)** | The microphone is ignored; only the fingertip pads and **BtnA** fire the effect. |

`thresholdPct` and the beat detector are therefore two interchangeable *inputs* to
the same effect pipeline, not two different renderers.

### 5.2 Effects (`effect`) — one chosen per strip

| Value | Name | Visual |
|-------|------|--------|
| `0` | **Jet** | A travelling blob / jet of light that moves per `speed`, `reverse` and `loop`, and fades over `fadeMs`. |
| `1` | **Flash** | The whole strip lights up in the strip colour and fades out over `fadeMs`. Holding the pad sustains full brightness. |
| `2` | **Burst** | A shockwave that splits from the strip centre outward toward both ends. |
| `3` | **Sparkle** | A glittering shower: a pseudo-random subset of LEDs lights up per frame and decays. Holding the pad keeps the shower alive. |

### 5.3 Beat detection (per strip)

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

Because each strip has its own detector, a bass-band strip and a vocal-band strip
can lock to different rhythms — that is what makes the two rocks feel independent.

### 5.4 Retrigger

* `retrigger = 1`: a new effect may fire once `retriggerMs` has elapsed since the
  last one on that strip.
* `retrigger = 0` (**Single**): nothing new fires while any effect is still active.

## 6. Mode 2 — reserved

Reserved for the next architectural step: multiple independent *layers* rendering
into one physical strip (several light beams with their own thresholds, colours and
beat behaviour, summed in the linear frame buffer). The accumulation buffer already
exists in `LedStrip`, so this needs no driver change.

---

## 7. Parameters

`ledCount` is per strip because the physical count is not fixed by the firmware —
each build can differ, and it can be re-tuned at any time. Everything is clamped on
the way in, so no value coming over HTTP (or out of flash) can reach the strip
driver out of range.

### Per strip

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| `mode` | 0/1/2 | 0 | Trigger input: `0` = band threshold, `1` = auto beat, `2` = manual pads |
| `effect` | 0/1/2/3 | 0 | `0` = jet, `1` = flash, `2` = burst, `3` = sparkle |
| `enabled` | 0/1 | 1 | Power. Off = strip dark, settings kept |
| `brightnessPct` | 1…100 | 100 | Global max brightness ceiling for this strip |
| `ledCount` | 1…1000 | 100 | Actual number of LEDs on this strip (shows calibration dot at `ledCount-1` while tuning) |
| `centerHz` | 65…7600 | 150 / 1000 | Band centre. Strip 1 bass, strip 2 mids by default |
| `widthHz` | 40…16000 | 250 / 1400 | Band width; wide = behaves like broadband. Also the beat detector's listening window |
| `thresholdPct` | 0…100 | 35 / 40 | Band level that fires the effect in **Threshold** mode (drag directly on the live level meter) |
| `beatSensitivityPct` | 0…100 | 55 | Beat detector eagerness in **Auto beat** mode (0 = only obvious beats) |
| `retrigger` | 0/1 | 1 | `1` = retrigger after `retriggerMs`, `0` = single (wait until the strip is idle) |
| `retriggerMs` | 0…2000 | 300 | Minimum gap between effects on one strip |
| `blobLeds` | 1…1000 | 8 / 5 | Length of the light group (jet / burst) |
| `speed` | 1…500 | 45 / 80 | LEDs per second |
| `fadeMs` | 50…10000 | 2000 / 1200 | Time for an effect to fade to nothing |
| `color` | `000000`…`FFFFFF` | `FF6A00` / `0066FF` | Effect colour |
| `reverse` | 0/1/2 | 0 | `0` = forward (`0→N-1`), `1` = reverse (`N-1→0`), `2` = ping-pong (`0↔N-1`) |
| `loop` | 0/1 | 1 | `1` = loop / continuous bounce, `0` = one-shot |

### Global

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| `gainDb` | 0, 6, 12, 18 | 0 | Microphone gain, hardware quantised |

Changing `ledCount` re-initialises the strip driver. It is safe but not free, so the
UI only sends it when the slider is released.

---

## 8. Web interface

* The board raises an access point **`LightRocks`** (password `lightrocks`).
* `http://192.168.4.1/` serves a single dark page, layout designed for a phone.
* The content takes **5/6 of the viewport width**, leaving the right **1/6**
  (`16.7vw`) as an empty thumb-scroll gutter so scrolling never moves sliders or
  taps buttons by accident.
* Strip 1 / Strip 2 tabs select which strip the parameter card edits.
* An inline **Canvas Colour Wheel** + brightness slider + current colour square
  streams colour changes in real time while dragging.
* A **Trigger input** selector switches that strip between `Threshold`,
  `Auto Beat` and `Manual (Pads)`; the panel below shows only what is relevant
  (level bar + threshold marker, beat pulse + BPM + sensitivity, …).
* A **Triggered effect** selector chooses among `Jet`, `Flash`, `Burst`, `Sparkle`.
* **Full-Screen Effect Pads**: a dedicated overlay split into one group per strip,
  each with the four effect pads. Pads are multi-touch and support press-and-hold
  (Flash and Sparkle sustain; Jet re-fires at the retrigger gap). Each group shows
  that strip's live trigger state and output level.
* Live readouts: broadband level, dBFS, dominant frequency, per-strip beat pulse /
  BPM, active effect count, and a bar showing the level against the threshold.

### API

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | The control page |
| GET | `/api/state` | Full state + telemetry + preset availability as JSON |
| POST | `/api/params?strip=<i>&<key>=<value>`, `POST /api/params?gainDb=<n>` | Change live settings (204, no body) |
| POST | `/api/tap?strip=<i>&effect=<0..3>&down=<0\|1>` | Fire an effect from a pad; `down=1` also starts/refreshes the hold |
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
* The settings blob is versioned (`v4`, 26 bytes per strip) and fixed-layout.
  A `v3` blob is migrated automatically on load (missing fields take their
  defaults); any other version falls back to the code defaults.

---

## 10. Display and buttons

| Control | Action |
|---------|--------|
| Screen | Spectrum bars, band markers for the selected strip, two level meters, AP status |
| BtnA click | Manual test trigger on both strips (tune the look without music) |
| BtnA hold | Toggle the access point |
| BtnB click | Cycle mic gain 0 → 6 → 12 → 18 → 0 |
| BtnB hold | Save settings to flash |
| Boot | 3 s banner with SSID, password and URL |

---

## 11. Non-goals (for now)

* No station mode / no router connection.
* No OTA updates.
* No per-blob colour variation, no hue animation along the strip.
* No audio recording or playback — the microphone is read-only, one frame at a time.
