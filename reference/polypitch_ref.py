"""PolyPitch reference implementation (Python + numba).

This is the specification the C++ engine (engine/PolyPitchEngine.h) is a line-for-line streaming port of.
tests/test_engine_vs_reference.py holds the two together. Offline, whole-signal: it computes every band's envelope
for the whole input first, then makes the same decisions the engine makes, using only frames the engine would
already have at that moment.

    from polypitch_ref import shift
    y = shift(x, -12)              # x: mono float array at 44.1 kHz; y: the shifted signal, same length

How it works: docs/how-it-works.md. In short, a bank of narrow band-pass filters; then each band is handled as

  a "plain" band (one partial in it)
        no reader and no jumps: the band's loudness is passed on as it happens and its phase is advanced `ratio`
        times as fast as the input's (measured frame by frame; shifting up, that advance is smoothed over a few
        milliseconds, see _smooth). The partial comes out exactly in tune, nothing is repeated or skipped, and the
        band is only as late as the band filter itself.
  or a "beating" band (two partials in it: two notes' harmonics close together, or two low notes)
        its envelope repeats once per beat. A reader plays the band at the shifted speed and jumps by exactly one
        such repeat, with the band's phase carried across the jump. Both partials come out right. (Advancing the
        phase as for a plain band would move the weaker partial to a wrong frequency.) A repeat is looked for up to
        `reach_ms` back; shifting up, one longer than `slow_ms` counts only once it has held for half as long as it lasts.

  attacks, shifting down: the attack is played straight from the input (the bridge); the bands take over as
        readers at the same position and each then settles into one of the two ways.
  attacks, shifting up:   every band's phase is set equal to the input's just before the attack reaches it, so the
        bands add up to the attack itself; from there the phases run at the shifted rate.

Copyright (c) 2026 Ben Juodvalkis. MIT License (see LICENSE).
"""
import numpy as np
from numba import njit, prange
from filterdesign import prototype

SR = 44100


