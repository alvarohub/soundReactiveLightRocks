# Sound-Reactive Light Rocks — Specification

> Living document. It describes what the firmware is **meant** to do, not what it
> happens to do today. When behaviour and this file disagree, one of them is a bug.
> Progress against this spec is tracked in [JOURNAL.md](./JOURNAL.md).

Status: **v0.7 — physical strips as containers for up to 4 light layers, five non-exclusive trigger inputs (threshold, beat, pads, accelerometer, gyroscope), five effects including the whip, per-layer intensity, solo preview, per-layer pad view, M5StickS3 + M5StickC Plus2**.

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

### 3.0 Two independent rates

The microphone and the strip do **not** run at the same rate, and the firmware no
longer pretends they do:

* an **audio task** owns the blocking capture path (`M5.Mic` is I2S/PDM with DMA,
  but `record()` waits for a full block), runs the FFT and publishes an
  `AudioSnapshot` — the broadband level, the 30 display bands and, per layer, its
  band level and spectral flux;
* the **render loop** runs at up to ~100 fps, reads the newest snapshot and never
  touches the FFT. It advances every animation from its own clock.

Onset detection runs **once per audio block** (guarded by the snapshot sequence
number) so each flux sample is consumed exactly once, while motion, fades and the
trigger envelope all use the render clock.

If the audio task cannot start, the loop falls back to calling the analyser inline.

### 3.0b Automatic gain control

`agc = 1` keeps the input usable when the room changes. Every 400 ms it looks at the
peak broadband level of the window and moves `gainDb` by one 3 dB step:

| peak level | action |
|------------|--------|
| < 0.08 | nothing — silence must not be amplified |
| < 0.30 | gain up one step |
| 0.30…0.90 | nothing (the hysteresis band) |
| > 0.90 | gain down one step |

The band is wide on purpose: a narrow one would oscillate. Moving the gain slider on
the phone turns AGC off, because that is a manual override.


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

### 4.2b Motion blur (sub-frame sweep)

The render loop samples **one position per audio frame** (~31 fps). That is only
enough while the blob moves less than its own width per frame: at 500 LED/s it
advances ~16 LEDs, so with a small blob consecutive frames do not touch and it
appears to teleport.

`LayerEngine::drawBlob()` therefore sweeps the path the blob covered since the
previous frame:

* travel is the shortest signed distance on a ring, or a plain difference otherwise;
* `ghosts = 1 + |travel| / (halfWidth / 2)`, capped at 12. The spacing must scale
  with the blob: a fixed 0.6-LED step put each ghost on a different LED for a 1-LED
  blob, which produced a dotted line instead of a streak;
* each ghost carries `amp / ghosts`, so the same light is spread over the streak and
  a fast blob does not get brighter;
* only one sample is drawn while the blob moves less than that step, so slow motion
  is bit-identical to before.

For very short blobs even that is not enough to look continuous, which is what the
per-layer **softness** is for.

### 4.2c Fade shape

`fadeMs` is a **perceived** fade. Gamma is applied once at the output, so decaying
the light amplitude linearly would display as `amp^2.2` — a fast drop followed by a
long dim tail. The blob therefore ages a `life` value linearly and derives
`amp = life^(1/2.2)`, which makes displayed brightness fall linearly with time:

| fraction of `fadeMs` | old (amp linear) | now |
|---|---|---|
| 0.25 | 0.53 | 0.75 |
| 0.50 | 0.22 | 0.50 |
| 0.75 | 0.05 | 0.25 |

Note that `speed` stays an **absolute** rate (LEDs/s), independent of `blobLeds`: a
long blob is not slower, it simply moves a smaller fraction of its own length per
frame, which reads as slower.

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
| `kTrigTap` | 4 | fingertip pad from the full-screen pad view | — |
| `kTrigAccel` | 8 | the stick is whipped / shaken | `motionThresholdPct` on \|a\| − 1 g |
| `kTrigGyro` | 16 | the stick is twisted quickly | `motionThresholdPct` on \|ω\| |
| `kTrigLfo` | 32 | a free-running metronome, independent of everything | `lfoPeriodMs` |

