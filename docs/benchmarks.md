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

All four tests here also take the words `lite` and `eco` (`python tests/scorecard.py engine lite all`), which run
them with the Quality control on that setting; their numbers are under "Lite and Eco" below. The gate runs Lite's
-12, +2 and +12 rows against the same limits as Full's, and Eco's against limits of its own (`GATE_ECO`).

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
| -12 | -21.4 / **-32.9** dB | -20.7 / **-31.4** dB |
| +2 | -21.4 / **-28.8** | -20.4 / **-29.4** |
| +7 | -18.9 / **-21.5** | -17.4 / **-22.6** |
| +12 | -18.7 / -18.7 | -16.1 / **-19.0** |

That is the ruler of the real-recording tables below (186 ms frames, fine in frequency). The test also prints the
same thing in 46 ms frames, fine in time and coarse in frequency, for the first 120 ms after a strum and for the
rest. Going up, the start of a chord is the dirtier part for both: at +12 this shifter has -23.2 dB early and -28.3
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

### Lite and Eco

The Quality control (`docs/how-it-works.md`, step 7) has two settings that do less. Lite takes about four fifths of
Full's CPU shifting down and under three fifths shifting up ("Cost" below): it looks for beating only below 2.5 kHz
of the input and, shifting up on Fast and Balanced, keeps the doubled bands only below 1.25 kHz. Eco takes about a
third of Full's CPU, less than the reference device's shifter at any interval: it is Lite with the bands looked at
every 64 samples instead of every 32 and, shifting up, nothing put out above 10.5 kHz. Everything above this
section is Full. The same tests, Full / Lite, on the Fast response:

| Shift | Pairs under -40 dB (of 28) | Two-note chords | Full chords |
|---|---|---|---|
| -12 | 28 / 28 | -42.4 / -40.1 dB | -37.4 / -36.5 dB |
| -5 | 28 / 28 | -44.1 / -43.9 | -37.4 / -36.6 |
| -2 | 28 / 28 | -45.3 / -45.3 | -40.1 / -40.1 |
| +2 | 28 / 27 | -43.1 / -42.9 | -38.9 / -38.2 |
| +7 | 28 / 27 | -41.7 / -35.9 | -35.9 / -33.8 |
| +12 | 26 / 25 | -35.1 / -31.8 | -28.8 / -27.9 |

(`python tests/scorecard.py engine lite all`.) The scorecard's other columns do not move: attacks within 0.1 ms,
tuning and single notes' timbre the same.

| Real recordings, Full / Lite | -12 | -5 | -2 | +2 | +7 | +12 |
|---|---|---|---|---|---|---|
| Twelve pairs of DI takes mixed | -29.3 / -29.3 dB | -31.0 / -31.0 | -31.2 / -31.2 | -27.1 / -27.0 | -22.4 / -22.9 | -20.8 / -21.0 |
| Chords built from single notes | -35.5 / -35.5 dB | -39.0 / -39.1 | -42.5 / -42.6 | -36.9 / -37.0 | -32.4 / -32.0 | -29.2 / -28.5 |
| Loop mixes | -25.0 / -25.2 dB | -27.8 / -27.8 | -28.1 / -28.2 | -24.2 / -24.4 | -20.9 / -21.0 | -19.9 / -19.9 |

| Full / Lite | |
|---|---|
| Eleven real held chords at +12: dirt, flutter | -27.7 / -27.0 dB, -21.0 / -20.8 dB |
| Ten synthetic held chords at +12 (`held_chords.py`): dirt, flutter | -37.7 / -33.9 dB, -37.8 / -37.4 dB |
| Strummed chords, strums 0.6 s apart, at -12, +2, +7, +12 | -32.9 / -32.4, -28.8 / -28.3, -21.5 / -20.9, -18.7 / -18.3 dB |
| Pitch that moves (slide, bend, vibrato; both octaves) | the same numbers |
| The middle note of a full chord at +2, +7, +12: loudness off by | 1.6 / 1.6, 2.2 / 2.2, 4.5 / 4.5 dB |
| Partials that stand alone in a full chord at +2, +7, +12: loudness off by | 1.4 / 2.1, 1.6 / 1.7, 2.4 / 3.0 dB |