# ------------------------------------------------------------------------------------------------ analysis
def analyze(x, K, h, H, kmax):
    """-> Z[k, m], k = 0..kmax-1, band k centred at k*sr/K (so a guitar's low E, 82 Hz, sits in the middle of band 1
    when K = 512, and no band has to share its pass-band with its own mirror image). Band 0 is what is left below
    the first band (real; halved here because the read formula doubles)."""
    n = len(x); L = len(h); M = n // H + 1; P = int(np.ceil(L / K)) * K; hp = np.zeros(P); hp[:L] = h
    xp = np.concatenate([np.zeros(P), np.asarray(x, np.float64), np.zeros(H)]); Z = np.empty((kmax, M), np.complex64); k = np.arange(kmax); nn = np.arange(P)
    for a in range(0, M, 2048):
        t = np.arange(a, min(a + 2048, M)) * H
        u = xp[P + t[:, None] - nn[None, :]] * hp[None, :]
        A = np.fft.ifft(u.reshape(len(t), P // K, K).sum(1), axis=1)[:, :kmax] * K
        Z[:, a:a + len(t)] = (A * np.exp(-2j * np.pi * k[None, :] * (t[:, None] % K) / K)).T
    Z[0] *= 0.5
    return Z


@njit(cache=True)
def onsets(x, sr, ratio=2.0, floor=1e-4):
    """1 where the 1 ms level jumps to `ratio` x the highest it has been lately (a peak-hold released over 80 ms,
    read 3 ms back). A steady low note's own wave shape reaches the same peak every cycle, so it does not count."""
    n = len(x); o = np.zeros(n, np.uint8); ef = 0.0; pk = 0.0; af = 1.0 - np.exp(-1.0 / (0.001 * sr)); rel = np.exp(-1.0 / (0.080 * sr)); D = int(0.003 * sr); buf = np.zeros(D); j = 0
    for i in range(n):
        v = abs(x[i]); ef += af * (v - ef); old = buf[j]; buf[j] = ef; j += 1
        if j == D: j = 0
        pk = max(pk * rel, old)
        if ef > ratio * pk and ef > floor: o[i] = 1
    return o


@njit(cache=True)
def _holds(ons, M, H, hold, span):
    """1 for the frames within `span` samples after an attack (the engine's rule for which attacks count: none within
    `hold` of the last one)"""
    h = np.zeros(M, np.uint8); wait = 0
    for i in range(len(ons)):
        if ons[i] == 1 and i >= wait:
            wait = i + hold
            for m in range(i // H + 1, min((i + span) // H + 1, M)): h[m] = 1
    return h


@njit(parallel=True, cache=True)
def _smooth(Zc, U, tf, te, hold):
    """US[k, m]: band k's phase with its frame-to-frame advance smoothed (time constant tf frames) and summed up again.
    A plain band's phase advance is taken from this when shifting up. A steady partial's is the same as before, and
    so is its long-run average (so the tuning stays exact). What is left out are the quick swings: a weak second
    partial, noise, the turn of phase in a beat's quiet moment (which is passed over for good). Those are then
    carried along with the partial instead of being scaled with it, which is what made held chords rough.
    - A frame more than 10 dB under the band's recent loudness (te frames) counts for less.
    - When the band gets louder at once (from 3 dB above its recent loudness, fully at 6 dB) the new advance is taken
      as it is. Both of these in a sliding way, so that a last-digit difference cannot tip anything.
    - In the frames marked `hold` (the 30 ms after an attack) nothing is smoothed: while the attack's click dies away
      in a band and the partial takes over, the advance changes, and smoothing that change would leave the bands that
      share the partial a little out of step.
    - Where a frame or the one before it holds nothing at all (digital silence has no phase) the advance is zero.
    Only the advance is ever used, so US starts at 0."""
    kmax, M = Zc.shape; US = np.zeros((kmax, M)); a = 1.0 - np.exp(-1.0 / tf); ae = 1.0 - np.exp(-1.0 / te)
    for k in prange(kmax):
        fs = 0.0; eb = 0.0; ep = 0.0
        for m in range(1, M):
            e = Zc[k, m].real * Zc[k, m].real + Zc[k, m].imag * Zc[k, m].imag
            if e <= 1e-24 or ep <= 1e-24: fs = 0.0
            else:
                g = a
                if e < 0.1 * eb: g = a * e / (0.1 * eb)
                if e > 2.0 * eb: g = a + (1.0 - a) * min((e - 2.0 * eb) / (2.0 * eb), 1.0) if eb > 0.0 else 1.0
                if hold[m] == 1: g = 1.0
                fs += g * (U[k, m] - U[k, m - 1] - fs)
            eb += ae * (e - eb); ep = e; US[k, m] = US[k, m - 1] + fs
    return US


@njit(cache=True)
def _zat(Zk, p, H):
    """band envelope at time p (cubic between frames) -> (re, im)"""
    q = p / H; i = int(np.floor(q)); f = q - i
    if i < 1 or i + 2 >= len(Zk): return 0.0, 0.0
    a = Zk[i - 1]; b = Zk[i]; c = Zk[i + 1]; d = Zk[i + 2]
    zr = b.real + 0.5 * f * (c.real - a.real + f * (2 * a.real - 5 * b.real + 4 * c.real - d.real + f * (3 * (b.real - c.real) + d.real - a.real)))
    zi = b.imag + 0.5 * f * (c.imag - a.imag + f * (2 * a.imag - 5 * b.imag + 4 * c.imag - d.imag + f * (3 * (b.imag - c.imag) + d.imag - a.imag)))
    return zr, zi


@njit(cache=True)
def _direct(x, n, ratio, dA0, br0, br1, drt, xf):
    """the attack bridge: -> (direct signal, its weight). Between br0 and br1 the output is the input itself played at
    the shifted speed from dA0 behind (re-started at every attack in drt); the weight fades in at br0 and out at br1."""
    y = np.zeros(n); w = np.zeros(n); nb = len(br0); nd = len(drt); fin = max(xf // 2, 2); fout = xf
    if nb == 0: return y, w
    ib = 0; idr = 0; p = 0.0; p_old = 0.0; fade = 0; act = False
    for t in range(n):
        while ib < nb and t >= br1[ib] + fout: ib += 1; act = False
        if ib >= nb: break
        if t < br0[ib]: continue
        p += ratio
        if fade > 0: p_old += ratio
        while idr < nd and drt[idr] <= t:
            if drt[idr] >= br0[ib]:
                if act: p_old = p; fade = fin
                p = drt[idr] - dA0 + (t - drt[idr]) * ratio; act = True
            idr += 1
        if t < br0[ib] + fin: w[t] = 0.5 - 0.5 * np.cos(np.pi * (t - br0[ib] + 1) / fin)
        elif t < br1[ib]: w[t] = 1.0
        else: w[t] = 0.5 + 0.5 * np.cos(np.pi * (t - br1[ib] + 1) / fout)
        i = int(np.floor(p)); f = p - i
        v = 0.0
        if i >= 1 and i + 2 < n: v = x[i] + 0.5 * f * (x[i + 1] - x[i - 1] + f * (2 * x[i - 1] - 5 * x[i] + 4 * x[i + 1] - x[i + 2] + f * (3 * (x[i] - x[i + 1]) + x[i + 2] - x[i - 1])))
        if fade > 0:
            i = int(np.floor(p_old)); f = p_old - i; vo = 0.0
            if i >= 1 and i + 2 < n: vo = x[i] + 0.5 * f * (x[i + 1] - x[i - 1] + f * (2 * x[i - 1] - 5 * x[i] + 4 * x[i + 1] - x[i + 2] + f * (3 * (x[i] - x[i + 1]) + x[i + 2] - x[i - 1])))
            g = fade / float(fin); g = 0.5 - 0.5 * np.cos(np.pi * g); v = (1.0 - g) * v + g * vo; fade -= 1
        y[t] = v
    return y, w


# ------------------------------------------------------------------------------------------------ one band's output
@njit(cache=True)
def _wrap(a): return a - 2.0 * np.pi * np.round(a / (2.0 * np.pi))

@njit(cache=True)
def _ulin(Uk, p, H):
    q = p / H; i = int(np.floor(q)); f = q - i
    if i < 0: return Uk[0]
    if i + 1 >= len(Uk): return Uk[len(Uk) - 1]
    return Uk[i] + f * (Uk[i + 1] - Uk[i])

@njit(cache=True)
def _cx(Z, A, U, US, CE, k, t, mode, t0, d0, psi, L, Jn, ratio, H, wk, wl, tau, dpv, flr):
    """one stretch of band k's output at time t (may be fractional, may lie before t0), as a complex (analytic) value.
    mode 1: loudness as it happens; phase = the band's own + (ratio - 1) x the phase of its leader band L (itself
            when it stands alone), the latter from US (the smoothed advance; US is U when nothing is smoothed). A band
            that holds a share of the same partial as its stronger neighbour takes that neighbour as leader, which
            keeps the two shares in step whatever happens (a quiet moment in a beat, the attack itself).
    mode 0: a reader that was d0 behind at t0 and moves at `ratio`. Its loudness is corrected to the band's level
            now (Jn > 0: both levels taken over one beat = Jn frames): a reader plays material from up to a beat ago,
            which on a dying note is louder, and without the correction every jump leaves a small step in loudness."""
    if mode == 1:
        p = t - dpv; q = p / H; i = int(np.floor(q)); f = q - i
        if i < 0 or i + 1 >= A.shape[1]: return 0.0, 0.0
        a = A[k, i] + f * (A[k, i + 1] - A[k, i])
        ph = U[k, i] + f * (U[k, i + 1] - U[k, i]) + wk * (p - tau) + (ratio - 1.0) * (US[L, i] + f * (US[L, i + 1] - US[L, i]) + wl * (p - tau)) + psi
        return a * np.cos(ph), a * np.sin(ph)
    p = t0 - d0 + ratio * (t - t0); zr, zi = _zat(Z[k], p, H); ph = wk * (p - tau) + psi; c = np.cos(ph); sn = np.sin(ph); g = 1.0
    if Jn > 0:
        qp = p / H; ip = int(np.floor(qp)); fp = qp - ip; qn = (t - flr) / H; im = int(np.floor(qn)); fn = qn - im
        if ip - Jn >= 0 and im + 1 < CE.shape[1] and im - Jn >= 0 and ip + 1 < CE.shape[1]:
            ep = CE[k, ip] + fp * (CE[k, ip + 1] - CE[k, ip]) - CE[k, ip - Jn] - fp * (CE[k, ip - Jn + 1] - CE[k, ip - Jn])
            en = CE[k, im] + fn * (CE[k, im + 1] - CE[k, im]) - CE[k, im - Jn] - fn * (CE[k, im - Jn + 1] - CE[k, im - Jn])
            if ep > 1e-24 and en > 0.0:
                g = np.sqrt(en / ep)
                if g > 2.0: g = 2.0
                if g < 0.5: g = 0.5
    return g * (zr * c - zi * sn), g * (zr * sn + zi * c)

@njit(cache=True)
def _align(Z, A, U, US, CE, k, t, mo, to, do, po, lo, jo, mn, dn, ln, jn, ratio, H, wk, dw, tau, dpv, flr, M, step):
    """the turn that lines a new stretch of band k (mode mn, delay dn, leader ln, starting at t) up with the running
    one (mo, to, do, po, lo) over the last M*step samples of output - not just at the splice, where a beat's quiet
    moment can mislead. dw = band spacing in radians per sample."""
    cr = 0.0; ci = 0.0
    for j in range(M):
        tt = t - j * step
        ar, ai = _cx(Z, A, U, US, CE, k, tt, mo, to, do, po, lo, jo, ratio, H, wk, lo * dw, tau, dpv, flr); br, bi = _cx(Z, A, U, US, CE, k, tt, mn, float(t), dn, 0.0, ln, jn, ratio, H, wk, ln * dw, tau, dpv, flr)
        cr += ar * br + ai * bi; ci += ai * br - ar * bi
    return np.arctan2(ci, cr)

@njit(cache=True)
def _place(Z, A, U, US, CE, k, t, mode, st0, sd0, psi, lead, sjn, mn, dn0, jn, lo, hi, nc, ratio, H, dw, tau, dpv, flr, M, step, m0, Wl, nb_on):
    """Where to put band k's reader, and with what turn. A reader's position matters by one beat of the band's
    envelope: the band's two partials come out right anywhere on that grid, but a partial whose other share sits in
    the neighbouring band only adds up with that share at one place per beat. So: try nc delays from dn0 + lo to
    dn0 + hi and keep the one where the new stretch lines up best with (a) the band's own running stretch and (b) the
    neighbours' running output, the latter at the phase the two bands have between them in the input.
    -> (delay, turn)"""
    kmax = Z.shape[0]; wk = k * dw; ir = np.zeros(2); ii = np.zeros(2); nbs = np.array([k - 1, k + 1])
    if nb_on == 1:
        for q in range(2):
            a = nbs[q]
            if a < 1 or a >= kmax or m0 - Wl < 0: continue
            sr_ = 0.0; si_ = 0.0
            for j in range(Wl):
                m = m0 - j; za = Z[a, m]; zb = Z[k, m]; pr = za.real * zb.real + za.imag * zb.imag; pi_ = za.imag * zb.real - za.real * zb.imag
                ang = (a - k) * dw * (m * H - tau); c = np.cos(ang); sn = np.sin(ang); sr_ += pr * c - pi_ * sn; si_ += pr * sn + pi_ * c
            nrm = np.sqrt(sr_ * sr_ + si_ * si_); ea = 1e-30; eb = 1e-30
            for j in range(Wl):
                m = m0 - j; za = Z[a, m]; zb = Z[k, m]; ea += za.real * za.real + za.imag * za.imag; eb += zb.real * zb.real + zb.imag * zb.imag
            if nrm > 1e-30:                                     # unit phase x the share the two bands have in common
                cw = nrm / np.sqrt(ea * eb); ir[q] = sr_ / nrm * cw; ii[q] = si_ / nrm * cw
    best = -1.0; bd = dn0; bp = 0.0; mags = np.zeros(nc + 1); bi_ = 0
    for c_i in range(nc + 1):
        if c_i < nc: dn = dn0 + (lo + (hi - lo) * c_i / (nc - 1) if nc > 1 else 0.0)
        else:                                                   # once more, between the grid points: where a parabola through the best three peaks
            if nc < 3 or bi_ == 0 or bi_ == nc - 1: break
            den = mags[bi_ - 1] - 2.0 * mags[bi_] + mags[bi_ + 1]
            if den > -1e-30: break
            dn = bd + 0.5 * (mags[bi_ - 1] - mags[bi_ + 1]) / den * (hi - lo) / (nc - 1)
        xr = 0.0; xi = 0.0
        for j in range(M):
            tt = t - j * step
            br, bi = _cx(Z, A, U, US, CE, k, tt, mn, float(t), dn, 0.0, k, jn, ratio, H, wk, wk, tau, dpv, flr)
            ar, ai = _cx(Z, A, U, US, CE, k, tt, mode[k], st0[k], sd0[k], psi[k], lead[k], sjn[k], ratio, H, wk, lead[k] * dw, tau, dpv, flr)
            xr += ar * br + ai * bi; xi += ai * br - ar * bi
            for q in range(2):
                if ir[q] == 0.0 and ii[q] == 0.0: continue
                a = nbs[q]; nr, ni = _cx(Z, A, U, US, CE, a, tt, mode[a], st0[a], sd0[a], psi[a], lead[a], sjn[a], ratio, H, a * dw, lead[a] * dw, tau, dpv, flr)
                o_r = nr * br + ni * bi; o_i = ni * br - nr * bi; xr += o_r * ir[q] + o_i * ii[q]; xi += o_i * ir[q] - o_r * ii[q]
        mag = xr * xr + xi * xi; mags[c_i] = mag
        if c_i < nc:
            if mag > best: best = mag; bd = dn; bp = np.arctan2(xi, xr); bi_ = c_i
        elif mag >= best: bd = dn; bp = np.arctan2(xi, xr)
    return bd, bp

@njit(cache=True)
def _render_band(Z, A, U, US, CE, k, n, ratio, H, dw, tau, dpv, flr, et, em, ed, ep, el, ej, ef, en):
    y = np.zeros(n); k = np.int64(k); wk = k * dw; mode = 1; t0 = 0.0; d0 = 0.0; psi = 0.0; L = k; Jn = 0; omode = 1; ot0 = 0.0; od0 = 0.0; opsi = 0.0; oL = k; oJn = 0; fade = 0; flen = 1; j = 0
    for t in range(n):
        while j < en and et[j] <= t:
            omode = mode; ot0 = t0; od0 = d0; opsi = psi; oL = L; oJn = Jn; mode = em[j]; t0 = float(et[j]); d0 = ed[j]; psi = ep[j]; L = np.int64(el[j]); Jn = np.int64(ej[j]); flen = ef[j]; fade = flen; j += 1
        vr, vi = _cx(Z, A, U, US, CE, k, float(t), mode, t0, d0, psi, L, Jn, ratio, H, wk, L * dw, tau, dpv, flr); v = 2.0 * vr
        if fade > 0:
            g = fade / float(flen); g = 0.5 - 0.5 * np.cos(np.pi * g); orr, oi = _cx(Z, A, U, US, CE, k, float(t), omode, ot0, od0, opsi, oL, oJn, ratio, H, wk, oL * dw, tau, dpv, flr); v = (1.0 - g) * v + g * 2.0 * orr; fade -= 1
        y[t] = v
    return y

@njit(parallel=True, cache=True)
def _render(Z, A, U, US, CE, n, ratio, H, K, tau, dpv, flr, et, em, ed, ep, el, ej, ef, en):
    y = np.zeros(n); dw = 2.0 * np.pi / K
    for k in prange(Z.shape[0]):
        y += _render_band(Z, A, U, US, CE, np.int64(k), n, ratio, H, dw, tau, dpv, flr, et[k], em[k], ed[k], ep[k], el[k], ej[k], ef[k], en[k])
    return y

@njit(parallel=True, cache=True)
def _coh(Z, m0, Wn, ws, lmin, nl, on, CO):
    """CO[k, i] = how alike band k's newest stretch and the one (lmin + i) frames earlier are, a common turn and a
    common gain allowed (1 = the earlier stretch is the same thing; 0 also when the two differ a lot in level)"""
    for k in prange(Z.shape[0]):
        if on[k] == 0: continue
        Zk = Z[k]; e0 = 1e-30
        for j in range(Wn):
            a = Zk[m0 - j * ws]; e0 += a.real * a.real + a.imag * a.imag
        for i in range(nl):
            l = lmin + i; cr = 0.0; ci = 0.0; e1 = 1e-30
            for j in range(Wn):
                a = Zk[m0 - j * ws]; b = Zk[m0 - j * ws - l]
                cr += a.real * b.real + a.imag * b.imag; ci += a.imag * b.real - a.real * b.imag; e1 += b.real * b.real + b.imag * b.imag
            c = np.sqrt((cr * cr + ci * ci) / (e0 * e1))
            if e1 < 0.1 * e0 or e1 > 10.0 * e0: c = 0.0
            CO[k, i] = c

@njit(cache=True)
def _repeat(c, nl, lmin, drop, rho, cabs, tol2):
    """What a band's likeness-by-lag curve says. -> (status, lag in frames, likeness there)
    status 0: the curve never falls: one partial, any lag would do (plain band)
           1: it falls and does not come back: no repeat within reach
           2: it falls and comes back: the first lag at which it is back (within tol2 of the best later peak) is the
              repeat of the band's envelope"""
    top = c[0]; i_d = -1
    for i in range(nl):
        if c[i] > top: top = c[i]
        if c[i] < top - drop: i_d = i; break
    if i_d < 0: return 0, -1.0, top
    cmin = c[i_d]; g = -1.0
    for i in range(i_d, nl):
        if c[i] < cmin: cmin = c[i]
    for i in range(i_d + 1, nl - 1):
        if c[i] > c[i - 1] and c[i] >= c[i + 1] and c[i] > g: g = c[i]
    if g < cabs or g < top - (1.0 - rho) * (top - cmin): return 1, -1.0, g
    for i in range(i_d + 1, nl - 1):
        if c[i] > c[i - 1] and c[i] >= c[i + 1] and c[i] >= g - tol2:
            den = c[i - 1] - 2.0 * c[i] + c[i + 1]; off = 0.0
            if den < -1e-9: off = 0.5 * (c[i - 1] - c[i + 1]) / den
            return 2, lmin + i + off, c[i]
    return 1, -1.0, g

@njit(cache=True)
def _near(c, nl, lmin, Jq):
    """the peak of the curve next to lag Jq (frames) -> (lag, value), lag < 0 if there is none"""
    i = int(np.round(Jq)) - lmin
    if i < 1 or i > nl - 2: return -1.0, 0.0
    b = i
    for s in range(-2, 3):
        ii = i + s
        if ii >= 1 and ii <= nl - 2 and c[ii] > c[b]: b = ii
    if c[b] < c[b - 1] or c[b] < c[b + 1]: return -1.0, c[b]
    den = c[b - 1] - 2.0 * c[b] + c[b + 1]; off = 0.0
    if den < -1e-9: off = 0.5 * (c[b - 1] - c[b + 1]) / den
    return lmin + b + off, c[b]

@njit(cache=True)
def _emit(et, em, ed, ep, el, ej, ef, en, k, t, mode, d, psi, L, Jn, flen):
    j = en[k]
    if j > 0 and et[k, j - 1] >= t: j -= 1
    if j >= et.shape[1]: return
    et[k, j] = t; em[k, j] = mode; ed[k, j] = d; ep[k, j] = psi; el[k, j] = L; ej[k, j] = Jn; ef[k, j] = flen; en[k] = j + 1

@njit(cache=True)
def _control(Z, A, U, US, CE, n, ons, ratio, H, K, hopf, Wn, ws, lmin, lmax, xf, hold, settle, efloor, bridge, dA0, tau, drop, rho, cabs, tol2, n_in, n_out, dlim, M, sel, rd_on, pre, clock, Wl, lock_on, nb_on, pgain, wmin, gain_on, drop_out, slj, slq):
    kmax = Z.shape[0]; hop = hopf * H; up = ratio > 1.0001; xfu = max(xf // 2, 2) if up else xf; xa = 48
    dpv = float(H + 1); flr = float(2 * H + 2 + (int((ratio - 1.0) * xfu) + 2 if up else 0)); drift = 1.0 - ratio; dw = 2.0 * np.pi / K
    nh = n // hop + 1; nl = lmax - lmin + 1; cap = nh + nh // 2 + 64
    et = np.zeros((kmax, cap), np.int64); em = np.zeros((kmax, cap), np.int8); ed = np.zeros((kmax, cap)); ep = np.zeros((kmax, cap)); el = np.zeros((kmax, cap), np.int32); ej = np.zeros((kmax, cap), np.int32); ef = np.zeros((kmax, cap), np.int32); en = np.zeros(kmax, np.int64); sjn = np.zeros(kmax, np.int64)
    mode = np.ones(kmax, np.int8); st0 = np.zeros(kmax); sd0 = np.zeros(kmax); psi = np.zeros(kmax); J = np.zeros(kmax); Jc = np.zeros(kmax); lead = np.arange(kmax); want = np.arange(kmax); lcnt = np.zeros(kmax, np.int64)
    cin = np.zeros(kmax, np.int64); cout = np.zeros(kmax, np.int64); busy = np.zeros(kmax, np.int64); plain = np.zeros(kmax, np.uint8)
    E0 = np.zeros(kmax); on = np.zeros(kmax, np.uint8); CO = np.zeros((kmax, nl)); wk = np.zeros(kmax); cpair = np.zeros(kmax)
    scnt = np.zeros(kmax, np.int64); Js = np.zeros(kmax); nls = min(max(int(slj / H) - lmin + 2, 3), nl)        # (slow beats: how long one has held, its length, and the lags that are not slow)
    for k in range(kmax): wk[k] = k * dw
    br0 = np.zeros(nh + 8, np.int64); br1 = np.zeros(nh + 8, np.int64); nbr = 0; drt = np.zeros(nh + 8, np.int64); ndr = 0; tsw = -1; pend = -1
    wait = 0; quiet_until = 0; oi = 0; diag = np.zeros((nh, 3)); usebr = bridge == 1 and not up

    for hix in range(nh):
        t = hix * hop; m0 = t // H; ts = -1
        while oi <= t and oi < n:
            if ons[oi] == 1 and ts < 0 and oi >= wait: ts = oi
            oi += 1
        if ts >= 0:
            wait = ts + hold
            if usebr:
                tnew = ts + int(np.ceil((tau + flr - dA0) / drift)); dB = dA0 + (tnew - ts) * drift - tau
                for k in range(kmax):
                    _emit(et, em, ed, ep, el, ej, ef, en, k, tnew, 0, dB, 0.0, k, 0, 1); sjn[k] = 0; mode[k] = 0; st0[k] = tnew; sd0[k] = dB; psi[k] = 0.0; J[k] = 0.0; cin[k] = 0; cout[k] = 0; busy[k] = tnew; lead[k] = k; lcnt[k] = 0
                if nbr > 0 and ts <= br1[nbr - 1]: br1[nbr - 1] = tnew
                else: br0[nbr] = ts; br1[nbr] = tnew; nbr += 1
                drt[ndr] = ts; ndr += 1; tsw = tnew; quiet_until = max(ts + settle, tnew)
            else:
                pend = max(ts + int(tau) - pre, t)
        if tsw >= 0:
            if t < tsw: continue
            tsw = -1
        if m0 - lmax - Wn * ws - 2 >= 0:
            emax = 0.0
            for k in range(1, kmax):
                Zk = Z[k]; e = 0.0
                for j in range(Wn):
                    a = Zk[m0 - j * ws]; e += a.real * a.real + a.imag * a.imag
                E0[k] = e
                if e > emax: emax = e
            for k in range(kmax): on[k] = 1 if (k > 0 and E0[k] > efloor * emax and E0[k] > 1e-13) else 0
            if rd_on == 1: _coh(Z, m0, Wn, ws, lmin, nl, on, CO)
            nrd = 0
            for k in range(1, kmax):
                st = 0; Jf = -1.0; cf = 0.0
                if on[k] == 1 and rd_on == 1:
                    dr = drop if mode[k] == 1 else drop_out          # (a band already on a reader stays on it down to a shallower dip)
                    if slj > 0.0:
                        # A slow beat (longer than slj) is a matter of trust. Two steady partials 10 to 20 Hz apart repeat exactly and a reader gets
                        # both right; a real note's partial is itself a cluster that wanders, looks like a slow beat for a moment, and a reader that
                        # jumps 50 to 100 ms on it only adds flutter. So a slow repeat counts once it has been there (a peak of the curve at that
                        # lag, as good as the best) for slq times its own length, or when the band is already on it; until then the band goes by
                        # what the curve says within slj.
                        st, Jf, cf = _repeat(CO[k], nl, lmin, dr, rho, cabs, tol2)
                        if st == 2:
                            cn = cf
                            if scnt[k] > 0:                          # the slow repeat being watched: is its peak still there, as good as the best?
                                Jn, cn = _near(CO[k], nl, lmin, Js[k])
                                if Jn > 0.0 and cn >= cf - tol2: scnt[k] += 1; Js[k] = Jn
                                else: scnt[k] = 0
                            if Jf * H > slj:
                                if scnt[k] == 0: scnt[k] = 1; Js[k] = Jf; cn = cf
                                if scnt[k] >= slq * Js[k] * H / hop: Jf = Js[k]; cf = cn
                                elif not (mode[k] == 0 and J[k] > slj): st, Jf, cf = _repeat(CO[k], nls, lmin, dr, rho, cabs, tol2)
                        else: scnt[k] = 0
                    else: st, Jf, cf = _repeat(CO[k], nl, lmin, dr, rho, cabs, tol2)
                plain[k] = 1 if st == 0 else 0
                if st == 2:
                    if cin[k] > 0 and abs(Jf - Jc[k]) <= 0.08 * Jc[k]: cin[k] += 1
                    else: cin[k] = 1
                    Jc[k] = Jf
                else: cin[k] = 0
                if mode[k] == 1:
                    if cin[k] >= n_in and t >= quiet_until and t >= busy[k] and pend < 0:
                        Jk = Jc[k] * H; dn = flr + Jk if up else flr; stp = max(Jk, wmin) / ratio / M; jnk = max(int(np.round(Jk / H)), 1) if gain_on == 1 else 0
                        if nb_on == 1 or nb_on == 2:              # anywhere within one beat: where the neighbours' shares fit
                            dn, ps = _place(Z, A, U, US, CE, k, float(t), mode, st0, sd0, psi, lead, sjn, 0, dn, jnk, 0.0, Jk * 15.0 / 16.0, 16, ratio, H, dw, tau, dpv, flr, M, stp, m0, max(int(np.round(Jk / H)), 8), 1)
                            dn, ps = _place(Z, A, U, US, CE, k, float(t), mode, st0, sd0, psi, lead, sjn, 0, dn, jnk, -Jk / 24.0, Jk / 24.0, 5, ratio, H, dw, tau, dpv, flr, M, stp, m0, max(int(np.round(Jk / H)), 8), 1)
                            if dn < flr: dn = flr
                        else: ps = _align(Z, A, U, US, CE, k, float(t), 1, st0[k], 0.0, psi[k], lead[k], 0, 0, dn, k, jnk, ratio, H, wk[k], dw, tau, dpv, flr, M, stp)
                        _emit(et, em, ed, ep, el, ej, ef, en, k, t, 0, dn, ps, k, jnk, xf); sjn[k] = jnk; mode[k] = 0; st0[k] = t; sd0[k] = dn; psi[k] = ps; J[k] = Jk; busy[k] = t + xf; cout[k] = 0; lead[k] = k; lcnt[k] = 0
                    continue
                dk = sd0[k] + drift * (t - st0[k])              # a reader: how far behind it is now
                if J[k] > 0.0:
                    keep = False
                    if st == 2:
                        if abs(Jf * H - J[k]) <= 0.08 * J[k]: J[k] = Jf * H; keep = True
                        else:
                            Jn, cn = _near(CO[k], nl, lmin, J[k] / H)
                            if Jn > 0.0 and cn >= cf - tol2: J[k] = Jn * H; keep = True
                            elif cin[k] >= n_in: J[k] = Jf * H; keep = True
                    if keep: cout[k] = 0
                    else: cout[k] += 1
                else:                                           # free-running after the bridge: no jump needed yet
                    if cin[k] >= n_in: J[k] = Jc[k] * H; cout[k] = 0
                    elif dk - flr > dlim or on[k] == 0: cout[k] = n_out
                must = up and dk - (ratio - 1.0) * hop <= flr         # shifting up: the reader is about to reach "now"
                if (cout[k] >= n_out and t >= busy[k]) or (must and (J[k] <= 0.0 or cout[k] > 0)):
                    ps = _align(Z, A, U, US, CE, k, float(t), 0, st0[k], sd0[k], psi[k], k, sjn[k], 1, 0.0, k, 0, ratio, H, wk[k], dw, tau, dpv, flr, M, max(J[k], wmin) / ratio / M)
                    _emit(et, em, ed, ep, el, ej, ef, en, k, t, 1, 0.0, ps, k, 0, xfu); sjn[k] = 0; mode[k] = 1; st0[k] = t; sd0[k] = 0.0; psi[k] = ps; J[k] = 0.0; busy[k] = t + xfu; cout[k] = 0; cin[k] = 0; lead[k] = k; lcnt[k] = 0
                    continue
                nrd += 1
                if J[k] > 0.0:
                    dn = dk
                    if up:
                        if must: dn = dk + J[k]
                    elif t >= busy[k]:
                        while dn - J[k] >= flr: dn -= J[k]
                    if dn != dk:
                        stp = max(J[k], wmin) / ratio / M; jnk = max(int(np.round(J[k] / H)), 1) if gain_on == 1 else 0
                        if nb_on >= 2:                           # the jump, give or take an eighth of a beat: pulls the shares into step and keeps them there as things drift
                            lo_ = -J[k] / 8.0
                            if dn + lo_ < flr: lo_ = flr - dn
                            dq, ps = _place(Z, A, U, US, CE, k, float(t), mode, st0, sd0, psi, lead, sjn, 0, dn, jnk, lo_, J[k] / 8.0, 9, ratio, H, dw, tau, dpv, flr, M, stp, m0, max(int(np.round(J[k] / H)), 8), 1)
                            if pgain < 1.0:                      # go only part of the way each time: the search is a little noisy, the drift it corrects is slow
                                dn = dn + pgain * (dq - dn); ps = _align(Z, A, U, US, CE, k, float(t), 0, st0[k], sd0[k], psi[k], k, sjn[k], 0, dn, k, jnk, ratio, H, wk[k], dw, tau, dpv, flr, M, stp)
                            else: dn = dq
                        else: ps = _align(Z, A, U, US, CE, k, float(t), 0, st0[k], sd0[k], psi[k], k, sjn[k], 0, dn, k, jnk, ratio, H, wk[k], dw, tau, dpv, flr, M, stp)
                        _emit(et, em, ed, ep, el, ej, ef, en, k, t, 0, dn, ps, k, jnk, xfu); sjn[k] = jnk; st0[k] = t; sd0[k] = dn; psi[k] = ps; busy[k] = t + xfu
            diag[hix, 0] = nrd
            # ---- shares of one partial in neighbouring bands: the weaker band follows the stronger one's phase
            if lock_on == 1 and m0 - Wl - 1 >= 0:
                for k in range(1, kmax - 1):                    # how steady the phase between band k and k+1 has been
                    sr_ = 0.0; si_ = 0.0; nn = 1e-30; Za = Z[k]; Zb = Z[k + 1]
                    for j in range(Wl):
                        m = m0 - j; a = Za[m]; b = Zb[m]; pr = a.real * b.real + a.imag * b.imag; pi_ = a.imag * b.real - a.real * b.imag; ang = -dw * (m * H - tau); c = np.cos(ang); s_ = np.sin(ang)
                        sr_ += pr * c - pi_ * s_; si_ += pr * s_ + pi_ * c; nn += np.sqrt((a.real * a.real + a.imag * a.imag) * (b.real * b.real + b.imag * b.imag))
                    cpair[k] = np.sqrt(sr_ * sr_ + si_ * si_) / nn
                for k in range(1, kmax):
                    want[k] = k
                    if mode[k] != 1 or on[k] == 0: continue
                    best = E0[k]
                    lk = lead[k]                                 # (a band already following a neighbour lets go only when the phase between them gets clearly unsteady)
                    if k > 1 and mode[k - 1] == 1 and on[k - 1] == 1 and cpair[k - 1] >= (clock - 0.15 if (lk == k - 1 or lk == lead[k - 1]) and lk != k else clock) and E0[k - 1] > best: want[k] = k - 1; best = E0[k - 1]
                    if k + 1 < kmax and mode[k + 1] == 1 and on[k + 1] == 1 and cpair[k] >= (clock - 0.15 if (lk == k + 1 or lk == lead[k + 1]) and lk != k else clock) and E0[k + 1] > best: want[k] = k + 1
                for k in range(1, kmax):
                    if mode[k] != 1: continue
                    root = k
                    for it in range(8):
                        if want[root] == root: break
                        root = want[root]
                    stale = lead[k] != k and (mode[lead[k]] != 1 or lead[lead[k]] != lead[k])
                    if root != lead[k]: lcnt[k] += 1
                    else: lcnt[k] = 0
                    if (lcnt[k] >= 2 or stale) and root != lead[k] and t >= busy[k]:
                        if root != k and lead[root] == root and mode[root] == 1:
                            _emit(et, em, ed, ep, el, ej, ef, en, k, t, 1, 0.0, psi[root], root, 0, xfu); psi[k] = psi[root]; lead[k] = root; st0[k] = t; busy[k] = t + xfu; lcnt[k] = 0
                        elif root == k or stale:                # on its own again: carry on from where it is
                            L = lead[k]; pp = float(t) - dpv; ps = _wrap((ratio - 1.0) * (_ulin(US[L], pp, H) + wk[L] * (pp - tau) - _ulin(US[k], pp, H) - wk[k] * (pp - tau)) + psi[k])
                            _emit(et, em, ed, ep, el, ej, ef, en, k, t, 1, 0.0, ps, k, 0, xfu); psi[k] = ps; lead[k] = k; st0[k] = t; busy[k] = t + xfu; lcnt[k] = 0
                    elif lead[k] != k and not stale and psi[k] != psi[lead[k]] and t >= busy[k]:        # the leader was given a new turn
                        _emit(et, em, ed, ep, el, ej, ef, en, k, t, 1, 0.0, psi[lead[k]], lead[k], 0, xfu); psi[k] = psi[lead[k]]; st0[k] = t; busy[k] = t + xfu
        if pend >= 0 and pend < t + hop:                        # the phase reset ahead of an attack (no bridge)
            pr = float(pend - H); me = pend // H
            for k in range(1, kmax):
                if sel > 0.0 and me >= 16:
                    Zk = Z[k]; e1 = 0.0; e0 = 1e-30
                    for j in range(2): e1 += Zk[me - j].real ** 2 + Zk[me - j].imag ** 2
                    for j in range(8, 16): e0 += Zk[me - j].real ** 2 + Zk[me - j].imag ** 2
                    if e1 < sel * e0 / 4.0: continue
                ps = _wrap((1.0 - ratio) * (_ulin(US[k], pr, H) + wk[k] * (pr - tau)))
                _emit(et, em, ed, ep, el, ej, ef, en, k, pend, 1, 0.0, ps, k, 0, xa); sjn[k] = 0; mode[k] = 1; st0[k] = pend; sd0[k] = 0.0; psi[k] = ps; busy[k] = pend + xa; cin[k] = 0; cout[k] = 0; J[k] = 0.0; lead[k] = k; lcnt[k] = 0
            quiet_until = pend + settle; pend = -1
    return et, em, ed, ep, el, ej, ef, en, diag, br0[:nbr], br1[:nbr], drt[:ndr]

def shift(x, st, sr=SR, K=512, tau_ms=None, tail_ms=None, H=32, fmax=10000.0, reach_ms=100.0, cmp_ms=24.0, lmin_ms=2.2, hop_ms=2.9, drop=None, rho=0.7, cabs=0.9, tol2=0.01,
          n_in=3, n_out=None, settle_ms=25.0, efloor=1e-5, bridge=True, dA_ms=1.0, dlim_ms=10.0, M=16, sel=0.0, pre_ms=2.0, rd=True, lock=True, clock=0.95, lock_ms=12.0, place=None, pgain=None, gain=True, drop_out=None, over=None, smooth_ms=None, slow_ms=None, slow_hold=0.5, debug=False):
    """Shift mono signal x by st semitones (-12 .. +12). -> the shifted signal, same length, no dry signal mixed in.
    Everything after sr is a tuning constant of the engine; the defaults are what the C++ engine uses. The ones set
    to None differ between shifting up and shifting down and are filled in below.
    debug=True also returns per-hop diagnostics; debug="bands" returns every band's output and the event lists."""
    x = np.asarray(x, np.float64); r = 2 ** (st / 12); up = r > 1.0001
    if tau_ms is None: tau_ms = 8.0 if up else 12.0
    if tail_ms is None: tail_ms = 40.0 if up else 60.0
    if place is None: place = 2 if up else 3            # shifting up: place readers when they start and nudge them at jumps; down: nudge only (the bridge starts them in step)
    if pgain is None: pgain = 0.3 if up else 0.5
    # Shifting up there is no bridge, so sustained chords lean on the readers more. There the engine runs twice the
    # bands, calls a band "beating" at a finer threshold (a second partial 30 dB down counts) and keeps a band on its
    # reader longer. On held chords at +12 that took the dirt between the notes from -22.6 to -26.4 dB
    # (docs/benchmarks.md). Shifting down the same settings cost 2 to 5 dB on mixes of real takes, so they stay off.
    if over is None: over = 2 if up else 1
    if drop is None: drop = 0.001 if up else 0.004
    if drop_out is None: drop_out = 0.00025 if up else drop
    if n_out is None: n_out = 8 if up else 3
    # Also shifting up only (both measured on held chords and on chords built from single-note recordings, docs/benchmarks.md):
    # a plain band's phase advance is smoothed over 5 ms, and a beat slower than 50 ms has to hold (for slow_hold times its own
    # length) before a reader takes it.
    if smooth_ms is None: smooth_ms = 5.0 if up else 0.0
    if slow_ms is None: slow_ms = 50.0 if up else 0.0
    tau = int(round(tau_ms * sr / 1000)); h = prototype(K, tau, int(round(tail_ms * sr / 1000)))
    # over = 2: twice as many bands, half a band apart, each as wide as before (the same filter, so no extra delay). Every
    # partial then has bands in which it is the main thing, also where three partials crowd into one band's width.
    Kb = K * int(over); kmax = int(min(fmax, 0.45 * sr / max(r, 1.0)) * Kb / sr); Z = analyze(x, Kb, h, H, kmax); Z[0] = 0.0
    Zc = Z.astype(np.complex128); A = np.abs(Zc); U = np.unwrap(np.angle(Zc), axis=1); CE = np.cumsum(A * A, axis=1)
    ons = onsets(x, sr); hold = int(0.012 * sr); settle = int(settle_ms * sr / 1000); pre = int(pre_ms * sr / 1000)
    US = _smooth(Zc, U, smooth_ms * sr / 1000 / H, 15.0 * sr / 1000 / H, _holds(ons, Z.shape[1], H, hold, tau - pre + settle)) if smooth_ms > 0 else U; del Zc
    hopf = max(int(round(hop_ms * sr / 1000 / H)), 1); ws = 2; Wn = max(int(round(cmp_ms * sr / 1000 / H / ws)), 2)
    lmin = max(int(round(lmin_ms * sr / 1000 / H)), 1); lmax = int(reach_ms * sr / 1000 / H); xf = int(2.6667 * sr / 1000)
    flr = float(2 * H + 2 + (int((r - 1.0) * (max(xf // 2, 2))) + 2 if up else 0))
    ev = _control(Z, A, U, US, CE, len(x), ons, r, H, Kb, hopf, Wn, ws, lmin, lmax, xf, hold, settle, efloor, 1 if bridge else 0, dA_ms * sr / 1000, float(tau),
                    drop, rho, cabs, tol2, int(n_in), int(n_out), dlim_ms * sr / 1000, int(M), float(sel), 1 if rd else 0, pre, float(clock), max(int(lock_ms * sr / 1000 / H), 4), 1 if lock else 0, int(place), float(pgain), 0.012 * sr, 1 if gain else 0, drop_out, slow_ms * sr / 1000, float(slow_hold))
    dpv = float(H + 1); dw = 2.0 * np.pi / Kb
    if debug == "bands":
        return [_render_band(Z, A, U, US, CE, k, len(x), r, H, dw, float(tau), dpv, flr, ev[0][k], ev[1][k], ev[2][k], ev[3][k], ev[4][k], ev[5][k], ev[6][k], ev[7][k]) for k in range(kmax)], ev
    y = _render(Z, A, U, US, CE, len(x), r, H, Kb, float(tau), dpv, flr, ev[0], ev[1], ev[2], ev[3], ev[4], ev[5], ev[6], ev[7]) / float(over)
    if bridge and not up and len(ev[9]):
        yd, w = _direct(x, len(x), r, dA_ms * sr / 1000, ev[9], ev[10], ev[11], xf); y = y * (1.0 - w) + yd * w
    return (y, ev[8]) if debug else y