* `triggers = 0` means the layer is silent: no source can fire it.
* The whip **tints** the layer's own colour by the direction of the swing (25 % at a
  nudge, 60 % at a hard whip); it never replaces it, so the colour wheel keeps
  showing the colour that was chosen and other triggers are unaffected.
* The pad path is gated by `kTrigTap`, so unticking it really does stop pads from
  reaching that layer; BtnB selects the next strip and BtnA cycles the trigger set.
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
| — | *(no Glow)* | There is deliberately no Glow effect. A **glow** is a **Flash** with a long `attackMs`/`releaseMs` fired by the **LFO** trigger; a strobe is the same with short ones. Trigger and effect stay orthogonal, so any input can drive any shape. |

### 5.3 Beat detection (per layer)

Three stages, running at the ~31 fps analysis rate:

```
audio frame -> FFT -> band flux -> whitening + log -> peak pick -> onset
                                                        |
                                      inter-onset histogram -> period
                                                        |
                                    phase-locked beat grid -> beat
```

* **Onset function**: `bandFlux()` sums only the *positive* frame-to-frame magnitude
  change inside `center ± width/2`, normalised for window width so narrow and wide
  windows trigger comparably. Deliberately **not** scaled by the microphone gain:
  gain is a display/gating preference, and letting it in meant a negative gain also
  silenced the detector.
* **Conditioning** (`beat.cpp`): the flux is divided by its own ~1.5 s running
  average and log-compressed. The same hit then reads the same at any loudness or
  gain (a signal 36 dB quieter gives an identical result), and a big transient
  cannot swamp the statistics.
* **Peak picking** with one frame of lookahead: an onset must be a *local maximum*
  of the conditioned signal, clear `mean + k·dev` (k from `beatSensitivityPct`,
  1.7 at 0 % down to 0.7 at 100 %) and *rise* at least 0.55 out of the floor since
  the last onset. The rise test is what keeps noise from firing: noise is all local
  maxima but no rise.
* **Tempo**: an inter-onset interval histogram, 250–2000 ms in 10 ms bins, decayed
  per onset so a tempo can be forgotten. Each interval also votes for its exact
  halves and thirds (a missed beat therefore reinforces the real period), and
  accented onsets additionally vote from the *previous accented* onset — that is
  what keeps a swing or a syncopated line at its real tempo instead of locking onto
  the subdivision. A mild metrical prior (centred on 500 ms) and a "is twice as long
  nearly as well supported?" test suppress the classic octave error.
* **Beat grid**: a phase-locked loop runs at the estimated period. Onsets pull the
  phase towards them (gain 0.3); while locked the period is measured directly over
  the last ~8 beats, which removes the histogram's 10 ms quantisation (120 BPM reads
  120, not 119).
* **Locking** is decided by a grid-fit test, not by a streak: the last 8 onsets are
  fitted to the grid with a free phase offset, and the normalised fit must exceed
  0.55 (uniformly scattered onsets score ~0.24, music scores 0.7–1.0). A swing can
  therefore lock even though half of its onsets sit off the beat.
* **Beats**: while locked, a beat is raised by the *grid*, so a masked or swallowed
  onset still gives one (`predicted`), and off-beat onsets never double the tempo.
  Below the lock the detector stays responsive and every onset is a beat. The lock
  is dropped after repeated off-grid onsets, on silence, or when nothing has landed
  on the grid for 2.2 periods.
* **Silence gate**: a band level below `0.06` counts as silence: no beat, and the
  tempo lock is dropped immediately so silence never reports a BPM. The level is
  compared **with the microphone gain removed**, so turning the gain down attenuates
  what you hear without switching the detector off.
* **Reported**: `bpm` (0 unless locked), `beatConf` (the grid-fit quality, 0–100)
  and the pulse envelope (`τ ≈ 120 ms`) published as `beatPct`.

