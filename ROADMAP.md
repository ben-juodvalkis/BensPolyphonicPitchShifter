# Roadmap

Ordered by what would help most. Numbers quoted here are from `docs/benchmarks.md`; each item says how to tell when
it is done.

## Order of work: sound before speed

Decided 2026-10-04. Work through section 3 (sound) before most of section 2 (performance):

1. **Now: no allocation on the audio thread** (section 2). It is a dropout risk on stage when crossing between
   shifting up and shifting down, not just a cost, and it does not touch the sound.
2. **Then: sound** (section 3). These change what the engine computes, and each idea is tried in the Python
   reference first. Some will cost CPU (a long-history mode for held chords), so the final load is not known until
   they are in.
3. **Any time: the faster transform and the vectorized band loop** (section 2). The same math done faster; the gate
   proves the sound did not move. Worth doing early if the sound work needs the headroom.
4. **Last: spreading the analysis and doing less of it** (section 2). Both change when and which bands are checked,
   which is the logic the sound work rewrites. Optimizing it first would mean doing it twice.

Revisit this if the CPU load or the 128-sample buffer gets in the way of live playing before the sound work is done.

## 1. Before a first public release

- **Run the Max object and the device inside Max.** They are built but untested there. `max/tools/check_in_max.py`
  does it in under a minute; done when it prints `ALL MATCH`. Then freeze the device so it travels as one file.
- **A listening pass on real rigs.** The numbers say "better than the reference device shifting down, close to it
  shifting up". One player has confirmed it by ear at octave down and octave up; more ears and more instruments
  are needed.
- **Signed, notarized macOS binaries and a release archive** (plug-ins, Max package, frozen device). Today's
  builds are signed ad hoc and only run without warnings on the machine that built them.
- **Check the name.** Renamed from "PolyPitch", which Line 6 uses for a Helix effect, to Ben's Polyphonic Pitch Shifter.
  Only web searches have been done; a proper trademark check is still needed before release.

## 2. Performance

Today: 8 to 10 % of one core shifting down and 19 to 21 % shifting up (48 kHz, Apple M1 Max), with the worst
64-sample block taking 33 % and 64 % of its time slot.

- **Spread the analysis.** The engine compares every band with its past in one lump every 128 samples. Keeping
  those sums running frame by frame instead costs the same on average and removes the peak. Done when the worst
  64-sample block stays under 25 % of its slot at octave up.
- **Do less of it.** Most bands hold one partial most of the time and need only a coarse check; the full comparison
  can be kept for bands in doubt and bands already on a reader.
- **A faster transform.** The band values come from a plain double-precision FFT written for clarity. A real,
  single-precision FFT (vDSP on Apple, pffft elsewhere) would cut that part several times over.
- **Vectorize the band loop.** The per-sample work is the same few multiplications for every band.
- **No allocation on the audio thread.** Crossing between shifting up and shifting down loads a different filter
  and resizes buffers there. Both filter sets should be loaded up front.
- Target: under 5 % shifting down and under 10 % shifting up.

## 3. Sound

- **Held chords shifting up.** About 2 dB rougher than the reference device on average, up to 9 dB on some
  chords, and the cause is not known. One lead: the reference device trails bends by 45 ms when shifting up
  (Ben's Polyphonic Pitch Shifter: under 10 ms), so it may be leaning on a much longer look-back for sustained sound. An optional
  long-history mode for sustained notes would test that.
- **Slides and vibrato shifting down.** The pitch wobbles 10.6 cents around the right value on a one-octave slide
  (reference device: 2.1). A reader assumes the last beat will repeat; it should follow the pitch as it moves.
- **Three partials in one band.** A reader gets two right. Separating them by using what the neighboring bands
  know about each partial would get all three.
- **The first beat after an attack, shifting up.** Until a beating band's beat has gone by once it is treated as
  plain. Carrying the previous note's knowledge across an attack on the same pitch would shorten that.
- **Timbre on dense chords.** Partials' loudness is 3 to 6 dB off on full chords at octave up (0.3 dB on single
  notes).
- **Chord-note tuning** from 0.1 to 0.6 cents to zero.
- **Below 60 Hz.** The lowest band is unused, so bass-guitar fundamentals below that are lost.
- **A response setting for shifting up.** A longer band filter is cleaner and later: 10 ms gives about 1 dB cleaner
  chords with attacks 9.5 ms late; 12 ms matches the reference device's chords with attacks 11.3 ms late. Expose
  the choice.

## 4. Features

- **Change the interval while playing without a click** (run the old and new settings side by side for a few
  milliseconds).
- **More sample rates.** Filter tables for 88.2 and 96 kHz, with twice the bands so the band width stays the same.
- **Stereo.** Today the shifted sound is mono. Options: an engine per channel, or mid and side.
- **Optional latency reporting**, delaying the dry signal to line up with the shifted one, for parallel use.
- **Fine tuning in cents**, and whether intervals beyond an octave are worth supporting.
- **A plug-in interface of its own.** It currently shows the host's generic sliders.

## 5. Platforms and formats

- Windows: VST3 and the Max object (`.mxe64`). Nothing in the engine is Mac-specific.
- Linux: VST3 or LV2.
- CLAP.

## 6. The project itself

- **Continuous integration**: build and run the gate on every push.
- **A readability pass** over the reference and the engine. Both were written to be compared line against line,
  and it shows: long lines, terse names.
- **A public set of real recordings** for the benchmarks, so the real-material tables in `docs/benchmarks.md` can
  be reproduced by anyone. Contributions of DI takes under an open licence are welcome.
