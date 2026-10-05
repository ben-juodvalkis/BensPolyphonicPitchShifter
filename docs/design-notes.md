# Design notes: what was tried, and what it taught

Ben's Polyphonic Pitch Shifter came out of a practical problem: a live guitar rig used a commercial polyphonic "capo" effect for an
octave down, and its attacks came out about 10 ms late. The goal was something at least as clean on chords with
earlier attacks. This page records the path, because most of the ideas that did not work are ideas a newcomer
would try first.

Everything here was decided by measurement. The reference device was only ever studied as a black box: audio in,
audio out.

## The path

| Step | Idea | What happened |
|---|---|---|
| 1 | One time-domain reader for the whole signal, jumping by a length at which the waveform repeats | Excellent on single notes, with attacks about 2 ms late. On chords the notes drift a few cents apart and the result is rough, because one jump length cannot suit two notes that share no cycle. |
| 2 | Narrow bands, one such reader per band | Clean and in tune on test tones. **Fails on real notes**: a partial always spills into two or three bands, each band estimates a slightly different repeat, the copies drift apart, and the partial wobbles or half cancels. |
| 3 | Bands share readers: all start on one, a band leaves only when that reader's jump is not a repeat for what the band holds | Real notes clean again, chord notes in tune to 1 or 2 cents. Level with the reference device on chords. In effect one reader per note. |
| 4 | Play attacks straight from the input until the bands are the same signal (the bridge) | Attack lateness 14 ms to 2 ms when shifting down. Nothing else changes. Not possible when shifting up. |
| 5 | A phase correction per partial at each jump | Exact on well-separated partials, worse on dense chords. Not better overall. |
| 6 | Better band filters, designed by least squares for a given delay | Leak into the next band from -20 dB to -44 dB at 12 ms delay. Everything downstream got easier. |
| 7 | Choose each band's jump by how its envelope repeats, with the phase carried across exactly | Two sines 15 Hz apart come out clean and in tune, which no earlier version managed. But any short jump on a band with two partials is as wrong as none. |
| 8 | **No reader where a band holds one partial** (advance its phase instead); a reader that jumps exactly one beat where a band holds two | The current design. Exact tuning by construction, nothing repeated in most bands, and up-shift attacks 6 ms earlier because no reader has to catch up. |
| 9 | Keep the two shares of a partial in step (followers and placement) | Fixed two faults that only real recordings showed (below). |
| 10 | Shifting up: twice the bands at the same width, steadier decisions, readers' loudness corrected | Held chords at octave up went from clearly rougher than the reference device to close to it. |
| 11 | A plain band's phase advance smoothed over 5 ms; a slow beat has to hold before a reader takes it (first for shifting up) | Held chords at octave up 1 dB cleaner on average and 4 dB on the one that was worst, chords built from real notes 2 to 4 dB at every up-shift, with a little less flutter than the reference device. Steady synthetic clusters of four or more partials in one band got worse (below). |

## Things worth knowing before changing the engine

**A partial lives in two bands.** With these filters every partial has a share in each of two neighboring bands.
If the two shares are not kept in step the partial comes out quiet. A real bass note lost 15 to 19 dB of its
fundamental at octave down because its level dipped deeply about once a second and, at each dip, the two bands
turned their phase opposite ways. A lone sine half-way between two bands came out 10 dB quiet at octave up because
the bands' phases were set during the attack, before the filters had settled. Synthetic chords and clean DI takes
showed neither.

**Short jumps on a beating band are not "almost right".** If a band holds two partials and its reader jumps by
anything other than a whole beat, the weaker partial's phase steps at every jump. Many small steps add up to a
different frequency: the weaker partial moves so that the beat rate is preserved instead of scaled. That is exactly
what a phase vocoder does to partials it cannot separate.

**Measure the phase turn over a whole beat.** Measured at the splice itself, the turn is at the mercy of whatever
dominates the band at that instant, and in the quiet moment of a beat that is the leaked rubbish from other bands.

