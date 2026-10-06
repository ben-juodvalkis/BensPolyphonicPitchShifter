"""Band-filter design for PolyPitch.

The engine splits the input into K bands (one every sr/K Hz). The bands must add back up to the input delayed by
tau samples, so that a group of bands read from one position is the input itself. For a prototype filter h that is
the condition h[tau] = 1/K and h[tau + mK] = 0. Within that, the filter should pass as little as possible beyond its
own band (what a partial leaks into other bands sets the floor of dirt) and do it with a short delay tau.

designed() fits h by least squares to a flat-topped band with a raised-cosine roll-off at delay tau, with the stop
band weighted heavier and the far stop band heavier still, the add-back-up condition imposed exactly.
prototype() is what the reference and the table generator call; results are cached in reference/.cache/.
"""
import os
import numpy as np


def designed(K, tau, tail, beta=1.0, w_stop=300.0, stop_from=None, ngrid=6000, w_far=None, far_from=6.0):
    """Least-squares fit to a target band shape with delay tau: flat top, raised-cosine roll-off of width beta
    (beta = 1: from the center to one full spacing away), nothing beyond; stop band weighted w_stop times heavier.
    The add-back-up condition is imposed exactly, and the pass band stays flat and in phase.
    w_far: a heavier weight still from `far_from` spacings on. What a partial leaks into the ~100 far bands is too
    fast for the frame rate to follow, so every one of them turns it into a wrong tone: with the plain weight that
    haze sits 41 dB under a lone sine at tau = 8 ms; w_far = 30000 takes 20 dB off it for 2 dB more leak next door."""
    L = tau + tail + 1; n = np.arange(L); D0 = 1.0 / K                 # band spacing in cycles/sample
    edge0 = 0.5 * D0 * (1 - beta); edge1 = 0.5 * D0 * (1 + beta); stop0 = stop_from * D0 if stop_from else edge1
    # frequency grid: dense where it matters
    f = np.unique(np.concatenate([np.linspace(0, 3 * D0, ngrid // 2), np.linspace(3 * D0, 0.5, ngrid // 2)]))
    Dm = np.where(f <= edge0, 1.0, np.where(f >= edge1, 0.0, 0.5 * (1 + np.cos(np.pi * (f - edge0) / max(edge1 - edge0, 1e-12)))))
    W = np.where(f >= stop0, w_stop, 1.0)
    if w_far: W = np.where(f >= far_from * D0, w_far, W)
    W = W * np.gradient(f)                                              # weight x grid density
    E = np.exp(-2j * np.pi * f[:, None] * n[None, :]); T = Dm * np.exp(-2j * np.pi * f * tau)
    sw = np.sqrt(W)[:, None]; Ar = np.vstack([(E.real * sw), (E.imag * sw)]); br = np.concatenate([T.real * sw[:, 0], T.imag * sw[:, 0]])
    idx = [tau + m * K for m in range(-(tau // K), (tail // K) + 1) if 0 <= tau + m * K < L]
    C = np.zeros((len(idx), L)); d = np.zeros(len(idx))
    for i, j in enumerate(idx): C[i, j] = 1.0; d[i] = 1.0 / K if j == tau else 0.0
    # equality-constrained least squares (KKT)
    AtA = Ar.T @ Ar; Atb = Ar.T @ br; M = np.block([[AtA, C.T], [C, np.zeros((len(idx), len(idx)))]]); rhs = np.concatenate([Atb, d])
    return np.linalg.solve(M + 1e-12 * np.eye(len(M)), rhs)[:L]


def hann_sinc(K, tau, tail):
    """The simple fallback (a sinc under a lopsided Hann window): what the engine uses at sample rates it has no
    designed table for. Leaky: -20 dB one band away."""
    c = np.arange(-tau, tail + 1, dtype=np.float64)
    w = np.where(c <= 0, 0.5 + 0.5 * np.cos(np.pi * c / (tau + 1)), 0.5 + 0.5 * np.cos(np.pi * c / (tail + 1)))
    return np.sinc(c / K) * w / K


def prototype(K, tau, tail, beta=1.0, w_stop=300.0, w_far=30000.0):
    """h[n], n = 0 (newest sample) .. tau + tail: the designed filter, cached on disk (a design takes a few seconds)."""
    d = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache"); os.makedirs(d, exist_ok=True)
    p = os.path.join(d, f"proto_K{K}_t{tau}_l{tail}_b{beta}_w{w_stop}_f{w_far}.npy")
    if os.path.exists(p): return np.load(p)
    h = designed(K, tau, tail, beta, w_stop, w_far=w_far); np.save(p, h); return h


def describe(h, K, tau, sr):
    """-> text: the band's gain and phase at a few offsets from its center (dB re the center, degrees re a pure delay)"""
    n = np.arange(len(h)); G = lambda f: np.sum(h * np.exp(-2j * np.pi * f * (n - tau) / sr)) * K; sp = sr / K; g0 = abs(G(0.0))
    return ", ".join(f"{m:g} band{'s' if m != 1 else ''}: {20 * np.log10(abs(G(m * sp)) / g0 + 1e-12):.1f} dB / {np.degrees(np.angle(G(m * sp))):.0f} deg" for m in (0.25, 0.5, 1, 1.5, 2, 4, 8, 16))