`test/beat_test.cpp` drives the detector offline with synthetic onsets: silence,
60/90/120/128/150 BPM, a 36 dB quieter signal, noise, a masked beat, syncopation,
swing, a tempo change, stopping, and beat timing. Run it with
`g++ -std=c++17 -O2 test/beat_test.cpp src/beat.cpp -o /tmp/beat_test && /tmp/beat_test`.

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
| `layerCount` | 1…4 | 1 | How many light layers are live inside this strip. There is always at least one (switch it off instead of removing it) |

A layer that is **added** starts switched **off**, with a plain threshold-driven
jet, its own colour from a four-colour palette and its own band
(`150 Hz + 250 Hz × slot`). Adding a layer therefore never changes what is on the
strip until it is switched on, and four stacked layers give four different bands.
| — | — | — | *(no strip-level blur: see `softnessPct` per layer below)* |

### Per light layer (the glowing object)

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| `triggers` | bitmask 0…31 | 1 | Any combination of threshold 1, beat 2, tap 4, accel 8, gyro 16 |
| `effect` | 0…4 | 0 | `0` = jet, `1` = flash, `2` = burst, `3` = sparkle, `4` = whip |
| `enabled` | 0/1 | 1 | Layer power. Off = this layer contributes nothing, settings kept |
| `intensityPct` | 1…100 | 100 | This layer's weight in the merged output |
| `softnessPct` | 0…100 | 0 | Widens this layer's spatial profile (`halfWidth × (1 + softness)`), so its edges bleed outward. A **layer** property: it shapes the blob before anything is merged |
| `centerHz` | 65…7600 | 150 / 1000 | Band centre (strip 1 layer 1 bass, strip 2 layer 1 mids by default) |
| `widthHz` | 40…16000 | 250 / 1400 | Band width; also the beat detector's listening window |
| `thresholdPct` | 0…100 | 35 / 40 | Level that fires the effect in **Threshold** mode (drag the white marker) |
| `beatSensitivityPct` | 0…100 | 55 | Detector eagerness: the peak threshold is `mean + k·dev` with k from 1.7 (0 %) down to 0.7 (100 %) |
| `motionThresholdPct` | 0…100 | 30 | Gate for the accelerometer/gyroscope inputs |
| `attackMs` | 0…2000 | 20 | How fast a **Flash** rises |
| `releaseMs` | 10…5000 | 400 | How slowly a **Flash** falls |
| `lfoPeriodMs` | 100…5000 | 2000 | Metronome period for the **LFO** trigger |
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
| `gainDb` | −24…+18 in 3 dB steps | 0 | Microphone gain applied to the analysis. **Negative values attenuate**, which is what a noisy room needs |
| `agc` | 0/1 | 0 | **Automatic gain control**: the firmware nudges `gainDb` to keep the level in range. Moving the slider turns it off |

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
* **+ Add layer** duplicates the layer being edited in another colour at 60 %
  weight and selects it, so a second beam is visible immediately. **Remove**
  deletes the layer being edited and shifts the rest up; **Solo** previews one
  layer alone.
* With a single layer the **tab row is hidden** and the count reads
  `1 of 4 · single beam`, so an empty row can never be mistaken for hidden
  layers. The layer cards below keep working normally.
* An inline **Canvas Colour Wheel** + brightness slider + current colour square
  streams colour changes in real time while dragging.
* Everything **physical** lives in the strip card, under the Strip 1 / Strip 2
  buttons: `Strip brightness`, `LEDs on this strip` (slider + −10/←/→/+10),
  `Calibrate`, then the layer list. There is no separate calibration card.
* The per-layer pair `Layer power` + `Layer intensity` closes the strip card, right
  under the layer title, so switching a layer off and setting its weight are the
  same gesture. Colour and softness live in the **Display** card below.
* **Layer tabs are painted with the layer's colour** (filled when selected, dimmed
  when the layer is powered off), so a beam is recognisable without reading titles.
* **Two channels, on purpose.** Live numbers (levels, dBFS, peak, per-layer
  pulses, tempi, the spectrum) are pushed as one **91-byte binary frame at ~25 Hz
  over a WebSocket** on its own port; settings travel as JSON and are fetched
  rarely. Mixing them was what made the UI trail reality by about a second.
