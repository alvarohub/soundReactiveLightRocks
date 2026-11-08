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
