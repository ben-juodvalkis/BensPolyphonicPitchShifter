"""Full chords strummed in time: is the shifter linear where the strings meet? (docs/benchmarks.md, "Strummed chords")

    python tests/strummed_chords.py                     the C++ engine at -12, +2, +7 and +12
    python tests/strummed_chords.py reference 2 5       another target (engine | reference | plugin), other intervals
    python tests/strummed_chords.py engine clean 12     with the response set to balanced or clean

A perfect shifter is linear: shifting a chord must give what shifting its strings one at a time and adding them up
gives. Twelve full chords (open E, A barre, B minor barre) are strummed 0.6 s apart, and again 0.3 s apart, every
string struck anew at every strum. Each string's part is shifted alone, then the whole pattern. Printed is the new
energy that appears only in the shifted pattern (dB re the strings shifted alone; lower is cleaner), three ways:

  all     over the whole pattern in 186 ms frames (metrics.superposition, the ruler tests/mix_test.py and the
          real-recording tables use): fine in frequency, so a partial that comes out more than 11 Hz off counts
  early   in 46 ms frames whose middle lies within 120 ms after a strum began: fine in time, coarse in frequency
  late    the same in the frames after that, until the next strum

Early against late shows what the start of a chord costs: a band that holds two strings' partials cannot be told
from one that holds a single partial until their beat has gone by once.

Nothing is gated here; the numbers this prints are in docs/benchmarks.md.
"""
import sys
import numpy as np
from signals import SR, N, pluck, fade
from metrics import superposition, dbp
import engines

CHORDS = dict(E=N("E2", "B2", "E3", "G#3", "B3", "E4"), A=N("A2", "E3", "A3", "C#4", "E4", "A4"), Bm=N("B2", "F#3", "B3", "D4", "F#4", "B4"))
ORDER = ("E", "E", "A", "A", "Bm", "Bm", "E", "E", "A", "Bm", "E", "A")
SPREAD = 0.014; LEAD = 0.25


def build(r, period):
    """-> the six strings' parts (each its plucks, one per strum, cut off just before the next) and the strum times"""
    n = int((len(ORDER) * period + 1.0) * SR); parts = [np.zeros(n) for _ in range(6)]; t0 = LEAD + period * np.arange(len(ORDER))
    for j, name in enumerate(ORDER):
        for i, f0 in enumerate(CHORDS[name]):
            s = fade(pluck(f0, period - 0.004, seed=7 * j + i, shift=r)[0], 1.0, 12.0); a = int((t0[j] + i * SPREAD) * SR); parts[i][a:a + len(s)] += s[:n - a]
    return parts, t0


def early_late(ym, parts, t0, within=0.120, n_fft=2048, hop=256, k=2.5):
    """the new energy in 46 ms frames: those whose middle is within `within` s after a strum began, and the rest"""
    from scipy.ndimage import maximum_filter1d
    w = np.blackman(n_fft); ex = [0.0, 0.0]; tot = [0.0, 0.0]; on = (t0 * SR).astype(np.int64); n = len(ym)
    for i in range(0, n - n_fft, hop):
        c = i + n_fft // 2; j = np.searchsorted(on, c, side="right") - 1
        if j < 0: continue
        ref = sum(np.abs(np.fft.rfft(p[i:i + n_fft] * w)) ** 2 for p in parts); M = np.abs(np.fft.rfft(ym[i:i + n_fft] * w)) ** 2; q = 0 if c - on[j] < within * SR else 1
        ex[q] += np.maximum(M - k * maximum_filter1d(ref, 5), 0).sum(); tot[q] += ref.sum()
    return float(dbp(ex[0] / tot[0])), float(dbp(ex[1] / tot[1]))


def run(target="engine", sts=(-12, 2, 7, 12), response=0):
    fn = engines.BY_NAME[target]; gap = np.zeros(SR // 2)
    print(f"{engines.RESPONSES[response] if response else target:>10} | strums 0.6 s apart: all, early, late | strums 0.3 s apart: all, early, late      (new energy, dB)")
    for st in sts:
        r = 2 ** (st / 12); row = []
        for period in (0.6, 0.3):
            parts, t0 = build(r, period); n = len(parts[0]); x = np.concatenate([gap] + [v for p in parts + [sum(parts)] for v in (p, gap)])
            y = fn(x, st, response=response); seg = [y[len(gap) + q * (n + len(gap)):len(gap) + q * (n + len(gap)) + n] for q in range(7)]
            e, l = early_late(seg[6], seg[:6], t0); row.append(f"{superposition(seg[6], *seg[:6])['excess_db']:21.1f}, {e:5.1f}, {l:5.1f}")
        print(f"{st:+10d} | " + " | ".join(row))


if __name__ == "__main__":
    a = sys.argv[1:]; target = a[0] if a and a[0] in engines.BY_NAME else "engine"; resp = max([engines.RESPONSES.index(v) for v in a if v in engines.RESPONSES] + [0])
    run(target, tuple(int(v) for v in a if v not in engines.BY_NAME and v not in engines.RESPONSES) or ((2, 7, 12) if resp else (-12, 2, 7, 12)), resp)