In words: on recordings of guitars Lite is within 0.7 dB of Full everywhere, a little cleaner on mixes of DI takes
at +7 and +12 and a little dirtier on chords built from single notes and on held chords at +12. What it gives up
shows on clean, steady synthetic chords, in their upper harmonics: two-note chords are 2 dB dirtier at octave down,
6 dB at +7 and 3 dB at +12, held chords at +12 4 dB, and a chord's lone partials are 0.6 to 0.7 dB further from
their right loudness at +2 and +12. Attacks, tuning and pitch that moves are untouched.

With the other responses (the scorecard at +2 / +7 / +12, Full then Lite): Balanced has two-note chords at
-46.9 / -43.3 / -35.7 dB against -45.9 / -40.7 / -33.0 and full chords at -42.2 / -36.4 / -30.2 against
-42.1 / -34.6 / -29.6; Clean, which keeps all its bands, has two-note chords within 0.6 dB and full chords at
-40.4 / -37.3 / -33.4 against -40.3 / -36.6 / -31.7. On the real recordings both are within 0.7 dB of Full, as
Fast is. Synthetic held chords at +12: Balanced -39.1 against -35.7 dB, Clean -37.1 against -35.8.

**Eco**, the same tests, Lite / Eco, on the Fast response (`python tests/scorecard.py engine eco all`):

| Shift | Pairs under -40 dB (of 28) | Two-note chords | Full chords | Attack late by |
|---|---|---|---|---|
| -12 | 28 / 27 | -40.1 / -40.4 dB | -36.5 / -36.5 dB | 2.1 / 2.1 ms |
| -5 | 28 / 28 | -43.9 / -42.8 | -36.6 / -36.9 | 1.4 / 1.4 |
| -2 | 28 / 28 | -45.3 / -44.8 | -40.1 / -38.8 | 1.2 / 1.2 |
| +2 | 27 / 26 | -42.9 / -42.1 | -38.2 / -38.3 | 8.5 / 9.1 |
| +7 | 27 / 28 | -35.9 / -38.7 | -33.8 / -34.3 | 7.8 / 8.5 |
| +12 | 25 / 26 | -31.8 / -32.1 | -27.9 / -28.6 | 7.4 / 8.9 |

| Real recordings, Lite / Eco | -12 | -5 | -2 | +2 | +7 | +12 |
|---|---|---|---|---|---|---|
| Twelve pairs of DI takes mixed | -29.3 / -28.5 dB | -31.0 / -29.7 | -31.2 / -31.4 | -27.0 / -26.2 | -22.9 / -21.7 | -21.0 / -19.8 |
| Chords built from single notes | -35.5 / -35.1 dB | -39.1 / -38.7 | -42.6 / -42.0 | -37.0 / -33.4 | -32.0 / -31.1 | -28.5 / -28.3 |
| Loop mixes | -25.2 / -25.1 dB | -27.8 / -27.9 | -28.2 / -26.8 | -24.4 / -22.9 | -21.0 / -21.0 | -19.9 / -19.3 |

| Lite / Eco | |
|---|---|
| Eleven real held chords at +12: dirt, flutter | -27.0 / -26.6 dB, -20.8 / -20.5 dB |
| Ten synthetic held chords at +12: dirt, flutter | -33.9 / -32.0 dB, -37.4 / -36.9 dB |
| Strummed chords, strums 0.6 s apart, at -12, +2, +7, +12 | -32.4 / -29.5, -28.3 / -28.2, -20.9 / -20.0, -18.3 / -17.5 dB |
| Pitch that moves at octave down: a slide trails by, wobbles | 8.1 / 9.0 ms, 3.3 / 3.8 cents |
| Pitch that moves at octave up: a bend trails by, a vibrato wobbles | 12.0 / 12.3 ms, 3.9 / 4.1 cents |

Eco is meant to be played with Response on Balanced shifting up, where it is as late as the reference device (12 to
13 ms) and comes closest to it. Real recordings, reference device / Eco on Balanced, at +2 / +7 / +12: twelve DI
pairs -27.5 / **-28.5**, **-24.5** / -23.7, -22.1 / -22.2 dB; chords built from single notes -32.1 / **-35.4**,
-29.0 / **-32.4**, -26.2 / **-28.7**; loop mixes -23.9 / -24.1, -21.6 / **-22.6**, -19.6 / **-20.5**; held chords
at octave up **-28.4** / -27.2 (flutter -20.8 / -20.9). Shifting down Eco keeps most of this shifter's lead (the tables above against
"Real recordings"). What the reference device keeps: the ten clean synthetic held chords at octave up, -40.0 dB
against Eco's -33.9 on Balanced, and about 1 dB on the DI pairs at +7. Eco on Balanced scores, at +2 / +5 / +7 /
+12: pairs under -40 dB 27 / 27 / 28 / 27, two-note chords -46.1 / -42.3 / -40.4 / -32.8 dB, full chords
-39.1 / -36.3 / -33.6 / -28.9, attacks 13.2 / 12.8 / 12.4 / 11.8 ms; on Clean 25 / 28 / 28 / 28, -43.3 / -41.3 /
-39.0 / -35.0, -39.3 / -37.9 / -35.8 / -32.0, 17.2 / 16.8 / 16.4 / 15.8 ms.

