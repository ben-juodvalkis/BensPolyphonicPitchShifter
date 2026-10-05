# Roadmap

Ordered by what would help most. Numbers quoted here are from `docs/benchmarks.md`; each item says how to tell when
it is done.

## Order of work: sound before speed

Decided 2026-10-04. Work through section 3 (sound) before most of section 2 (performance). The one performance
item that could not wait is done: nothing is allocated on the audio thread any more (crossing between shifting up
and shifting down used to load a filter there, a dropout risk on stage).

1. **Now: sound** (section 3). These change what the engine computes, and each idea is tried in the Python
   reference first. Some will cost CPU (a long-history mode for held chords), so the final load is not known until
   they are in.
2. **Any time: the faster transform and the vectorized band loop** (section 2). The same math done faster; the gate
   proves the sound did not move. Worth doing early if the sound work needs the headroom.
3. **Last: spreading the analysis and doing less of it** (section 2). Both change when and which bands are checked,
   which is the logic the sound work rewrites. Optimizing it first would mean doing it twice.

Revisit this if the CPU load or the 128-sample buffer gets in the way of live playing before the sound work is done.

## 1. Before a first public release

- **Run the Max object and the device inside Max.** They are built but untested there, the device's Response
  buttons included. `max/tools/check_in_max.py` does it in under a minute; done when it prints `ALL MATCH`. Then
  freeze the device so it travels as one file.
- **Choose the default response by ear.** Shifting up there are three (Fast, Balanced, Clean: attacks 8, 12 and
  16 ms late, each cleaner in its own way; `docs/benchmarks.md`). Fast is the default because it was the engine's
  sound before the choice existed. Balanced is as late as the reference device and measures cleaner than Fast on
  nearly everything.
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
- Target: under 5 % shifting down and under 10 % shifting up.

## 3. Sound

- **Held chords shifting up.** 0.7 dB rougher than the reference device on average (it was 2 dB), up to 4.7 dB
  on a clean high chord (it was 9). The cause was not a short look-back (longer and narrower filters changed
  nothing): a real note's partial is a slowly beating cluster, and the engine was scaling its phase swings and
  putting readers on beats that were not there (`docs/design-notes.md`). What is left, on clean chords: a band that
  lies between two partials is on a reader, and its jumps leave sidebands about 35 dB down. Taking the two
  partials apart from what the bands on either side know, with no reader, would remove them; a first sketch of
  that is in the design notes. Done when `tests/held_chords.py` reaches the reference device's -40 dB.
- **Mixes of several parts at small upward shifts.** On twelve pairs of DI takes the reference device is 3 dB
  cleaner at +2 in every response, most where strummed chords meet another take; loop mixes show 1 to 2.4 dB at +2.
  (At +7 and +12 the Clean response is level with it or ahead; on Fast it is 1.3 to 2.1 dB behind there.) Cause
  unknown: not band width, not filter leak, and setting the phase only in the bands an attack reaches, instead of
  in all of them, changed nothing.
- **Slides shifting down.** The pitch wobbled 11 cents around the right value on a one-octave slide and 4 on a
  vibrato; it is now 3.3 and 0.3 (reference device: 2.1 and 1.5). Both causes are in `docs/design-notes.md`.
  What is left on the slide are short disturbances where a partial crosses from one band into the next and the
  bands change between plain and reader. Done when `tests/moving_pitch.py` shows 2 cents or less at -12 and -5.
- **Three partials in one band, without the lateness.** A reader gets two right. Shifting up, the Clean response
  gives each a band of its own by halving the band width, and pays 8 ms for it. Separating them by using what the
  neighboring bands know about each partial would get all three at the Fast response's 8 ms, and when shifting
  down. (Four or more steady partials in one band's width got worse when slow beats stopped being trusted at once:
  -9 dB of dirt where it was -15, on a synthetic test.)
- **The first beat after an attack, shifting up.** Until a beating band's beat has gone by once it is treated as
  plain. Carrying the previous note's knowledge across an attack on the same pitch would shorten that.
- **The middle note of a full chord on the Fast and Balanced responses.** At octave up the third of a full chord
  loses 6 to 9 dB of its fundamental, or 13 dB of its second harmonic, because two other notes' partials sit 30 to
  50 Hz either side and all three share every band there ("three partials in one band", heard). The Clean response
  has it within 1 dB, as the reference device does. Done when Fast has it too; that is the item above. (The other
  3 to 6 dB in the old "timbre" figure are partials that two notes share: their slow beat keeps its old rate, on
  the reference device as well.)
- **The loudness of a chord's partials at small shifts.** Partials that stand alone are 1.2 and 0.9 dB off at +2
  and +7 on Clean, 2.6 and 1.6 on Fast; the reference device has 0.3 and 0.5.
- **Chord-note tuning** from 0.1 to 0.6 cents to zero.
- **Below 60 Hz.** The lowest band is unused, so bass-guitar fundamentals below that are lost.
- **A cleaner Clean: its bands twice over.** 2048 bands (Clean's 43 Hz bands, half a band apart, as Fast does
  with the wide ones) measured -29.6 dB on the real held chords in the reference implementation, the only thing
  tried that beats the reference device's -28.4, and two-note chords 3 dB cleaner than Clean. It is about twice
  the work, so it waits for the faster transform (section 2).
- **Vibrato on the Clean response** wobbles 7 to 10 cents (Fast: 2.4 to 3.9), because a band half as wide follows
  a moving pitch half as fast. An idea, untried: let a plain band's phase advance lean on its neighbors when the
  pitch is moving.

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
