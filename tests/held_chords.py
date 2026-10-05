"""Held chords, after the attack: what an averaged scorecard hides (docs/benchmarks.md, "Held chords").

    python tests/held_chords.py                    the C++ engine at +12
    python tests/held_chords.py reference 7 12     another target (engine | reference | plugin), other intervals
    python tests/held_chords.py engine clean       with the response set to balanced or clean
    python tests/held_chords.py engine lite        with the quality set to lite

Ten sustained chords in the high, sparse register, where every partial should come out as one clean line. Each note
is a plucked-string model. In five of the chords every partial is a close pair (0.3 to 2.5 Hz apart), as a real
string's is (two polarisations, a unison string, a chorus), so each partial beats slowly; in the other five the
partials are pure. Measured from 0.4 s into each chord:

  dirt     the output's energy that is not within reach of any shifted line of the input (metrics.line_dirt)
  flutter  fast loudness flutter, 20 to 150 Hz, per third-octave band (metrics.flutter)

Both in dB, lower is cleaner. Nothing is gated here; the numbers this prints are in docs/benchmarks.md.
"""
import sys
import numpy as np
from signals import SR, N, fade
from metrics import line_dirt, flutter
import engines

CHORDS = ("C4 G4 C5 E5", "G#3 D#4 G#4 C5", "A4 C#5 E5", "E3 B3 E4 G#4", "D4 A4 D5 F#5")
GAP = int(0.4 * SR); DUR = 2.6


def note(f0, paired, seed, ratio, pos=0.18, B=1.2e-4):
    """one plucked string (as signals.pluck, without the pick noise); paired: every partial has a twin a little off"""
    n = int(DUR * SR); t = np.arange(n) / SR; x = np.zeros(n); rng = np.random.default_rng(seed); t60 = float(np.clip(6.0 * (110.0 / f0) ** 0.5, 1.5, 9.0))
    for k in range(1, 80):
        fk = k * f0 * np.sqrt(1 + B * k * k)
        if fk > 0.42 * SR / max(ratio, 1.0): break
        a = np.sin(k * np.pi * pos) / k * np.exp(-t / (t60 / 6.91 / (1 + 0.012 * k * k + 0.0000015 * fk)))
        d = rng.uniform(0.3, 2.5) * rng.choice([-1, 1]); g = rng.uniform(0.3, 0.9); ph = rng.uniform(0, 2 * np.pi)
        x += a * (np.sin(2 * np.pi * fk * t) + (g * np.sin(2 * np.pi * (fk + d) * t + ph) if paired else 0.0))
    return x


def build(st):
    r = 2 ** (st / 12); parts = [np.zeros(SR // 2)]; segs = []; pos = SR // 2
    for paired in (False, True):
        for nm in CHORDS:
            x = np.zeros(int(DUR * SR))
            for i, f0 in enumerate(N(*nm.split())):
                o = int(i * 0.012 * SR); x[o:] += note(f0, paired, 7 + i, r)[:len(x) - o]
            x = fade(x * 0.5 / np.abs(x).max(), 0.0, 20.0); segs.append((("paired " if paired else "pure   ") + nm, pos, len(x))); parts += [x, np.zeros(GAP)]; pos += len(x) + GAP
    return np.concatenate(parts + [np.zeros(SR // 2)]), segs


def run(target="engine", sts=(12,), response=0):
    fn = engines.BY_NAME[target]
    for st in sts:
        x, segs = build(st); y = fn(x, st, response=response); r = 2 ** (st / 12); rows = [(nm, line_dirt(x[a:a + n], y[a:a + n], r), flutter(y[a + int(0.4 * SR):a + n - int(0.15 * SR)])) for nm, a, n in segs]
        print(f"{target} {st:+d}{', ' + engines.RESPONSES[response] + ' response' if response else ''}{', lite' if engines.LITE else ''}: held chords from 0.4 s on, dirt / flutter (dB)")
        for nm, d, fl in rows: print(f"    {nm:24s} {d:6.1f} / {fl:6.1f}")
        print(f"    {'mean':24s} {np.mean([d for _, d, _ in rows]):6.1f} / {np.mean([fl for _, _, fl in rows]):6.1f}")


if __name__ == "__main__":
    a = engines.words(sys.argv[1:]); target = a[0] if a and a[0] in engines.BY_NAME else "engine"; resp = max([engines.RESPONSES.index(v) for v in a if v in engines.RESPONSES] + [0])
    run(target, tuple(int(v) for v in a if v not in engines.BY_NAME and v not in engines.RESPONSES) or (12,), resp)
