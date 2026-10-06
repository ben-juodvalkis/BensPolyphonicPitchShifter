# How Ben's Polyphonic Pitch Shifter works

Ben's Polyphonic Pitch Shifter shifts pitch in real time, by up to an octave either way, without changing how fast the music goes by.
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

So Ben's Polyphonic Pitch Shifter does not treat the signal as one thing.

## Step 1: split the sound into narrow bands

The input goes through a bank of band-pass filters, one band every 86 Hz at 44.1 kHz (512 bands across the
spectrum; the ones above 10 kHz are not used). One FFT every 32 samples gives, for every band, a slowly changing
number: how loud the band is and where its phase stands. Those per-band tracks are all the engine works with.

Two properties of the filters matter:

- **The bands add back up to the input**, delayed by a fixed time (12 ms when shifting down; 8 ms when shifting
  up, or 12 or 16 with the Response control, step 6). If every band is played back from the same place, the result
  is the input itself.
- **A band lets through very little from outside itself.** What a note leaks into other bands is what the engine
  later cannot treat correctly, so the filters are designed by least squares for the least leak at the chosen
  delay (`reference/filterdesign.py`). They are stored as tables because designing one takes seconds.

A partial (one sine component of a note) nearly always lands in two neighboring bands, most of it in one and a
share in the next.

## Step 2: each band is handled in one of two ways

**A plain band holds one partial.** Nothing is cut or repeated. The band's loudness is passed on as it happens,
and its phase is advanced at the shifted rate: for an octave up the output phase turns twice as fast as the
input's. The partial comes out exactly in tune, and the band is only as late as the filter itself.

The rate is not taken from one frame to the next but smoothed over 5 ms. A steady partial comes out the same.
What changes is everything small and quick that rides on it: a real string's partial is a tight cluster that beats
slowly, with a faint neighbor and some noise beside it, and each of those makes the band's phase swing for a
moment. Advancing the phase at the shifted rate scales those swings too; with the smoothed rate they are carried
along with the partial as they are. Shifting up, that is what keeps a held chord smooth; shifting down, what keeps
a vibrato from wobbling. (For 30 ms after an attack the smoothing rests, so that the bands which share a partial
settle in step.)

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

Only the bands below 5 kHz of the input are checked this way. Above that every band is treated as plain: a real
note's harmonics up there are weak and short-lived, and looking for beats among them changed nothing that could be
measured, at a sixth of the engine's work.

A slow beat (longer than 50 ms: two partials less than 20 Hz apart) has to earn that. Two steady partials that
close repeat exactly, and a reader gets both right. But a real note's partial, being a cluster that wanders, looks
like a slow beat for a moment and then does not, and so does a single partial with vibrato on it (its pitch comes
back to where it was half a vibrato later). A reader that jumps 50 to 100 ms on either adds flutter to what was
smooth. So a slow beat is believed only once it has stayed put for half its own length; until then the band goes by
what it sees within 50 ms, and two partials that close are carried along together as one beating note.

How much a reader is worth depends on the size of the shift. What a plain band gets wrong on the weaker of two
partials is the shift's share of their spacing: an eighth of it at two semitones up, all of it at an octave. And a
reader plays its band up to one beat late until its next jump, which at a small shift is a long way off. So when
shifting up by less than a fifth the engine is choosier: a beat has to hold wherever a reader would put right less
than 10 Hz (every beat longer than 20 ms at +2, longer than 33 ms at +5), and a band leaves a reader as soon as its
beat has been gone for three checks. On mixes of two parts and on strummed chords that is 3 dB cleaner at +2.

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
  whatever the filter delay. Every band takes over as a reader at that one place, where all of them are in step.
  A band that turns out to beat stays there; one that holds a single partial goes back to plain. Each is given
  40 ms to show which it is, because a band that left too early and came back as a reader would no longer be in
  step with its neighbors.
- **Shifting up** there is no such trick, because the playback would have to read ahead of the input. Instead,
  just before the attack reaches the bands, every band's phase is set equal to the input's, so the bands add up to
  the attack itself; from there the phases run at the shifted rate. The attack is as late as the filter: about
  7.5 ms on the Fast response.

## Step 5: shifting up runs twice the bands

Going up there is no direct playback to lean on, so held chords depend entirely on the bands. Where three partials
crowd into one band's width, a reader can only get two of them right. Shifting up therefore runs 1024 bands, half a
band apart, each as wide as before. The filter is the same, so nothing gets later, and each of the three partials
now has a band in which only it and one neighbor matter.

## Step 6: the Response control (shifting up only)

How clean the bands can be depends on how long the filter is allowed to be, and the filter's length is how late an
attack comes out. Shifting down that cost is hidden by the direct playback of step 4. Shifting up it is not, so
the player chooses:

- **Fast** (the default): the 8 ms filter. One band away from its center it is 30 dB down.
- **Balanced**: the same bands behind the 12 ms filter that shifting down uses, 41 dB down one band away. Less of
  every partial reaches the bands around it, so less is treated wrongly there: pure intervals come out 5 to 7 dB
  cleaner, chords up to 4 dB and mixes up to 2 dB. Attacks are 4 ms later.
