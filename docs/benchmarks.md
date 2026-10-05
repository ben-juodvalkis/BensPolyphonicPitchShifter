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
| +2 | -55.3 | 28 | -43.1 | -38.9 | 0.1 | 8.5 | 0.1 | 0.1 |
| +7 | -54.5 | 28 | -41.7 | -35.9 | 0.2 | 7.9 | 0.1 | 0.1 |
| +12 | -52.9 | 26 | -35.1 | -28.8 | 0.2 | 7.4 | 0.3 | 0.2 |

What the columns mean:

- **Pairs**: two sines at once, 28 combinations of register and interval. Dirt is all the power that is not the
  two shifted tones, relative to them.
- **Two-note chords, full chords**: plucked-string models, 20 two-note chords across the neck and 10 guitar
  voicings. Dirt is the power that is not at any partial the perfect shift would have.
- **Chord notes off by**: each note's tuning error in cents, read from a partial no other note shares (median).
- **Attack late by**: when a single plucked note's loudness reaches half its peak, against the perfect shift.
- **Timbre error**: how far each partial's loudness is from the perfect shift (rms over the partials within 30 dB
  of the strongest), on single notes.

`scripts/gate.sh` runs the -12, +2 and +12 rows and fails if they fall outside limits set a little looser than this.

Shifting up there are three **responses** (the Response control). The table above is Fast, the default.
`python tests/scorecard.py engine balanced 2 7 12` (or `clean`) prints the other two:

| Shift | Response | Two-sine pairs, median dirt | Pairs under -40 dB (of 28) | Two-note chords | Full chords | Chord notes off by | Attack late by |
|---|---|---|---|---|---|---|---|
| +2 | Fast | -55.3 dB | 28 | -43.1 dB | -38.9 dB | 0.1 c | 8.5 ms |
| | Balanced | -60.7 | 28 | -46.9 | -42.2 | 0.1 | 12.5 |
| | Clean | -53.4 | 24 | -42.9 | -40.4 | 0.1 | 16.5 |
| +7 | Fast | -54.5 | 28 | -41.7 | -35.9 | 0.2 | 7.9 |
| | Balanced | -59.4 | 28 | -43.3 | -36.4 | 0.2 | 11.9 |
| | Clean | -53.2 | 28 | -40.3 | -37.3 | 0.0 | 15.8 |
| +12 | Fast | -52.9 | 26 | -35.1 | -28.8 | 0.2 | 7.4 |
| | Balanced | -60.1 | 27 | -35.7 | -30.2 | 0.2 | 11.3 |
| | Clean | -55.2 | 28 | -35.1 | -33.4 | 0.0 | 15.4 |

Balanced is the same bands behind a longer filter (12 ms instead of 8), which lets less of each partial into the
bands around it. Clean has bands half as wide behind a 16 ms filter. Single notes are in tune to 0.1 to 0.3 cents
and their timbre within 0.2 dB in all three. The gate also runs the +12 row of Balanced and of Clean.

`python tests/held_chords.py` measures what that table averages away: ten sustained chords in the high, sparse
register, from 0.4 s into each chord, where every partial should come out as one clean line. In five of them each
partial is a close pair, as a real string's is. It prints the dirt between the notes and the fast loudness flutter
(the two measures of "Held chords" below), here at octave up:

| | Dirt between the notes | Fast loudness flutter |
|---|---|---|
| Reference device (see below) | **-40.0 dB** | **-40.2 dB** |
| This shifter (Fast) | -37.7 | -37.8 |
| Balanced | -39.1 | -38.6 |
| Clean | -37.1 | -37.4 |

`python tests/moving_pitch.py` prints the "Pitch that moves" table further down (a slide, a bend and a vibrato whose
right answer is known at every moment).

`python tests/strummed_chords.py` asks whether the shifter is linear on full chords strummed in time: three
six-string chords, every string struck anew at each strum, shifted as a whole and one string at a time. A perfect
shifter gives the same sound both ways. The new energy that appears only in the whole pattern:

