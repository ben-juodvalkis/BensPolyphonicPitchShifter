"""A test for your own recordings, with no reference needed: a perfect shifter is linear, so shifting two takes
together must give the same sound as shifting them separately and adding the results. What shows up only in the
shifted mix is dirt the shifter made where the notes met.

    python tests/mix_test.py a.wav b.wav [engine|reference|plugin] [semitones ...]

Prints the new energy in the mix (dB re the two takes shifted alone; lower is better) and the energy the mix lost.
Mono or stereo files at any sample rate (they are summed to mono and brought to 44.1 kHz); the first 12 s are used.
"""
import sys
import numpy as np, soundfile as sf
from scipy.signal import resample_poly
from signals import SR
from metrics import superposition
import engines


def load(p, seconds=12.0):
    x, sr = sf.read(p, dtype="float64", always_2d=True); m = x.mean(1)[:int(seconds * sr)]
    return resample_poly(m, SR, sr) if sr != SR else m


if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) < 2: raise SystemExit(__doc__)
    A, B = load(a[0]), load(a[1]); n = min(len(A), len(B)); A, B = A[:n], B[:n]; rest = a[2:]
    target = rest[0] if rest and rest[0] in engines.BY_NAME else "engine"; sts = [int(v) for v in rest if v not in engines.BY_NAME] or [-12, 12]
    z = np.zeros(SR // 2); fn = engines.BY_NAME[target]
    for st in sts:
        ya, yb, ym = (fn(np.concatenate([z, s, z]), st)[len(z):len(z) + n] for s in (A, B, A + B)); r = superposition(ym, ya, yb)
        print(f"{target} {st:+d}: new energy in the shifted mix {r['excess_db']:.1f} dB, energy lost {r['missing_db']:.1f} dB")
