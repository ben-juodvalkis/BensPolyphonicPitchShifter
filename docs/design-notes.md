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
| 11 | Shifting up: a plain band's phase advance smoothed over 5 ms; a slow beat has to hold before a reader takes it | Held chords at octave up 1 dB cleaner on average and 4 dB on the one that was worst, chords built from real notes 2 to 4 dB at every up-shift, with a little less flutter than the reference device. Steady synthetic clusters of four or more partials in one band got worse (below). |

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