* The WebSocket lives at `ws://<board>:81/ws`. If it cannot be established the page
  fetches the identical frame from `GET /api/telemetry` four times a second, so the
  UI degrades in smoothness, never in correctness. The header carries a link badge:
  `live 25 Hz` (green), `polling` (amber), `no link` (red), `idle` (grey, tab hidden).
* The board tells the page its socket port (`wsPort` in `/api/state`); the page
  never guesses it.
* **Writes** are queued, timed out and reconciled against the state (see below):
  one request in flight, exponential backoff, and a write is re-sent until the
  board's state agrees. Controls with a write in flight are never overwritten by a
  read.
* The page pauses both channels when the tab is hidden and resumes on return.
* **Layer tabs are painted with the layer's colour** (filled when selected, dimmed
  when the layer is powered off), so a beam is recognisable without reading titles.
* The page keeps **one request in flight at a time** — the board serves one socket —
  and every request has a 2.5 s deadline. Each write is also remembered as a
  *desire*: a state read clears it when the board agrees and **re-queues** it when it
  does not, so a dropped request costs a retry rather than the edit. Controls with a
  write in flight are never overwritten by a read. While idle the page re-reads
  `/api/state` every 600 ms, so the screen converges on what the board holds.
* The microphone card shows a **room-level meter** on the analyser's own scale
  (−55…−18 dBFS) with quiet / good / hot zones, and one sentence saying what the AGC
  is doing (`raising`, `holding`, `lowering` or `idle`) and on what peak level.