| Shift | Strums 0.6 s apart, reference device / this shifter | Strums 0.3 s apart |
|---|---|---|
| -12 | -21.4 / **-33.0** dB | -20.7 / **-31.5** dB |
| +2 | -21.4 / **-28.8** | -20.4 / **-29.4** |
| +7 | -18.9 / **-21.4** | -17.4 / **-22.5** |
| +12 | -18.7 / -18.6 | -16.1 / **-19.0** |

That is the ruler of the real-recording tables below (186 ms frames, fine in frequency). The test also prints the
same thing in 46 ms frames, fine in time and coarse in frequency, for the first 120 ms after a strum and for the
rest. Going up, the start of a chord is the dirtier part for both: at +12 this shifter has -22.9 dB early and -28.3
late, the reference device -25.1 and -39.8. By that ruler the reference device is the cleaner one once a chord
has settled (by 4 dB at +7 and 11 dB at +12), which is the roughness of "Held chords" below seen another way.

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
| +2 | 21 / **28** | -39.6 / **-43.1** | -33.9 / **-38.9** | 11.8 / **8.5** |
| +7 | 21 / **28** | -31.5 / **-41.7** | -28.8 / **-35.9** | 12.0 / **7.9** |
| +12 | 21 / **26** | -33.4 / **-35.1** | -28.9 / -28.8 | 11.7 / **7.4** |

The reference device is exactly in tune on every chord note (0.0 to 0.1 cents); Ben's Polyphonic Pitch Shifter is within 0.4. On pairs
of pure sines the reference device's median is lower going up (-52 to -62 dB against -53 to -56 dB): where it is
clean it is very clean.

### Real recordings

These use material that is not in the repository: DI guitar and bass takes by the author, and notes and loops from
a commercial sample library.

The engine's likeness sums have since gone from single to double precision (`docs/design-notes.md`, "Speed"), which
moves a borderline decision here and there. Everything below was measured again after that, and eight figures moved
by 0.1 dB (one by 0.2), except the first table's "Two DI takes mixed" and the attack figures on DI takes (in
that table and in the table of the three responses): their script is not on the computer the work was done on. A
closely related measure of the same two mixes moved by 0.03 dB at most.

**Two takes mixed** (shifting them together should equal shifting them apart and adding; the number is the new
energy that appears only in the shifted mix), and **attack lateness** on the same takes:

| Shift | Two DI takes mixed, reference / this shifter | Attack late by, median | Slowest tenth of attacks |
|---|---|---|---|
| -12 | -21.4 / **-35.9** dB | 9.8 / **2.0** ms | 22 / **11** ms |
| -5 | -25.2 / **-32.6** | 10.8 / **1.9** | 23 / **14** |
| -2 | -25.9 / **-34.4** | 11.9 / **1.9** | 29 / **11** |
| +2 | -25.7 / **-31.1** | 12.0 / **8.1** | 30 / **10** |
| +7 | -22.8 / **-26.6** | 12.1 / **8.0** | 33 / **11** |
| +12 | -20.6 / **-21.1** | 12.2 / **7.8** | 37 / **11** |

The mixes in that table are two pairs of takes (a bass line with a guitar line, and two guitar lines). **Twelve
pairs**, the ten added ones mostly with strummed chord takes, give a different picture going up (mean of the twelve):

| Shift | Twelve pairs of DI takes mixed, reference / this shifter |
|---|---|
| -12 | -24.1 / **-29.3** dB |
| -5 | -28.2 / **-31.0** |
| -2 | -29.3 / **-31.2** |
| +2 | **-27.5** / -27.1 |
| +7 | **-24.5** / -22.4 |
| +12 | **-22.1** / -20.8 |

At +2 the two are now level on these twelve; they were 3.2 dB apart until shifts up by less than a fifth got
settings of their own (`docs/design-notes.md`). On all 45 pairings of the ten takes:

