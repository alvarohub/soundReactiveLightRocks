# Sound-Reactive Light Rocks — Specification

> Living document. It describes what the firmware is **meant** to do, not what it
> happens to do today. When behaviour and this file disagree, one of them is a bug.
> Progress against this spec is tracked in [JOURNAL.md](./JOURNAL.md).

Status: **v0.2 — Mode 1 (loop & one-shot blobs, presets, real-time colour wheel, M5StickS3 + M5StickC Plus2)**. Mode 2 is still a placeholder.

---

## 1. Purpose

Drive two NeoPixel strips so they react to music from the room:

* the strips are embedded in plastic rocks, so only their glow is visible;
* each strip can operate either as a **closed ring (Loop)** — light that leaves one
  end comes back at the other — or as a **linear strand (One-shot)** where blobs
  disappear once they travel past the last LED;
* when the music gets loud in a chosen frequency band, a **blob of light** runs
  along the strip and fades out.

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

## 4. Mode 1 — Orbiting blobs

### 4.1 Trigger & Retrigger

* Each frame, the strip's band level is compared with its **threshold** (which is
  adjusted directly by dragging the white marker on the strip's live level meter).
* When `retrigger = 1` (**Retrig ON**): a blob is spawned when `level >= threshold`
  and at least `retriggerMs` has passed since that strip's last spawn.
* When `retrigger = 0` (**Single / No retrigger**): a new blob cannot be spawned
  while any blob is still active on that strip (`activeCount() > 0`).
* At most `kMaxBlobs` (12) blobs exist per strip. Beyond that the **dimmest** blob
  is recycled, so a busy passage stays continuous instead of going silent.

### 4.2 Motion (`Loop` vs. `One-shot` & `Forward` / `Reverse` / `Ping-pong`)

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

### 4.3 Shape

* The blob spans `blobLeds` LEDs, centred on its position.
* Brightness follows a raised-cosine profile across that span: full at the centre,
  zero at the edges. This avoids the hard edges of a square pulse and keeps the
  motion looking smooth at any speed.

### 4.4 Lifetime

* Every blob ages. Amplitude falls linearly from 1 to 0 over `fadeMs`, then the blob
  is removed (or earlier if `loop = 0` and it runs off the end of the strip).
* In Loop mode, `fadeMs` sets how long the ring keeps glowing after the sound
  stops: distance travelled = `speed × fadeMs`.
* Blobs are drawn **additively**.

### 4.5 Colour and output

* Each strip has one colour, chosen on an inline **real-time colour wheel** (with
  a live colour swatch square) in the Web UI. Dragging on the wheel updates the
  strip colour in real time.
* Gamma (`γ = 2.2`) is applied to the blob's brightness envelope rather than
  independently crushing each RGB channel during fade-out, so secondary channels
  keep their exact hue ratio as the blob dims. If overlapping blobs exceed `1.0`,
  RGB channels are scaled proportionally to preserve hue.
* Only `ledCount` LEDs are transmitted per strip — see §6.

---

## 5. Mode 2 — not designed yet

Reserved. Candidates discussed so far: a spectrum/vu meter running down the strip,
or level-driven brightness with a travelling wave. Nothing is implemented; the
`mode` field exists in the settings blob so adding it needs no storage migration.

---

## 6. Parameters

`ledCount` is per strip because the physical count is not fixed by the firmware —
each build can differ, and it can be re-tuned at any time. Everything is clamped on
the way in, so no value coming over HTTP (or out of flash) can reach the strip
driver out of range.

