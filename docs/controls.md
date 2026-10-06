# The controls

Five controls, the same in the plug-in, in the Max object (`polypitch~`) and in the Max for Live device. This page
says what each one does to the sound. `docs/how-it-works.md` says why. `docs/benchmarks.md` has the measured numbers
quoted here; the Mix and Tone curves come from the code (`engine/PolyPitchProcessor.h`).

| Control | Range | Default | What it does |
|---|---|---|---|
| **Semitones** | -12 to +12 | -12 | The interval, in whole semitones. |
| **Mix** | 0 to 100 | 100 | Dry against shifted: 0 is the dry sound only, 50 both at full level, 100 the shifted sound only. |
| **Tone** | 0 to 100 | 100 | How much of a fixed tone curve, chosen by the interval, is applied to the shifted sound. |
| **Response** | Fast, Balanced, Clean | Fast | Shifting up only: how late an attack may come out, in return for a cleaner sound. |
| **Quality** | Full, Lite, Eco | Full | How much work the engine does, in return for CPU. |

Two things are fixed: the shifted sound is mono (the two inputs summed) and goes to every output, while the dry
sound passes in stereo; and the dry sound is never delayed, and no latency is reported to the host. The shifted
sound is late by the time the engine needs: about 2 ms shifting down, and about 8, 12 or 16 ms shifting up
depending on Response.

## Semitones

The interval, from an octave down to an octave up in whole semitones. At 0 the shifted path is the input itself
(summed to mono), with no delay.

Shifting down and shifting up are done differently inside (`docs/how-it-works.md`, step 4), which is why a picked
note comes out about 2 ms late going down and about 8 ms late going up.

Changing it while playing restarts the bands, and that can click: the largest step in the sound at the moment of
the change measured about 4 times the largest in steady playing going up, and 30 times going down. Change it
between phrases, or automate it onto a gap. A click-free change is on the roadmap.

## Mix

At 50 both signals are at full level. From there down, the shifted sound falls away on a curve while the dry stays
at full level: 12 dB down at 25, 26 dB at 15, 48 dB at 10, off at 0. From 50 up, the dry sound falls away the same
way: 12 dB down at 75, 26 at 85, 48 at 90, off at 100.

The change is smoothed over 10 ms, so sweeping or jumping Mix does not click.

Because the shifted sound is a few milliseconds late and the dry sound is not, a Mix in the middle doubles each
attack slightly, more so shifting up. Delaying the dry sound to line the two up is on the roadmap.

## Tone

A pair of shelves on the shifted sound only, whose shape is fixed by the interval. Tone sets how much of it is
applied: at 100 (the default) all of it, at 50 half the decibels, at 0 none (the shifted sound comes out flat).

- **Shifting down**, a boost above about 1 kHz and a cut above about 7 kHz, both growing with the interval.
  Shifting a guitar down an octave takes its brightness down with it, so the top is put back; and the hiss and
  fizz that the shift drags down into the 7 kHz region is taken out.
- **Shifting up**, a cut above about 1 kHz, growing with the interval. Shifting up pushes everything into the
  harsh region, and this takes the edge off.
- At no shift, nothing.

| Interval | Above about 1 kHz | Above about 7 kHz |
|---|---|---|
| -12 | +11.2 dB | -14.9 dB |
| -7 | +7.4 | -9.9 |
| -5 | +5.6 | -7.5 |
| -2 | +2.4 | -3.2 |
| +2 | -0.6 | |
| +5 | -1.8 | |
| +7 | -2.6 | |
| +12 | -5.3 | |

Sweeping Tone does not click. Jumping it in one step can, at large shifts down: the shelves change at once, and
at octave down the largest step in the sound measured 3 to 10 times the largest in steady playing (at -7 two to
three times; at -5 and shifting up nothing showed). Smoothing it is on the roadmap.

## Response (shifting up only)

How clean the bands can be depends on how long the band filter is allowed to be, and the length of that filter is
how late an attack comes out. Shifting down that cost is hidden (`docs/how-it-works.md`, step 4); shifting up it is
not, so the player chooses. Shifting down, Response changes nothing.