| Shift | 45 pairings, the ruler used above, reference / this shifter | A sharper ruler |
|---|---|---|
| +2 | **-28.6** / -27.1 dB | -24.1 / **-25.2** dB |
| +4 | **-26.7** / -25.5 | -23.2 / -23.1 |
| +5 | **-26.3** / -24.5 | **-23.0** / -22.5 |
| +7 | **-25.6** / -21.9 | **-22.4** / -21.0 |

**The ruler matters at small shifts.** The measure used above allows a partial to sit up to 11 Hz from its place
before it counts as new energy. The sharper one (frames four times as long) allows 2.7 Hz. Under it the reference
device loses 3 to 4.5 dB and this shifter 1 to 2.5: where two partials of different takes are too close for it to
tell apart, the reference device carries them along as one beating note, which leaves the weaker one a few Hz from
where it belongs. By the sharp ruler this shifter is 1.1 dB ahead at +2, level at +4 and half a dB behind at +5.

**Chords built from single-note recordings** (26 notes of piano, keys, guitar, bass, bells and plucks from a
sample library, built into 30 chords; shifting each note alone and adding them up is an exact reference), and
**mixes of guitar, bass and keys loops** from the same library:

| Shift | Chords from single notes, reference / this shifter | Loop mixes, reference / this shifter |
|---|---|---|
| -12 | -27.4 / **-35.5** dB | -19.1 / **-25.0** dB |
| -5 | -33.1 / **-39.0** | -25.1 / **-27.8** |
| -2 | -34.2 / **-42.5** | -25.7 / **-28.1** |
| +2 | -32.1 / **-36.9** | -23.9 / **-24.2** |
| +7 | -29.0 / **-32.4** | **-21.6** / -20.9 |
| +12 | -26.2 / **-29.2** | -19.6 / **-19.9** |

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
| Octave up, Balanced | 22 to 46 / **13.9 to 14.9** ms | 14.9 / **3.0** c | 5.8 / **1.0** c | 31.6 / **5.6** c |
| Octave up, Clean | 22 to 46 / **17.7 to 20.1** ms | 14.9 / **4.8** c | 5.8 / **1.8** c | 31.6 / **9.5** c |

### The three responses, shifting up

Everything above is the Fast response. The same tests with the other two, at +2 / +7 / +12 (the scorecard's rows
for them are under "What you can run yourself"):

| | Fast | Balanced | Clean | Reference device |
|---|---|---|---|---|
| Attack late by, median (DI takes) | 8.1 / 8.0 / 7.8 ms | 12.1 / 12.0 / 11.7 | 16.0 / 15.9 / 15.8 | 12.0 / 12.1 / 12.2 |
| Slowest tenth of attacks | 10 / 11 / 11 ms | 14 / 13 / 14 | 18 / 19 / 19 | 30 / 33 / 37 |
| Two DI takes mixed | -31.1 / -26.6 / -21.1 dB | **-32.1** / -26.7 / -22.7 | -26.8 / **-28.8** / **-24.7** | -25.7 / -22.8 / -20.6 |
| Twelve pairs of DI takes mixed | -27.1 / -22.4 / -20.8 dB | -27.7 / -23.7 / -21.5 | **-28.4** / **-24.6** / **-23.3** | -27.5 / -24.5 / -22.1 |
| Chords from single notes | -36.9 / **-32.4** / -29.2 dB | **-37.7** / **-32.4** / -30.6 | -36.3 / -31.5 / **-31.0** | -32.1 / -29.0 / -26.2 |
| Loop mixes | -24.2 / -20.9 / -19.9 dB | -25.5 / **-22.7** / **-22.0** | **-26.3** / -22.4 / -21.1 | -23.9 / -21.6 / -19.6 |
| The middle note of a full chord: loudness off by | 1.6 / 2.2 / 4.5 dB | 1.2 / **0.4** / 2.5 | 0.9 / 0.6 / 0.7 | **0.5** / 0.5 / **0.5** |
| Partials that stand alone in a full chord: loudness off by | 1.4 / 1.6 / 2.4 dB | 1.2 / 1.2 / 1.9 | 2.0 / 0.9 / **0.9** | **0.3** / **0.5** / 1.0 |
| Held chords at +12: dirt / flutter | -27.7 / **-21.0** dB | -27.9 / **-21.0** | -27.5 / -20.5 | **-28.4** / -20.8 |

