"""The built plug-in (hosted headless, the way a DAW would run it) against the bare engine.

    python tests/test_plugin.py          needs scripts/build.sh tools plugin
Checks: shifted-only output equals the engine's; Mix 0 passes the input through untouched; Mix and Tone do what the
processor does offline. Exit status 1 on a mismatch.
"""
import sys
import numpy as np
from signals import SR, NOTE, N, sine, pluck, strum
import engines


def db(a): return 20 * np.log10(np.sqrt(np.mean(np.square(a))) + 1e-12)


def main():
    z = np.zeros(SR // 4); ok = True
    for st in (-12, 12):
        for name, s in (("sine 440", sine(440.0, 1.0, -12)[0]), ("plucked A2", pluck(NOTE["A2"], 1.2)[0]), ("E open chord", strum(N("E2", "B2", "E3", "G#3", "B3", "E4"), 2.0, 0.5)[0])):
            x = np.concatenate([z, s, z]).astype(np.float32).astype(np.float64); yp = engines.plugin(x, st); ye = engines.engine(x, st)
            d = db(yp - ye) - db(ye); good = d < -100.0; ok = ok and good
            print(f"{st:+3d} {name:14s}: plug-in minus engine {d:7.1f} dB   {'ok' if good else 'MISMATCH'}", flush=True)
        x = np.concatenate([z, pluck(NOTE["A2"], 1.2)[0], z]).astype(np.float32).astype(np.float64)
        yp = engines.plugin(x, st, mix=50.0, tone=100.0); ye = engines.engine(x, st, mix=50.0, tone=100.0); d = db(yp - ye) - db(ye); good = d < -100.0; ok = ok and good
        print(f"{st:+3d} Mix 50, Tone 100 : plug-in minus offline processor {d:7.1f} dB   {'ok' if good else 'MISMATCH'}")
    x = np.random.default_rng(1).standard_normal(SR) * 0.05; y = engines.plugin(x, -12, mix=0.0); d = float(np.abs(y - x.astype(np.float32)).max()); good = d < 1e-6; ok = ok and good
    print(f"Mix 0: largest difference from the input {d:.2e}   {'ok' if good else 'MISMATCH'}")
    return ok


if __name__ == "__main__":
    sys.exit(0 if main() else 1)
