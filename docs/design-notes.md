# Design notes: what was tried, and what it taught

PolyPitch came out of a practical problem: a live guitar rig used a commercial polyphonic "capo" effect for an
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