* **Presets** are three slot chips that preview their contents (one dot per layer,
  in that layer's colour) plus two verbs, `Recall` and `Save here`. Recall is
  disabled on an empty slot; overwriting and recalling both require a second tap
  within 4 s. `/api/state` carries `presets:[{has,layers,cols}]`.
* **Band map**: a log-frequency canvas per physical strip, one stacked row per
  layer (the shape of the TFT), showing each layer's band edges, its centre line,
  its colour and how much of that band is present right now, over a faint
  silhouette of the 30-band spectrum. It answers "are these layers actually
  listening to different parts of the music, or are they fighting?".
* Live readouts (`lvl …%, … active`, the layer subtitle, the pad group headers) are
  single-line and truncated rather than wrapped, so growing numbers never reflow
  the page.
* The page header shows the **firmware version** (`LightRocks-1 · 192.168.4.1 · v1.0`)
  so it is obvious whether an upload actually took.
* **Trigger inputs** are five tickable chips (`Threshold`, `Auto beat`, `Tap pads`,
  `Accelerometer`, `Gyroscope`). Several can be armed at once; only the relevant
  controls below are shown (level bar + marker, beat sensitivity, motion
  threshold).
* A **Triggered effect** selector chooses among `Jet`, `Flash`, `Burst`, `Sparkle`,
  `Whip`.
* **Layer intensity** scales one layer against the others; **Strip max brightness**
  scales the merged result.
* **Full-Screen Effect Pads**: a dedicated overlay, laid out as one **column per
  physical strip** (Strip A left, Strip B right) with only the layers that have been
  added. Each layer block has four compact effect pads; they are multi-touch and
  support press-and-hold (Flash and Sparkle sustain; Jet re-fires at the retrigger
  gap). Each block shows that layer's live trigger state and output. The overlay
  scrolls and keeps the same 1/6 thumb gutter as the control page.
* Live readouts: broadband level, dBFS, dominant frequency, per-strip beat pulse /
  BPM, active effect count, and a bar showing the level against the threshold.

### API

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | The control page |
| GET | `/api/state` | **Settings** (+ preset summaries, `wsPort`) as JSON. Polled rarely: live values are not here |
| GET | `/api/telemetry` | The live frame, 91 bytes, `application/octet-stream` (fallback for the socket) |
| WS | `:81/ws` | The same frame pushed at ~25 Hz |
| POST | `/api/params?strip=<p>&<key>=<value>` | Change the **physical** strip (`ledCount`, `brightnessPct`, `layerCount`, `solo`) |
| POST | `/api/params?strip=<p>&layerAdd=<n>` | Append a fresh layer, switched off (the value is ignored; no-op at 4) |
| POST | `/api/params?strip=<p>&layerRemove=<l>` | Delete layer `l` and compact the rest (no-op at 1) |
| POST | `/api/params?strip=<p>&layer=<l>&<key>=<value>` | Change one **layer** (204, no body) |
| POST | `/api/params?gainDb=<n>` | Microphone gain |
| POST | `/api/tap?strip=<p>&layer=<l>&effect=<0..3>&down=<0\|1>` | Fire an effect from a pad; `down=1` also starts/refreshes the hold |
| POST | `/api/preset?slot=<1\|2\|3>&action=<save\|load>` | Save the live state to a slot, or load a slot into the live state. Returns the new state, or 400 when the slot is empty/unwritable |
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
* The settings blob is versioned and fixed-layout. Current layout is **v10**:
  3-byte header (version, gainDb, agc) + 6-byte physical block + 32-byte layer block
  × 4 layers per strip = 277 bytes.
  Blobs from **v3 through v9** are migrated automatically on load: an old single
  `mode` enum becomes the matching trigger bit, a v7 `Glow` effect becomes
  `Flash + LFO`, and a v9 per-strip bleed becomes the per-layer softness. Any other
  version falls back to defaults.

---

## 10. Display and buttons

| Control | Action |
|---------|--------|
| Screen | Header `S<n> <n>L` + the strip's merged output, the shared spectrum, then one thin row per layer of the **selected** strip (see below), a HUD line, AP status |
| BtnB click | Select the next physical strip — the screen switches to it |
| BtnB hold | Save settings to flash |
| BtnA click | Cycle the trigger set of every live layer on the selected strip |
| BtnA hold | Toggle the access point |

**Layer rows on the LCD.** Each row of the selected strip spans exactly that
layer's frequency window on the *same* x axis as the spectrum above, so a layer
and its bars are the same width and line up. Inside the window a bar fills with
the band level (grey while the layer is switched off), the left gutter carries the
trigger letter (`S` threshold, `B` beat, `A` accel/gyro, `M` tap, `-` off), and a
dot on the right lights while the layer is actually emitting. Four rows stack, so
a container is readable at a glance; `BtnB` moves to the other strip.
| Boot | 3 s banner with SSID, password and URL |

---

## 11. Non-goals (for now)

* No station mode / no router connection.
* No OTA updates.
* No per-blob colour variation, no hue animation along the strip.
* No audio recording or playback — the microphone is read-only, one frame at a time.

## 13. Live telemetry frame

Fixed layout, little-endian, version 1, 91 bytes (`src/telemetry.h` is the
authority; the page decodes by offset).

| off | size | field |
|-----|------|-------|
| 0 | 1 | version (1) |
| 1 | 1 | flags: bit0 AGC on, bit1 settings unsaved |
| 2 | 2 | sequence, wraps (the page uses it to spot a stalled link) |
| 4 | 1 | broadband level, % |
| 5 | 1 | dBFS, signed |
| 6 | 2 | peak frequency, Hz |
| 8 | 1 | mic gain, dB, signed |
| 9 | 1 | AGC peak, % |
| 10 | 1 | AGC action: −1 lower, 0 idle, 1 raise, 2 holding |
| 11 | 2 | per-strip output brightness, % |
| 13 | 8 | per (strip, layer): band level, % |
| 21 | 8 | ... beat pulse, % |
| 29 | 16 | ... detected BPM, u16 |
| 45 | 8 | ... beat confidence, % |
| 53 | 8 | ... active effects, count |
| 61 | 30 | spectrum bands, % |

Why 25 Hz: the analyser produces a block every 32 ms (~31 Hz), so anything faster
repeats numbers, and 25 Hz is 24× less traffic than the JSON it replaced.
