# Benchmarks

What Ben's Polyphonic Pitch Shifter scores, how it was measured, and how a commercial reference device scores on the same tests.

All numbers are from October 2026, at 44.1 kHz unless stated, on the engine of version 0.1.0. In the dB columns,
lower (more negative) is cleaner.

## What you can run yourself

`python tests/scorecard.py engine all` runs the engine through synthetic material with exact answers and prints
this table (about four minutes; it needs no audio files):

| Shift | Two-sine pairs, median dirt | Pairs under -40 dB (of 28) | Two-note chords | Full chords | Chord notes off by | Attack late by | Single notes off by | Timbre error |
|---|---|---|---|---|---|---|---|---|
| -12 | -54.0 dB | 28 | -40.5 dB | -37.5 dB | 0.6 c | 2.1 ms | 0.1 c | 0.1 dB |
| -5 | -52.8 | 28 | -43.1 | -38.2 | 0.1 | 1.4 | 0.1 | 0.1 |
| -2 | -52.4 | 28 | -45.3 | -40.4 | 0.0 | 1.2 | 0.3 | 0.1 |
| +2 | -56.2 | 28 | -42.9 | -37.0 | 0.1 | 8.5 | 0.1 | 0.4 |
| +7 | -53.4 | 28 | -37.4 | -34.9 | 0.2 | 7.9 | 0.1 | 0.2 |
| +12 | -51.8 | 26 | -33.7 | -29.4 | 0.1 | 7.4 | 0.4 | 0.3 |

What the columns mean:

- **Pairs**: two sines at once, 28 combinations of register and interval. Dirt is all the power that is not the
  two shifted tones, relative to them.
- **Two-note chords, full chords**: plucked-string models, 20 two-note chords across the neck and 10 guitar
  voicings. Dirt is the power that is not at any partial the perfect shift would have.
- **Chord notes off by**: each note's tuning error in cents, read from a partial no other note shares (median).
- **Attack late by**: when a single plucked note's loudness reaches half its peak, against the perfect shift.
- **Timbre error**: how far each partial's loudness is from the perfect shift (rms over the partials within 30 dB
  of the strongest), on single notes.

`scripts/gate.sh` runs the -12 and +12 rows and fails if they fall outside limits set a little looser than this.

`python tests/mix_test.py a.wav b.wav` runs the "is it linear" test on two recordings of your own.

## Against a reference device

The project set out to match Line 6's Helix Native "Poly Capo" block, a well-regarded polyphonic shifter. It was
measured as a black box: the plug-in was hosted headless, fed the same test signals, and its output scored by the
same code. Its own tone compensation was turned off for scoring, and it was 100 % wet. Ben's Polyphonic Pitch Shifter is not affiliated
with Line 6; these are one person's measurements of one version of that product, and you should trust your ears
over this table.

### The synthetic scorecard

| Shift | Pairs under -40 dB, reference / this shifter | Two-note chords | Full chords | Attack late by |
|---|---|---|---|---|
| -12 | 15 / **28** | -27.8 / **-40.5** dB | -17.8 / **-37.5** dB | 9.8 / **2.1** ms |
| -5 | 18 / **28** | -31.1 / **-43.1** | -21.7 / **-38.2** | 10.8 / **1.4** |
| -2 | 19 / **28** | -36.3 / **-45.3** | -32.1 / **-40.4** | 11.4 / **1.2** |
| +2 | 21 / **28** | -39.6 / **-42.9** | -33.9 / **-37.0** | 11.8 / **8.5** |
| +7 | 21 / **28** | -31.5 / **-37.4** | -28.8 / **-34.9** | 12.0 / **7.9** |
| +12 | 21 / **26** | -33.4 / **-33.7** | -28.9 / **-29.4** | 11.7 / **7.4** |

The reference device is exactly in tune on every chord note (0.0 to 0.1 cents); Ben's Polyphonic Pitch Shifter is within 0.6. On pairs
of pure sines the reference device's median is lower going up (-52 to -62 dB against -52 to -56 dB): where it is
clean it is very clean.

### Real recordings

These use material that is not in the repository: DI guitar and bass takes by the author, and notes and loops from
a commercial sample library.

