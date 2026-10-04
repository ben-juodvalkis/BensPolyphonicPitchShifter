# How PolyPitch works

PolyPitch shifts pitch in real time, by up to an octave either way, without changing how fast the music goes by.
It is built for an instrument played live through it: chords have to stay in tune and clean, and a picked note has
to come out when it was picked.

This page explains the method in plain terms. The code is in two places that do the same thing:
`reference/polypitch_ref.py` (Python, whole signal at once, easy to experiment with) and
`engine/PolyPitchEngine.h` (C++, one sample at a time, what the plug-in and the Max object run).

## The problem

To raise a note by an octave you can play its recording twice as fast, but then it is over in half the time. A
real-time shifter has to keep pace with the player, so it must repeat material when shifting up and skip material
when shifting down, and do it without the joins being heard.

For one note that is manageable: jump by exactly one cycle of the note and the join is invisible. For a chord it is
not, because the notes do not share a cycle. A shifter that makes one cut for the whole signal leaves each note of
a chord a few degrees out of step at every join, always in the same direction, which is a steady detune: thirds
come out a few cents apart and the chord sounds rough.

So PolyPitch does not treat the signal as one thing.

## Step 1: split the sound into narrow bands

The input goes through a bank of band-pass filters, one band every 86 Hz at 44.1 kHz (512 bands across the
spectrum; the ones above 10 kHz are not used). One FFT every 32 samples gives, for every band, a slowly changing
number: how loud the band is and where its phase stands. Those per-band tracks are all the engine works with.

Two properties of the filters matter:

- **The bands add back up to the input**, delayed by a fixed time (8 ms when shifting up, 12 ms when shifting
  down). If every band is played back from the same place, the result is the input itself.
- **A band lets through very little from outside itself.** What a note leaks into other bands is what the engine
  later cannot treat correctly, so the filters are designed by least squares for the least leak at the chosen
  delay (`reference/filterdesign.py`). They are stored as tables because designing one takes seconds.

A partial (one sine component of a note) nearly always lands in two neighboring bands, most of it in one and a
share in the next.

## Step 2: each band is handled in one of two ways

**A plain band holds one partial.** Nothing is cut or repeated. The band's loudness is passed on as it happens,
and its phase is advanced at the shifted rate: for an octave up the output phase turns twice as fast as the
input's. The partial comes out exactly in tune, and the band is only as late as the filter itself.

**A beating band holds two partials**, for example the third harmonic of one string and the second of another, or
two low notes. Scaling the phase would get the stronger one right and move the weaker one to a wrong pitch. But two
partials in one band beat, and the beat repeats. So a beating band is played by a **reader** that moves through the
band's recent past at the shifted speed and, when it has drifted far enough, jumps by exactly one beat, with the
band's phase carried across the jump. After such a jump both partials continue as if nothing had happened.

How the engine tells the two apart: every 2.9 ms it compares each band's last 24 ms with the same band earlier, at
every distance from 2 ms to 100 ms back, allowing for a common turn of phase and a common change of level. For a
plain band that likeness stays near 1 at every distance. For a beating band it falls and comes back; the first
distance at which it is back is the beat. A band becomes a reader band when that pattern has held for three checks
in a row, and goes back to plain when it has been gone for a while.

## Step 3: keep the two shares of a partial together

Because a partial sits in two bands, its two shares must come out in step or they partly cancel and the partial
gets quieter. Two rules look after that:

- **Followers.** When two neighboring plain bands have held a steady phase between them for 12 ms, they are
  carrying the same partial. The weaker band then takes its phase advance from the stronger one.
- **Placement.** A reader can sit anywhere on a grid one beat apart and still get its own two partials right, but
  only one place per beat lines its partials up with the shares in the neighboring bands. When a reader starts, and
  a little at every jump, it is moved to where its output agrees best with its neighbors'.

A reader also plays material from up to a beat ago, which on a dying note is slightly louder than the note is now.
Its loudness is scaled to the band's present level so that jumps leave no step.

## Step 4: attacks

A picked note starts with a click that is spread across all bands. The engine watches the input level against its
recent peak and treats a doubling within a millisecond as an attack.

- **Shifting down**, the attack is played straight from the input at the shifted speed, starting 1 ms behind. That
  direct playback falls further behind as it goes. At the moment it is exactly as far behind as the bands are, the
  bands are the same signal, and they take over. This is why attacks come out about 2 ms late when shifting down,
  whatever the filter delay.
- **Shifting up** there is no such trick, because the playback would have to read ahead of the input. Instead,
  just before the attack reaches the bands, every band's phase is set equal to the input's, so the bands add up to
  the attack itself; from there the phases run at the shifted rate. The attack is as late as the filter: about
  7.5 ms.

## Step 5: shifting up runs twice the bands

Going up there is no direct playback to lean on, so held chords depend entirely on the bands. Where three partials
crowd into one band's width, a reader can only get two of them right. Shifting up therefore runs 1024 bands, half a
band apart, each as wide as before. The filter is the same, so nothing gets later, and each of the three partials
now has a band in which only it and one neighbor matter.

## Around the engine

`engine/PolyPitchProcessor.h` adds what makes it an effect:

- the inputs are summed to mono for the shifter, and the shifted sound goes to every output;
- **Tone**: a pair of high shelves on the shifted sound, scaled by the interval (an octave down gets brighter above
  1 kHz and loses the very top; an octave up gets a little darker);
- **Mix**: both signals at full level at 50 %, and the quieter side falling away on a dB curve toward the ends;
- the dry signal is never delayed, and no latency is reported to the host.

## What limits it

- **Three or more partials in one band** have no common beat within reach. A reader gets two of them right; the
  rest come out smeared. This is the main source of leftover roughness on dense, low chords.
- **The first beat after an attack, shifting up.** A beating band cannot be recognized until its beat has gone by
  once, so for that long (10 to 100 ms, depending on how close the two partials are) the weaker partial is treated
  as in a plain band.
- **Pitch that moves.** A reader assumes the last beat will repeat. On slides and wide vibrato that is only
  roughly true, and the output wobbles a few cents around the right pitch.
- **Below about 60 Hz** the lowest band is not used, so bass-guitar fundamentals below that are lost.

`docs/benchmarks.md` puts numbers on each of these; `ROADMAP.md` lists what is planned about them.

## The numbers behind the words

| | Shifting down | Shifting up |
|---|---|---|
| Bands (44.1 kHz) | 512, 86 Hz apart | 1024, 43 Hz apart, each 86 Hz wide |
| Filter delay / length | 12 ms / 72 ms | 8 ms / 48 ms |
| Frame (how often a band's value is taken) | 32 samples | 32 samples |
| How often decisions are made | every 128 samples (2.9 ms) | same |
| Window for the likeness check | 24 ms | 24 ms |
| How far back a repeat is looked for | 2.2 to 100 ms | same |
| A band counts as beating when the likeness dips by | 0.004 | 0.001 |
| Checks in a row to become a reader / to stop | 3 / 3 | 3 / 8 |
| Crossfade at a reader's jump | 2.7 ms | 1.3 ms |
| Attack | direct playback, hands over after about 27 ms at an octave | phase set 2 ms before the attack reaches the bands |
