# Ideas & research notes

A running list, kept deliberately cheap to append to. The point is that a
half-formed idea, a library worth remembering, or a measured result should not be
lost between sessions. Nothing here is a promise.

Legend: **idea** · **library** · **result** · **reusable**

---

## Transport: Wi-Fi AP vs. Bluetooth (raised 2026-11-08)

The question: every ESP32 project so far has used a Wi-Fi soft-AP for
provisioning and control. Would pure Bluetooth be better, given Bluetooth would
also allow MIDI, or even real-time audio on the older M5StickC Plus2?

### The hardware constraint that decides it

| Board | Radio | Consequence |
|-------|-------|-------------|
| **M5StickS3** (ESP32-S3) | Wi-Fi 2.4 GHz + **BLE 5 only** | **No Bluetooth Classic (BR/EDR)**, so no standard A2DP audio sink and no classic serial profile. BLE MIDI is fine. |
| **M5StickC Plus2** (ESP32) | Wi-Fi + Classic BT + BLE | Classic A2DP possible in principle, but sharing one antenna between Wi-Fi and BT caused the audio stutter recorded in [JOURNAL.md](./JOURNAL.md). |

Wi-Fi and Bluetooth share the same 2.4 GHz front end, so **they cannot really be
used simultaneously** at full duty — that is not a firmware limitation but physics
(coexistence is time-sliced, and audio or LED timing suffers first).

### Verdict, per use case

| Need | Best transport | Why |
|------|----------------|-----|
| **Parameter calibration from a phone** | **Wi-Fi soft-AP + HTTP** (current) | Zero install, any phone, no pairing dance, works with no app. A BLE equivalent needs a Web Bluetooth page or a custom app, and Web Bluetooth is not available in every mobile browser (notably iOS Safari). |
| **MIDI control** | **BLE MIDI** (standard GATT service) | Works on both boards, low latency (~10 ms), no pairing per session after the first. This is the strongest argument for BLE here. |
| **Real-time audio in** | Wi-Fi (UDP) on S3; Classic A2DP only on StickC Plus2 | S3 has no classic audio sink. UDP FFT-band streaming is the practical route. |
| **Multi-rock sync** | **ESP-NOW** | Peer-to-peer, no router, ~1 ms latency, far cheaper than HTTP for 10 rocks. Wi-Fi radio, no AP required. |

### Practical recommendation

Keep **Wi-Fi AP + HTTP as the calibration path** (it is the only zero-install
option on iOS), and *add* **BLE MIDI as a second, concurrent-free control input**
for performance: knobs and pads on a phone or a hardware controller. Do **not**
plan on running Wi-Fi and Bluetooth together for anything timing-critical.

**Idea:** a single `Transport` interface with `WifiHttp`, `BleMidi` and `EspNow`
implementations, so the control code never knows which is active.

**Library:** `ESP32-BLE-MIDI` (BLE-only, small) — or the NimBLE stack directly,
which is already in the Arduino-ESP32 core.

**Idea:** **Improv Wi-Fi** (open standard) for provisioning — lets a phone or a
browser put the board on an existing network *and* hand it credentials, instead of
the board raising its own AP. Useful the moment there is more than one rock.

---

## Refresh rate: where the ceiling actually is

**Result (computed, to be confirmed on hardware).** WS2812B needs ~30 µs per LED, so
one full frame costs:

| LEDs | RMT transfer |
|------|--------------|
| 100 | 3.0 ms |
| 300 | 9.0 ms |
| 1000 | 30 ms |

That is **not** what limits us today. The loop is paced by a *blocking* microphone
capture — `512 samples / 16 kHz = 32 ms` — so the firmware runs at ~31 fps no matter
how fast the strip is or what `speed` is set to. At 500 LED/s that is 16 LEDs of
travel per frame, which is exactly why fast blobs used to alias.

**So: the LED transfer only becomes the limit above ~300 LEDs.**

**Done 2026-11-08:** `AudioTask` owns the blocking capture path on core 0 and
publishes an `AudioSnapshot`; the render loop runs at up to ~100 fps. Confirmed that
M5Unified's `record()` was always DMA-backed and task-driven — the blocking *API* was
what pinned the loop, not the hardware.

**Still open:** measure the achieved render rate on hardware (100 LEDs should allow
~100 fps against a 3 ms RMT transfer) and decide whether the motion-blur ghosts can be
dropped above some rate.

**Idea (cheap stopgap):** cap `speed` at `frameRate × 1 LED` (≈31 LED/s at 31 fps)
and instead express speed in "screens per second", so the UI cannot ask for motion
the frame rate cannot represent.

## Rendering: sub-frame motion (done 2026-11-08)

**Result:** at the audio-paced frame rate (~31 fps), `speed` and `blobLeds`
interact badly in perception. At 500 LED/s a blob advances ~16 LEDs per frame, so
with a small blob consecutive frames do not touch and it reads as teleporting.
Fixed by sweeping the path between the previous and current position with up to 12
ghost samples, each carrying `1/n` of the amplitude (light conserved).

**Still open:** a genuinely *perceptual* speed mode. "LEDs per second" is an
absolute rate, so a long blob looks slow because it moves a small fraction of its
own length per frame. An alternative unit — **blob lengths per second** — would
make a long blob travel proportionally faster. Worth exposing as a switch if the
feel is still wrong.

**Idea:** run the render/update loop faster than the audio loop. The audio blocks
at ~32 ms, but the strip could be re-rendered at 60–100 Hz by interpolating
positions, which would remove the need for ghosts entirely. Costs RMT transfer
time (30 µs/LED), so it only pays off below ~300 LEDs.

---

## Reusable: LED_scoreboard (VictorLeung)

Worth mining rather than rewriting — several pieces are already generic:

