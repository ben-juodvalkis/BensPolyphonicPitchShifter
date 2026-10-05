// PolyPitch engine: a polyphonic pitch shifter for live playing (no dependencies, header only).
// One sample in, one shifted sample out. A streaming port of reference/polypitch_ref.py: same constants, same
// decisions; tests/test_engine_vs_reference.py holds the two together.
//
// How it works (docs/how-it-works.md has the long version):
// The input runs through a bank of narrow band-pass filters (one FFT every 32 samples gives each band's slowly
// changing envelope). Each band is then handled in one of two ways.
//   "plain" band (one partial in it): its loudness is passed on as it happens and its phase is advanced `ratio`
//       times as fast as the input's (that advance smoothed over a few milliseconds). Nothing is
//       repeated or skipped; the partial is exactly in tune. A band that holds the weaker share of a partial follows
//       its stronger neighbour's phase, so the two shares stay in step.
//   "beating" band (two partials in it): a reader plays the band at the shifted speed and jumps by exactly one
//       repeat of the band's envelope (one beat), with the phase carried across. Both partials come out right.
// Attacks, shifting down: played straight from the input (the bridge); the bands take over where they are the same
// signal. Attacks, shifting up: every band's phase is set equal to the input's just before the attack reaches it.
// Shifting up runs twice as many bands (half a band apart, same filter) than shifting down, and the response picks the
// filter: fast (8 ms), balanced (the same bands behind 12 ms) or clean (bands half as wide behind 16 ms). Each step is
// cleaner and lets an attack out 4 ms later.
//
// Copyright (c) 2026 Ben Juodvalkis. MIT License (see LICENSE).
#pragma once
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "PolyPitchFilters.h"

