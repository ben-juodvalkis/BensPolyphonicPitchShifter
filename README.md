# Ben's Polyphonic Pitch Shifter

Inside the code it goes by its old working name, PolyPitch: the source files, the bundle files and the Max object
`polypitch~`.

A polyphonic pitch shifter for live playing. Play a chord through it and the chord comes out an octave down, or
up, or anything in between: in tune, clean, and without the late attacks that make a shifter feel like wading.

It is one small C++ engine with no dependencies, offered three ways:

- a **plug-in** (Audio Unit and VST3),
- a **Max object**, `polypitch~`,
- a **Max for Live device** built on that object.

**Status: 0.1.0, not yet released.** macOS only so far. The plug-in is tested and passes Apple's validation. The
Max object and the device are built but have not been run inside Max yet. There are no downloads yet: build from
source (below).

## What it does

| | Octave down | Octave up |
|---|---|---|
| How late a picked note comes out | about 2 ms | about 7.5 ms |
| Chord notes in tune to | 0.4 cents | 0.2 cents |
| Dirt on full plucked chords (lower is cleaner) | -37.4 dB | -28.8 dB |
| CPU, one core at 48 kHz (Apple M1 Max) | 8 to 10 % | 19 to 21 % |

Shifts from -12 to +12 semitones. No latency is reported to the host and the dry signal is never delayed.

Measured against a well-regarded commercial polyphonic shifter on the same tests, Ben's Polyphonic Pitch Shifter is clearly cleaner and
about 8 ms earlier shifting down, and close to it in cleanliness with attacks 4 ms earlier shifting up. It is
behind on a few things, most of all mixes of several parts at small upward shifts and clean high chords held at
octave up. `docs/benchmarks.md` has the method, every number, and the list of where it loses.

## Build and install (macOS)

```
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt     # for the tests

scripts/build.sh tools plugin                               # the command-line tools and the plug-in
C74_SDK=/path/to/max-sdk-base scripts/build.sh max          # the Max object (needs Cycling '74's max-sdk-base)

scripts/install-macos.sh                                    # plug-in and Max package into your user folders
scripts/gate.sh                                             # the tests: about two minutes
```

The plug-in needs CMake and JUCE 8 (a checkout at `~/JUCE` is used if present, otherwise it is fetched). Details,
and how to run each test, are in `docs/building.md`. The Max side is described in `docs/max.md`.

## Controls

| | |
|---|---|
| **Semitones** | -12 to +12 |
| **Mix** | 0 = dry only, 50 = both at full level, 100 = shifted only |
| **Tone** | How much of a tone curve fitted to the interval is applied to the shifted sound. An octave down is brightened above 1 kHz; an octave up is darkened a little. 0 = flat. |

The shifted sound is mono (the inputs summed); the dry signal passes in stereo.

## How it works, briefly

The input is split into a few hundred narrow bands. A band that holds a single partial is not cut up at all: its
loudness passes straight through and its phase is advanced at the shifted rate, so it is exactly in tune and only
as late as the band filter. A band that holds two partials (two strings' harmonics close together) beats, and is
played by a reader that repeats or skips exactly one beat at a time, which keeps both partials right. Attacks are
played straight from the input when shifting down, which is why they are only 2 ms late.

`docs/how-it-works.md` explains it properly. `docs/design-notes.md` records what was tried first and why it failed.

## Limits

- Changing Semitones while playing restarts the bands and can click.
- 44.1 and 48 kHz are fully supported. Other sample rates run on a simpler, leakier filter and have not been scored.
- The work arrives in lumps: use a buffer of 128 samples or more for now.
- Held chords shifted up are a little rougher than the best commercial shifter, and the middle note of a full
  chord comes out too quiet at octave up.
- Nothing below about 60 Hz is shifted (bass guitar's lowest notes).

`ROADMAP.md` is the plan for all of these.

## What is in the repository

| | |
|---|---|
| `engine/` | The shifter (`PolyPitchEngine.h`), mix and tone around it (`PolyPitchProcessor.h`), generated filter tables |
| `plugin/` | The AU and VST3 wrapper (JUCE) |
| `max/` | The Max package with `polypitch~`, the Max for Live device, and the scripts that generate and check them |
| `reference/` | The same algorithm in Python, which the engine is tested against, and the band-filter design |
| `tests/` | Synthetic test signals, the scorecard and its regression gate |
| `docs/` | How it works, benchmarks, design notes, building, Max |

## Licence

MIT (see `LICENSE`). The plug-in links against JUCE, which changes the terms for plug-in binaries; the Max object
links against Cycling '74's SDK. `THIRD-PARTY.md` spells out what that means.

Ben's Polyphonic Pitch Shifter is an independent project, not affiliated with any of the companies whose products or formats it
mentions.
