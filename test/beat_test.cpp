// Offline regression suite for the beat detector.
//
//   g++ -std=c++17 -O2 test/beat_test.cpp src/beat.cpp -o /tmp/beat_test
//   /tmp/beat_test          (exit code 0 = every case as expected)
//
// beat.cpp is Arduino-free on purpose: the whole detector can be driven from
// synthetic onset envelopes here, which is the only way to know what it does at
// 90 vs 150 BPM, through a masked beat, through a swing, or in noise.

#include "../src/beat.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

constexpr int kFrameMs = 32; // one FFT block at 16 kHz / 512 samples

struct Spike {
  int atMs;
  float amp;
};

struct Run {
  int beats = 0;
  int predicted = 0;
  int bpm = 0;
  bool locked = false;
  int conf = 0;
  std::vector<int> beatFrames;
};

Run run(const std::vector<Spike> &spikes, float level, int durMs,
        uint8_t sens = 60, bool noise = false) {
  BeatDetector d;
  d.reset();
  Run r;
  size_t si = 0;
  unsigned seed = 12345u;
  for (int t = 0; t < durMs; t += kFrameMs) {
    float onset = 0.0f;
    while (si < spikes.size() && spikes[si].atMs < t + kFrameMs) {
      if (spikes[si].atMs >= t) {
        onset += spikes[si].amp;
      }
      si++;
    }
    if (noise) {
      seed = seed * 1103515245u + 12345u;
      onset += 0.35f * (float)((seed >> 16) & 0x7FFF) / 32767.0f;
    }
    const BeatInfo b = d.update(onset, level, kFrameMs, sens);
    if (b.onBeat) {
      r.beats++;
      r.beatFrames.push_back(t);
      if (b.predicted) {
        r.predicted++;
      }
    }
    r.bpm = b.bpm;
    r.locked = b.locked;
    r.conf = b.confidencePct;
  }
  return r;
}

std::vector<Spike> train(double bpm, int durMs, int fromMs = 0, float amp = 1.0f) {
  std::vector<Spike> v;
  const double period = 60000.0 / bpm;
  for (double t = (double)fromMs; t < (double)fromMs + (double)durMs; t += period) {
    v.push_back({(int)(t + 0.5), amp});
  }
  return v;
}

int gFailures = 0;

void expect(const char *name, bool ok, const char *detail = "") {
  printf("%-46s %s %s\n", name, ok ? "ok  " : "FAIL", detail);
  if (!ok) {
    gFailures++;
  }
}

// A case "passes" when the tempo lands within 4 BPM and the detector locked.
void expectTempo(const char *name, const Run &r, int bpm) {
  char buf[96];
  const bool ok = r.locked && std::abs(r.bpm - bpm) <= 4;
  snprintf(buf, sizeof(buf), "bpm=%3d locked=%d conf=%3d beats=%2d (want ~%d)",
           r.bpm, (int)r.locked, r.conf, r.beats, bpm);
  expect(name, ok, buf);
}

} // namespace

