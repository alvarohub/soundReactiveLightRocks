# Sound-Reactive Light Rocks

Two NeoPixel strips hidden inside plastic rocks, reacting to music. Built on the
**M5StickS3** (default) or **M5StickC Plus2**, controlled from a phone over its
own Wi-Fi access point.

When the music gets loud in a chosen frequency band, a **blob of light** runs
along the strip — either orbiting continuously as a closed loop (`Loop`), or
disappearing past the end (`One-shot`) — while fading out. Each strip has its own
band, threshold, speed, fade time, blob length, direction, loop mode, and colour.

* Behaviour specification: [docs/SPEC.md](docs/SPEC.md)
* How it is built and why: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
* Work log and decisions: [docs/JOURNAL.md](docs/JOURNAL.md)

---

## Build and flash

```bash
pio run                       # build M5StickS3 (default)
pio run -t upload             # flash M5StickS3
pio device monitor            # serial output @115200

pio run -e m5stick-c-plus2            # build M5StickC Plus2
pio run -e m5stick-c-plus2 -t upload  # flash M5StickC Plus2
```

## Wiring

| Signal | M5StickS3 | M5StickC Plus2 | Grove wire |
|--------|-----------|----------------|------------|
| Strip 1 data | **G9** | **G32** | yellow |
| Strip 2 data | **G10** | **G33** | white |
| Ground | GND | GND | black, **shared with the strip supply** |

Power the strips from a separate 5 V supply — 100 LEDs at full white draw about
6 A, far beyond what the Grove connector can deliver. Only the data lines and the
ground go to the stick.

## Using it

1. Power the stick. The screen shows the network name and password for ~3 s.
2. Join Wi-Fi **`LightRocks`** (password `lightrocks`).
3. Open <http://192.168.4.1>.

Pick a strip tab, then tune:

* **Motion (`Loop` / `One-shot`)** — whether blobs keep orbiting around the ring or
  disappear once they pass the last LED.
* **Direction (`Forward` / `Reverse`)** — travel `0 → N-1` or `N-1 → 0`.
* **Colour wheel** — drag directly on the colour wheel to adjust the strip colour
  in real time (current colour is shown in the square swatch next to the wheel).
* **Band centre / width** — which part of the spectrum that strip listens to. The
  coloured line under the spectrum bars shows the window on the device screen.
* **Trigger threshold** — the bar on the page and the white tick on the device
  screen show the live band level against this threshold. Set it just under the
  peaks of the sound you care about.
* **Blob length, speed, fade time** — the look of the moving light.
* **Retrigger gap** — how often a blob can be born while the sound stays loud.

Live settings save automatically to flash ~0.8 s after you stop adjusting. You can
also store and recall up to **3 preset banks** (`Save 1..3` / `Load 1..3`), or use
*Factory reset* to restore the hardcoded code defaults without erasing your saved
presets.

### Buttons

| Control | Action |
|---------|--------|
| BtnA click | Manual test trigger on both strips |
| BtnA hold | Wi-Fi access point on/off |
| BtnB click | Cycle mic gain 0 → 6 → 12 → 18 → 0 |
| BtnB hold | Save settings now |

## Layout

```
src/
  main.cpp              setup/loop, screen, buttons — the conductor
  config.h              pins, audio constants, limits
  settings.h            parameters, clamping, defaults
  settings_blob.*       versioned byte layout for flash (struct ↔ 21 B per strip)
  store.*               NVS persistence
  audio_fft.*           mic capture, FFT, log bands, band level
  fft_backend*.{h,cpp}  ESP-DSP (default) and arduinoFFT backends
  strip.*               NeoPixel ring driver (accumulate → gamma → show)
  blob_mode.*           the orbiting-blob engine — one instance per strip
  webui.*               access point, REST API, embedded page
docs/
  SPEC.md               what the firmware is meant to do
  ARCHITECTURE.md       the layers, one frame step by step, why the split
  JOURNAL.md            work log
```

The short version of [ARCHITECTURE.md](docs/ARCHITECTURE.md): one audio block is
one frame, and it flows down a chain of three boxes — `SpectrumAnalyzer` (audio →
one number per strip) → `BlobEngine` (that number → linear light in a frame
buffer) → `LedStrip` (frame buffer → gamma → RMT bytes). `main.cpp` drives the
chain; `Settings` is the only state shared with the web page.

The audio pipeline, storage and web-server structure are adapted from the earlier
`LightSoundReactive` project; the light output is completely different, since a
NeoPixel strip takes one wire and carries per-LED colour instead of one PWM
channel per colour.