namespace polypitch
{

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hopf = std::max ((int) std::lround (2.9 * sr / 1000.0 / H), 1); hop = hopf * H;
        Wn = std::max ((int) std::lround (24.0 * sr / 1000.0 / H / ws), 2);
        lmin = std::max ((int) std::lround (2.2 * sr / 1000.0 / H), 1);
        lmax = std::min ((int) (100.0 * sr / 1000.0 / H), NF / 3); nl = lmax - lmin + 1;
        xf = (int) (2.6667 * sr / 1000.0);
        hold = (int) (0.012 * sr); settle = (int) (25.0 * sr / 1000.0);
        dA0 = 1.0 * sr / 1000.0; twait = 40.0 * sr / 1000.0; pre = (int) (2.0 * sr / 1000.0);
        Wl = std::max ((int) (12.0 * sr / 1000.0 / H), 4); wmin = 0.012 * sr;
        afc = 1.0 - std::exp (-1.0 / (0.001 * sr)); rel = std::exp (-1.0 / (0.080 * sr));
        onsD = std::max ((int) (0.003 * sr), 1); onsBuf.assign ((size_t) onsD, 0.0);
        kS = std::min ((int) (std::min (fmax, 0.45 * sr) * KMAX / sr), KMAX / 2);      // room for the larger bank
        dpv = (double) (H + 1);
        xring.assign (XN, 0.0);
        Zr.assign ((size_t) kS * 2 * NF, 0.0f); Zi.assign ((size_t) kS * 2 * NF, 0.0f);
        Am.assign ((size_t) kS * NF, 0.0); Um.assign ((size_t) kS * NF, 0.0); Cm.assign ((size_t) kS * NF, 0.0); lastAng.assign ((size_t) kS, 0.0);
        Us.assign ((size_t) kS * NF, 0.0); fsm.assign ((size_t) kS, 0.0); ebm.assign ((size_t) kS, 0.0); epm.assign ((size_t) kS, 0.0);
        smA = 1.0 - std::exp (-1.0 / (5.0 * sr / 1000.0 / H)); smE = 1.0 - std::exp (-1.0 / (15.0 * sr / 1000.0 / H));
        const size_t n = (size_t) kS;
        mode.assign (n, 1); omode.assign (n, 1); lead.assign (n, 0); olead.assign (n, 0); fade.assign (n, 0); flen.assign (n, 1); sjn.assign (n, 0); ojn.assign (n, 0);
        st0.assign (n, 0.0); sd0.assign (n, 0.0); psi.assign (n, 0.0); ot0.assign (n, 0.0); od0.assign (n, 0.0); opsi.assign (n, 0.0);
        evT.assign (n, -1); busy.assign (n, 0); J.assign (n, 0.0); Jc.assign (n, 0.0); cin.assign (n, 0); cout.assign (n, 0); lcnt.assign (n, 0); want.assign (n, 0);
        rc.assign (n, 1.0); rs.assign (n, 0.0); rdc.assign (n, 1.0); rds.assign (n, 0.0); ramp.assign (n, 0.0); rdamp.assign (n, 0.0); rdirty.assign (n, 1);
        orc.assign (n, 1.0); ors.assign (n, 0.0); ordc.assign (n, 1.0); ords.assign (n, 0.0); oramp.assign (n, 0.0); ordamp.assign (n, 0.0); ordirty.assign (n, 1); stC.assign (n, 1.0); stS.assign (n, 0.0);
        E0.assign (n, 0.0); on.assign (n, 0); cpair.assign (n, 0.0); wk.assign (n, 0.0); CO.assign (n * (size_t) nl, 0.0); scnt.assign (n, 0); Js.assign (n, 0.0);
        fftRe.assign ((size_t) KMAX, 0.0); fftIm.assign ((size_t) KMAX, 0.0);
        lockC.assign ((size_t) NF, 0.0); lockS.assign ((size_t) NF, 0.0); mags.assign (32, 0.0); runR.assign ((size_t) NF + 8, 0.0f); runI.assign ((size_t) NF + 8, 0.0f);
        pvR.assign (4 * M, 0.0); pvI.assign (4 * M, 0.0);
        for (int b = 0; b < 4; ++b) makeBank (b);      // every filter now, so that changing direction or response while playing allocates nothing
        configured = false; prepared = false;
        setSemitones (semis);
        reset();
    }

    void setSemitones (double st)
    {
        semis = st; ratio = std::pow (2.0, st / 12.0);
        up = ratio > 1.0001; unity = std::abs (ratio - 1.0) < 1e-9;
        if (sr <= 0.0) return;
        if (! configured || bankFor() != loaded) loadFilter();
        // shifting up: twice the bands (half a band apart, same filter), a finer threshold for calling a band "beating", and a band on a reader stays longer
        dropIn = up ? 0.001 : 0.004; dropOut = up ? 0.00025 : dropIn; nOut = up ? 8 : 3;
        // in both directions: a beat slower than 50 ms has to hold before a reader takes it (and a plain band's phase advance is smoothed over 5 ms: analyzeFrame)
        slj = 50.0 * sr / 1000.0; nls = std::min (std::max ((int) (slj / H) - lmin + 2, 3), nl);
        xfu = up ? std::max (xf / 2, 2) : xf;
        flr = (double) (2 * H + 2 + (up ? (int) ((ratio - 1.0) * xfu) + 2 : 0));
        drift = 1.0 - ratio;
        kmax = std::min ((int) (std::min (fmax, 0.45 * sr / std::max (ratio, 1.0)) * Kb / sr), kS);
        usebr = ! up && ! unity;
        placeMode = up ? 2 : 3; pgain = up ? 0.3 : 0.5;
        for (size_t k = 0; k < stC.size(); ++k) { stC[k] = std::cos (wk[k] * ratio); stS[k] = std::sin (wk[k] * ratio); }      // a reader's carrier turns this much per sample
        if (prepared) softReset();
    }

    // shifting up only: how late an attack may come out for a cleaner sound. 0 = fast (the attack as early as it can be: an 8 ms
    // band filter), 1 = balanced (the same bands behind a 12 ms filter: attacks 4 ms later), 2 = clean (bands half as wide behind a
    // 16 ms filter: attacks 8 ms later than fast; the middle note of a full chord, crowded by other notes' partials in the wide
    // bands, gets bands of its own). Changing it while shifting up restarts the bands, as changing the interval does; while
    // shifting down it changes nothing until the interval goes up.
    void setResponse (int r)
    {
        r = std::max (0, std::min (2, r));
        if (r == response) return;
        response = r;
        if (sr > 0.0 && bankFor() != loaded) setSemitones (semis);
    }
    int getResponse() const { return response; }

    void reset()
    {
        std::fill (xring.begin(), xring.end(), 0.0);
        clearFrames();
        std::fill (onsBuf.begin(), onsBuf.end(), 0.0);
        t = 0; ef = 0.0; pkh = 0.0; onsJ = 0;
        softReset();
        prepared = true;
    }

    // one sample in, one wet sample out
    double processSample (double x)
    {
        xring[(size_t) (t & (XN - 1))] = x;
        if ((t % H) == 0) analyzeFrame();
        // attack detector: the 1 ms level against the highest it has been lately (held, released over 80 ms, read 3 ms back)
        const double v = std::abs (x);
        ef += afc * (v - ef);
        const double old = onsBuf[(size_t) onsJ]; onsBuf[(size_t) onsJ] = ef; if (++onsJ == onsD) onsJ = 0;
        pkh = std::max (pkh * rel, old);
        const bool flag = ef > 2.0 * pkh && ef > 1e-4;
        if (brActive) { dirP += ratio; if (dirFade > 0) dirPOld += ratio; }
        if (unity) { ++t; return x; }

        if (flag && t >= wait) onAttack();
        if (pendSwitch >= 0 && t >= pendSwitch)
        {
            // the bands take over from the direct reader: all at one position, where they are the same signal
            for (int k = 0; k < kS; ++k) emit (k, t, 0, pendDelay, 0.0, k, 0, 1);
            pendSwitch = -1;
        }
        if ((t % hop) == 0 && pendSwitch < 0) control();
        if (pend >= 0 && t >= pend) phaseReset();

        // the bands. Between frames a plain band's loudness and phase run in straight lines, and a reader's carrier turns
        // at a fixed rate, so each stretch is a spinning vector that is set exactly once per frame (and when it changes)
        double y = 0.0; const bool tick = ((t - 1) % H) == 0;
        for (int k = 1; k < kmax; ++k)
        {
            const size_t i = (size_t) k;
            if (tick || rdirty[i]) { setVector (k, mode[i], st0[i], sd0[i], psi[i], lead[i], rc[i], rs[i], rdc[i], rds[i], ramp[i], rdamp[i]); rdirty[i] = 0; }
            double s = stepVector (k, mode[i], st0[i], sd0[i], sjn[i], rc[i], rs[i], rdc[i], rds[i], ramp[i], rdamp[i]);
            if (fade[i] > 0)
            {
                if (tick || ordirty[i]) { setVector (k, omode[i], ot0[i], od0[i], opsi[i], olead[i], orc[i], ors[i], ordc[i], ords[i], oramp[i], ordamp[i]); ordirty[i] = 0; }
                double g = (double) fade[i] / (double) flen[i]; g = 0.5 - 0.5 * std::cos (kPi * g);
                s = (1.0 - g) * s + g * stepVector (k, omode[i], ot0[i], od0[i], ojn[i], orc[i], ors[i], ordc[i], ords[i], oramp[i], ordamp[i]);
                --fade[i];
            }
            y += s;
        }
        y /= (double) over;
        // the attack bridge: between br0 and br1 the output is the input itself at the shifted speed
        if (brActive)
        {
            const int fin = std::max (xf / 2, 2), fout = xf;
            double w;
            if (t < br0 + fin) { const double u = 0.5 - 0.5 * std::cos (kPi * (double) (t - br0 + 1) / fin); w = brW0 + (1.0 - brW0) * u; }
            else if (t < br1) w = 1.0;
            else if (t < br1 + fout) w = 0.5 + 0.5 * std::cos (kPi * (double) (t - br1 + 1) / fout);
            else { w = 0.0; brActive = false; dirAct = false; }
            brW = w;
            if (w > 0.0)
            {
                double vd = readDirect (dirP);
                if (dirFade > 0)
                {
                    double g = (double) dirFade / fin; g = 0.5 - 0.5 * std::cos (kPi * g);
                    vd = (1.0 - g) * vd + g * readDirect (dirPOld);
                    --dirFade;
                }
                y = y * (1.0 - w) + vd * w;
            }
        }
        ++t;
        return y;
    }

    int getNumBands() const { return kmax; }
    int readersNow() const { int n = 0; for (int k = 1; k < kmax; ++k) n += mode[(size_t) k] == 0 ? 1 : 0; return n; }
    bool usingDesignedFilter() const { return designed; }

private:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr int KMAX = 1024, H = 32, NF = 512, FM = NF - 1, XN = 8192, ws = 2, M = 16, nIn = 3, xa = 48;
    static constexpr double fmax = 10000.0, efloor = 1e-5, rho = 0.7, cabs = 0.9, tol2 = 0.01, clock = 0.95;

    double sr = 0.0, semis = -12.0, ratio = 0.5, drift = 0.5, flr = 66.0, dpv = 33.0, dA0 = 44.1, twait = 1764.0, wmin = 529.2, dw = 0.0, pgain = 0.5, tauD = 529.0, dropIn = 0.004, dropOut = 0.004;
    double smA = 0.0, smE = 0.0, slj = 0.0, slq = 0.5;
    bool up = false, unity = false, usebr = true, prepared = false, configured = false, designed = false;
    int tau = 529, L = 3176, hopf = 4, hop = 128, Wn = 17, lmin = 3, lmax = 137, nl = 135, xf = 117, xfu = 117, hold = 529, settle = 1102, pre = 88, Wl = 16, kmax = 116, kS = 116, placeMode = 3, Kb = 512, over = 1, nOut = 3, kB = 116, nls = 135;
    std::vector<double> rc, rs, rdc, rds, ramp, rdamp, orc, ors, ordc, ords, oramp, ordamp, stC, stS;
    std::vector<char> rdirty, ordirty;
    struct Bank { std::vector<double> h, twC, twS; int K = 512, over = 1, tau = 0, L = 0; bool designed = false; };      // one band filter and its FFT twiddles
    Bank banks[4];                                                    // [0] shifting down; shifting up: [1] fast, [2] balanced, [3] clean. Made in prepare()
    int response = 0, loaded = -1;
    int bankFor() const { return up ? 1 + response : 0; }
    const double* h = nullptr; const double* twC = nullptr; const double* twS = nullptr;                // the ones in use
    std::vector<double> xring, fftRe, fftIm, Am, Um, Us, fsm, ebm, epm, Cm, lastAng, st0, sd0, psi, ot0, od0, opsi, J, Jc, Js, E0, cpair, wk, CO, onsBuf, lockC, lockS, mags;
    std::vector<float> Zr, Zi, runR, runI;
    std::vector<double> pvR, pvI;
    std::vector<int> mode, omode, lead, olead, fade, flen, cin, cout, lcnt, want, on, sjn, ojn, scnt;
    std::vector<int64_t> evT, busy;
    int64_t t = 0, mNow = -1, wait = 0, quietUntil = 0, pendSwitch = -1, pend = -1, br0 = 0, br1 = 0, smHold = -1;
    double pendDelay = 0.0, ef = 0.0, pkh = 0.0, afc = 0.0, rel = 0.0, dirP = 0.0, dirPOld = 0.0, brW = 0.0, brW0 = 0.0;
    int onsD = 132, onsJ = 0, dirFade = 0;
    bool brActive = false, dirAct = false;

    static inline double wrap (double a) { return a - 2.0 * kPi * std::round (a / (2.0 * kPi)); }

    // called from prepare(), never while audio runs: one band filter and its FFT twiddles.
    //   which 0: shifting down             512 bands (86 Hz apart at 44.1 kHz), 12 ms delay (the attack bridge hides it)
    //   which 1: shifting up, fast         the same bands behind an 8 ms filter, and twice as many of them, half a band apart
    //   which 2: shifting up, balanced     as fast, behind the 12 ms filter (the one shifting down uses): less of a partial leaks
    //                                      into the bands around it
    //   which 3: shifting up, clean        1024 bands half as wide, 16 ms delay: where three partials 30 to 50 Hz apart crowd together
    //                                      (the third of a full chord between its neighbours' harmonics) each has a band of its own
    void makeBank (int which)
    {
        static const int Ks[4] = { 512, 512, 512, 1024 }, overs[4] = { 1, 2, 2, 1 };
        static const double delayMs[4] = { 12.0, 8.0, 12.0, 16.0 }, simpleTailMs[4] = { 40.0, 28.0, 40.0, 56.0 };
        Bank& b = banks[which]; b.K = Ks[which]; b.over = overs[which]; b.tau = (int) std::lround (delayMs[which] * sr / 1000.0);
        // the designed filter for this rate, band count and delay, or (other sample rates) the simple one: a sinc under a lopsided Hann window
        const filters::Table* tb = nullptr;
        for (int i = 0; i < filters::numTables; ++i)
            if (filters::tables[i].sampleRate == (int) std::lround (sr) && filters::tables[i].K == b.K && filters::tables[i].tau == b.tau) tb = &filters::tables[i];
        if (tb != nullptr) { b.L = tb->length; b.h.assign (tb->h, tb->h + b.L); b.designed = true; }
        else
        {
            const int tail = (int) std::lround (simpleTailMs[which] * sr / 1000.0);
            b.L = std::min (b.tau + tail + 1, XN - 64); b.h.assign ((size_t) b.L, 0.0); b.designed = false;
            for (int n = 0; n < b.L; ++n)
            {
                const double c = (double) (n - b.tau), w = c <= 0.0 ? 0.5 + 0.5 * std::cos (kPi * c / (b.tau + 1)) : 0.5 + 0.5 * std::cos (kPi * c / (tail + 1)), a = c / b.K;
                b.h[(size_t) n] = (std::abs (a) < 1e-12 ? 1.0 : std::sin (kPi * a) / (kPi * a)) * w / b.K;
            }
        }
        const int kb = b.K * b.over; b.twC.assign ((size_t) kb, 0.0); b.twS.assign ((size_t) kb, 0.0);
        for (int i = 0; i < kb; ++i) { b.twC[(size_t) i] = std::cos (2.0 * kPi * i / kb); b.twS[(size_t) i] = std::sin (2.0 * kPi * i / kb); }
    }

    // switch to the filter for the current direction and response (all were made in prepare(), so nothing is allocated here)
    void loadFilter()
    {
        loaded = bankFor(); const Bank& b = banks[loaded];
        h = b.h.data(); twC = b.twC.data(); twS = b.twS.data(); tau = b.tau; L = b.L; designed = b.designed;
        tauD = (double) tau;
        over = b.over; Kb = b.K * over; dw = 2.0 * kPi / Kb; kB = std::min ((int) (std::min (fmax, 0.45 * sr) * Kb / sr), kS);     // bands this bank has below 10 kHz
        for (int k = 0; k < kS; ++k) wk[(size_t) k] = k * dw;
        if (configured) clearFrames();          // frames made with the other filter are no use
        configured = true;
    }

    void clearFrames()
    {
        std::fill (Zr.begin(), Zr.end(), 0.0f); std::fill (Zi.begin(), Zi.end(), 0.0f);
        std::fill (Am.begin(), Am.end(), 0.0); std::fill (Um.begin(), Um.end(), 0.0); std::fill (Cm.begin(), Cm.end(), 0.0); std::fill (lastAng.begin(), lastAng.end(), 0.0);
        std::fill (Us.begin(), Us.end(), 0.0); std::fill (fsm.begin(), fsm.end(), 0.0); std::fill (ebm.begin(), ebm.end(), 0.0); std::fill (epm.begin(), epm.end(), 0.0);
    }

    void softReset()
    {
        for (size_t k = 0; k < mode.size(); ++k)
        {
            mode[k] = 1; omode[k] = 1; lead[k] = (int) k; olead[k] = (int) k; fade[k] = 0; flen[k] = 1; sjn[k] = 0; ojn[k] = 0;
            st0[k] = (double) t; sd0[k] = 0.0; psi[k] = 0.0; ot0[k] = (double) t; od0[k] = 0.0; opsi[k] = 0.0;
            rdirty[k] = 1; ordirty[k] = 1; evT[k] = -1; busy[k] = 0; J[k] = 0.0; Jc[k] = 0.0; cin[k] = 0; cout[k] = 0; lcnt[k] = 0; want[k] = (int) k; scnt[k] = 0; Js[k] = 0.0;
        }
        wait = t; quietUntil = 0; pendSwitch = -1; pend = -1; brActive = false; dirAct = false; dirFade = 0; brW = 0.0; brW0 = 0.0; smHold = -1;
    }

    // ---- analysis: one frame of every band's envelope
    void fft (bool inverse)
    {
        const int n = Kb;
        for (int i = 1, j = 0; i < n; ++i)
        {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) { std::swap (fftRe[(size_t) i], fftRe[(size_t) j]); std::swap (fftIm[(size_t) i], fftIm[(size_t) j]); }
        }
        for (int len = 2; len <= n; len <<= 1)
        {
            const int step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < len / 2; ++j)
                {
                    const double c = twC[(size_t) (j * step)], s = inverse ? twS[(size_t) (j * step)] : -twS[(size_t) (j * step)];
                    const size_t a = (size_t) (i + j), b = (size_t) (i + j + len / 2);
                    const double xr = fftRe[b] * c - fftIm[b] * s, xi = fftRe[b] * s + fftIm[b] * c;
                    fftRe[b] = fftRe[a] - xr; fftIm[b] = fftIm[a] - xi; fftRe[a] += xr; fftIm[a] += xi;
                }
        }
    }

    void analyzeFrame()
    {
        // band k centred at k sr / K:  Z_k = e^{-j w_k t} sum_n x[t - n] h[n] e^{+j w_k n}
        for (int q = 0; q < Kb; ++q)
        {
            double acc = 0.0;
            for (int n = q; n < L; n += Kb) acc += xring[(size_t) ((t - n) & (XN - 1))] * h[(size_t) n];
            fftRe[(size_t) q] = acc; fftIm[(size_t) q] = 0.0;
        }
        fft (true);
        const int64_t m = t / H; mNow = m; const size_t col = (size_t) (m & FM);
        const int tm = (int) (t % Kb);
        for (int k = 0; k < kB; ++k)
        {
            float zr = 0.0f, zi = 0.0f;
            if (k > 0)
            {
                const size_t ix = (size_t) ((k * tm) & (Kb - 1)); const double c = twC[ix], s = -twS[ix];
                const double ar = fftRe[(size_t) k], ai = fftIm[(size_t) k];
                zr = (float) (ar * c - ai * s); zi = (float) (ar * s + ai * c);
            }
            const size_t b = (size_t) k * 2 * NF;
            Zr[b + col] = zr; Zr[b + col + NF] = zr; Zi[b + col] = zi; Zi[b + col + NF] = zi;
            const double re = zr, im = zi, ang = std::atan2 (im, re);
            const size_t c1 = (size_t) k * NF + col, c0 = (size_t) k * NF + (size_t) ((m - 1) & FM);
            Am[c1] = std::sqrt (re * re + im * im);
            Cm[c1] = (m == 0 ? 0.0 : Cm[c0]) + re * re + im * im;                           // energy so far (for the readers' loudness correction)
            Um[c1] = m == 0 ? ang : Um[c0] + wrap (ang - lastAng[(size_t) k]);          // the phase, never wrapped back
            lastAng[(size_t) k] = ang;
            {
                // The same phase with its frame-to-frame advance smoothed and summed up again: what a plain band's phase advance is taken
                // from. A steady partial's is unchanged (and so is its long-run average: the tuning stays exact); the quick
                // swings of a weak second partial, of noise or of a beat's quiet moment are left out, so those are carried along with the
                // partial instead of being scaled with it.
                // A frame more than 10 dB under the band's recent loudness counts for less; a band that gets louder at once (from 3 dB above
                // its recent loudness, fully at 6 dB) has its new advance taken as it is (both in a sliding way, so that a last-digit
                // difference cannot tip anything). For 30 ms after an attack nothing is smoothed: while the click dies away in a band and
                // the partial takes over, the advance changes, and smoothing that change would leave the bands that share the partial a
                // little out of step. Where a frame or the one before it holds nothing at all (digital silence has no phase) the advance is
                // zero. Only the advance is ever used, so the smoothed phase starts at 0.
                const size_t kk = (size_t) k;
                if (m == 0) { Us[c1] = 0.0; fsm[kk] = 0.0; ebm[kk] = 0.0; epm[kk] = 0.0; }
                else
                {
                    const double e = re * re + im * im;
                    if (e <= 1e-24 || epm[kk] <= 1e-24) fsm[kk] = 0.0;
                    else
                    {
                        double g = smA; const double eb = ebm[kk];
                        if (e < 0.1 * eb) g = smA * e / (0.1 * eb);
                        if (e > 2.0 * eb) g = eb > 0.0 ? smA + (1.0 - smA) * std::min ((e - 2.0 * eb) / (2.0 * eb), 1.0) : 1.0;
                        if (t <= smHold) g = 1.0;
                        fsm[kk] += g * (Um[c1] - Um[c0] - fsm[kk]);
                    }
                    ebm[kk] += smE * (e - ebm[kk]); epm[kk] = e; Us[c1] = Us[c0] + fsm[kk];
                }
            }
        }
    }

    inline bool frameOk (int64_t i) const { return i >= 0 && i <= mNow && i > mNow - NF; }
    inline const float* zrp (int k, int64_t m) const { return &Zr[(size_t) k * 2 * NF + (size_t) (m & FM) + NF]; }      // p[-j] = frame m - j, j < NF
    inline const float* zip (int k, int64_t m) const { return &Zi[(size_t) k * 2 * NF + (size_t) (m & FM) + NF]; }

    inline void zat (int k, double p, double& re, double& im) const
    {
        const double q = p / H, fl = std::floor (q); const int64_t i = (int64_t) fl; const double f = q - fl;
        if (i < 1 || i + 2 > mNow || i - 1 <= mNow - NF) { re = 0.0; im = 0.0; return; }
        const float* zr = &Zr[(size_t) k * 2 * NF]; const float* zi = &Zi[(size_t) k * 2 * NF];
        const size_t ia = (size_t) ((i - 1) & FM), ib = (size_t) (i & FM), ic = (size_t) ((i + 1) & FM), id = (size_t) ((i + 2) & FM);
        const double ar = zr[ia], br = zr[ib], cr = zr[ic], dr = zr[id], ai = zi[ia], bi = zi[ib], ci = zi[ic], di = zi[id];
        re = br + 0.5 * f * (cr - ar + f * (2 * ar - 5 * br + 4 * cr - dr + f * (3 * (br - cr) + dr - ar)));
        im = bi + 0.5 * f * (ci - ai + f * (2 * ai - 5 * bi + 4 * ci - di + f * (3 * (bi - ci) + di - ai)));
    }

    inline const double* leadPhase() const { return Us.data(); }       // what a leader's phase advance is taken from: the smoothed phase

    inline double ulin (int k, double p) const
    {
        const double q = p / H, fl = std::floor (q); const int64_t i = (int64_t) fl; const double f = q - fl;
        if (! frameOk (i) || ! frameOk (i + 1)) return 0.0;
        const double* u = leadPhase(); const double a = u[(size_t) k * NF + (size_t) (i & FM)], b = u[(size_t) k * NF + (size_t) ((i + 1) & FM)];
        return a + f * (b - a);
    }

    // a reader plays material from up to a beat ago, which on a dying note is louder: scale it to the band's level now
    // (both levels taken over one beat = Jn frames), or every jump leaves a small step in loudness
    inline double readerGain (int k, double p, double tt, int Jn) const
    {
        if (Jn <= 0) return 1.0;
        const double qp = p / H, flp = std::floor (qp), fp = qp - flp, qn = (tt - flr) / H, fln = std::floor (qn), fn = qn - fln;
        const int64_t ip = (int64_t) flp, im = (int64_t) fln;
        if (! (frameOk (ip - Jn) && frameOk (ip + 1) && frameOk (im - Jn) && frameOk (im + 1))) return 1.0;
        const double* c = &Cm[(size_t) k * NF];
        auto at = [c] (int64_t i) { return c[(size_t) (i & FM)]; };
        const double ep = at (ip) + fp * (at (ip + 1) - at (ip)) - at (ip - Jn) - fp * (at (ip - Jn + 1) - at (ip - Jn));
        const double en = at (im) + fn * (at (im + 1) - at (im)) - at (im - Jn) - fn * (at (im - Jn + 1) - at (im - Jn));
        if (! (ep > 1e-24 && en > 0.0)) return 1.0;
        return std::min (2.0, std::max (0.5, std::sqrt (en / ep)));
    }

    // one stretch of band k's output at time tt, as a complex value. mode 1: loudness as it happens, phase = the band's
    // own + (ratio - 1) x its leader's. mode 0: a reader that was d0 behind at t0 and moves at `ratio`.
    inline void cx (int k, double tt, int md, double t0, double d0, double ps, int Ld, int Jn, double& re, double& im) const
    {
        if (md == 1)
        {
            const double p = tt - dpv, q = p / H, fl = std::floor (q); const int64_t i = (int64_t) fl; const double f = q - fl;
            if (! frameOk (i) || ! frameOk (i + 1)) { re = 0.0; im = 0.0; return; }
            const size_t c0 = (size_t) (i & FM), c1 = (size_t) ((i + 1) & FM), bk = (size_t) k * NF, bl = (size_t) Ld * NF;
            const double a = Am[bk + c0] + f * (Am[bk + c1] - Am[bk + c0]); const double* ul = leadPhase();
            const double ph = Um[bk + c0] + f * (Um[bk + c1] - Um[bk + c0]) + wk[(size_t) k] * (p - tauD)
                            + (ratio - 1.0) * (ul[bl + c0] + f * (ul[bl + c1] - ul[bl + c0]) + wk[(size_t) Ld] * (p - tauD)) + ps;
            re = a * std::cos (ph); im = a * std::sin (ph); return;
        }
        const double p = t0 - d0 + ratio * (tt - t0); double zr, zi; zat (k, p, zr, zi);
        const double ph = wk[(size_t) k] * (p - tauD) + ps, c = std::cos (ph), s = std::sin (ph), g = readerGain (k, p, tt, Jn);
        re = g * (zr * c - zi * s); im = g * (zr * s + zi * c);
    }

    // set a stretch's spinning vector exactly, for sample t
    inline void setVector (int k, int md, double t0, double d0, double ps, int Ld, double& c, double& sn, double& dc, double& ds, double& amp, double& damp) const
    {
        if (md == 1)
        {
            const double p = (double) t - dpv, q = p / H, fl = std::floor (q); const int64_t i = (int64_t) fl; const double f = q - fl;
            if (! frameOk (i) || ! frameOk (i + 1)) { c = 1.0; sn = 0.0; dc = 1.0; ds = 0.0; amp = 0.0; damp = 0.0; return; }
            const size_t c0 = (size_t) (i & FM), c1 = (size_t) ((i + 1) & FM), bk = (size_t) k * NF, bl = (size_t) Ld * NF;
            const double* lp = leadPhase();
            const double a0 = Am[bk + c0], a1 = Am[bk + c1], uk = Um[bk + c1] - Um[bk + c0], ul = lp[bl + c1] - lp[bl + c0];
            amp = a0 + f * (a1 - a0); damp = (a1 - a0) / H;
            const double ph = Um[bk + c0] + f * uk + wk[(size_t) k] * (p - tauD) + (ratio - 1.0) * (lp[bl + c0] + f * ul + wk[(size_t) Ld] * (p - tauD)) + ps;
            const double sl = uk / H + wk[(size_t) k] + (ratio - 1.0) * (ul / H + wk[(size_t) Ld]);
            c = std::cos (ph); sn = std::sin (ph); dc = std::cos (sl); ds = std::sin (sl); return;
        }
        const double p = t0 - d0 + ratio * ((double) t - t0), ph = wk[(size_t) k] * (p - tauD) + ps;
        c = std::cos (ph); sn = std::sin (ph); dc = stC[(size_t) k]; ds = stS[(size_t) k]; amp = 0.0; damp = 0.0;
    }

    // one sample of a stretch (doubled: the band and its mirror image), then turn the vector on
    inline double stepVector (int k, int md, double t0, double d0, int Jn, double& c, double& sn, double dc, double ds, double& amp, double damp) const
    {
        double v;
        if (md == 1) { v = 2.0 * amp * c; amp += damp; }
        else
        {
            const double p = t0 - d0 + ratio * ((double) t - t0); double zr, zi; zat (k, p, zr, zi);
            v = 2.0 * readerGain (k, p, (double) t, Jn) * (zr * c - zi * sn);
        }
        const double c2 = c * dc - sn * ds; sn = c * ds + sn * dc; c = c2;
        return v;
    }

    inline double readDirect (double p) const
    {
        const double fl = std::floor (p); const int64_t i = (int64_t) fl; const double f = p - fl;
        if (i < 1) return 0.0;
        const double a = xring[(size_t) ((i - 1) & (XN - 1))], b = xring[(size_t) (i & (XN - 1))], c = xring[(size_t) ((i + 1) & (XN - 1))], e = xring[(size_t) ((i + 2) & (XN - 1))];
        return b + 0.5 * f * (c - a + f * (2 * a - 5 * b + 4 * c - e + f * (3 * (b - c) + e - a)));
    }

    // a new stretch for band k from time te on (cross-faded from the running one over flenNew samples)
    inline void emit (int k, int64_t te, int md, double d, double ps, int Ld, int Jn, int flenNew)
    {
        const size_t i = (size_t) k;
        if (evT[i] < te) { omode[i] = mode[i]; ot0[i] = st0[i]; od0[i] = sd0[i]; opsi[i] = psi[i]; olead[i] = lead[i]; ojn[i] = sjn[i]; ordirty[i] = 1; }     // (same instant: replaces it)
        rdirty[i] = 1;
        mode[i] = md; st0[i] = (double) te; sd0[i] = d; psi[i] = ps; lead[i] = Ld; sjn[i] = Jn; flen[i] = std::max (flenNew, 1); fade[i] = flen[i]; evT[i] = te;
    }

    // the turn that lines a new stretch of band k up with the running one over the last M*step samples of output
    double align (int k, int mo, double to, double dOld, double po, int lo, int jo, int mn, double dn, int ln, int jn, double step) const
    {
        double cr = 0.0, ci = 0.0;
        for (int j = 0; j < M; ++j)
        {
            const double tt = (double) t - j * step; double ar, ai, br, bi;
            cx (k, tt, mo, to, dOld, po, lo, jo, ar, ai); cx (k, tt, mn, (double) t, dn, 0.0, ln, jn, br, bi);
            cr += ar * br + ai * bi; ci += ai * br - ar * bi;
        }
        return std::atan2 (ci, cr);
    }

    // where to put band k's reader (within dn0 + lo .. dn0 + hi, nc trials and one more in between) and with what
    // turn: where the new stretch lines up best with the band's own running stretch and with the neighbours' output
    // (at the phase the bands have between them in the input)
    void place (int k, int mn, double dn0, int jn, double lo, double hi, int nc, double step, int WlP, double& bd, double& bp)
    {
        const int64_t m0 = t / H; double ir[2] = { 0.0, 0.0 }, ii[2] = { 0.0, 0.0 }; const int nbs[2] = { k - 1, k + 1 };
        WlP = std::min (WlP, NF - 4);
        for (int q = 0; q < 2; ++q)
        {
            const int a = nbs[q];
            if (a < 1 || a >= kmax || m0 - WlP < 0) continue;
            const float* ar_ = zrp (a, m0); const float* ai_ = zip (a, m0); const float* br_ = zrp (k, m0); const float* bi_ = zip (k, m0);
            double sr_ = 0.0, si_ = 0.0, ea = 1e-30, eb = 1e-30;
            for (int j = 0; j < WlP; ++j)
            {
                const double zar = ar_[-j], zai = ai_[-j], zbr = br_[-j], zbi = bi_[-j];
                const double pr = zar * zbr + zai * zbi, pi_ = zai * zbr - zar * zbi, ang = (a - k) * dw * ((double) (m0 - j) * H - tauD), c = std::cos (ang), sn = std::sin (ang);
                sr_ += pr * c - pi_ * sn; si_ += pr * sn + pi_ * c; ea += zar * zar + zai * zai; eb += zbr * zbr + zbi * zbi;
            }
            const double nrm = std::sqrt (sr_ * sr_ + si_ * si_);
            if (nrm > 1e-30) { const double cw = nrm / std::sqrt (ea * eb); ir[q] = sr_ / nrm * cw; ii[q] = si_ / nrm * cw; }
        }
        double best = -1.0; bd = dn0; bp = 0.0; int bi0 = 0; const size_t kk = (size_t) k;
        nc = std::min (nc, 30);
        // what the new stretch is compared with at each of the M moments (it does not depend on the trial position):
        // the band's own running stretch plus the neighbours' output turned to the phase the bands have in the input
        for (int j = 0; j < M; ++j)
        {
            const double tt = (double) t - j * step; double ar, ai;
            cx (k, tt, mode[kk], st0[kk], sd0[kk], psi[kk], lead[kk], sjn[kk], ar, ai);
            for (int q = 0; q < 2; ++q)
            {
                if (ir[q] == 0.0 && ii[q] == 0.0) continue;
                const size_t a = (size_t) nbs[q]; double nr, ni;
                cx ((int) a, tt, mode[a], st0[a], sd0[a], psi[a], lead[a], sjn[a], nr, ni);
                ar += nr * ir[q] + ni * ii[q]; ai += ni * ir[q] - nr * ii[q];           // (n conj(I)): summed with the own stretch, the same total as before
            }
            pvR[(size_t) j] = ar; pvI[(size_t) j] = ai;
        }
        for (int ci_ = 0; ci_ <= nc; ++ci_)
        {
            double dn;
            if (ci_ < nc) dn = dn0 + (nc > 1 ? lo + (hi - lo) * ci_ / (nc - 1) : 0.0);
            else
            {
                if (nc < 3 || bi0 == 0 || bi0 == nc - 1) break;
                const double den = mags[(size_t) bi0 - 1] - 2.0 * mags[(size_t) bi0] + mags[(size_t) bi0 + 1];
                if (den > -1e-30) break;
                dn = bd + 0.5 * (mags[(size_t) bi0 - 1] - mags[(size_t) bi0 + 1]) / den * (hi - lo) / (nc - 1);
            }
            double xr = 0.0, xi = 0.0;
            for (int j = 0; j < M; ++j)
            {
                const double tt = (double) t - j * step; double br, bi;
                cx (k, tt, mn, (double) t, dn, 0.0, k, jn, br, bi);
                xr += pvR[(size_t) j] * br + pvI[(size_t) j] * bi; xi += pvI[(size_t) j] * br - pvR[(size_t) j] * bi;
            }
            const double mag = xr * xr + xi * xi; mags[(size_t) ci_] = mag;
            if (ci_ < nc) { if (mag > best) { best = mag; bd = dn; bp = std::atan2 (xi, xr); bi0 = ci_; } }
            else if (mag >= best) { bd = dn; bp = std::atan2 (xi, xr); }
        }
    }

    // what band k's likeness-by-lag curve says: 0 = never falls (one partial), 1 = falls and does not come back,
    // 2 = falls and comes back: Jf = the first lag (frames) at which it is back = the repeat of the band's envelope
    // (over the first n lags of the curve)
    int repeat (int k, int n, double& Jf, double& cf) const
    {
        const double* c = &CO[(size_t) k * (size_t) nl]; const double drop = mode[(size_t) k] == 1 ? dropIn : dropOut;      // (a band already on a reader stays on it down to a shallower dip)
        double top = c[0]; int id = -1;
        for (int i = 0; i < n; ++i) { if (c[i] > top) top = c[i]; if (c[i] < top - drop) { id = i; break; } }
        Jf = -1.0; cf = top;
        if (id < 0) return 0;
        double cmin = c[id], g = -1.0;
        for (int i = id; i < n; ++i) if (c[i] < cmin) cmin = c[i];
        for (int i = id + 1; i < n - 1; ++i) if (c[i] > c[i - 1] && c[i] >= c[i + 1] && c[i] > g) g = c[i];
        cf = g;
        if (g < cabs || g < top - (1.0 - rho) * (top - cmin)) return 1;
        for (int i = id + 1; i < n - 1; ++i)
            if (c[i] > c[i - 1] && c[i] >= c[i + 1] && c[i] >= g - tol2)
            {
                const double den = c[i - 1] - 2.0 * c[i] + c[i + 1]; double off = 0.0;
                if (den < -1e-9) off = 0.5 * (c[i - 1] - c[i + 1]) / den;
                Jf = lmin + i + off; cf = c[i]; return 2;
            }
        return 1;
    }

    void nearPeak (int k, double Jq, double& Jn, double& cn) const
    {
        const double* c = &CO[(size_t) k * (size_t) nl];
        const int i = (int) std::nearbyint (Jq) - lmin; Jn = -1.0; cn = 0.0;
        if (i < 1 || i > nl - 2) return;
        int b = i;
        for (int s = -2; s <= 2; ++s) { const int q = i + s; if (q >= 1 && q <= nl - 2 && c[q] > c[b]) b = q; }
        cn = c[b];
        if (c[b] < c[b - 1] || c[b] < c[b + 1]) return;
        const double den = c[b - 1] - 2.0 * c[b] + c[b + 1]; double off = 0.0;
        if (den < -1e-9) off = 0.5 * (c[b - 1] - c[b + 1]) / den;
        Jn = lmin + b + off;
    }

    // ---- an attack
    void onAttack()
    {
        const int64_t ts = t; wait = ts + hold; smHold = ts + (int64_t) tau - pre + settle;      // (the smoothing of the phase advance rests this long)
        if (usebr)
        {
            const int64_t tnew = ts + (int64_t) std::ceil ((tauD + flr - dA0) / drift);
            pendDelay = dA0 + (double) (tnew - ts) * drift - tauD; pendSwitch = tnew;
            for (int k = 0; k < kS; ++k) { const size_t i = (size_t) k; J[i] = 0.0; cin[i] = 0; cout[i] = 0; busy[i] = tnew; lcnt[i] = 0; }
            if (brActive)                                        // already bridging: carry on, re-started from here
            {
                if (t >= br1) { brW0 = brW; br0 = ts; }          // (it was fading out: fade back in from where it is)
                br1 = tnew;
                if (dirAct) { dirPOld = dirP; dirFade = std::max (xf / 2, 2); }
            }
            else { brActive = true; br0 = ts; br1 = tnew; brW0 = 0.0; dirFade = 0; }
            dirP = (double) ts - dA0; dirAct = true;
            quietUntil = std::max (ts + settle, tnew);
        }
        else
        {
            const int64_t nextHop = ((ts + hop - 1) / hop) * hop;
            pend = std::max (ts + (int64_t) tau - pre, nextHop);
        }
    }

    // shifting up: just before an attack reaches the bands, every band's phase is set equal to the input's
    void phaseReset()
    {
        const double pr = (double) (pend - H);
        for (int k = 1; k < kS; ++k)
        {
            const size_t i = (size_t) k;
            const double ps = wrap ((1.0 - ratio) * (ulin (k, pr) + wk[i] * (pr - tauD)));
            emit (k, pend, 1, 0.0, ps, k, 0, xa); busy[i] = pend + xa; cin[i] = 0; cout[i] = 0; J[i] = 0.0; lcnt[i] = 0;
        }
        quietUntil = pend + settle; pend = -1;
    }

    // ---- every hop: which bands beat (and with what repeat), reader jumps, who follows whom
    void control()
    {
        const int64_t m0 = t / H;
        if (m0 - lmax - Wn * ws - 2 < 0) return;
        double emax = 0.0;
        for (int k = 1; k < kmax; ++k)
        {
            const float* zr = zrp (k, m0); const float* zi = zip (k, m0); double e = 0.0;
            for (int j = 0; j < Wn; ++j) { const double a = zr[-j * ws], b = zi[-j * ws]; e += a * a + b * b; }
            E0[(size_t) k] = e; if (e > emax) emax = e;
        }
        on[0] = 0;
        for (int k = 1; k < kmax; ++k) on[(size_t) k] = (E0[(size_t) k] > efloor * emax && E0[(size_t) k] > 1e-13) ? 1 : 0;
        // how alike each band's newest stretch and the one `lag` frames earlier are (a common turn and gain allowed).
        // The window takes every second frame, so the band's recent frames are laid out as two straight runs (even and
        // odd steps back from now) and every lag is a plain dot product of two of them.
        const int nq = Wn + lmax / 2 + 2;
        for (int k = 1; k < kmax; ++k)
        {
            if (! on[(size_t) k]) continue;
            const float* zr = zrp (k, m0); const float* zi = zip (k, m0); double* c = &CO[(size_t) k * (size_t) nl];
            float* er = runR.data(); float* ei = runI.data(); float* orr = runR.data() + nq; float* oi = runI.data() + nq;
            for (int j = 0; j < nq; ++j) { er[j] = zr[-2 * j]; ei[j] = zi[-2 * j]; orr[j] = zr[-2 * j - 1]; oi[j] = zi[-2 * j - 1]; }
            float e0f = 0.0f;
            for (int j = 0; j < Wn; ++j) e0f += er[j] * er[j] + ei[j] * ei[j];
            const double e0 = (double) e0f + 1e-30;
            for (int i = 0; i < nl; ++i)
            {
                const int l = lmin + i, q = l >> 1; const float* br = (l & 1) ? orr + q : er + q; const float* bi = (l & 1) ? oi + q : ei + q;
                float cr = 0.0f, ci = 0.0f, e1f = 0.0f;
                for (int j = 0; j < Wn; ++j) { cr += er[j] * br[j] + ei[j] * bi[j]; ci += ei[j] * br[j] - er[j] * bi[j]; e1f += br[j] * br[j] + bi[j] * bi[j]; }
                const double e1 = (double) e1f + 1e-30;
                double v = std::sqrt (((double) cr * cr + (double) ci * ci) / (e0 * e1));
                if (e1 < 0.1 * e0 || e1 > 10.0 * e0) v = 0.0;
                c[i] = v;
            }
        }
        for (int k = 1; k < kmax; ++k)
        {
            const size_t i = (size_t) k; int st = 0; double Jf = -1.0, cf = 0.0;
            if (on[i])
            {
                if (slj > 0.0)
                {
                    // A slow beat (longer than slj) is a matter of trust: two steady partials 10 to 20 Hz apart repeat exactly, but a real note's
                    // partial is itself a cluster that wanders and looks like a slow beat for a moment, and a reader jumping 50 to 100 ms on it only
                    // adds flutter. So a slow repeat counts once it has been there (a peak of the curve at that lag, as good as the best) for slq
                    // times its own length, or when the band is already on it; until then the band goes by what the curve says within slj.
                    st = repeat (k, nl, Jf, cf);
                    if (st == 2)
                    {
                        double cn = cf;
                        if (scnt[i] > 0)                         // the slow repeat being watched: is its peak still there, as good as the best?
                        {
                            double Jn = -1.0; nearPeak (k, Js[i], Jn, cn);
                            if (Jn > 0.0 && cn >= cf - tol2) { ++scnt[i]; Js[i] = Jn; } else scnt[i] = 0;
                        }
                        if (Jf * H > slj)
                        {
                            if (scnt[i] == 0) { scnt[i] = 1; Js[i] = Jf; cn = cf; }
                            if ((double) scnt[i] >= slq * Js[i] * H / hop) { Jf = Js[i]; cf = cn; }
                            else if (! (mode[i] == 0 && J[i] > slj)) st = repeat (k, nls, Jf, cf);
                        }
                    }
                    else scnt[i] = 0;
                }
                else st = repeat (k, nl, Jf, cf);
            }
            if (st == 2)
            {
                if (cin[i] > 0 && std::abs (Jf - Jc[i]) <= 0.08 * Jc[i]) ++cin[i]; else cin[i] = 1;
                Jc[i] = Jf;
            }
            else cin[i] = 0;
            if (mode[i] == 1)
            {
                if (cin[i] >= nIn && t >= quietUntil && t >= busy[i] && pend < 0)
                {
                    const double Jk = Jc[i] * H, stp = std::max (Jk, wmin) / ratio / M; double dn = up ? flr + Jk : flr, ps; const int jnk = std::max ((int) std::nearbyint (Jk / H), 1);
                    if (placeMode == 1 || placeMode == 2)       // anywhere within one beat: where the neighbours' shares fit
                    {
                        const int wl = std::max ((int) std::nearbyint (Jk / H), 8);
                        place (k, 0, dn, jnk, 0.0, Jk * 15.0 / 16.0, 16, stp, wl, dn, ps);
                        place (k, 0, dn, jnk, -Jk / 24.0, Jk / 24.0, 5, stp, wl, dn, ps);
                        if (dn < flr) dn = flr;
                    }
                    else ps = align (k, 1, st0[i], 0.0, psi[i], lead[i], 0, 0, dn, k, jnk, stp);
                    emit (k, t, 0, dn, ps, k, jnk, xf); J[i] = Jk; busy[i] = t + xf; cout[i] = 0; lcnt[i] = 0;
                }
                continue;
            }
            const double dk = sd0[i] + drift * ((double) t - st0[i]);            // a reader: how far behind it is now
            if (J[i] > 0.0)
            {
                bool keep = false;
                if (st == 2)
                {
                    if (std::abs (Jf * H - J[i]) <= 0.08 * J[i]) { J[i] = Jf * H; keep = true; }
                    else
                    {
                        double Jn, cn; nearPeak (k, J[i] / H, Jn, cn);
                        if (Jn > 0.0 && cn >= cf - tol2) { J[i] = Jn * H; keep = true; }
                        else if (cin[i] >= nIn) { J[i] = Jf * H; keep = true; }
                    }
                }
                if (keep) cout[i] = 0; else ++cout[i];
            }
            else                                                 // free-running after the bridge: no jump needed yet. It waits twait to see whether
            {                                                    // the band beats (long enough to see a beat three times; a time, not a distance)
                if (cin[i] >= nIn) { J[i] = Jc[i] * H; cout[i] = 0; }
                else if ((double) t - st0[i] > twait || ! on[i]) cout[i] = nOut;
            }
            const bool must = up && dk - (ratio - 1.0) * hop <= flr;             // shifting up: the reader is about to reach "now"
            if ((cout[i] >= nOut && t >= busy[i]) || (must && (J[i] <= 0.0 || cout[i] > 0)))
            {
                const double ps = align (k, 0, st0[i], sd0[i], psi[i], k, sjn[i], 1, 0.0, k, 0, std::max (J[i], wmin) / ratio / M);
                emit (k, t, 1, 0.0, ps, k, 0, xfu); J[i] = 0.0; busy[i] = t + xfu; cout[i] = 0; cin[i] = 0; lcnt[i] = 0;
                continue;
            }
            if (J[i] > 0.0)
            {
                double dn = dk;
                if (up) { if (must) dn = dk + J[i]; }
                else if (t >= busy[i]) { while (dn - J[i] >= flr) dn -= J[i]; }
                if (dn != dk)
                {
                    const double stp = std::max (J[i], wmin) / ratio / M; double ps; const int jnk = std::max ((int) std::nearbyint (J[i] / H), 1);
                    if (placeMode >= 2)                          // the jump, give or take an eighth of a beat, part of the way: keeps the shares in step
                    {
                        double lo = -J[i] / 8.0, dq;
                        if (dn + lo < flr) lo = flr - dn;
                        place (k, 0, dn, jnk, lo, J[i] / 8.0, 9, stp, std::max ((int) std::nearbyint (J[i] / H), 8), dq, ps);
                        if (pgain < 1.0) { dn = dn + pgain * (dq - dn); ps = align (k, 0, st0[i], sd0[i], psi[i], k, sjn[i], 0, dn, k, jnk, stp); }
                        else dn = dq;
                    }
                    else ps = align (k, 0, st0[i], sd0[i], psi[i], k, sjn[i], 0, dn, k, jnk, stp);
                    emit (k, t, 0, dn, ps, k, jnk, xfu); busy[i] = t + xfu;
                }
            }
        }
        // shares of one partial in neighbouring bands: the weaker band follows the stronger one's phase
        if (m0 - Wl - 1 < 0) return;
        for (int j = 0; j < Wl; ++j) { const double ang = -dw * ((double) (m0 - j) * H - tauD); lockC[(size_t) j] = std::cos (ang); lockS[(size_t) j] = std::sin (ang); }
        for (int k = 1; k < kmax - 1; ++k)                       // how steady the phase between band k and k+1 has been
        {
            const float* ar_ = zrp (k, m0); const float* ai_ = zip (k, m0); const float* br_ = zrp (k + 1, m0); const float* bi_ = zip (k + 1, m0);
            double sr_ = 0.0, si_ = 0.0, nn = 1e-30;
            for (int j = 0; j < Wl; ++j)
            {
                const double ar = ar_[-j], ai = ai_[-j], br = br_[-j], bi = bi_[-j], pr = ar * br + ai * bi, pi_ = ai * br - ar * bi;
                sr_ += pr * lockC[(size_t) j] - pi_ * lockS[(size_t) j]; si_ += pr * lockS[(size_t) j] + pi_ * lockC[(size_t) j];
                nn += std::sqrt ((ar * ar + ai * ai) * (br * br + bi * bi));
            }
            cpair[(size_t) k] = std::sqrt (sr_ * sr_ + si_ * si_) / nn;
        }
        for (int k = 1; k < kmax; ++k)
        {
            const size_t i = (size_t) k; want[i] = k;
            if (mode[i] != 1 || ! on[i]) continue;
            double best = E0[i]; const int lk = lead[i];
            if (k > 1 && mode[i - 1] == 1 && on[i - 1] && cpair[i - 1] >= (((lk == k - 1 || lk == lead[i - 1]) && lk != k) ? clock - 0.15 : clock) && E0[i - 1] > best) { want[i] = k - 1; best = E0[i - 1]; }
            if (k + 1 < kmax && mode[i + 1] == 1 && on[i + 1] && cpair[i] >= (((lk == k + 1 || lk == lead[i + 1]) && lk != k) ? clock - 0.15 : clock) && E0[i + 1] > best) want[i] = k + 1;
        }
        for (int k = 1; k < kmax; ++k)
        {
            const size_t i = (size_t) k;
            if (mode[i] != 1) continue;
            int root = k;
            for (int it = 0; it < 8; ++it) { if (want[(size_t) root] == root) break; root = want[(size_t) root]; }
            const size_t ld = (size_t) lead[i];
            const bool stale = lead[i] != k && (mode[ld] != 1 || lead[ld] != lead[i]);
            if (root != lead[i]) ++lcnt[i]; else lcnt[i] = 0;
            if ((lcnt[i] >= 2 || stale) && root != lead[i] && t >= busy[i])
            {
                if (root != k && lead[(size_t) root] == root && mode[(size_t) root] == 1)
                {
                    emit (k, t, 1, 0.0, psi[(size_t) root], root, 0, xfu); busy[i] = t + xfu; lcnt[i] = 0;
                }
                else if (root == k || stale)                     // on its own again: carry on from where it is
                {
                    const double pp = (double) t - dpv;
                    const double ps = wrap ((ratio - 1.0) * (ulin ((int) ld, pp) + wk[ld] * (pp - tauD) - ulin (k, pp) - wk[i] * (pp - tauD)) + psi[i]);
                    emit (k, t, 1, 0.0, ps, k, 0, xfu); busy[i] = t + xfu; lcnt[i] = 0;
                }
            }
            else if (lead[i] != k && ! stale && psi[i] != psi[ld] && t >= busy[i])    // the leader was given a new turn
            {
                emit (k, t, 1, 0.0, psi[ld], lead[i], 0, xfu); busy[i] = t + xfu;
            }
        }
    }
};

} // namespace polypitch