**Two takes mixed** (shifting them together should equal shifting them apart and adding; the number is the new
energy that appears only in the shifted mix), and **attack lateness** on the same takes:

| Shift | Two DI takes mixed, reference / this shifter | Attack late by, median | Slowest tenth of attacks |
|---|---|---|---|
| -12 | -21.4 / **-34.9** dB | 9.8 / **2.0** ms | 22 / **10** ms |
| -5 | -25.2 / **-32.5** | 10.8 / **1.9** | 23 / **14** |
| -2 | -25.9 / **-35.1** | 11.9 / **1.9** | 29 / **12** |
| +2 | -25.7 / **-26.3** | 12.0 / **8.1** | 30 / **11** |
| +7 | -22.8 / **-24.1** | 12.1 / **8.0** | 33 / **10** |
| +12 | -20.6 / **-20.7** | 12.2 / **7.7** | 37 / **10** |

**Chords built from single-note recordings** (26 notes of piano, keys, guitar, bass, bells and plucks from a
sample library, built into 30 chords; shifting each note alone and adding them up is an exact reference), and
**mixes of guitar, bass and keys loops** from the same library:

| Shift | Chords from single notes, reference / this shifter | Loop mixes, reference / this shifter |
|---|---|---|
| -12 | -27.4 / **-35.7** dB | -19.1 / **-24.1** dB |
| -5 | -33.1 / **-38.3** | -25.1 / **-26.5** |
| -2 | -34.2 / **-41.7** | -25.7 / **-26.4** |
| +2 | **-32.1** / -31.8 | **-23.9** / -19.8 |
| +7 | **-29.0** / -28.2 | **-21.6** / -20.6 |
| +12 | -26.2 / **-27.2** | **-19.6** / -18.9 |

**Held chords at octave up**, measured from 0.4 s into each chord so that only the sustained part counts (11
chords: two held DI chords, six sample-library guitar chords, three chords built from single notes):

| | Dirt between the notes | Fast loudness flutter (20 to 150 Hz, per third-octave band) |
|---|---|---|
| Reference | **-28.2 dB** | **-20.8 dB** |
| This shifter | -26.4 | -20.4 |

**Pitch that moves** (a one-octave slide, a whole-tone bend, a 6 Hz vibrato): how far the shifted pitch trails
the input, and how much it wobbles around the right value once that delay is allowed for.

| | Trails by, reference / this shifter | Wobble on the slide | on the bend | on the vibrato |
|---|---|---|---|---|
| Octave down | 12.3 to 13.4 / 11.6 to 12.4 ms | **2.1** / 10.6 c | 0.7 / 0.7 c | **1.5** / 3.8 c |
| Octave up | 22 to 46 / **8.7 to 9.7** ms | 14.9 / **3.5** c | 5.8 / **0.8** c | 31.6 / **4.3** c |

## Where Ben's Polyphonic Pitch Shifter is behind

- **Held chords and loop mixes when shifting up**: 1 to 4 dB rougher than the reference device on sample-library
  material, about 2 dB on held chords. Of the eleven held chords it is cleaner on three, level on one and rougher on seven, by
  1 to 9 dB, and the reason for the difference has not been found.
- **Slides and vibrato at octave down** wobble more around the right pitch (the table above).
- **Pure tones going up**: where the reference device is clean it reaches -60 dB; Ben's Polyphonic Pitch Shifter sits around -52 dB.
- **Tuning of chord notes**: 0.0 to 0.6 cents against 0.0 to 0.1.

## Cost

Measured with `build/tools/polypitch_load` on DI chord takes at 48 kHz, one core of an Apple M1 Max:

| | Average load | Worst 64-sample block |
|---|---|---|
| Shifting down | 8 to 10 % | 33 % of its time slot |
| Shifting up | 19 to 21 % | 64 % of its time slot |

The worst block is high because the engine does its analysis in one lump every 128 samples. Until that is spread
out (see `ROADMAP.md`), use a buffer of 128 samples or more.

## Sample rates

44.1 and 48 kHz use the designed band filters, and the engine matches the reference implementation at both
(`tests/test_engine_vs_reference.py`). At any other rate the engine falls back to a simpler, leakier filter and has
not been scored.