### Per strip

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| `enabled` | 0/1 | 1 | Power. Off = strip dark, settings kept |
| `brightnessPct` | 1…100 | 100 | Global max brightness ceiling for this strip |
| `ledCount` | 1…1000 | 100 | Actual number of LEDs on this strip (shows calibration dot at `ledCount-1` while tuning) |
| `centerHz` | 65…7600 | 150 / 1000 | Band centre. Strip 1 bass, strip 2 mids by default |
| `widthHz` | 40…16000 | 250 / 1400 | Band width; wide = behaves like broadband |
| `thresholdPct` | 0…100 | 35 / 40 | Band level that triggers a blob (drag directly on the live level meter) |
| `retrigger` | 0/1 | 1 | `1` = retrigger after `retriggerMs`, `0` = single blob (wait until strip is empty) |
| `retriggerMs` | 0…2000 | 300 | Minimum gap between blobs on one strip |
| `blobLeds` | 1…1000 | 8 / 5 | Length of the light group |
| `speed` | 1…500 | 45 / 80 | LEDs per second |
| `fadeMs` | 50…10000 | 2000 / 1200 | Time for a blob to fade to nothing |
| `color` | `000000`…`FFFFFF` | `FF6A00` / `0066FF` | Blob colour |
| `reverse` | 0/1/2 | 0 | `0` = forward (`0→N-1`), `1` = reverse (`N-1→0`), `2` = ping-pong (`0↔N-1`) |
| `loop` | 0/1 | 1 | `1` = loop / continuous bounce, `0` = one-shot |

### Global

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| `gainDb` | 0, 6, 12, 18 | 0 | Microphone gain, hardware quantised |

Changing `ledCount` re-initialises the strip driver. It is safe but not free, so the
UI only sends it when the slider is released.

---

## 7. Web interface

* The board raises an access point **`LightRocks`** (password `lightrocks`).
* `http://192.168.4.1/` serves a single dark page, layout designed for a phone.
* The content takes **5/6 of the viewport width**, leaving the right **1/6**
  (`16.7vw`) as an empty thumb-scroll gutter so scrolling never moves sliders or
  taps buttons by accident.
* Strip 1 / Strip 2 tabs select which strip the parameter card edits.
* An inline **Canvas Colour Wheel** + brightness slider + current colour square
  streams colour changes in real time while dragging.
* Live readouts: broadband level, dBFS, dominant frequency, active blob count per
  strip, and a bar showing each strip's band level against its threshold.

### API

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | The control page |
| GET | `/api/state` | Full state + telemetry + preset availability as JSON |
| POST | `/api/params?strip=<i>&<key>=<value>`, `POST /api/params?gainDb=<n>` | Change live settings, returns the new state |
| POST | `/api/preset?slot=<1\|2\|3>&action=<save\|load>` | Save current state to preset bank 1–3, or load a bank into live state |
| POST | `/api/reset` | Factory reset (restores hardcoded code defaults, keeps presets) |

Unknown arguments are ignored; out-of-range values are clamped.

---

## 8. Persistence & Presets

* All live parameters are written automatically to NVS (`Preferences`, namespace
  `lightrocks`, key `live`).
* Writes are **debounced** (800 ms after the last change) so dragging sliders or
  the colour wheel does not wear flash. On power-up, `live` is loaded automatically.
* **Three preset banks** (`preset0`, `preset1`, `preset2`) can be saved and loaded
  from the UI. Loading a preset overwrites the current live parameters (and saves
  them to `live`).
* **Factory reset** restores the hardcoded defaults from `defaultSettings()` in
  code (never overwritten by user saves) without deleting the 3 preset banks.
* The settings blob is versioned (`v2`, 22 bytes per strip) and fixed-layout.

---

## 9. Display and buttons

| Control | Action |
|---------|--------|
| Screen | Spectrum bars, band markers for the selected strip, two level meters, AP status |
| BtnA click | Manual test trigger on both strips (tune the look without music) |
| BtnA hold | Toggle the access point |
| BtnB click | Cycle mic gain 0 → 6 → 12 → 18 → 0 |
| BtnB hold | Save settings to flash |
| Boot | 3 s banner with SSID, password and URL |

---

## 10. Non-goals (for now)

* No station mode / no router connection.
* No OTA updates.
* No per-blob colour variation, no hue animation along the strip.
* No audio recording or playback — the microphone is read-only, one frame at a time.
