#pragma once

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Hardware and audio configuration.
//
// Everything that depends on how the rocks are wired lives here, so moving a
// strip to another pin or raising the LED ceiling is a one-line edit.
// ---------------------------------------------------------------------------

// NeoPixel data pins, one per strip, both on the Grove connector:
//   - M5StickS3:      G9 (yellow wire) and G10 (white wire)
//   - M5StickC Plus2: G32 (yellow wire) and G33 (white wire)
#if defined(BOARD_M5STICKS3)
constexpr int kStripPins[] = {9, 10};
#else
constexpr int kStripPins[] = {32, 33};
#endif
constexpr size_t kStripCount = sizeof(kStripPins) / sizeof(kStripPins[0]);

// The physical LED count is not fixed by the firmware: it varies per build and
// can be re-tuned from the web UI at any time, so it is a runtime parameter. This
// constant is only the tallest strip the firmware will drive -- the render
// buffers are sized from it, while per-frame loops only touch `0 .. ledCount-1`.
constexpr uint16_t kMaxLedsPerStrip = 1000;
constexpr uint16_t kDefaultLedsPerStrip = 100;

// ---------------------------------------------------------------------------
// Audio capture.
// ---------------------------------------------------------------------------

constexpr uint32_t kSampleRate = 16000;
constexpr size_t kFftSamples = 512; // power of two; 512 @ 16 kHz = 31.25 Hz bins

// Usable analysis range, also the limits of the frequency windows.
constexpr float kAudioMinHz = 65.0f;
constexpr float kAudioMaxHz = 7600.0f;

// ---------------------------------------------------------------------------
// Light output.
// ---------------------------------------------------------------------------

// Concurrent blobs per strip. Oldest is recycled when the pool is full.
constexpr size_t kMaxBlobs = 12;

// Strips are linear-light devices driven in a dark room, so a linear fade looks
// like it drops off a cliff. Encoding with this gamma makes the fade read evenly.
constexpr float kRenderGamma = 2.2f;

// Below this amplitude a blob is invisible after gamma, so it is retired.
constexpr float kBlobEpsilon = 0.004f;

// ---------------------------------------------------------------------------
// Web UI.
// ---------------------------------------------------------------------------

constexpr size_t kStateJsonMaxBytes = 1400;
