"""Pitch that moves: a one-octave slide, a whole-tone bend and a 6 Hz vibrato (docs/benchmarks.md, "Pitch that moves").

    python tests/moving_pitch.py                     the C++ engine at -12 and +12
    python tests/moving_pitch.py reference -12       another target (engine | reference | plugin), other intervals

Each is a steady harmonic tone (40 partials) whose pitch follows a known track, so the right shifted pitch is known
at every moment:
  slide     110 Hz up to 220 Hz in a second, held, back down in half a second
  bend      G3 up a whole tone in 0.3 s, held, back
  vibrato   E4, 50 cents either way, six times a second
Printed for each: how far the shifted pitch trails the input (ms), and how much it wobbles around the right value
once that delay is allowed for (cents, rms; metrics.pitch_track). Nothing is gated here.
"""
import sys
import numpy as np
from signals import SR, NOTE, fade, dbamp
from metrics import pitch_track
import engines


def tone(f_track, level_db=-12.0, nh=40):
    f = np.asarray(f_track, float); ph = 2 * np.pi * np.cumsum(f) / SR; x = np.zeros(len(f))
    for k in range(1, nh + 1):
        if k * f.max() > 0.45 * SR: break
        x += np.sin(k * ph) / k
    return fade(x * dbamp(level_db) / np.abs(x).max())


def tracks():
    t = np.arange(int(3.0 * SR)) / SR; slide = 110.0 * 2 ** np.clip((t - 0.5) / 1.0, 0, 1) * 2 ** (-np.clip((t - 2.0) / 0.5, 0, 1))
    t = np.arange(int(2.4 * SR)) / SR; bend = NOTE["G3"] * 2 ** (200 * (np.clip((t - 0.5) / 0.3, 0, 1) - np.clip((t - 1.5) / 0.3, 0, 1)) / 1200)
    vib = NOTE["E4"] * 2 ** (50 * np.sin(2 * np.pi * 6.0 * t) * np.clip((t - 0.3) / 0.3, 0, 1) / 1200)
    return (("slide", slide), ("bend", bend), ("vibrato", vib))


def run(target="engine", sts=(-12, 12)):
    fn = engines.BY_NAME[target]; T = tracks(); z = np.zeros(SR // 2); parts = [z]; segs = []; pos = len(z)
    for nm, tr in T:
        s = tone(tr); segs.append((nm, pos, tr)); parts += [s, z]; pos += len(s) + len(z)
    x = np.concatenate(parts)
    print(f"{target:>10} | " + " | ".join(f"{nm + ': trails, wobble':>24}" for nm, _ in T))
    for st in sts:
        y = fn(x, st); r = 2 ** (st / 12); res = [pitch_track(y[a:a + len(tr)], tr, r) for _, a, tr in segs]
        print(f"{st:+10d} | " + " | ".join(f"{lag:13.1f} ms, {wob:4.1f} c" for lag, wob in res))


if __name__ == "__main__":
    a = sys.argv[1:]; target = a[0] if a and a[0] in engines.BY_NAME else "engine"; sts = tuple(int(v) for v in a if v not in engines.BY_NAME) or (-12, 12)
    run(target, sts)
