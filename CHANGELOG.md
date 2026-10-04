# Changelog

## 0.1.0 (unreleased)

First version in this repository. The engine comes out of a research project that set out to replace a commercial
polyphonic "capo" effect in a live guitar rig; `docs/design-notes.md` tells that story.

- The engine (`engine/PolyPitchEngine.h`): a bank of narrow band filters; a band with one partial has its phase
  advanced at the shifted rate, a band with two partials is played by a reader that repeats exactly one beat.
  Shifts of -12 to +12 semitones. Attacks about 2 ms late shifting down, about 7.5 ms shifting up.
- The processor (`engine/PolyPitchProcessor.h`): mix and tone around the engine, shared by every build.
- A plug-in (AU and VST3, macOS, universal), a Max object (`polypitch~`, macOS, universal) and a Max for Live
  device that uses the object.
- A Python reference implementation the engine is tested against, a scorecard with a regression gate, and the
  band-filter design that generates the engine's tables.

Known gaps are listed in `ROADMAP.md`. The Max object and the device have been built but not yet run inside Max.
