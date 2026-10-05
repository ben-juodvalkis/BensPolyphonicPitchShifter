"""An input sample that is not a number, or is infinite, counts as silence.

A host or a plug-in before this one can hand over such a sample. Let into the engine it stays in its running sums,
and from there on the output is not a number either, until a reset. Here a plucked chord has such samples in the
middle of it (one that is not a number, one of each infinity, and a block of 64 that are not numbers). The output has
to be finite everywhere and, sample for sample, the output for the same chord with zeros in those places: in the bare
engine and in the whole processor at Mix 50, where the input is also passed on dry.

    python tests/test_bad_samples.py          needs scripts/build.sh tools; takes a few seconds
Exit status 1 on a failure.
"""
import sys
import numpy as np
from signals import SR, N, strum
import engines


def main():
    z = np.zeros(SR // 4); zero = np.concatenate([z, strum(N("E2", "B2", "E3", "G#3", "B3", "E4"), 2.2, 0.5)[0], z]).astype(np.float32); bad = zero.copy(); ok = True
    for at, v, n in ((1.0, np.nan, 1), (1.2, np.inf, 1), (1.4, -np.inf, 1), (1.6, np.nan, 64)):
        i = int(at * SR); bad[i:i + n] = v; zero[i:i + n] = 0.0
    for kw, what in ((dict(), "engine"), (dict(mix=50.0, tone=100.0), "processor, Mix 50")):
        for st, resp in ((-12, 0), (0, 0), (2, 0), (12, 0), (12, 1), (12, 2)):
            y = engines.engine(bad, st, response=resp, **kw); y0 = engines.engine(zero, st, response=resp, **kw)
            lost = int(np.count_nonzero(~np.isfinite(y))); same = np.array_equal(y, y0); good = lost == 0 and same and np.abs(y0).max() > 0.01; ok = ok and good
            print(f"{what:18s} {st:+3d} {engines.RESPONSES[resp] if st > 0 else '':8s}: {lost} of {len(y)} output samples not finite, {'the same as' if same else 'NOT the same as'} with zeros there   {'ok' if good else 'FAILED'}", flush=True)
    return ok


if __name__ == "__main__":
    sys.exit(0 if main() else 1)
