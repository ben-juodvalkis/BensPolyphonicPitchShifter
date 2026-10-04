"""Measures used by the scorecard and the tests. y is always a mono render on the same timeline as the input (sample i
of y was produced while sample i of the input was being fed in), so any lateness is in the audio itself."""
import numpy as np
from scipy.signal import butter, sosfiltfilt, get_window, hilbert
from signals import SR

EPS = 1e-15


def dbp(p): return 10 * np.log10(np.maximum(p, 1e-30))


def dba(a): return 20 * np.log10(np.maximum(a, EPS))


def rms(a): return float(np.sqrt(np.mean(np.square(a)))) if len(a) else 0.0


def slide_rms(y, win):
    """e[i] = rms of y[i:i+win]"""
    win = max(int(win), 1); c = np.cumsum(np.concatenate([[0.0], np.asarray(y, float) ** 2]))
    return np.sqrt(np.maximum(c[win:] - c[:-win], 0) / win)


def first_cross(e, level):
    i = np.nonzero(e >= level)[0]
    return int(i[0]) if len(i) else None


def spectrum(y, pad=4, window="blackmanharris"):
    w = get_window(window, len(y)); S = np.abs(np.fft.rfft(y * w, pad * len(y))) / (w.sum() / 2)
    return np.fft.rfftfreq(pad * len(y), 1 / SR), S


def line_amp(f, S, f0, tol_hz=3.0):
    m = (f >= f0 - tol_hz) & (f <= f0 + tol_hz)
    return float(S[m].max()) if m.any() else 0.0


def narrowband(y, fc, periods=4):
    """Analytic signal of the component of y near fc: heterodyne to 0 Hz and average with a Hann window exactly
    `periods` periods of fc long. That window has spectral nulls at every other multiple of fc (so the other
    harmonics of a tone with fundamental fc are rejected completely) and settles within one window length."""
    n = len(y); w = np.hanning(int(round(periods * SR / fc)) | 1); w = w / w.sum()
    osc = np.exp(-2j * np.pi * fc * np.arange(n) / SR); base = y * osc
    z = np.convolve(base.real, w, "same") + 1j * np.convolve(base.imag, w, "same")
    return 2 * z * np.conj(osc)


def inst_freq(z):
    ph = np.unwrap(np.angle(z)); return np.gradient(ph) * SR / (2 * np.pi)


def onset_latency(y, yi, f_out, pre, plateau=(0.20, 0.40), search=0.30):
    """Envelope onset of y relative to the ideal (zero-latency) octave-down signal yi. Both arrays start `pre`
    samples before the note. Sliding RMS over one output period (>= 4 ms); a level is "reached" when the window
    ENDING at that sample reaches it. Returns ms relative to the ideal: first (-40 dB re plateau peak), t10, t50, t90."""
    win = int(max(SR / f_out, 0.004 * SR)); out = {}
    for tag, s in (("y", y), ("i", yi)):
        e = np.concatenate([np.zeros(win - 1), slide_rms(s, win)])          # e[i] = rms of s[i-win+1 .. i]
        if plateau is None: ref = float(e[pre:pre + int(search * SR)].max())
        else: ref = rms(s[pre + int(plateau[0] * SR):pre + int(plateau[1] * SR)])
        out[tag] = (e, ref)
    res = {}
    (e, ref), (ei, refi) = out["y"], out["i"]
    for name, frac in (("t10", 0.1), ("t50", 0.5), ("t90", 0.9)):
        a = first_cross(e, frac * ref); b = first_cross(ei, frac * refi)
        res[name] = None if a is None or b is None else (a - b) / SR * 1000
    if plateau is None: pkref = float(np.abs(y[pre:pre + int(search * SR)]).max()); pkrefi = float(np.abs(yi[pre:pre + int(search * SR)]).max())
    else: pkref = ref * np.sqrt(2); pkrefi = refi * np.sqrt(2)
    a = first_cross(np.abs(y), 0.01 * pkref); b = first_cross(np.abs(yi), 0.01 * pkrefi)
    res["first"] = None if a is None or b is None else (a - b) / SR * 1000
    res["onset_level_db"] = float(dba(ref) - dba(refi))          # output plateau / attack-peak level re the ideal's
    return res