**The attack detector must compare with the recent peak, not the recent average.** A steady low note with a strong
second harmonic has a peaky wave shape; compared with a running average it looks like an attack every cycle.

**Far leak is not harmless.** What a partial leaks into bands far away rotates too fast for the frame rate to
follow, so each of about a hundred far bands turns it into a wrong tone. At an 8 ms filter that haze sat 41 dB
under a lone sine until the filter design was given extra weight far from the band (now -65 dB).

**Up and down want different settings.** Twice the bands and a finer threshold help held chords at octave up and
cost 2 to 5 dB on mixes of real takes at octave down. The engine switches settings with the direction.

## Held chords shifting up: what the roughness was, and what did not fix it

The engine was about 2 dB rougher than the reference device on eleven real held chords at octave up, and 9 dB on one
clean, high chord (`docs/benchmarks.md`). The lead at the time was that the reference device, which trails bends by
45 ms going up, leans on a longer look-back. That was wrong, and the opposite helped. What the measurements said,
all on those eleven chords (dirt between the notes, engine -26.7 dB before, reference device -28.4):

| Tried | Result |
|---|---|
| A longer band filter (10, 12, 16 ms delay) | -27.1, -26.7, -26.8 dB. No. |
| Narrower bands (1024 at 16 ms; the same with twice the bands) | -25.7, -26.8 dB. No. |
| Every decision constant, one at a time: how deep a beat must be, how far it must come back, checks to start and to stop, how often decisions are made, the comparison window, placement on or off, loudness correction off, followers off | All within 0.6 dB of where it was. (Asking for a likeness of 0.98 at the repeat: -23.0.) |
| A reader only for a second partial that stands above what a loud neighbor leaks into the band | No change. |
| No nudging of a reader's position at its jumps | In one clean two-partial band the nudges put sidebands at -31 dB that are at -62 dB without them, but the total did not move. |
| Looking back only 50 ms for a repeat (so two partials less than 20 Hz apart are never given a reader) | -27.4 dB, the clean high chord from -30.6 to -36.7. But synthetic chords lost 2 dB on the scorecard: there, pairs 10 to 20 Hz apart are steady and a reader is exact. |
| The same, but a slow beat counts once it has held for half its length | Keeps both: what is in the engine now. |
| A plain band's phase advance smoothed, loudness-weighted | Cleaner, but single notes came out up to 2.7 cents off at octave down: a weighted average of a beating band's phase advance leans toward the weaker partial by its share of the power. A plain average does not (the long-run average advance of a beating band is exactly its stronger partial's). |
| Smoothing over 10 ms or 20 ms instead of 5 | 0.3 dB cleaner at 10 ms and no more at 20, and bends trail by 14 ms and 19 ms instead of 12 (unsmoothed: 10); at 20 ms a 6 Hz vibrato wobbles by 10 cents. |
| Smoothing right through an attack | Loop mixes 1 dB rougher at +7. While the click dies away in a band and the partial takes over, the band's phase advance changes; smoothing that change leaves each band behind by an amount that depends on where the partial sits in it, so two bands that share the partial come out of step until one follows the other. Hence the 30 ms rest after an attack. |
| The smoothing as a tracker that follows the phase itself (a phase and a rate, both corrected) | No rest needed, bends trail no more than unsmoothed, vibrato wobble halved. But a slow beat between two nearly equal partials (a fifth's shared harmonics) turns the phase half a turn at each quiet moment, the tracker follows that turn, and those chords come out 1.5 dB rougher than with no smoothing; smoothing the advance instead passes over the turn for good. |
| No readers at all: find each partial where a band is loudest, work out its share in the bands around it from the filter's shape, render each partial once and what is left with the nearest partial's phase | Exact on two steady sines 69 Hz apart, and better than the reference device on some clean chords. On real chords no better than plain bands throughout (-11 dB): two partials 80 Hz apart at unequal loudness do not both make a loudest band, so the weaker one is never found. It would need partials to be found in what is left over after the strong ones are taken out. Not pursued. |

What it came down to:

- **A real note's partial is not a line.** It is a tight cluster (two polarisations of the string, a unison string,
  a chorus) that beats slowly and wanders. Synthetic partials are lines. Everything the engine did well on
  synthetic chords and badly on real ones traces back to that.
- **A slow beat on a real note is not to be trusted.** A cluster looks like two partials 10 to 20 Hz apart for a
  moment. A reader that jumps 50 to 100 ms on it splices things that do not match, and each splice is a little
  flutter. The reference device never tries: it carries partials that close along as one beating note.
- **Advancing a plain band's phase frame by frame scales whatever rides on the partial**: the phase swing of a
  beat's quiet moment, a faint neighbor, noise. Taking the advance from a smoothed phase carries those along
  instead.
- **The two measures disagree, and both are right.** The held-chord measure reads the input's lines from its own
  spectrum and allows 19 Hz around each, so a slow beat carried along at its old rate is clean to it. The scorecard
  knows every partial exactly and calls the same thing dirt. A real chord wants the first, a synthetic chord the
  second. Chords built from single-note recordings, where shifting each note alone and adding them up is an exact
  reference, sided with the real chords.
- **What got worse.** Four or more steady partials inside one band's width (a synthetic test: no real chord is
  that steady) used to come out with -12 to -16 dB of dirt, because the engine would take any slow repeat it could
  find; now it is -6 to -11 dB. Three partials are unchanged.