The middle note is the third of six of the scorecard's full chords (its fundamental, rms over the six); the
partials that stand alone are those of all ten chords with no other partial within 20 Hz.

In words: Balanced is as late as the reference device and cleaner than Fast on nearly everything, by 5 to 7 dB on
pure intervals, up to 4 dB on chords and up to 2 dB on mixes; a vibrato wobbles a little more (5.6 cents against
3.9 at octave up). Clean is 8 ms later than Fast. It is the one that gets a chord's middle note right, and it is 2
to 3.6 dB cleaner than Fast on mixes of DI takes at +7 and +12. What it costs besides the 8 ms: pitch that moves is
followed less closely (a vibrato wobbles 7 to 10 cents, bends trail by 16 to 20 ms), and pure intervals at +2 and
+7 are 1 to 3 dB less clean than on Fast. At +2 Clean is the cleanest of the three on the twelve pairs and on loop
mixes but the roughest on the two-take mix of the first table (a bass line with a guitar line, and two guitar
lines), which lost 3 dB on Clean when small shifts got their own settings while the other two responses gained.

## Where Ben's Polyphonic Pitch Shifter is behind

- **Mixes going up by a fifth or more, on the Fast response**: on twelve pairs of DI takes the reference device is
  2.1 dB cleaner at +7 and 1.3 dB at +12, and on loop mixes 0.7 dB at +7. The Clean response is level with it on
  the twelve pairs at +7 and 1.2 dB ahead at +12, and Balanced is ahead on loop mixes at both. The reason for what
  is left has not been found. (Below a fifth the gap is closed on the twelve pairs and on loop mixes; on all 45
  pairings the reference device is 1.1 to 1.8 dB ahead from +2 to +5 by the usual ruler, and within about 1 dB
  either way by the sharp one.)
- **Low notes a small interval apart, shifted up a little.** Closing that gap had a price: a few real chords on the
  lowest strings (a minor third on low E, a minor triad on low G) came out 4 to 5 dB rougher at +2 while the thirty
  chords together gained 3 dB, and one loop of bass with guitar 2.7 dB rougher while the ten loop mixes together
  gained 2.7 dB.
- **Held chords when shifting up**: 0.7 dB rougher than the reference device on average, and 1.5 to 4.7 dB on five
  of the eleven. On clean, sparse chords (`tests/held_chords.py`) the gap is 2.3 dB on Fast and 0.9 dB on
  Balanced. What made them rough, and what is left of it, is in `docs/design-notes.md`.
- **A fast slide at octave down** wobbles a little more around the right pitch (3.3 cents against 2.1).
- **The middle note of a full chord at octave up, unless the response is Clean.** On the scorecard's synthetic
  chords, with the Fast response, the third of an open E and of an A barre chord has its fundamental 6 and 9 dB too
  quiet and up to a hertz flat, and the third of a B minor barre chord has its second harmonic 13 dB too quiet; the
  reference device has all three within 0.7 dB, and so does Clean (within 1.0 dB, and on pitch). Two other notes'
  partials sit 30 to 50 Hz either side of that partial and share its bands; in Clean's half-width bands each has
  one of its own. Balanced helps the open E (1.6 dB) and not the other two. Partials that share their place with
  another note's partial are 4 to 5 dB off in every response and on the reference device, for a different reason:
  their slow beat keeps its old rate.
- **The loudness of a chord's partials at small shifts.** Partials that stand alone are 1.2 to 2.0 dB off at +2 and
  0.9 to 1.6 dB at +7, depending on the response, against 0.3 and 0.5 for the reference device.
