"""Synthetic test signals. Every generator returns (signal, ideal): the ideal is the perfectly shifted version (each
partial at `shift` times its frequency, same envelope, same timing), which is what the measures compare against.
44.1 kHz throughout."""
import numpy as np

SR = 44100
NOTE = {n: 440.0 * 2 ** ((i - 57) / 12) for i, n in enumerate(f"{p}{o}" for o in range(0, 8) for p in ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"))}


def N(*names): return [NOTE[n] for n in names]


def dbamp(x): return 10 ** (x / 20)

def fade(y, ms_in=1.0, ms_out=5.0):
    y = y.copy(); a = int(ms_in * SR / 1000); b = int(ms_out * SR / 1000)
    if a: y[:a] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(a) / a)
    if b: y[-b:] *= 0.5 + 0.5 * np.cos(np.pi * np.arange(b) / b)
    return y


def sine(f, dur, level_db=-12.0, shift=0.5, ms_in=1.0):
    t = np.arange(int(dur * SR)) / SR; a = dbamp(level_db)
    return fade(a * np.sin(2 * np.pi * f * t), ms_in), fade(a * np.sin(2 * np.pi * f * shift * t), ms_in)


def pluck(f0, dur, level_db=-9.0, pos=0.18, B=1.2e-4, t60=None, seed=0, shift=0.5, pick_db=-14.0):
    """Guitar-like plucked string by additive synthesis: partials sin(k*pi*pos)/k (pickup-velocity spectrum of a
    string plucked at `pos`), slight stiffness inharmonicity, higher partials decay faster, plus a 3 ms pick noise.
    All partials start at t=0 in sine phase -> the onset time is exact and so is every partial frequency."""
    n = int(dur * SR); t = np.arange(n) / SR; x = np.zeros(n); y = np.zeros(n)
    if t60 is None: t60 = float(np.clip(6.0 * (110.0 / f0) ** 0.5, 1.5, 9.0))
    for k in range(1, 80):
        fk = k * f0 * np.sqrt(1 + B * k * k)
        if fk > 0.42 * SR: break
        a = np.sin(k * np.pi * pos) / k
        tau = t60 / 6.91 / (1 + 0.012 * k * k + 0.0000015 * fk * 1.0)
        env = np.exp(-t / tau)
        x += a * env * np.sin(2 * np.pi * fk * t); y += a * env * np.sin(2 * np.pi * fk * shift * t)
    g = dbamp(level_db) / np.abs(x).max(); x *= g; y *= g
    rng = np.random.default_rng(seed); m = int(0.003 * SR)
    pk = rng.standard_normal(m) * np.hanning(2 * m)[m:] * dbamp(pick_db) * dbamp(level_db) * 3.0
    x[:m] += pk                      # pick noise is broadband; its ideal shifted version is the same burst
    y[:m] += pk
    return fade(x, 0.0, 20.0), fade(y, 0.0, 20.0)


def strum(freqs, dur, ratio, spread_ms=14.0, level_db=-9.0, seed=40):
    """A plucked chord: one pluck per note, `spread_ms` apart (0 = all at once)."""
    n = int(dur * SR); x = np.zeros(n); y = np.zeros(n)
    for i, f in enumerate(freqs):
        o = int(i * spread_ms * SR / 1000); a, b = pluck(f, dur, level_db=0.0, seed=seed + i, shift=ratio); x[o:] += a[:n - o]; y[o:] += b[:n - o]
    g = dbamp(level_db) / np.abs(x).max(); return x * g, y * g
