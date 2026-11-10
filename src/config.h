#pragma once

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Hardware and audio configuration.
//
// Everything that depends on how the rocks are wired lives here, so moving a
// strip to another pin or raising the LED ceiling is a one-line edit.
// ---------------------------------------------------------------------------

// NeoPixel data pins, one per strip. Configured per environment in
// platformio.ini (-DSTRIP1_PIN=... -DSTRIP2_PIN=...), with safe per-chip
// defaults here as a fallback:
//   - M5StickS3 (ESP32-S3):   G9 (yellow wire) and G10 (white wire)
//   - M5StickC Plus2 (ESP32): G32 (yellow wire) and G33 (white wire)
#ifndef STRIP1_PIN
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define STRIP1_PIN 9
#else
#define STRIP1_PIN 32
#endif
#endif

#ifndef STRIP2_PIN
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define STRIP2_PIN 10
#else
#define STRIP2_PIN 33
#endif
#endif

constexpr int kStripPins[] = {STRIP1_PIN, STRIP2_PIN};
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
// Wi-Fi Access Point.
//
// Change here in config.h, or override at compile time in platformio.ini via
// -DWIFI_SSID=\"LightRocks-2\" and -DWIFI_PASSWORD=\"lightrocks\".
// ---------------------------------------------------------------------------

#ifndef WIFI_SSID
#define WIFI_SSID "LightRocks-1"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "lightrocks"
#endif

constexpr char kWifiSsid[] = WIFI_SSID;
constexpr char kWifiPassword[] = WIFI_PASSWORD;

// ---------------------------------------------------------------------------
// Microphone gain.
//
// This is a software gain applied to the analysis, not a hardware register, so
// it can go negative: in a loud room the microphone is over-sensitive and the
// useful fix is attenuation, not just "less boost".
// ---------------------------------------------------------------------------

constexpr int8_t kMinGainDb = -24;
constexpr int8_t kMaxGainDb = 18;
// 3 dB steps: fine enough for the automatic controller to settle without
// audibly pumping, coarse enough that the slider stays usable.
constexpr int8_t kGainStepDb = 3;

// ---------------------------------------------------------------------------
// Web UI.
// ---------------------------------------------------------------------------

constexpr size_t kStateJsonMaxBytes = 4600;

// Shown in the page header so it is obvious which build is running. Bump this
// whenever behaviour changes; "did my upload take?" is asked more often than one
// would think.
constexpr char kFirmwareVersion[] = "v1.4";
