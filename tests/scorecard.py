"""The scorecard: how clean, how in tune and how late, on synthetic material with exact answers.

    python tests/scorecard.py                     the C++ engine at -12, +2 and +12, checked against the gate
    python tests/scorecard.py reference -12 7     another target (engine | reference | plugin), other intervals
    python tests/scorecard.py engine all          all six intervals (-12 -5 -2 +2 +7 +12)
    python tests/scorecard.py engine clean 12     with the response set to balanced or clean (it only matters shifting up)

What is measured (per interval, one test signal of about two minutes):
  pairs    28 pairs of sines (4 registers x 7 intervals): power that is not the two shifted tones, dB re them.
           "clean" counts the pairs under -40 dB.
  dyads    20 plucked two-note chords across the neck: power not at any expected partial, and each note's tuning
  chords   10 plucked guitar voicings (open, barre, high triads): the same two numbers
  notes    6 single plucked notes: how late the attack comes out, tuning, and timbre (how far each partial's
           loudness is from the ideal shift, rms dB over the partials within 30 dB of the strongest)

Exit status 1 if a gated interval (-12, +2 and +12; +12 for the balanced and clean responses) falls outside the limits in
GATE or GATE_RESPONSE: a regression guard, set a little looser than what the engine scores today (docs/benchmarks.md).
"""
import sys, time, json, os
import numpy as np
from signals import SR, NOTE, N, sine, pluck, strum
from metrics import purity, chord_stats, onset_latency, pitch_vs_ideal, line_cents, note_cents, pluck_partials, spectrum, dba
import engines

GAP = int(0.3 * SR); PRE = int(0.2 * SR)
ALL = (-12, -5, -2, 2, 7, 12)
P_ROWS = (220.0, 440.0, 880.0, 1760.0); P_IVS = (1, 2, 3, 4, 6, 7, 12)
D_ROOTS = ("E2", "D3", "B3", "A4", "E5"); D_IVS = ((3, "m3"), (4, "M3"), (7, "P5"), (9, "M6"))
CH = [("E5 power", N("E2", "B2", "E3")), ("A5 power", N("A2", "E3", "A3")), ("E open", N("E2", "B2", "E3", "G#3", "B3", "E4")), ("Am open", N("A2", "E3", "A3", "C4", "E4")),
      ("C open", N("C3", "E3", "G3", "C4", "E4")), ("A barre 5th", N("A2", "E3", "A3", "C#4", "E4", "A4")), ("Bm barre 7th", N("B2", "F#3", "B3", "D4", "F#4", "B4")),
      ("E shape 12th", N("E3", "B3", "E4", "G#4", "B4", "E5")), ("A triad high", N("A4", "C#5", "E5")), ("D triad 14th", N("D5", "F#5", "A5"))]
PL = ("E2", "A2", "G3", "E4", "A4", "E5")
# limits for the regression gate: (key, worst allowed, "max" = must not exceed / "min" = must reach)
GATE = {-12: (("pairs_clean", 26, "min"), ("dyads_db", -37.0, "max"), ("chords_db", -34.0, "max"), ("chord_cents", 1.5, "max"), ("attack_ms", 3.0, "max"), ("note_cents", 0.5, "max"), ("timbre_db", 1.5, "max")),
        2: (("pairs_clean", 25, "min"), ("dyads_db", -39.0, "max"), ("chords_db", -37.0, "max"), ("chord_cents", 1.0, "max"), ("attack_ms", 9.5, "max"), ("note_cents", 0.5, "max"), ("timbre_db", 1.5, "max")),      # a shift up by less than a fifth has settings of its own
        12: (("pairs_clean", 23, "min"), ("dyads_db", -31.0, "max"), ("chords_db", -27.0, "max"), ("chord_cents", 1.0, "max"), ("attack_ms", 8.5, "max"), ("note_cents", 0.6, "max"), ("timbre_db", 1.5, "max"))}
# the same for the balanced (1) and clean (2) responses, which only matter shifting up
GATE_RESPONSE = {1: {12: (("pairs_clean", 24, "min"), ("dyads_db", -31.5, "max"), ("chords_db", -28.5, "max"), ("chord_cents", 1.0, "max"), ("attack_ms", 12.5, "max"), ("note_cents", 0.6, "max"), ("timbre_db", 1.5, "max"))},
                 2: {12: (("pairs_clean", 25, "min"), ("dyads_db", -31.0, "max"), ("chords_db", -31.5, "max"), ("chord_cents", 1.0, "max"), ("attack_ms", 16.5, "max"), ("note_cents", 0.6, "max"), ("timbre_db", 1.5, "max"))}}