def onset_delays(y, x, win_ms=12.0, rise_db=6.0, look_ms=20.0, min_gap_ms=120.0, max_lag_ms=60.0):
    """Real audio: for every clear note attack in the input x (envelope climbing >= rise_db within look_ms), how
    much later the output y's loudness makes the same climb. The climb itself (the rising part of the log envelope
    around the attack) is slid along the output's until they line up best. -> list of delays in ms."""
    w = int(win_ms * SR / 1000); hop = max(w // 6, 1); ms = SR / hop / 1000.0
    ex = np.concatenate([np.zeros(w - 1), slide_rms(x, w)])[::hop]; ey = np.concatenate([np.zeros(w - 1), slide_rms(y, w)])[::hop]
    n = min(len(ex), len(ey)); fl = 10 ** (-50 / 20) * ex.max(); k = int(look_ms * ms)
    lx = dba(np.maximum(ex[:n], fl)); ly = dba(np.maximum(ey[:n], fl)); rise = np.concatenate([np.zeros(k), lx[k:] - lx[:-k]])
    dx = np.maximum(np.diff(lx, prepend=lx[0]), 0); dy = np.maximum(np.diff(ly, prepend=ly[0]), 0)
    picked = []; out = []; pre = int(25 * ms); post = int(40 * ms); L = int(max_lag_ms * ms); neg = int(5 * ms)
    for i in np.argsort(rise)[::-1]:
        if rise[i] < rise_db: break
        if any(abs(i - j) < min_gap_ms * ms for j in picked): continue
        picked.append(i)
        if i - pre - neg < 0 or i + post + L >= n: continue
        a = dx[i - pre:i + post]
        cc = np.array([np.dot(a, dy[i - pre + l:i + post + l]) for l in range(-neg, L + 1)])
        if cc.max() <= 0: continue
        j = int(np.argmax(cc)); d = 0.0
        if 0 < j < len(cc) - 1:
            den = cc[j - 1] - 2 * cc[j] + cc[j + 1]; d = 0.5 * (cc[j - 1] - cc[j + 1]) / den if abs(den) > 1e-12 else 0.0
        out.append((i, (j - neg + d) / ms))
    return [d for _, d in sorted(out)]


def pitch_stats(y, f_out, t0=0.3, t1=None, lock_tol=20.0, lock_hold=0.05):
    """Output fundamental vs the expected f_out: median error and wobble in cents over the sustained part, and how
    long after the start it takes to get within lock_tol cents and stay there."""
    z = narrowband(y, f_out); f = inst_freq(z); a = np.abs(z)
    n = len(y); t1 = n / SR - 0.1 if t1 is None else t1
    s = slice(int(t0 * SR), int(t1 * SR)); cents = 1200 * np.log2(np.maximum(f, 1e-3) / f_out)
    k = max(int(0.01 * SR), 3); cs = np.convolve(cents, np.ones(k) / k, "same")
    ok = (np.abs(cs) < lock_tol) & (a > 0.1 * np.median(a[s]))
    hold = int(lock_hold * SR); run = np.convolve(ok.astype(float), np.ones(hold), "valid") >= hold - 0.5
    i = np.nonzero(run)[0]
    return dict(cents_med=float(np.median(cents[s])), cents_std=float(np.std(cs[s])), lock_ms=(float(i[0]) / SR * 1000 if len(i) else None),
                f0_level_db=float(dba(np.median(a[s]))))


def pitch_vs_ideal(y, yi, f_out, **kw):
    """pitch_stats of y, with the same detector run on the ideal signal as its floor: lock_rel_ms is how much
    later than an ideal shifter the output settles on pitch (the detector itself needs a few periods)."""
    r = pitch_stats(y, f_out, **kw); ri = pitch_stats(yi, f_out, **kw)
    r["lock_rel_ms"] = None if r["lock_ms"] is None or ri["lock_ms"] is None else r["lock_ms"] - ri["lock_ms"]
    r["cents_std_floor"] = ri["cents_std"]; r["f0_gain_db"] = r["f0_level_db"] - ri["f0_level_db"]
    return r


def purity(y, lines, t0=0.4, t1=None, tol_hz=None, fmax=None):
    """Power not at the expected lines, relative to the power at them (dB), over the steady part.
    Returns also the strongest unexpected lines."""
    n = len(y); t1 = n / SR - 0.15 if t1 is None else t1; s = y[int(t0 * SR):int(t1 * SR)]
    f, S = spectrum(s, pad=2); P = S ** 2
    tol = max(4.0 * SR / len(s), 2.5) if tol_hz is None else tol_hz
    m = np.zeros(len(f), bool)
    for l in lines: m |= (f >= l - tol) & (f <= l + tol)
    band = (f > 15.0) & (f < (fmax or 0.45 * SR))
    on = P[m & band].sum(); off = P[~m & band].sum()
    Po = np.where(m | ~band, 0, P); spurs = []
    for _ in range(5):
        i = int(np.argmax(Po))
        if Po[i] <= 0: break
        spurs.append((round(float(f[i]), 1), round(float(dbp(Po[i]) - dbp(P[m & band].max() if (m & band).any() else 1)), 1)))
        Po[(f >= f[i] - 2 * tol) & (f <= f[i] + 2 * tol)] = 0
    return dict(spur_db=float(dbp(off) - dbp(on)), spurs=spurs, line_db=[round(float(dba(line_amp(f, S, l, tol))), 2) for l in lines[:8]])


def pluck_partials(f0, B=1.2e-4, shift=0.5, fmax=0.42 * SR):
    out = []
    for k in range(1, 80):
        fk = k * f0 * np.sqrt(1 + B * k * k)
        if fk > fmax: break
        out.append(fk * shift)
    return out


def chord_stats(y, yi, freqs, t0=0.25, t1=2.0, fmax=1500.0, shift=0.5):
    """Synthetic plucked chord: how much of the output's energy below fmax is NOT at any expected (halved) partial,
    and how much the lowest partials' loudness wobbles compared with the ideal octave-down chord."""
    lines = [f for f0 in freqs for f in pluck_partials(f0, shift=shift) if f < fmax]
    p = purity(y, lines, t0, t1, tol_hz=6.0, fmax=fmax); pi = purity(yi, lines, t0, t1, tol_hz=6.0, fmax=fmax)
    war = []
    for f0 in freqs:
        fe = f0 * shift
        def env(s):
            n = len(s); z = s * np.exp(-2j * np.pi * fe * np.arange(n) / SR); sos = butter(4, min(12.0, 0.4 * fe), fs=SR, output="sos")
            return np.abs(sosfiltfilt(sos, z.real) + 1j * sosfiltfilt(sos, z.imag))
        a = env(y)[int(t0 * SR):int(t1 * SR)]; b = env(yi)[int(t0 * SR):int(t1 * SR)]
        r = dba(a) - dba(b); tt = np.arange(len(r)); r = r - np.polyval(np.polyfit(tt, r, 1), tt); war.append(float(np.std(r)))
    return dict(off_partial_db=p["spur_db"], off_partial_ideal_db=pi["spur_db"], warble_db=float(np.mean(war)), warble_each=[round(w, 2) for w in war], spurs=p["spurs"])

def line_cents(y, f, others=(), t0=0.45, t1=1.5):
    """Where the output line near f actually sits (cents from f), from the phase slope of a narrow filter. The
    filter is made long enough that the nearest OTHER expected line falls outside it (a line closer than 3 Hz is
    the same line for this purpose)."""
    near = [abs(o - f) for o in others if abs(o - f) > 3.0]; T = float(np.clip(2.5 / min(near), 0.12, 0.42)) if near else 0.12
    z = narrowband(y, f, periods=max(int(round(T * f)), 4))[int(t0 * SR):int(t1 * SR)]
    if np.abs(z).mean() < 1e-6: return None
    ph = np.unwrap(np.angle(z)); fm = np.polyfit(np.arange(len(ph)) / SR, ph, 1)[0] / (2 * np.pi)
    return float(1200 * np.log2(max(fm, 1e-3) / f))


def note_cents(s, freqs, r, t1):
    """Tuning error of each note of a plucked chord, read from its lowest partial that no OTHER note has a partial
    near (within 5 Hz at the output) - otherwise two almost-equal lines blur into one and the reading means
    nothing. None for a note with no such partial among its first five."""
    P = [[p for p in pluck_partials(f0, shift=r) if p < 6000.0] for f0 in freqs]; out = []
    for i in range(len(freqs)):
        others = [p for j, q in enumerate(P) if j != i for p in q]; c = None
        for k, fk in enumerate(P[i][:5]):
            if all(abs(o - fk) > 5.0 for o in others): c = line_cents(s, fk, others + [p for p in P[i] if p != fk], t1=t1); break
        out.append(c)
    return out


def superposition(ym, ya, yb, n_fft=8192, hop=2048, k=2.5, floor_db=70.0):
    """New spectral energy in dev(a+b) that dev(a) and dev(b) do not have (dB re the total), and energy they
    have that the mix lost. Fine frequency resolution (5.4 Hz bins), so sidebands and wrong tones between the
    partials count, while a partial that is merely a little louder or softer does not (k = 4 dB of slack, and a
    +-2 bin neighbourhood so a few cents of drift is not counted either)."""
    from scipy.ndimage import maximum_filter1d
    n = min(len(ym), len(ya), len(yb)); w = np.blackman(n_fft); ex = []; mi = []; tot = []
    for i in range(0, n - n_fft, hop):
        A = np.abs(np.fft.rfft(ya[i:i + n_fft] * w)) ** 2; B = np.abs(np.fft.rfft(yb[i:i + n_fft] * w)) ** 2; M = np.abs(np.fft.rfft(ym[i:i + n_fft] * w)) ** 2
        ref = A + B; fl = ref.max() * 10 ** (-floor_db / 10)
        ex.append(np.maximum(M - k * maximum_filter1d(ref, 5) - fl, 0).sum()); mi.append(np.maximum(ref / k - maximum_filter1d(M, 5) - fl, 0).sum()); tot.append(ref.sum())
    ex = np.array(ex); mi = np.array(mi); tot = np.array(tot); live = tot > tot.max() * 10 ** (-30 / 10)
    return dict(excess_db=float(dbp(ex[live].sum() / tot[live].sum())), missing_db=float(dbp(mi[live].sum() / tot[live].sum())),
                excess_p90_db=float(np.percentile(dbp(ex[live] / tot[live] + 1e-12), 90)))


def line_dirt(x, y, r, t0=0.4, t1=None, n_fft=8192, hop=2048, tol_bins=3.5, floor_db=55.0):
    """Held sound with no ideal signal to compare with (real recordings are measured this way too): the input's
    spectrum is a set of lines, a perfect shifter moves each to r x its frequency and adds nothing. -> the output's
    energy that is NOT within tol_bins (19 Hz) of any moved line, dB re the output's total, over the frames between
    t0 and t1 (seconds; no attack may fall inside). The input's lines are read frame by frame, down to floor_db
    below the strongest. Two partials closer than the frame can tell apart count as one line, so a slow beat that is
    carried along at its old rate is not dirt here; scorecard.py, which knows every partial, is the stricter test."""
    w = np.blackman(n_fft); f = np.fft.rfftfreq(n_fft, 1 / SR); bw = f[1]; t1 = len(x) / SR if t1 is None else t1
    band = (f > 30.0) & (f < min(5000.0 * max(r, 0.5), 0.45 * SR)); off = 0.0; tot = 0.0
    for i in range(int(t0 * SR), min(int(t1 * SR), len(x), len(y)) - n_fft, hop):
        X = np.abs(np.fft.rfft(x[i:i + n_fft] * w)); P = 20 * np.log10(X + 1e-15)
        pk = np.nonzero((P[1:-1] > P[:-2]) & (P[1:-1] >= P[2:]) & (P[1:-1] > P.max() - floor_db))[0] + 1
        q = (pk + 0.5 * (P[pk - 1] - P[pk + 1]) / (P[pk - 1] - 2 * P[pk] + P[pk + 1] - 1e-12)) * r; m = np.zeros(len(f), bool)
        for v in q: m[max(int(np.floor(v - tol_bins)), 0):int(np.ceil(v + tol_bins)) + 1] = True
        Py = np.abs(np.fft.rfft(y[i:i + n_fft] * w)) ** 2; off += Py[band & ~m].sum(); tot += Py[band].sum()
    return float(dbp(off / (tot + 1e-30) + 1e-12))


def flutter(y, fmin=100.0, fmax=8000.0, lo=20.0, hi=150.0):
    """Fast loudness flutter of a held sound: in each third-octave band of y, the power of the envelope's movement
    between lo and hi Hz against its steady level; energy-weighted over the bands, dB. Partials that share a band
    beat, so this is not zero for a perfect shifter: it is for comparing two renderings of the same chord."""
    num = 0.0; den = 0.0; fc = fmin; q = int(0.02 * SR)
    while fc <= fmax:
        b = sosfiltfilt(butter(4, [fc / 2 ** (1 / 6), fc * 2 ** (1 / 6)], btype="band", fs=SR, output="sos"), y); e = np.abs(hilbert(b))[q:-q]; h = np.hanning(len(e))
        E = np.abs(np.fft.rfft((e - e.mean()) * h)) ** 2; f = np.fft.rfftfreq(len(e), 1 / SR); p = float(np.mean(b[q:-q] ** 2))
        num += p * E[(f >= lo) & (f <= hi)].sum() * 2 / (np.sum(h ** 2) * len(e)) / (e.mean() ** 2 + 1e-30); den += p; fc *= 2 ** (1 / 3)
    return float(dbp(num / (den + 1e-30) + 1e-12))