- **Pure tones going up**: where the reference device is clean it reaches -60 dB. Fast sits around -53 to -56 dB;
  Balanced reaches -60.
- **Tuning of chord notes**: 0.1 to 0.4 cents against 0.0 to 0.1 (Clean going up: 0.0 to 0.1).

## Cost

At 48 kHz on one core of an Apple M4, one DI chord take, measured with `build/tools/polypitch_load` (runs interleaved
with the build before, the lowest of six):

| | Average load | The 99.9th-percentile 64-sample block, share of its 1.33 ms |
|---|---|---|
| Octave down | 3.2 % | 7.5 % |
| +2 | 5.9 % | 10.7 % |
| Octave up | 6.4 % | 13.4 % |
| Octave up, Clean | 6.5 % | 13.3 % |

The heavy blocks matter more than the average: a buffer of 64 samples has to be finished inside its 1.33 ms every
time. Until they are lighter still (see `ROADMAP.md`), use a buffer of 128 samples or more.

**Before the performance work** (the engine of commit `83187b0`) the same computer took 5.6, 11.2, 12.2 and 11.8 %
on average and 14.4, 25.9, 30.7 and 31.2 % of the slot in the heavy block. That engine was also measured on an Apple
M1 Max, which took 1.5 to 1.6 times as long (8.5 % at octave down and 18.6 % at octave up; 24 % and 51 % of the
slot). The engine as it is now has not been measured on an M1 Max.

What has changed so far: the likeness curves (how the engine tells a band with one partial from a band with two,
`docs/how-it-works.md`) used to be worked out from nothing for every band at every check, on the one sample in 128
where the decisions are made as well. Their sums are now kept running as the frames arrive, on a frame that has no
check, so they never fall in the same 64 samples as the decisions, and they cost the same whatever is being played.
The output is not sample for sample what it was: `docs/design-notes.md` ("Speed") has why, and what was measured.
And fitting a reader to its neighbors costs a third of what it did: what is the same for every position tried is
worked out once, and one sine and cosine do for all sixteen moments a reader is compared at.
The band loop, the part that makes every output sample, costs a third as well: all bands are taken in one pass that
the compiler works through several bands at a time, a reader keeps the frames it stands between instead of
fetching them at every sample, and at the start of a frame all the sines and cosines are worked out in one go.

Where the time goes now (`build/tools/polypitch_profile`, the same take, Fast response, the least disturbed of
three runs; the profile's own timers add a little):

| Stage | How often | Octave up | Octave down |
|---|---|---|---|
| Likeness curves: the running sums, and their scaling at a check | every 128 samples, on two different frames | 1.7 % of a core | 0.9 % |
| The band loop: one output sample from every band | every sample | 1.7 | 1.0 |
| Decisions: plain or reader, jumps, who follows whom | every 128 samples | 1.2 | 0.5 |
| (of the decisions: fitting a reader to its neighbors) | | (0.3) | (0.1) |
| Per-band work at a frame: loudness, phase, smoothing | every 32 samples | 1.0 | 0.6 |
| The transform | every 32 samples | 0.7 | 0.3 |
| The window | every 32 samples | 0.2 | 0.2 |
| All | | 6.6 | 3.6 |

In the heaviest hundredth of the blocks at octave up, 4 to 5 of the 12 to 14 % of the slot are the decisions (2 of
them fitting readers), 4 to 5 the band loop (more bands are fading from one stretch to the next there, and more
are on readers) and 2 the curves. The single heaviest block is a noisy number (it depends on
what else the computer is doing); the 99.9th percentile repeats.

Shifting up by less than a fifth costs a point less (fewer bands are on readers), and the three responses cost the
same.

## Sample rates

44.1 and 48 kHz use the designed band filters, and the engine matches the reference implementation at both
(`tests/test_engine_vs_reference.py`). At any other rate the engine falls back to a simpler, leakier filter and has
not been scored.
