# Roadmap

Ordered by what would help most. Numbers quoted here are from `docs/benchmarks.md`; each item says how to tell when
it is done.

## Order of work: speed now

Decided 2026-10-05, after a day of "sound before speed". The sound work that was planned is in: held chords and
slides, the Response control, small shifts up (section 3 has what is left, all of it small or research-sized). What
stands between the engine and a stage now is section 2: the average load is fine, but the work arrives in lumps.

1. **Now: performance** (section 2), in the order given there, which comes from a measurement of where the time
   goes and not from the guesses this file used to hold. The transform, for one, is a sixteenth of the load.
2. **The sound is held in place while that happens.** Work that only does the same sums faster must leave the
   output as it is: the gate proves it, and so does a bit-for-bit comparison with the build before. Work that
   changes when or how often a band is looked at changes the sound a little; that is a sound change like any
   other: reference first, both directions and all three responses scored, the docs' numbers updated.
3. **Sound again afterwards**, steered by what playing it turns up.

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

Today: 8 to 10 % of one core shifting down and 17 to 21 % shifting up (48 kHz, Apple M1 Max). A 64-sample block
takes 15 % of its time slot at the median and 51 % at the 99.9th percentile at octave up (6 % and 24 % at octave
down). `build/tools/polypitch_profile` says where it goes, and `docs/benchmarks.md` ("Cost") has the table: at
octave up the band loop is 7.5 points of the 18.6, the likeness curves 4.7, the decisions 3.4 (1.5 of them fitting
readers to their neighbors), the per-band work at each frame 1.3, the transform 1.2.

In this order:

- **Spread the work that is done every 128 samples.** The likeness curves and the decisions for every band happen
  on one sample: 31 of the 47 % of a time slot in the heavy blocks. Two ways. Keep the decisions where they are and
  build the curves as the frames arrive (the same numbers, so the same sound up to rounding). Or look at a quarter
  of the bands at each frame, which moves each band's decision by up to 2 ms and is therefore a small sound change
  (reference first). By arithmetic, either takes the heavy blocks from about 50 % to about 30 %. Done, together
  with the next item, when the 99.9th-percentile 64-sample block is under 25 % of its slot at octave up.
- **Fitting a reader to its neighbors** is 10 of the 17 % that decisions take in the heavy blocks: each fit tries
  up to 21 positions, and there are 11 fits per check on average at octave up. Spread the fits over the following
  frames, or make one fit cheaper.
- **Do less of the curves.** At octave up 125 of 213 bands are awake and about 30 are on readers; the rest hold one
  steady partial and are compared with their past at every lag from 2 to 100 ms, every 128 samples, all the same. A coarse check for
  those, and the full one for bands in doubt and bands on readers. This changes which bands are looked at when: a
  sound change, to be scored as one.
- **Vectorize the band loop.** The largest part of the average: the same few multiplications for every band at
  every sample, with branches in the way (plain or reader, fading or not). The same sums done faster; the output
  must not move.
- **The transform and the per-band work at each frame**: 2.5 points together at octave up. A real, single-precision
  FFT (vDSP on Apple, pffft elsewhere, behind the same call so the engine keeps its no-dependency fallback) and a
  cheaper phase and loudness per band. Last, because it is the least.
- Target: under 5 % shifting down and under 10 % shifting up, and the 99.9th-percentile 64-sample block under 25 %
  of its slot, so that a 64-sample buffer is safe.

## 3. Sound

- **Held chords shifting up.** 0.7 dB rougher than the reference device on average (it was 2 dB), up to 4.7 dB
  on a clean high chord (it was 9). The cause was not a short look-back (longer and narrower filters changed
  nothing): a real note's partial is a slowly beating cluster, and the engine was scaling its phase swings and
  putting readers on beats that were not there (`docs/design-notes.md`). What is left, on clean chords: a band that
  lies between two partials is on a reader, and its jumps leave sidebands about 35 dB down. Taking the two
  partials apart from what the bands on either side know, with no reader, would remove them; a first sketch of
  that is in the design notes. Done when `tests/held_chords.py` reaches the reference device's -40 dB.
- **Mixes of several parts, shifting up by a fifth or more.** On twelve pairs of DI takes the Fast response is
  2.1 dB behind the reference device at +7 and 1.3 dB at +12 (Clean: level, and 1.2 dB ahead). Below a fifth the
  gap is closed on those twelve (it was 3.2 dB at +2): there a band was being put on a reader too readily and kept
  on it too long, where a reader has little to put right (`docs/design-notes.md`). The same settings do not carry
  to +7 and above, where they cost held chords what they give mixes. Cause of what is left unknown: not band
  width, not filter leak, and setting the phase only in the bands an attack reaches changed nothing.
- **Low notes a small interval apart, shifted up a little.** The price of the item above: such notes beat slowly,
  strongly and for real, and now wait longer for their reader. A few real chords on the lowest strings came out 4
  to 5 dB rougher at +2 while thirty together gained 3. Telling a strong steady beat from a passing one sooner
  would get both (leaving deep beats as they were did not: it gave the whole gain back). The clean response also
  lost two pure pairs and one two-take mix at +2 to the same change.
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
  plain. On chords strummed in time the first 120 ms after a strum are 4 to 10 dB dirtier than the rest
  (`tests/strummed_chords.py`), on the reference device as well. On mixes of real takes, though, the time just
  after an attack is the cleanest part, so this is not where mixes lose. Carrying the last beat across an attack
  on the same pitch would save perhaps a third of the wait and be wrong at every chord change; it was not built.
  What would remove the wait: once a band's two partials are known, take them apart frame by frame instead of
  waiting for a repeat (the same direction as in "Held chords" above).
- **The middle note of a full chord on the Fast and Balanced responses.** At octave up the third of a full chord
  loses 6 to 9 dB of its fundamental, or 13 dB of its second harmonic, because two other notes' partials sit 30 to
  50 Hz either side and all three share every band there ("three partials in one band", heard). The Clean response
  has it within 1 dB, as the reference device does. Done when Fast has it too; that is the item above. (The other
  3 to 6 dB in the old "timbre" figure are partials that two notes share: their slow beat keeps its old rate, on
  the reference device as well.)
- **The loudness of a chord's partials at small shifts.** Partials that stand alone are 1.2 to 2.0 dB off at +2 and
  0.9 to 1.6 dB at +7, depending on the response; the reference device has 0.3 and 0.5.
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