## Slides and vibrato shifting down: two different faults

On a one-octave slide at octave down the shifted pitch wobbled 11 cents around the right value, and 4 cents on a
vibrato (reference device: 2 and 1.5). The roadmap blamed the readers for assuming that the last beat repeats.
Neither fault was that.

- **The slide: two shares of the fundamental a quarter turn apart.** After an attack every band takes over from
  the direct playback as a reader, all at one place, in step. A band was given until it had fallen 10 ms behind to
  show a beat, which at an octave down is 20 ms, about the time it takes to see a beat three times. The band that
  held the fundamental and the second harmonic missed it by a few milliseconds, went back to plain, came back as a
  reader 8 ms later, and was now 140 degrees out of step with the plain band below it that held the rest of the
  fundamental. Nudging pulled that to 83 degrees and stopped. Two shares of one partial that far apart, each
  trailing the input by a slightly different time, add up to a pitch that wanders whenever the pitch moves.
  Giving a band 40 ms instead: 11.2 to 3.8 cents.
- **The vibrato: a single partial taken for a slow beat.** A partial with vibrato on it comes back to the same
  pitch half a vibrato later, and for a moment the band looks just like it did 85 ms ago. The band that held the
  note's fundamental, alone, was put on a reader with an 85 ms "beat" once in every vibrato cycle and taken off
  again 12 ms later. The rule made for held chords (a slow beat has to hold for half its length) stops that, and
  with the smoothed phase advance as well the wobble is 0.3 cents; either one alone leaves it at 3.5.

| Tried | Result |
|---|---|
| A full nudge at every jump instead of half | The slide at 2.7 cents, but full chords 4 dB rougher (-33.5 dB) and chord notes 1.3 cents off: the nudge is a noisy measurement and wants averaging. |
| Placing a reader when it starts, as shifting up does | The slide at 3.8 cents, but chords built from real notes 4 dB rougher (-31.4 dB) and mixes of two takes 5 dB. Shifting down, a reader is best left where the direct playback put it. |
| Making a reader's splice fit at the splice itself when the pitch is moving, instead of on average over the last 12 ms | Worse (slide 13.7, vibrato 27.8 cents). Fitting on average puts a jumping reader's pitch 6 ms ahead of where it reads, which is what makes a reader trail the input by the same time as a plain band. Leave it. |
| The wait as a distance (until the band is 20 ms behind) | Right at an octave, where a band falls behind half a millisecond per millisecond. At two semitones it falls behind a ninth as fast, waited 180 ms, and by then played soft notes late: the slowest tenth of attacks on DI takes went from 12 to 18 ms. As a time (40 ms) it is 11. |