- **Clean**: bands half as wide (43 Hz), 1024 of them, behind a 16 ms filter. A filter twice as long is what a band
  half as wide needs to be as tight. This is the setting for full chords: a chord's middle note often has a
  partial with other notes' partials 30 to 50 Hz either side of it. In 86 Hz bands all three share every band
  there and the middle one comes out several dB too quiet; in 43 Hz bands each has a band of its own. Attacks are
  8 ms later than Fast, and a narrow band is also slower to follow a pitch that moves: a vibrato wobbles 7 to
  10 cents around the right pitch instead of 2 to 4.

Nothing else differs between the three: the same decisions, the same thresholds. `docs/benchmarks.md` has each
one's numbers. Changing the response while shifting up restarts the bands, as changing the interval does.

## Step 7: the Quality control

Full is everything above. **Lite** leaves out the two things that cost the most for what they give, and takes
about four fifths of the CPU shifting down and under three fifths shifting up:

- **Beating is looked for only below 2.5 kHz of the input**, not 5. Between the two, a band that holds two partials
  is treated as plain: the stronger one comes out right, the weaker one a little off its pitch. On clean, steady
  chords that is 2 to 6 dB more dirt in the upper harmonics. On recordings of guitars it could not be measured.
- **Shifting up, only the bands below 1.25 kHz are doubled** (on Fast and Balanced; Clean has bands of its own and
  keeps them all). The doubling of step 5 is there for partials that crowd into one band's width, and that is a
  matter of low notes: two neighboring harmonics of a low E are 82 Hz apart, closer than a band is wide. Above
  1.25 kHz every second band does as well. Just below 1.25 kHz the extra bands fade out over a few bands, each
  one's weight handed over to its neighbors, so that a partial there keeps its loudness.

Attacks, tuning, single notes and pitch that moves are the same in both.

**Eco** is Lite with two more things left out, for about half of Full's CPU shifting down and a third shifting up,
which is less than the reference device's shifter takes at any interval (on Clean at octave up, about the same):

- **The bands are looked at every 64 samples instead of every 32.** Everything that follows a band in time is then
  half as fine: the likeness check, the smoothing of the phase advance, the moment a reader jumps. On recordings of
  guitars that cost up to 1.4 dB against Lite shifting down and about a decibel shifting up; a slide at octave down
  trails by about a millisecond more.
- **Shifting up, nothing above 10.5 kHz is put out.** The reference device's shifted sound ends there too (at
  octave up its level above 12 kHz is 72 dB below the whole). At octave up that is the top octave of the sound.

Shifting up, Eco with Response on Balanced is as late as the reference device and within about a decibel of it on
recordings of guitars; on clean synthetic held chords at octave up the device is 6 dB ahead. `docs/benchmarks.md` has the numbers ("Lite and Eco"). Changing Quality while playing restarts
the bands, as changing the interval does.

## Around the engine

`engine/PolyPitchProcessor.h` adds what makes it an effect:

- the inputs are summed to mono for the shifter, and the shifted sound goes to every output;
- **Tone**: a pair of high shelves on the shifted sound, scaled by the interval (an octave down gets brighter above
  1 kHz and loses the very top; an octave up gets a little darker);
- **Mix**: both signals at full level at 50 %, and the quieter side falling away on a dB curve toward the ends;
- the dry signal is never delayed, and no latency is reported to the host.

## What limits it

- **Three or more partials in one band** have no common beat within reach. A reader gets two of them right; the
  rest come out smeared. This is the main source of leftover roughness on dense, low chords. (Shifting up, the
  Clean response halves the band width, which gives most of them a band each, 8 ms later.)
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
| Filter delay / length | 12 ms / 72 ms | 8 ms / 48 ms (the Fast response) |
| With Response on Balanced | | the same bands, filter 12 ms / 72 ms |
| With Response on Clean | | 1024 bands, 43 Hz apart, each 43 Hz wide; filter 16 ms / 96 ms |
| Frame (how often a band's value is taken) | 32 samples | 32 samples |
| How often decisions are made | every 128 samples (2.9 ms) | same |
| Window for the likeness check | 24 ms | 24 ms |
| How far back a repeat is looked for | 2.2 to 100 ms | same |
| Bands that are checked for beating | below 5 kHz of the input | same |
| With Quality on Lite | checked below 2.5 kHz | checked below 2.5 kHz; 1024 bands only below 1.25 kHz, 512 above (Fast and Balanced) |
| With Quality on Eco | as Lite, and a frame of 64 samples | as Lite, a frame of 64 samples, and no bands that land above 10.5 kHz |
| A band counts as beating when the likeness dips by | 0.004 | 0.001 |
| Checks in a row to become a reader / to stop | 3 / 3 | 3 / 8 (3 / 3 below a fifth up) |
| A plain band's phase advance is smoothed over | 5 ms, resting for 35 ms after an attack | 5 ms, resting for 30 ms after an attack |
| A beat has to hold for half its own length if it is longer than | 50 ms | 50 ms (below a fifth up: 20 ms at +1 to +3, rising to 41 ms at +6) |
| After an attack, a band waits to see whether it beats for | 40 ms | (no direct playback) |
| Crossfade at a reader's jump | 2.7 ms | 1.3 ms |
| Attack | direct playback, hands over after about 27 ms at an octave | phase set 2 ms before the attack reaches the bands |
