# Benchmarks

What Ben's Polyphonic Pitch Shifter scores, how it was measured, and how a commercial reference device scores on the same tests.

All numbers are from October 2026, at 44.1 kHz unless stated, on the engine of version 0.1.0. In the dB columns,
lower (more negative) is cleaner.

## What you can run yourself

`python tests/scorecard.py engine all` runs the engine through synthetic material with exact answers and prints
this table (about four minutes; it needs no audio files):

| Shift | Two-sine pairs, median dirt | Pairs under -40 dB (of 28) | Two-note chords | Full chords | Chord notes off by | Attack late by | Single notes off by | Timbre error |
|---|---|---|---|---|---|---|---|---|
| -12 | -52.1 dB | 28 | -42.4 dB | -37.4 dB | 0.4 c | 2.1 ms | 0.1 c | 0.1 dB |
| -5 | -53.0 | 28 | -44.1 | -37.4 | 0.2 | 1.4 | 0.1 | 0.1 |
| -2 | -53.9 | 28 | -45.3 | -40.1 | 0.1 | 1.2 | 0.3 | 0.1 |
| +2 | -55.8 | 28 | -43.1 | -37.6 | 0.1 | 8.5 | 0.1 | 0.1 |
| +7 | -54.5 | 28 | -41.8 | -35.9 | 0.2 | 7.9 | 0.1 | 0.1 |
| +12 | -52.9 | 26 | -35.2 | -28.8 | 0.2 | 7.4 | 0.3 | 0.2 |

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

`python tests/held_chords.py` measures what that table averages away: ten sustained chords in the high, sparse
register, from 0.4 s into each chord, where every partial should come out as one clean line. In five of them each
partial is a close pair, as a real string's is. It prints the dirt between the notes and the fast loudness flutter
(the two measures of "Held chords" below), here at octave up:

| | Dirt between the notes | Fast loudness flutter |
|---|---|---|
| Reference device (see below) | **-40.0 dB** | **-40.2 dB** |
| This shifter | -37.7 | -37.8 |

`python tests/moving_pitch.py` prints the "Pitch that moves" table further down (a slide, a bend and a vibrato whose
right answer is known at every moment).

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
| -12 | 15 / **28** | -27.8 / **-42.4** dB | -17.8 / **-37.4** dB | 9.8 / **2.1** ms |
| -5 | 18 / **28** | -31.1 / **-44.1** | -21.7 / **-37.4** | 10.8 / **1.4** |
| -2 | 19 / **28** | -36.3 / **-45.3** | -32.1 / **-40.1** | 11.4 / **1.2** |
| +2 | 21 / **28** | -39.6 / **-43.1** | -33.9 / **-37.6** | 11.8 / **8.5** |
| +7 | 21 / **28** | -31.5 / **-41.8** | -28.8 / **-35.9** | 12.0 / **7.9** |
| +12 | 21 / **26** | -33.4 / **-35.2** | -28.9 / -28.8 | 11.7 / **7.4** |

The reference device is exactly in tune on every chord note (0.0 to 0.1 cents); Ben's Polyphonic Pitch Shifter is within 0.4. On pairs
of pure sines the reference device's median is lower going up (-52 to -62 dB against -53 to -56 dB): where it is
clean it is very clean.

### Real recordings

These use material that is not in the repository: DI guitar and bass takes by the author, and notes and loops from
a commercial sample library.

**Two takes mixed** (shifting them together should equal shifting them apart and adding; the number is the new
energy that appears only in the shifted mix), and **attack lateness** on the same takes:

| Shift | Two DI takes mixed, reference / this shifter | Attack late by, median | Slowest tenth of attacks |
|---|---|---|---|
| -12 | -21.4 / **-35.9** dB | 9.8 / **2.0** ms | 22 / **11** ms |
| -5 | -25.2 / **-32.6** | 10.8 / **1.9** | 23 / **14** |
| -2 | -25.9 / **-34.4** | 11.9 / **1.9** | 29 / **11** |
| +2 | -25.7 / **-29.3** | 12.0 / **8.1** | 30 / **12** |
| +7 | -22.8 / **-26.6** | 12.1 / **8.0** | 33 / **11** |
| +12 | -20.6 / **-21.1** | 12.2 / **7.8** | 37 / **11** |