The middle note of a full chord ("The three responses" below): on Eco it is 3.1 / 0.8 / 2.5 dB off on Fast at
+2 / +7 / +12 (Full 1.6 / 2.2 / 4.5) and 1.6 / 1.3 / 2.2 on Balanced (Full 1.2 / 0.4 / 2.5), but on Clean 5.8 dB
off at octave up against Full's 0.7: Eco on Clean loses what Clean is for.

The top octave of the shifted sound is where the two settings differ most to the ear: at octave up Full and Lite put
out the input's 5 to 10 kHz at 10 to 20 kHz, Eco and the reference device do not.

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

At 48 kHz on one core of an Apple M4, one DI chord take, measured with `build/tools/polypitch_load` (the three
qualities interleaved, the lowest of eight; the average load, and the 99.9th-percentile 64-sample block as a share
of its 1.33 ms):

| | Full | Lite | Eco |
|---|---|---|---|
| Octave down | 2.2 %, block 5.8 % | 1.8 %, block 5.4 % | 1.2 %, block 4.7 % |
| -2 | 2.0 %, 5.9 % | 1.6 %, 5.7 % | 1.1 %, 5.1 % |
| +2 | 4.0 %, 7.3 % | 2.3 %, 4.2 % | 1.4 %, 3.2 % |
| +7 | 4.4 %, 8.2 % | 2.5 %, 4.4 % | 1.5 %, 3.2 % |
| Octave up | 4.5 %, 8.9 % | 2.6 %, 4.7 % | 1.5 %, 3.5 % |
| Octave up, Balanced | 4.5 %, 8.7 % | 2.6 %, 4.6 % | 1.5 %, 3.3 % |
| Octave up, Clean | 4.5 %, 7.9 % | 3.5 %, 6.4 % | 1.9 %, 3.8 % |

The heavy blocks matter more than the average: a buffer of 64 samples has to be finished inside its 1.33 ms every
time. Both figures vary by about a tenth from one sitting to the next. Heavier material costs a little more: at
octave up a DI take of chord stabs 4.5 % with 9 % of the slot in the heavy block (Lite 2.6 % and 5 %, Eco 1.5 %
and 3 %), and a synthetic signal that keeps 84 bands on readers 5.6 % and 10 % (Lite 3.1 % and 5 %, Eco 2.0 %
and 4 %).

Lite (the Quality control; `docs/how-it-works.md`, step 7) looks for beating only below 2.5 kHz and, shifting up
on Fast and Balanced, keeps the doubled bands only below 1.25 kHz. Eco is Lite with the bands looked at every 64
samples instead of every 32 and, shifting up, nothing put out above 10.5 kHz. What they cost in sound is under
"Lite and Eco" above. On Clean, Lite only has the first of its two things to save on. Where Lite's 2.6 % at octave
up goes: the band loop 0.8, the transform 0.5 (it is still the 1024-point one), decisions 0.4, the curves 0.4,
per-band work at a frame 0.3, the window 0.2. Eco halves all of it but the band loop, and the band loop is two
fifths shorter for the bands that would land above 10.5 kHz.

**Before the performance work** (the engine of commit `83187b0`) the same computer took 5.6, 11.2, 12.3 and 12.5 %
on average and 14.0, 25.9, 31.5 and 33.9 % of the slot in the heavy block (the stab take 14.3 % and 36 % at octave
up, the synthetic signal 15.5 % and 41 %). After that work, and before beating was looked for only below 5 kHz
(below), it took 2.7, 4.9, 5.5 and 5.6 %, with 7.0, 10.0, 12.4 and 12.6 % of the slot (a sitting in which the
engine as it is now read 6.2, 8.4, 10.6 and 9.7 % of the slot: the heavy block varies by a tenth from one sitting
to the next). The engine of `83187b0` was also measured on an Apple M1 Max, which took 1.5 to 1.6 times as long
(8.5 % at octave down and 18.6 % at octave up; 24 % and 51 % of the slot). **The engine as it is now has not been
measured on an M1 Max.** If that computer is slower by the same factor, Full would take about 3.5 % and 7.0 % on
the chord take with 10 % and 16 % of the slot in the heavy block, and Lite 2.9 % and 4.0 % with 9 %.

