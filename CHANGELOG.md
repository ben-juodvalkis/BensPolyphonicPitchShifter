# Changelog

## 0.1.0 (2026-10-05)

First version in this repository. The engine comes out of a research project that set out to replace a commercial
polyphonic "capo" effect in a live guitar rig; `docs/design-notes.md` tells that story.

- The engine (`engine/PolyPitchEngine.h`): a bank of narrow band filters; a band with one partial has its phase
  advanced at the shifted rate, a band with two partials is played by a reader that repeats exactly one beat.
  Shifts of -12 to +12 semitones. Attacks about 2 ms late shifting down, about 7.5 ms shifting up.
- A Response setting for shifting up (Fast, Balanced, Clean): attacks about 8, 12 or 16 ms late, each step
  cleaner on chords and mixes. Clean runs bands half as wide, which brings the middle note of a full chord up to
  its right loudness.
- The processor (`engine/PolyPitchProcessor.h`): mix and tone around the engine, shared by every build.
- A plug-in (AU and VST3, macOS, universal), a Max object (`polypitch~`, macOS, universal) and a Max for Live
  device that uses the object.
- Named Ben's Polyphonic Pitch Shifter wherever users see it (plug-in, device, Max package, docs). PolyPitch,
  the working name, stays in code and file names; Line 6 uses "Poly Pitch" for a Helix effect.
- A Python reference implementation the engine is tested against, a scorecard with a regression gate, and the
  band-filter design that generates the engine's tables.
- The engine's work is spread out and well under half of what it was: about 2 % of a core shifting down and 4.5 %
  shifting up at 48 kHz on an Apple M4, with the 99.9th-percentile 64-sample blocks at 6 and 9 % of their time
  (`docs/benchmarks.md`, "Cost"). Its likeness sums are in double precision, as the reference's are. Beating is
  looked for only in the bands below 5 kHz of the input, which changed no score.
- A Quality setting (Full, Lite, Eco). Lite takes about four fifths of the CPU shifting down and under three
  fifths shifting up (1.8 % and 2.6 % of a core at the octaves on an Apple M4): beating is looked for only below
  2.5 kHz, and shifting up the doubled bands are kept only below 1.25 kHz. Recordings of guitars came out within
  0.7 dB of Full; clean, steady chords are 2 to 6 dB less clean in their upper harmonics. Eco (1.2 % and 1.5 %) is
  Lite with the bands looked at every 64 samples instead of every 32 and, shifting up, nothing put out above
  10.5 kHz; it costs less than the commercial shifter the project is measured against, and with Response on
  Balanced it is as late as that shifter and within about a decibel of it on recordings of guitars where that
  shifter leads; on clean synthetic held chords at octave up it is 6 dB behind (`docs/benchmarks.md`, "Lite and
  Eco").
- Changing Semitones, Quality, or Response while shifting up no longer clicks: the processor cross-fades between
  two engines, one playing on with the old setting while the other starts over with the new one. Shifting up the new
  interval takes over about 0.15 s after the change; a sweep moves in steps. Nothing changes while the knobs are still.
- An input sample that is not a number, or is infinite, counts as silence, in the shifted sound and in the dry
  one. Before, one such sample from the host left the output not a number until the next reset.

- A release archive for macOS (`scripts/release-macos.sh`): the AU and VST3, the Max package and the Max for Live
  device (frozen, so it works without the package), signed with a Developer ID and notarized.

Known gaps are listed in `ROADMAP.md`. The Max for Live device has been played in Live; the Max object's own check
inside Max has not been run.