One thing costs: for a down-shift, scaling a band's phase *shrinks* whatever swing rides on a partial, so carrying
the swing along instead leaves more of it. Two pure sines at octave down went from -54.0 to -52.1 dB. Real material
gained more from the rest than it lost there (loop mixes 1 dB, twelve pairs of DI takes 1 to 2 dB).

A side effect to know about: with the smoothed advance, the shifted pitch follows a bend 4 ms *sooner* at octave
down (8 ms behind instead of 12). The loudness still arrives when it did.

## The response setting: what a longer filter and narrower bands buy, shifting up

Three roadmap items turned out to be one question: "timbre on dense chords", "three partials in one band" and "a
response setting".

The timbre item had no cause of its own. The partials of a full chord that came out 3 to 6 dB off at octave up
were of two kinds. Those that two notes share (a string's second harmonic on the fundamental of the string an
octave above) are a slow beat, which keeps its old rate after the shift; they read 4 to 5 dB off in this engine
and in the reference device alike. The rest was three partials in one band, heard: the third of a chord has a
partial with other notes' partials 30 to 50 Hz either side, all three inside every 86 Hz band there, and a reader
gets two of three right. The third of an open E lost 6 dB of its fundamental, of an A barre chord 9 dB, and the
third of a B minor barre chord 13 dB of its second harmonic. (An earlier note here and in the benchmarks said
"fundamental" for all three. For the B minor chord it was the second harmonic; its fundamental is right.)

What was measured, shifting up, in the reference implementation (dirt on the scorecard's full chords and on the
eleven real held chords, both at octave up; the reference device has -28.9 and -28.4 dB):

| Tried | Result |
|---|---|
| The bands as they were (1024, 86 Hz wide), 8 ms filter | Full chords -28.8 dB, held chords -27.7. A chord's middle note 4.5 dB off (rms of six chords). Attacks 7.4 ms late. This is the Fast response. |
| The same bands behind the 12 ms filter (the one shifting down uses: 41 dB down one band away instead of 30) | Full chords -30.2, held -27.9, the middle note 2.5 dB off, pure intervals 5 to 7 dB cleaner, loop mixes 1.4 to 2 dB. Attacks 11.3 ms late, the reference device's figure. The Balanced response. |
| Bands half as wide (1024, 43 Hz wide), 16 ms filter | Full chords -33.5, held -27.5, the middle note 0.7 dB off (reference device 0.5), mixes of DI takes 2 to 3.6 dB cleaner at +7 and +12. Attacks 15.4 ms late, and a 6 Hz vibrato wobbles 9.5 cents instead of 3.9. The Clean response. |
| Bands half as wide behind a 12 ms filter | Worse than either neighbor: full chords -31.0, held -26.1, and only 24 of 28 pure pairs under -40 dB. A band half as wide needs a filter twice as long to be as tight; with less it leaks. |
| Clean's bands twice over (2048, half a band apart, as Fast does with the wide ones), 16 ms | Held chords -29.6 dB, the only thing tried so far that beats the reference device there; two-note chords -38.2 (Clean: -35.1); full chords -33.0. About twice the work. Not built: it wants the faster transform first (`ROADMAP.md`). |

What it taught:

- **Lateness buys cleanliness in two different ways.** A longer filter on the same bands leaks less (Balanced). A
  longer filter spent on narrower bands separates partials that were sharing (Clean). They help different things:
  Balanced is the cleanest of the three on pure intervals and on loop mixes, Clean on full chords, on the middle
  note and on dense DI mixes.
- **Narrow bands follow moving pitch more slowly.** Vibrato wobble at octave up: 3.9, 5.6 and 9.5 cents for
  Fast, Balanced and Clean. Still far below the reference device's 32.
