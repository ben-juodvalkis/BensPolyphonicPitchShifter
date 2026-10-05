# Building and testing

Everything is driven by two scripts. macOS is the only platform built and tested so far.

## What you need

| For | You need |
|---|---|
| The tools and the tests | a C++17 compiler; Python 3.10 or newer with `requirements.txt` installed |
| The plug-in | CMake 3.22+, Xcode's command-line tools, and JUCE 8. A checkout at `~/JUCE` is used if it is there (or pass `JUCE_DIR=/path/to/JUCE`); otherwise CMake fetches JUCE 8.0.10. |
| The Max object | CMake, and Cycling '74's [max-sdk-base](https://github.com/Cycling74/max-sdk-base). Point `C74_SDK` at a checkout. |

```
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
```

## Build

```
scripts/build.sh tools                                   # build/tools/polypitch_cli, polypitch_load, polypitch_profile
scripts/build.sh plugin                                  # build/plugin/.../PolyPitch.vst3 and PolyPitch.component
C74_SDK=/path/to/max-sdk-base scripts/build.sh max       # max/PolyPitch/externals/polypitch~.mxo
scripts/build.sh                                         # all three
```

The plug-in and the Max object are built universal (Apple silicon and Intel), for macOS 11 or later. The plug-in
takes about two minutes the first time because JUCE is compiled with it.

## Install on this Mac

```
scripts/install-macos.sh            # plug-in into ~/Library/Audio/Plug-Ins, Max package into ~/Documents/Max 9/Packages
auval -v aufx PlyP Bjuo             # Apple's validation of the Audio Unit
```

Restart your host (or Max) afterwards. The Max for Live device is `max/device/Ben's Polyphonic Pitch Shifter.amxd`; see `docs/max.md`.

The builds are signed ad hoc, which is enough on the machine that built them. Binaries for other people need a
Developer ID signature and notarization (see `ROADMAP.md`).

## Test

```
scripts/gate.sh                                 # the gate: builds the tools, bad input samples, engine against reference, scorecard at -12, +2 and +12 (+12 in each response; all three in lite and in eco)
.venv/bin/python tests/test_bad_samples.py      # input samples that are not numbers, or infinite: the output stays finite (part of the gate)
.venv/bin/python tests/scorecard.py engine all  # all six intervals
.venv/bin/python tests/held_chords.py           # sustained chords after the attack (octave up; another interval as an argument)
.venv/bin/python tests/moving_pitch.py          # a slide, a bend and a vibrato: how far the pitch trails and how much it wobbles
.venv/bin/python tests/strummed_chords.py       # full chords strummed in time against their strings shifted one at a time
.venv/bin/python tests/test_plugin.py           # the built plug-in, hosted headless, against the engine
.venv/bin/python tests/mix_test.py a.wav b.wav  # the linearity test on two recordings of your own
```

Run the Python tests from the repository root as shown or from `tests/`. `scorecard.py` takes a target (`engine`,
`reference` or `plugin`) and a list of intervals; it, `held_chords.py`, `moving_pitch.py` and `strummed_chords.py`
also take `balanced` or `clean` to run that response (shifting up only; Fast is the default), and `lite` or `eco` to
run that quality.

The Max object and the device are checked inside Max with `max/tools/check_in_max.py` (see `docs/max.md`).

## The command-line tools

```
build/tools/polypitch_cli in.f32 out.f32 48000 -12            # raw 32-bit float mono in and out; the bare engine
build/tools/polypitch_cli in.f32 out.f32 48000 -12 50 100     # with Mix 50 and Tone 100: the whole processor
build/tools/polypitch_cli in.f32 out.f32 48000 12 response=2  # shifting up with another response: 0 fast, 1 balanced, 2 clean
build/tools/polypitch_cli in.f32 out.f32 48000 12 quality=2   # with another quality: 0 full, 1 lite, 2 eco
build/tools/polypitch_load in.f32 48000 12                    # CPU load and the worst 64-sample block (a fourth argument sets the response, a fifth the quality)
build/tools/polypitch_profile in.f32 48000 12                 # where the time goes, stage by stage (built with timers in the engine)
```

## The band-filter tables

`engine/PolyPitchFilters.h` is generated. To change the filters (another delay, another sample rate), edit
`reference/make_filters.py` and run it; each design takes a few seconds and is cached in `reference/.cache/`.

```
cd reference && ../.venv/bin/python make_filters.py
```

The engine picks the table for its sample rate, band count and delay (`makeBank`). If there is none it uses a simpler filter that needs
no table and leaks more; `Engine::usingDesignedFilter()` says which. Adding a rate means adding it to `RATES` in
`make_filters.py`. Rates of 88.2 kHz and above also need more bands to keep the same band width, which the engine
does not do yet.

## Layout

| Path | What it is |
|---|---|
| `engine/PolyPitchEngine.h` | The shifter. One header, no dependencies. |
| `engine/PolyPitchProcessor.h` | Mix and tone around the engine; what the plug-in and the Max object call. |
| `engine/PolyPitchFilters.h` | Generated band-filter tables. |
| `reference/` | The Python reference implementation, the filter design and the table generator. |
| `plugin/` | The JUCE wrapper (AU, VST3). |
| `max/PolyPitch/` | The Max package: object source, help patch, package description. |
| `max/device/Ben's Polyphonic Pitch Shifter.amxd` | The Max for Live device. |
| `max/tools/` | Generators for the device and help patch, and the in-Max check. |
| `tools/` | The command-line harness, the load meter and the profile. |
| `tests/` | Signals, measures, the scorecard, the held-chord, moving-pitch and strummed-chord tests, the engine-against-reference test, the plug-in test. |
| `scripts/` | Build, install, gate. |
| `docs/` | How it works, benchmarks, design notes, Max notes. |