| | Attack late by | What it is for | What it costs |
|---|---|---|---|
| **Fast** | about 8 ms | The earliest attacks. The sound as it was first tuned. | The middle note of a full chord comes out 6 to 9 dB too quiet at octave up. |
| **Balanced** | about 12 ms | The all-rounder: pure intervals 5 to 7 dB cleaner than Fast, chords up to 4 dB, mixes of parts up to 2 dB. As late as the reference device of the benchmarks. | 4 ms. A vibrato wobbles a little more (5.6 cents against 3.9 at octave up). |
| **Clean** | about 16 ms | Full chords and dense playing: the middle note of a chord at its right loudness, mixes of parts 2 to 3.6 dB cleaner than Fast at a fifth and an octave up. | 8 ms. Pitch that moves is followed less closely (a vibrato wobbles 7 to 10 cents, bends trail by 16 to 20 ms); pure intervals at small shifts are 1 to 3 dB less clean than on Fast; the first tenth of a second after a strum is a little rougher. |

On Full all three cost the same CPU; on Lite and Eco, Clean costs more at octave up (3.5 % against 2.6 % on Lite,
1.9 % against 1.5 % on Eco), because Lite's saving on the doubled bands does not apply to it. Changing Response while shifting up restarts the bands, as changing Semitones does,
and can click; while shifting down it does nothing until the interval goes up.

Which should be the default is a listening decision that has not been made; Fast is the default because it was
the engine's sound before the choice existed.

## Quality

What the engine leaves out to save CPU. Full is everything. The numbers are at 48 kHz on one core of an Apple M4,
one DI chord take, average load and the 99.9th-percentile 64-sample block as a share of its time slot
(`docs/benchmarks.md`, "Cost"; an Apple M1 Max took about 1.5 times as long on an older engine).

| | CPU at octave down | CPU at octave up | What is left out | What it costs in sound |
|---|---|---|---|---|
| **Full** | 2.2 %, block 5.8 % | 4.5 %, block 8.9 % | Nothing. | |
| **Lite** | 1.8 %, block 5.4 % | 2.6 %, block 4.7 % | Two notes beating in one band are looked for only below 2.5 kHz of the input (Full: 5 kHz), and shifting up on Fast and Balanced the doubled bands run only below 1.25 kHz. | Recordings of guitars came out within 0.7 dB of Full. Clean, steady synthetic chords are 2 to 6 dB less clean in their upper harmonics. Attacks, tuning, single notes and pitch that moves are the same. |
| **Eco** | 1.2 %, block 4.7 % | 1.5 %, block 3.5 % | Lite, and the bands are looked at every 64 samples instead of every 32, and shifting up nothing above 10.5 kHz is put out (the reference device's shifted sound ends there too). | Up to 1.4 dB rougher than Lite on guitar recordings shifting down and about a decibel shifting up; a slide at octave down trails by about a millisecond more; at octave up the top octave of the sound is gone. |

Eco takes less CPU than the reference device of the benchmarks at any interval (on Clean at octave up, about the
same). Shifting up, Eco with Response on Balanced is as late as that device and within about a decibel of it on
recordings of guitars; on clean synthetic held chords at octave up the device is 6 dB ahead.

Changing Quality while playing restarts the bands and can click.

## Where the controls are

- **Plug-in** (AU and VST3): Semitones (whole numbers), Mix and Tone (0 to 100), Response and Quality (a choice).
  The host's generic sliders show them; the plug-in has no interface of its own yet.
- **Max object**: `@semitones` (also the first argument: `[polypitch~ 12]`), `@mix`, `@tone`, `@response`
  (0 fast, 1 balanced, 2 clean), `@quality` (0 full, 1 lite, 2 eco). The message `clear` makes it forget everything
  heard so far. `docs/max.md` has the inlets and outlets.
- **Max for Live device**: three dials (Semitones, Mix, Tone), a row of buttons for Response and a row for Quality.