int main() {
  // Silence never invents a beat or a tempo.
  {
    const Run r = run({}, 0.0f, 10000);
    expect("silence: no beats, no tempo", r.beats == 0 && r.bpm == 0 && !r.locked);
  }

  // Clean metronomes across the usable range.
  expectTempo("steady 60 BPM", run(train(60, 12000), 0.5f, 12000), 60);
  expectTempo("steady 90 BPM", run(train(90, 10000), 0.5f, 10000), 90);
  expectTempo("steady 120 BPM", run(train(120, 10000), 0.5f, 10000), 120);
  expectTempo("steady 128 BPM", run(train(128, 10000), 0.5f, 10000), 128);
  expectTempo("steady 150 BPM", run(train(150, 10000), 0.5f, 10000), 150);

  // The onset function is normalised, so a 60x quieter signal must read the same.
  expectTempo("120 BPM at -36 dB, low band level",
              run(train(120, 10000, 0, 0.016f), 0.2f, 10000), 120);

  // White noise has no period: it must not produce a tempo grid.
  {
    const Run r = run({}, 0.5f, 10000, 60, true);
    char buf[64];
    snprintf(buf, sizeof(buf), "locked=%d bpm=%d", (int)r.locked, r.bpm);
    expect("noise: refuses to lock", !r.locked && r.bpm == 0, buf);
  }

  // A masked beat (every 4th onset missing) must not break the grid, and the
  // grid itself should supply the missing beat.
  {
    auto v = train(100, 12000);
    std::vector<Spike> m;
    for (size_t i = 0; i < v.size(); i++) {
      if (i % 4 != 3) {
        m.push_back(v[i]);
      }
    }
    const Run r = run(m, 0.5f, 12000);
    expectTempo("100 BPM, every 4th beat masked", r, 100);
    char buf[64];
    snprintf(buf, sizeof(buf), "predicted=%d", r.predicted);
    expect("masked beats are filled by the grid", r.predicted > 8, buf);
  }

  // Off-beat syncopation with weaker/missing off-beats must not double the tempo.
  {
    auto v = train(100, 12000);
    std::vector<Spike> s = v;
    for (size_t i = 0; i + 1 < v.size(); i++) {
      if (i % 3 != 2) {
        s.push_back({(v[i].atMs + v[i + 1].atMs) / 2, 0.35f});
      }
    }
    std::sort(s.begin(), s.end(), [](const Spike &a, const Spike &b) {
      return a.atMs < b.atMs;
    });
    expectTempo("100 BPM + weak/missing off-beats", run(s, 0.5f, 12000), 100);
  }

  // Swing (400/200 instead of 300/300): the accents, not the subdivision, are
  // the beat.
  {
    std::vector<Spike> sw;
    for (int t = 0; t < 12000; t += 600) {
      sw.push_back({t, 1.0f});
      sw.push_back({t + 400, 0.5f});
    }
    expectTempo("100 BPM swing (400/200)", run(sw, 0.5f, 12000), 100);
  }

  // A change of tempo is followed, not averaged towards.
  {
    auto a = train(90, 6000, 0);
    auto b = train(140, 14000, 6000);
    a.insert(a.end(), b.begin(), b.end());
    expectTempo("90 BPM then 140 BPM", run(a, 0.5f, 20000), 140);
  }

  // When the music stops, the tempo and the beats stop with it.
  {
    auto v = train(120, 5000);
    BeatDetector d;
    d.reset();
    int beatsAfter = 0;
    int bpmAfter = 0;
    for (int t = 0; t < 12000; t += kFrameMs) {
      float onset = 0.0f;
      for (const Spike &s : v) {
        if (s.atMs >= t && s.atMs < t + kFrameMs) {
          onset += s.amp;
        }
      }
      const bool playing = t < 5000;
      const BeatInfo b = d.update(onset, playing ? 0.5f : 0.0f, kFrameMs, 60);
      if (b.onBeat && t > 5200) {
        beatsAfter++;
      }
      if (t > 7000) {
        bpmAfter = b.bpm;
      }
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "beats after stop=%d bpm=%d", beatsAfter, bpmAfter);
    expect("silence after music: stops cleanly", beatsAfter == 0 && bpmAfter == 0,
           buf);
  }

  // Beats have to land on the music, not near it.
  {
    auto v = train(120, 8000);
    const Run r = run(v, 0.5f, 8000);
    int worst = 0;
    int matched = 0;
    for (int bf : r.beatFrames) {
      int best = 100000;
      for (const Spike &s : v) {
        best = std::min(best, std::abs(bf - s.atMs));
      }
      if (best < 200) {
        worst = std::max(worst, best);
        matched++;
      }
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%d beats, worst error %d ms", matched, worst);
    expect("120 BPM: beat timing", matched >= 12 && worst <= 64, buf);
  }

  printf("\n%s (%d failed)\n", gFailures == 0 ? "ALL PASS" : "FAILURES", gFailures);
  return gFailures == 0 ? 0 : 1;
}
