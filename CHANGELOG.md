# Changelog

## 0.1.0 (unreleased)

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
- The engine's work is spread out and well under half of what it was: about 2 % of a core shifting down and 5 %
  shifting up at 48 kHz on an Apple M4, with the heaviest 64-sample blocks at 6 and 11 % of their time
  (`docs/benchmarks.md`, "Cost"). Its likeness sums are in double precision, as the reference's are. Beating is
  looked for only in the bands below 5 kHz of the input, which changed no score.
- An input sample that is not a number, or is infinite, counts as silence, in the shifted sound and in the dry
  one. Before, one such sample from the host left the output not a number until the next reset.

Known gaps are listed in `ROADMAP.md`. The Max object and the device have been built but not yet run inside Max.