| Piece | What it gives us |
|-------|------------------|
| `Vec2f.h` | Minimal float 2-D vector, no dependencies |
| `ParticleSystem` | Float particles with **physics sub-stepping** (`substepMs`, max ~20 ms) so motion stays stable when a frame runs long |
| `ParticleTransform2D` | Rotation/scale/translate applied at *render* time, OpenGL-modelview style — the same idea as our layer `intensityPct`, generalised |
| `VirtualDisplay` | A `GFXcanvas16` per logical display, composited by `DisplayManager` — a clean answer to "several logical panels on one wire" |
| `OSCHandler` | OSC over UDP, i.e. control from TouchOSC etc. with no app |

**Reusable idea:** our `LedStrip` + `LayerEngine` split (linear buffer, one gamma
pass, N layers) is the same shape as their canvas + compositor, but for 1-D
strips. That could become a small library: *"N additive light layers with
per-layer trigger logic, rendering into one physical strip"* — it is the part of
this project that is genuinely not NeoPixel-specific.

**Idea:** adopt their **physics sub-stepping** pattern in `LayerEngine::update()`
even though our motion is linear today — it costs nothing and makes future
accel/spring-driven effects stable when a frame overruns.

---

## Signal processing

**Idea:** per-band beat detection already exists (one `BeatDetector` per layer).
The next step would be **tempo-phase prediction** (comb filter or Ellis dynamic
programming) so the first beat after a pause lands correctly, and so effects can
be *quantised* to the next beat.

**Idea:** **SuperFlux** onset function (maximum filter across frequency + a
vibrato-suppressing reference frame) reduces false onsets on sustained notes and
vibrato, for ~2 KB of extra buffer.

**Result:** the silence gate needs the mic gain removed before thresholding —
otherwise "turn the gain down in a loud room" silently disables beat detection.
Any future level-based gate must do the same.

**Idea:** the threshold trigger has no silence gate; the same
gain-compensated gate should be applied there.

---

## Interaction

**Idea:** BtnA/BtnB are already overloaded (strip select, trigger cycling). A
**long-press menu on the LCD** (hold to enter, click to move, hold to leave) would
make the device usable without a phone at all.

**Idea:** the accelerometer could select *layers*, not just fire them — tilt the
stick to pick which layer the buttons act on.

**Idea:** record a short motion gesture (accel/gyro trace) and match it later —
"flick left = next preset". Cheap DTW on 20 samples.

## Real-time telemetry: why the phone feels slow, and what would fix it

*Recorded 2026-11-10. The lag is not the "IoT" part; it is the transport design.*

### Where the delay actually comes from

| stage | cost | note |
|---|---|---|
| audio block | 32 ms | 512 samples at 16 kHz, DMA into a ring buffer |
| analysis + detectors | ~2 ms | runs once per block, in its own task |
| render + LED transfer | 10-30 ms | ~100 fps cap, real limit is the RMT transfer |
| **`/api/state` poll** | **600 ms** | the page asks; nothing is pushed |
| JSON build + send | 3.9 KB `snprintf` + TCP write | four appends per layer, 8 layers |

So the *board* knows the level 30 times a second and the *phone* learns about it
twice a second - twenty times slower than the data. Add the round trip over
Wi-Fi, the single-socket `WebServer` (one request at a time, which is why writes
and polls had to be serialised at all), and a 4 KB JSON payload rebuilt from
scratch on every request, and the UI is always a second behind reality.

The rule of thumb: **polling a full state document at 600 ms is fine for
settings and useless for telemetry.** Settings change when a human touches
something; levels change 30 times a second. They want different transports.

### What to do instead, in order of cost/benefit

1. **WebSocket push of a small binary telemetry frame (the real answer).**
   `esp_http_server` (which `WebServer` wraps) has `CONFIG_HTTPD_WS_SUPPORT` and
   `httpd_ws_send_frame`; `ESPAsyncWebServer` has a simpler API. Push at 20-30 Hz
   a frame of ~70 bytes:
   `seq(u16) levelPct(u8) dbfs(i8) peakHz(u16) bpm(u16) pulse(u8) agcPeak(u8)
   agcAction(i8) stripBright[2](u8) layerLevel[8](u8) bands[30](u8)`.
   The page renders on `requestAnimationFrame` from the newest frame and keeps
   HTTP only for *writes*. This is what ESPHome, WLED and every audio visualiser
   dashboard does. Latency becomes one audio block (~32 ms) instead of 600 ms.
2. **Server-Sent Events** if avoiding a WS library: one long-lived
   `text/event-stream` response, same push model, text only (send the same fields
   as base64 or as a short CSV - still ~150 bytes).
3. **UDP broadcast** of that same packet (no connection state, any number of
   phones at once, no reconnection logic) with HTTP kept for settings. Simplest
   possible server code; lossy, which is fine for a level meter.
4. **Cheap wins inside the current design**: cache the JSON and rebuild only when
   the settings change (telemetry fields patched in place), poll at 250 ms, and
   split settings from telemetry into two endpoints so a poll does not carry 3 KB
   of parameters the user is not touching. Worth doing even if (1) is planned.

### Not worth it

* A full FFT stream to the phone. The interesting information is 30 band levels
  (30 bytes) - the phone can draw them. Streaming raw bins wastes the link and
  buys nothing.
* Sending faster than the analysis rate (31 Hz). Above ~30 Hz the numbers repeat.

### Note on the phone side

Once telemetry is pushed, the page must stop re-rendering its whole DOM per frame:
keep `apply()` for settings (rare) and a separate `paintTelemetry()` for meters,
canvas and level bars (frequent, no allocations, no text reflow). That split is
also what keeps a live UI from stealing the touch target under a finger - the same
bug class that made the layer tabs and preset chips swallow taps.