For scale, the reference device's shifter was timed as a black box: its plug-in with nothing in it but that
shifter, hosted the same way as this project's own plug-in and timed in the same 64-sample blocks on the same take
and computer. It takes 1.9 % of a core whatever the interval, and 4 to 5 % of the slot in its 99.9th-percentile
block. This shifter's plug-in, timed in that host in the same sitting: on Full 2.4 % at octave down and 4.6 % at
octave up, 6 % and 9 % of the slot; on Lite 2.0 % and 2.7 %, 6 % and 5 %; on Eco 1.4 % and 1.6 %, 5 % and 4 %
(1.2 % at two semitones down, where the device read 1.8 % and 4 % in that sitting).

What changed (`docs/design-notes.md`, "Speed", has the measurements):

- The likeness curves (how the engine tells a band with one partial from a band with two,
  `docs/how-it-works.md`) used to be worked out from nothing for every band at every check, on the one sample in
  128 where the decisions are made as well. Their sums are now kept running as the frames arrive, on a frame that
  has no check, so they never fall in the same 64 samples as the decisions, and they cost the same whatever is
  being played. This is the one change after which the output is not sample for sample what it was.
- Fitting a reader to its neighbors costs a third of what it did: what is the same for every position tried is
  worked out once, and one sine and cosine do for all sixteen moments a reader is compared at.
- The band loop, the part that makes every output sample, costs a third as well: all bands are taken in one pass
  that the compiler works through several bands at a time, a reader keeps the frames it stands between instead of
  fetching them at every sample, and at the start of a frame all the sines and cosines are worked out in one go.
- The work at each frame is down by two fifths: every frame's loudness and phase are kept side by side for all
  bands, and the transform no longer works out what nobody reads.
- Beating is looked for only in the bands below 5 kHz of the input, which are half the bands. Above that every
  band is treated as plain. This is the second change that moves the output, and it moved no score: every
  scorecard row, held chord and moving-pitch number is what it was, the real-recording means are within 0.03 dB,
  and strummed chords moved by 0.1 to 0.6 dB (`docs/design-notes.md`, "Doing less").

Where the time goes now (`build/tools/polypitch_profile`, the chord take, Fast response, the least disturbed of
four runs; the profile's own timers add a little):

| Stage | How often | Octave up | Octave down |
|---|---|---|---|
| The band loop: one output sample from every band | every sample | 1.5 % of a core | 0.8 % |
| Likeness curves: the running sums, and their scaling at a check | every 128 samples, on two different frames | 0.9 | 0.4 |
| Decisions: plain or reader, jumps, who follows whom | every 128 samples | 0.9 | 0.3 |
| (of the decisions: fitting a reader to its neighbors) | | (0.2) | (0.04) |
| Per-band work at a frame: loudness, phase, smoothing | every 32 samples | 0.5 | 0.3 |
| The transform | every 32 samples | 0.5 | 0.2 |
| The window | every 32 samples | 0.2 | 0.2 |
| All | | 4.6 | 2.4 |

| 64-sample blocks, share of their time slot | Median | 99th percentile | 99.9th | Heaviest |
|---|---|---|---|---|
| Octave up | 4 % | 8 to 9 % | 9 to 10 % | 11 to 14 % |
| Octave down | 2 % | 4 % | 6 % | 8 to 10 % |

In the heaviest hundredth of the blocks at octave up, 3 of the 9 % of the slot are the decisions (half of that
fitting readers), 3 to 4 the band loop (more bands are fading from one stretch to the next there, and more are on
readers) and 1 the curves. The single heaviest block is a noisy number (it depends on what else the computer is
doing); the 99.9th percentile repeats.

Shifting up by less than a fifth costs a little less (fewer bands are on readers), and the three responses cost
about the same.

## Sample rates

44.1 and 48 kHz use the designed band filters, and the engine matches the reference implementation at both
(`tests/test_engine_vs_reference.py`). At any other rate the engine falls back to a simpler, leakier filter and has
not been scored.