The mixes in that table are two pairs of takes (a bass line with a guitar line, and two guitar lines). **Twelve
pairs**, the ten added ones mostly with strummed chord takes, give a different picture going up (mean of the twelve):

| Shift | Twelve pairs of DI takes mixed, reference / this shifter |
|---|---|
| -12 | -24.1 / **-29.3** dB |
| -5 | -28.2 / **-31.0** |
| -2 | -29.3 / **-31.2** |
| +2 | **-27.5** / -24.3 |
| +7 | **-24.5** / -22.4 |
| +12 | **-22.1** / -20.8 |

**Chords built from single-note recordings** (26 notes of piano, keys, guitar, bass, bells and plucks from a
sample library, built into 30 chords; shifting each note alone and adding them up is an exact reference), and
**mixes of guitar, bass and keys loops** from the same library:

| Shift | Chords from single notes, reference / this shifter | Loop mixes, reference / this shifter |
|---|---|---|
| -12 | -27.4 / **-35.5** dB | -19.1 / **-25.0** dB |
| -5 | -33.1 / **-39.0** | -25.1 / **-27.8** |
| -2 | -34.2 / **-42.5** | -25.7 / **-28.1** |
| +2 | -32.1 / **-33.9** | **-23.9** / -21.5 |
| +7 | -29.0 / **-32.4** | **-21.6** / -20.7 |
| +12 | -26.2 / **-29.1** | -19.6 / **-19.9** |

**Held chords at octave up**, measured from 0.4 s into each chord so that only the sustained part counts (11
chords: two held DI chords, six sample-library guitar chords, three chords built from single notes):

| | Dirt between the notes | Fast loudness flutter (20 to 150 Hz, per third-octave band) |
|---|---|---|
| Reference | **-28.4 dB** | -20.8 dB |
| This shifter | -27.7 | **-21.0** |

Chord by chord the shifter is cleaner on four, level (within half a dB) on two and rougher on five, by 1.5 to
4.7 dB; the largest gap is on the cleanest, highest chord. (`tests/held_chords.py` is the synthetic version of
this test.)

**Pitch that moves** (a one-octave slide, a whole-tone bend, a 6 Hz vibrato; synthetic, `tests/moving_pitch.py`):
how far the shifted pitch trails the input, and how much it wobbles around the right value once that delay is
allowed for.

| | Trails by, reference / this shifter | Wobble on the slide | on the bend | on the vibrato |
|---|---|---|---|---|
| Octave down | 12.3 to 13.4 / **8.0 to 8.7** ms | **2.1** / 3.3 c | 0.8 / **0.5** c | 1.5 / **0.3** c |
| Octave up | 22 to 46 / **11.2 to 12.0** ms | 14.9 / **3.3** c | 5.8 / **0.9** c | 31.6 / **3.9** c |

## Where Ben's Polyphonic Pitch Shifter is behind

- **Mixes going up**: on twelve pairs of DI takes the reference device is 1.3 to 3.2 dB cleaner at up-shifts (most
  at +2, most on strummed chords mixed with another take), and on loop mixes 2.4 dB cleaner at +2 and 0.9 dB at +7.
  The reason has not been found.
- **Held chords when shifting up**: 0.7 dB rougher than the reference device on average, and 1.5 to 4.7 dB on five
  of the eleven. On clean, sparse chords (`tests/held_chords.py`) the gap is 2.3 dB. What made them rough, and what
  is left of it, is in `docs/design-notes.md`.
- **A fast slide at octave down** wobbles a little more around the right pitch (3.3 cents against 2.1).
- **The middle note of a full chord at octave up.** On the scorecard's synthetic chords the third of an open E, an
  A barre and a B minor barre chord has its fundamental 6, 9 and 13 dB too quiet and up to a hertz flat; the
  reference device has it within 0.3 dB. Two other notes' partials sit 30 to 50 Hz either side of it and share its
  bands. Over all the partials of those chords that have no other partial within 20 Hz, the loudness is off by
  2.4 dB (rms) against 1.0 dB for the reference device. Partials that share their place with another note's
  partial are 4 to 5 dB off on both, for a different reason: their slow beat keeps its old rate.
- **Pure tones going up**: where the reference device is clean it reaches -60 dB; Ben's Polyphonic Pitch Shifter sits around -52 dB.
- **Tuning of chord notes**: 0.1 to 0.4 cents against 0.0 to 0.1.

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
