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