- **Neither helps mixes at +2.** Twelve pairs of DI takes: -24.3, -24.5 and -24.5 dB, against the reference
  device's -27.5. Whatever it does better at small upward shifts is not a matter of band width or filter leak.
- **Neither helps real held chords** (within half a dB), as the longer and narrower filters tried before the
  smoothed phase advance did not. Twice the narrow bands does.
- **Balanced is not uniformly kinder to the middle note**: at +2 the third of an open A minor chord is 8.7 dB too
  quiet on Balanced, 4.0 on Fast and right on Clean.
- The engine keeps every filter ready (`prepare()`), so the response can change while playing without allocating;
  shifting up, the change restarts the bands as a change of interval does.

Fast stays the default: which of the three should be is a question for ears and hands, not for these tables.

## Mixes at small upward shifts, and the first beat after an attack

Two roadmap items: "the first beat after an attack, shifting up" (a beating band is treated as plain until its beat
has gone by once; the idea was to carry a band's last beat across an attack) and "mixes of several parts at small
upward shifts" (the reference device 3 dB cleaner at +2, cause unknown). Measuring the first found the second.

**Where the dirt sits in time.** New energy in the shifted mix of two DI takes (twelve pairs), in 46 ms frames,
sorted by how long ago the last attack was:

| At +2 | First 70 ms after an attack | 70 to 250 ms | Later |
|---|---|---|---|
| This shifter, before | -25 to -30 dB | -21 to -23 | -17.6 (two thirds of all the new energy) |
| Reference device | -24 to -30 | -28 to -34 | -29.4 |

The time just after an attack was the engine's cleanest and the reference device's dirtiest; +7 and +12 read the
same way. So the first beat is not where real mixes lose. (On chords strummed in time it does cost, for both:
`tests/strummed_chords.py` prints the first 120 ms after a strum beside the rest.)

**What the sustain's dirt was.** All 45 pairings of the ten DI takes at +2, new energy in the mix; before -23.5 dB,
reference device -28.6:

| Tried | Result |
|---|---|
| No readers at all | -26.1 dB by the usual ruler but -23.8 by a sharper one (below): every weaker partial is then off by the shift's share of its spacing. At +7 it is no better by the usual ruler and 2 dB worse by the sharp one, and bass with guitar lines lose 5 dB at +2. |
| The down-shift threshold for calling a band beating (0.004 instead of 0.001) | Half a dB on twelve pairs. Not the lever. |
| A band leaves a reader whose beat has gone after 3 checks instead of 8 | -25.8 dB. After 5 checks: -23.7, nothing. |
| A beat has to hold before a reader takes it from 20 ms on instead of 50 | -25.8 dB. From 30 ms: -24.7. From 10 ms: -25.1. |
| Both | -27.1 dB. What the engine does now below a fifth. |
| Both, with a deep beat (two partials of like strength) left as it was | -23.9 to -24.5 dB: the gain is gone. It was the strong pairs that were costing, not the faint ones. |

Why: at a small shift a reader has little to put right and much time to do harm. What a plain band gets wrong on
the weaker of two partials d Hz apart is (r - 1) d Hz, an eighth of d at +2. And a reader plays its band up to one
beat late until its next jump; it gains only r - 1 ms per ms, so at +2 a reader on a 40 ms beat sits there for a
third of a second. A soft note that starts in that band in the meantime comes out late, and a reader whose beat
has gone keeps playing old material for as long as it is allowed to stay.

**How far up it carries** (45 pairings, gain by the usual ruler and by the sharp one):

| +1 | +2 | +3 | +4 | +5 | +6 |
|---|---|---|---|---|---|
| 3.9 / 1.8 dB | 3.6 / 1.9 | 2.5 / 1.1 | 3.5 / 1.4 | 2.7 / 1.3 | 1.0 / 0.4 |

From +7 up, leaving a reader after 3 checks would still give mixes 0.3 to 1.3 dB (+7, +9 and +12, either ruler),
but real held chords lose 0.4 to 0.6 dB there, and at +7 the scorecard's chords and the chords built from single
notes lose 0.7 to 0.8 dB. Hence "below a fifth". The boundary for a beat that has to hold is set where a reader would put right less than 10 Hz (20 ms up to
+3, 33 ms at +5), which is what the sweeps at +2, +4 and +5 preferred.

**The ruler.** The measure used for mixes allows a partial to sit 11 Hz from its place. At +2 that cannot see what a
plain band does to the weaker of two partials 40 Hz apart, which comes out 5 Hz off. A sharper ruler (frames four
times as long, 2.7 Hz) reads the 45 pairings at +2 as: before -23.3, now -25.2, no readers at all -23.8, reference
device -24.1. The reference device scores 4.5 dB worse under the sharp ruler than under the usual one: where two
partials are too close for it to tell apart it carries them along as one beating note, which is what a plain band
does. Its lead at +2 was largely a matter of the ruler. Both rulers agree that the change is a gain, which is why
it is in.

**What it costs.**

- Low notes a small interval apart beat slowly, strongly and for real, and now wait longer for their reader: a
  minor third on low E and a minor triad on low G, built from single-note recordings, came out 4 to 5 dB rougher at
  +2 (the thirty chords together: 3 dB cleaner), one loop of bass with guitar 2.7 dB.
- In the clean response at +2 two pure pairs 26 Hz apart went from -49 to -39 dB. Their readers still come, 20 ms
  later, and happen to settle less well in step with their neighbors. Clean's mixes gain the most of the three
  responses all the same (45 pairings: -24.6 to -27.9 dB; from leaving sooner alone: -26.2), so it was not exempted.
  But the two-take mix of the benchmarks' first table lost 3 dB on Clean (-29.9 to -26.8) while Fast and Balanced
  gained 2.
- Partials that stand alone in a full chord are 2.0 dB off in loudness on Clean at +2, where they were 1.2 (Fast
  and Balanced improved, to 1.4 and 1.2).
- The scorecard's full chords at +6 lost 0.7 dB (-36.8 to -36.1); at +1 to +5 they gained 0.7 to 1.7.

**The first beat itself** was not changed. On a pure pair with a 37 ms beat (clean response, +2, before this
change) the first band became a reader 66 ms after the tone began (one beat, plus the 24 ms the comparison needs,
plus three checks) and the others up to 60 ms later. Carrying the last beat across an attack on the same pitch could save perhaps a third of that, by an
estimate from how the check works, and would be wrong at every chord change. Not built. What would remove the wait
is taking a band's two partials apart frame by frame once their frequencies are known, instead of waiting for a
repeat: the same direction as "no readers at all" in the held-chords table above.

## Speed: the likeness curves as running sums, and what rounding decides

The first performance item (`ROADMAP.md`, section 2). The likeness curves and the decisions were all worked out on
one sample in every 128, and a 64-sample block took up to half of its time slot. CPU figures here are from an Apple
M4.

**What was built.** A band's curve at one lag is a sum of 17 or 18 products, one for every second frame of the
24 ms being compared. The check 2.9 ms later needs the same sum with two terms more and two fewer. So the sums are
kept running: on a frame that has no check, every lag of every band takes its new terms in and its oldest out, and
at a check only the newest frame's term is added and the sum scaled. That is a quarter of the multiplications, the
same work whatever is being played, and never in the same 64 samples as the decisions. At octave up the heavy
blocks went from 31 % of their slot to 23 %, and the average from 12.2 % to 10.8 % of a core.

**A running sum has to be exact to be kept.** Every step leaves its rounding behind, and what was rounding beside
a loud band is an error beside the same band 100 dB quieter. In single precision one step's rounding is already a
ten-millionth of the sum, and the checks ask about a thousandth. So the sums are in double precision, where a
product of two single-precision numbers is exact, and a band is summed afresh from its frames whenever its level
has fallen 40 dB (50 to 340 times a second on the test takes, a hundredth or two of the work) and once in a while
whatever its level. On five minutes of takes whose level jumps by up to
100 dB the output is the same as with sums made afresh at every check, to -160 dB; without the fresh sums the
stretch after a 100 dB drop differed at -65 dB.

**The old sums were in single precision, and that turned out to decide things.**

| Compared | Result |
|---|---|
| Running sums against sums made afresh at every check, both in double precision (88 renders: two DI takes and two synthetic signals, eleven settings, 44.1 and 48 kHz) | The same to -174 dB or better: a last bit of the output here and there. Keeping the sums running changes nothing. |
| Double precision against the single precision the engine had, the same 88 renders | Not the same. Single tones and pairs of tones: -102 to -125 dB in 16 of 22 renders, -58 to -92 dB in the other 6. DI takes: -22 to -123 dB, in the middle -55 to -71 dB. A dense chord of 32 steady partials: down to -36 dB. |
| The scorecard, 15 rows (six intervals, +5, and the balanced and clean rows) | Three cells moved, by 0.005, 0.010 and 0.015 dB, each across the rounding of its last printed digit (two-note chords at +7 and +12, full chords at +12 Clean). Everything else is the same to three decimals. |
| Held chords, moving pitch, strummed chords (`tests/`) | Every mean and every figure in the benchmarks the same; three single figures moved by 0.1 dB. |
| Real material, kept outside the repository: twelve pairs of DI takes and all 45 pairings, chords from single notes, loop mixes, eleven held chords, a chord's middle note (68 figures over the intervals and responses of the benchmarks) | 63 the same to within 0.03 dB. The other five: loop mixes at +7 and the twelve pairs at +7 on Clean 0.16 dB cleaner, loop mixes at +12 on Balanced 0.11 dB cleaner and on Clean 0.09 dB rougher, the twelve pairs at +7 on Balanced 0.06 dB rougher. Single pairs moved by up to 2 dB either way. |
| The engine against the reference (`tests/test_engine_vs_reference.py`, 40 cases) | Closer by more than 1 dB in 21 cases (by 20 dB or more in 7), farther in 2 (both still at -95 dB), the rest as before. A plucked note: at worst -50 dB before, -95 dB now. The worst case, a full chord, -42 dB before and -45 dB now. |

Why: a band whose curve dips by just about the threshold (0.001 shifting up, 0.004 down) is called beating or not
by the last digits of a sum, and in single precision those digits are rounding. From that check on the two builds
treat the band differently, and both are right. It is what has always separated the engine from the reference,
which sums in double precision; the engine is now on the reference's side of it. Shifting down the two builds
differ far less than shifting up (the listening files: -85 to -160 dB against -32 to -128 dB), because the
threshold there is four times as coarse.

What it means for the work after this: a change that leaves the likeness sums alone can be compared with the build
before it sample by sample, and one that touches them cannot, however small the change.

**Fitting a reader to its neighbors, at a third of the cost.** A fit compares a new reader, tried at up to 23
positions, with what is already sounding at 16 moments of the output. The band's own running stretch, the
neighbors' output and the band's level at each moment do not depend on the position tried and are now worked out
once per fit, not once per position (and once for the two searches that start a reader). A reader's carrier turns
by the same angle from each moment to the next, so one sine and cosine, turned on fifteen times, do where sixteen
were worked out; the turn of phase between two bands comes round every 16 or 32 frames and is read from the table
the followers already use. Fits went from 6 to 7 % of a slot in the heaviest blocks at octave up to 2 %. These are
the same sums in another order: the 88 renders are within -137 dB of the build before (all but two within
-150 dB) and every scorecard row is the same. Reading the curves two lags abreast, which is exactly the same
arithmetic, is in the same commit.

**The band loop, at a third of the cost.** Every output sample is a sum over 105 to 231 bands, and it was the
largest part of the average. Three things were in the way of doing the same sums fast.

- *Deciding inside the loop.* Plain or reader, fading or not, set afresh or not was asked for every band at every
  sample. A plain band is a spinning vector times a loudness that moves in a straight line; a reader is a spinning
  vector times the band's envelope where the reader stands, which has two parts. With a second part that is zero
  for plain bands, every band is the same six multiplications, and the compiler takes eight bands at a time. The
  readers' envelopes are worked out before that pass, and the bands that are fading from an old stretch to a new
  one (a short list, with the fade's shape from a table) after it.
- *A reader fetched its frames at every sample*: eight values of the band's envelope and eight of its energy, from
  arrays far larger than the processor's nearest cache, 31 to 84 readers at a time. It moves on to another frame
  only every 16 samples at octave up (64 at octave down), so it now keeps them and does the same arithmetic on what
  it kept. This alone took a fifth off the loop, and the output is bit-identical with and without it.
- *Two sines and two cosines per band at the start of every frame*, one library call each. They are now worked out
  for all bands in one go by the engine's own routine (bring the angle into a quarter turn, then a polynomial),
  two at a time. It is within two units in the last place of the library's for angles up to 1e13, which a phase
  that has been running for days does not reach. It needs the machine's multiply-and-add in one step to take the
  quarter turns out exactly; where there is none (a plain Intel build) the library's functions are used as before.

Against the build before: 78 of 88 renders bit-identical, the other ten different by a last bit in at most 144
samples, every scorecard row the same. At octave up the band loop went from 4.9 to 1.7 % of a core and the whole
engine from 9.9 to 6.4 %; the heavy block from 17 to 13 % of its slot.

**What could not be done.**

| Tried | Result |
|---|---|
| Keeping the single-precision sums exactly as they were and only doing them earlier | Not possible. The sum's last digits depend on the order the terms were added in, and the newest frame's term went in first: nothing of the sum can be made ready before that frame is there. |
| The exact old sums only where they matter: for a band whose curve never falls no number of the curve is used, so a rough curve would do for those | Nothing to gain. On DI takes at octave up 0.1 of 125 awake bands has a curve that never falls; 83 have one that falls and stays down, 41 one that comes back (62 awake shifting down: 0.1, 41 and 22). A real band is not a steady line (see "Held chords" above), and the roadmap's "do less of the curves", which counted on settled bands, needs another idea of settled. |
| Running sums only for the bands that are awake | Not built: a band that wakes needs its sums at once, which is the old work for that band, and after a loud note stops many bands wake in the same check. The lump would be back in the rare block, which is the one that counts. |

## Things worth knowing about measuring

- **Chord scores need each note's tuning**, read from a partial that no other note shares. A score that allows a
  few Hz of slack hides exactly the detune that makes chords sound wrong.
- **"Energy off the expected lines" cannot see a partial at the wrong loudness.** Check timbre separately.
- **Score held chords on their own, after the attack.** An averaged scorecard said "level with the reference
  device" at octave up while a listener heard a difference; measuring only the sustained part showed it at once.
- **A perfect shifter is linear**, so shifting two takes together must equal shifting them separately and adding.
  That gives a test on real recordings with no reference signal. Building chords from single-note recordings makes
  it exact: shift each note alone, add them up, compare with the shifted chord.
- **Predicting the perfect shifter's spectrum from the input does not work** as a measure: two partials a few Hz
  apart beat at a different rate before and after a correct shift.
- **Test with material you did not make.** The two worst faults were found within an hour of trying sample-library
  notes.
- **Know how much slack the ruler gives.** The mix measure allows a line 11 Hz of slack, the scorecard 6 Hz. At a
  small shift a partial can come out a few Hz from its place and be invisible to both. A second ruler with 2.7 Hz
  of slack took most of the reference device's lead at +2 away and halved a gain of this engine's.
- **Sort the dirt by time since the attack.** It showed in one run that the engine and the reference device have
  their dirt at opposite ends of a note.
- **Twelve pairs are few.** One decision that tips the other way moves a pair by 3 dB. All 45 pairings of the same
  ten takes gave steadier means; single pairs still swing by 5 dB either way between two settings.
