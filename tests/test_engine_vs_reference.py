"""The C++ engine against the Python reference on the same input.

The engine is a line-for-line port, so on simple material the two agree to rounding (below -90 dB). On dense material
a decision can tip the other way in one of them (single against double precision in the repeat-finding sums), after
which the two outputs are both valid but no longer the same; then only the level has to agree.

    python tests/test_engine_vs_reference.py            44.1 and 48 kHz, -12 and +12
Exit status 1 on a mismatch.
"""
import sys
import numpy as np
from scipy.signal import resample_poly
from signals import SR, NOTE, N, sine, pluck, strum
import engines


def db(a): return 20 * np.log10(np.sqrt(np.mean(np.square(a))) + 1e-12)


def main():
    z = np.zeros(SR // 4); ok = True
    items = [("sine 440", sine(440.0, 1.2, -12)[0], -90.0), ("two sines 440 + 622", sine(440.0, 1.2, -18)[0] + sine(622.25, 1.2, -18)[0], -90.0),
             ("plucked A2", pluck(NOTE["A2"], 1.4)[0], -40.0), ("plucked E open chord", strum(N("E2", "B2", "E3", "G#3", "B3", "E4"), 2.2, 0.5)[0], None)]
    for sr in (44100, 48000):
        for st in (-12, 12):
            for name, s, limit in items:
                x = np.concatenate([z, s, z]); x = resample_poly(x, 160, 147) if sr == 48000 else x
                x = x.astype(np.float32).astype(np.float64); yc = engines.engine(x, st, sr=sr); yp = engines.reference(x, st, sr=sr)
                diff = db(yc - yp) - db(yp); lev = db(yc) - db(yp); good = abs(lev) < 0.2 and (limit is None or diff < limit) and np.isfinite(yc).all()
                print(f"{sr} Hz {st:+3d} {name:22s}: difference {diff:7.1f} dB, level {lev:+.2f} dB   {'ok' if good else 'MISMATCH'}", flush=True)
                ok = ok and good
    return ok


if __name__ == "__main__":
    sys.exit(0 if main() else 1)