def build(st):
    r = 2 ** (st / 12); parts = [np.zeros(SR // 2)]; segs = []; ideal = {}; pos = SR // 2
    def add(name, kind, sig, yi=None, **meta):
        nonlocal pos
        segs.append(dict(name=name, kind=kind, start=pos, length=len(sig), **meta)); parts.extend([sig, np.zeros(GAP)]); pos += len(sig) + GAP
        if yi is not None: ideal[name] = yi
    for fc in P_ROWS:
        for iv in P_IVS:
            f2 = fc * 2 ** (iv / 12); add(f"pair {fc:.0f}+{iv}", "pair", sine(fc, 1.2, -18.0)[0] + sine(f2, 1.2, -18.0)[0], freqs=[fc, f2])
    for root in D_ROOTS:
        for iv, nm in D_IVS:
            fr = [NOTE[root], NOTE[root] * 2 ** (iv / 12)]; s, yi = strum(fr, 1.8, r, spread_ms=0.0); add(f"dyad {root}+{nm}", "dyad", s, yi, freqs=fr)
    for nm, fr in CH:
        s, yi = strum(fr, 2.2, r); add(f"chord {nm}", "chord", s, yi, freqs=fr)
    for nm in PL:
        s, yi = pluck(NOTE[nm], 1.4, shift=r); add(f"note {nm}", "note", s, yi, f0=NOTE[nm])
    parts.append(np.zeros(SR // 2)); return np.concatenate(parts), segs, ideal


def timbre(s, yi, f0, r):
    """rms error (dB) of the partials' loudness against the ideal shift, over the partials within 30 dB of the strongest"""
    fmax = 1500.0 * max(r / 0.5, 1.0); lines = [f for f in pluck_partials(f0, shift=r) if f < fmax]
    def levels(v):
        f, S = spectrum(v[int(0.3 * SR):int(1.2 * SR)], 2); return np.array([dba(np.sqrt(np.sum(S[(f > l - 4) & (f < l + 4)] ** 2)) + 1e-12) for l in lines])
    a = levels(yi); b = levels(s); keep = a > a.max() - 30; d = (b - a)[keep]; d = d - np.median(d)
    return float(np.sqrt(np.mean(d ** 2)))


def score(y, st, segs, ideal):
    r = 2 ** (st / 12); R = {}
    for m in segs:
        s = y[m["start"]:m["start"] + m["length"]]; k = m["kind"]
        if k == "pair":
            lines = [f * r for f in m["freqs"]]; d = dict(spur_db=purity(s, lines, t0=0.4, t1=1.1)["spur_db"])
        elif k in ("dyad", "chord"):
            t1 = m["length"] / SR - 0.3; cs = chord_stats(s, ideal[m["name"]], m["freqs"], t1=t1, shift=r, fmax=1500.0 * max(r / 0.5, 1.0))
            d = dict(off_db=cs["off_partial_db"], cents=note_cents(s, m["freqs"], r, min(t1, 1.5)))
        else:
            yi = ideal[m["name"]]; yp = y[m["start"] - PRE:m["start"] + m["length"]]; ol = onset_latency(yp, np.concatenate([np.zeros(PRE), yi]), m["f0"] * r, PRE, plateau=None)
            d = dict(t50_ms=ol["t50"], cents=pitch_vs_ideal(s, yi, m["f0"] * r, t0=0.2)["cents_med"], timbre_db=timbre(s, yi, m["f0"], r))
        R[m["name"]] = d
    return R


def card(R):
    g = lambda kind, key: np.array([v[key] for n, v in R.items() if n.startswith(kind) and v.get(key) is not None])
    cents = lambda kind: np.array([abs(c) for n, v in R.items() if n.startswith(kind) for c in v["cents"] if c is not None])
    p = g("pair", "spur_db")
    return dict(pairs_db=float(np.median(p)), pairs_clean=int((p < -40).sum()), dyads_db=float(np.median(g("dyad", "off_db"))), dyad_cents=float(np.median(cents("dyad"))),
                chords_db=float(np.median(g("chord", "off_db"))), chord_cents=float(np.median(cents("chord"))), attack_ms=float(np.median(g("note", "t50_ms"))),
                note_cents=float(np.max(np.abs(g("note", "cents")))), timbre_db=float(np.median(g("note", "timbre_db"))))


COLS = (("pairs_db", "pairs dB", 1), ("pairs_clean", "clean/28", 0), ("dyads_db", "dyads dB", 1), ("dyad_cents", "dyad c", 1), ("chords_db", "chords dB", 1), ("chord_cents", "chord c", 1),
        ("attack_ms", "attack ms", 1), ("note_cents", "note c", 1), ("timbre_db", "timbre dB", 1))


def run(target="engine", sts=(-12, 12), save=None, response=0):
    fn = engines.BY_NAME[target]; ok = True; out = {}; gate = GATE_RESPONSE[response] if response else GATE
    print(f"{engines.RESPONSES[response] if response else target:>10} | " + " ".join(f"{h:>9}" for _, h, _ in COLS))
    for st in sts:
        t0 = time.time(); x, segs, ideal = build(st); y = fn(x, st, response=response); c = card(score(y, st, segs, ideal)); out[str(st)] = c
        fails = [f"{k} {c[k]:.1f} (limit {lim})" for k, lim, how in gate.get(st, ()) if (c[k] > lim if how == "max" else c[k] < lim)]
        print(f"{st:+10d} | " + " ".join(f"{c[k]:9.{p}f}" for k, _, p in COLS) + f"   [{time.time() - t0:.0f} s]" + ("   OUTSIDE THE GATE: " + "; ".join(fails) if fails else ("   gate ok" if st in gate else "")))
        ok = ok and not fails
    if save: json.dump(out, open(save, "w"), indent=1)
    return ok


if __name__ == "__main__":
    a = sys.argv[1:]; target = a[0] if a and a[0] in engines.BY_NAME else "engine"; resp = max([engines.RESPONSES.index(v) for v in a if v in engines.RESPONSES] + [0])
    rest = [v for v in a if v not in engines.BY_NAME and v not in engines.RESPONSES]
    sts = ALL if rest == ["all"] else tuple(int(v) for v in rest) or ((12,) if resp else (-12, 2, 12))
    sys.exit(0 if run(target, sts, response=resp) else 1)
