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

// Built with -DPOLYPITCH_PROFILE (tools/polypitch_profile.cpp does) the engine keeps the time it spends in each stage of
// its work. No other build has any of it: the two macros below then stand for nothing.
// (a promise to the compiler that two arrays are not the same one, so that it can work through them several bands at a time)
#if defined (__GNUC__) || defined (__clang__) || defined (_MSC_VER)
 #define POLYPITCH_RESTRICT __restrict
#else
 #define POLYPITCH_RESTRICT
#endif
#ifdef POLYPITCH_PROFILE
 #include <chrono>
 #define POLYPITCH_STAGE_START(s) StageTimer stageTimer (stageTime, s)
 #define POLYPITCH_STAGE_NEXT(s) stageTimer.next (s)
#else
 #define POLYPITCH_STAGE_START(s)
 #define POLYPITCH_STAGE_NEXT(s)
#endif

namespace polypitch
{

class Engine
{
public:
#ifdef POLYPITCH_PROFILE
    // seconds spent, by stage. sPlacement is part of sDecisions (fitting a reader to its neighbours); the rest do not overlap.
    enum Stage { sWindow, sTransform, sFrameBands, sCurves, sDecisions, sPlacement, sAttacks, sLoopFrameStart, sLoop, numStages };
    mutable double stageTime[numStages] = {};
    static double stageNow() { return std::chrono::duration<double> (std::chrono::steady_clock::now().time_since_epoch()).count(); }
    struct StageTimer
    {
        double* acc; int s; double t0;
        StageTimer (double* a, int st) : acc (a), s (st), t0 (stageNow()) {}
        void next (int st) { const double n = stageNow(); acc[s] += n - t0; s = st; t0 = n; }
        ~StageTimer() { acc[s] += stageNow() - t0; }
    };
#endif

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hopf = std::max ((int) std::lround (2.9 * sr / 1000.0 / H), 1); hop = hopf * H;
        Wn = std::max ((int) std::lround (24.0 * sr / 1000.0 / H / ws), 2);
        lmin = std::max ((int) std::lround (2.2 * sr / 1000.0 / H), 1);
        lmax = std::min ((int) (100.0 * sr / 1000.0 / H), NF / 3); nl = lmax - lmin + 1;
        nPar = (hopf & 1) ? 2 : 1;                                                    // (checks fall on even frames only, unless a hop is an odd number of frames)
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
        rframes.assign (n, ReaderFrames()); rdGen = 0; imSeen = -1;
        tPh.assign (n, 0.0); tSl.assign (n, 0.0); tC.assign (n, 0.0); tS.assign (n, 0.0); tDc.assign (n, 0.0); tDs.assign (n, 0.0);
        rai.assign (n, 0.0); orai.assign (n, 0.0); bandV.assign (n, 0.0); rdList.assign (n, 0); fdList.assign (n, 0); fti.assign (n, -1); nRd = 0; nFd = 0; dirty = true;
        {
            // the cross-fade from an old stretch to a new one, for each length a fade can have: as much of the old one as there is left
            const int fl[4] = { xf, std::max (xf / 2, 2), xa, 1 }; int off = 0;
            for (int q = 0; q < 4; ++q) { fadeLen[q] = fl[q]; fadeOff[q] = off; off += fl[q] + 1; }
            fadeG.assign ((size_t) off, 0.0);
            for (int q = 0; q < 4; ++q) for (int j = 0; j <= fl[q]; ++j) { const double g = (double) j / (double) fl[q]; fadeG[(size_t) (fadeOff[q] + j)] = 0.5 - 0.5 * std::cos (kPi * g); }
        }
        E0.assign (n, 0.0); on.assign (n, 0); cpair.assign (n, 0.0); wk.assign (n, 0.0); CO.assign (n * (size_t) nl, 0.0); scnt.assign (n, 0); Js.assign (n, 0.0);
        Lr.assign ((size_t) nPar * n * (size_t) nl, 0.0); Li.assign ((size_t) nPar * n * (size_t) nl, 0.0); Ei.assign (n * 2 * NW, 0.0); Ew.assign (n * 2, 0.0); Lpk.assign ((size_t) nPar * n, 0.0);
        fftRe.assign ((size_t) KMAX, 0.0); fftIm.assign ((size_t) KMAX, 0.0);
        lockC.assign ((size_t) NF, 0.0); lockS.assign ((size_t) NF, 0.0); mags.assign (32, 0.0);
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
        // shifting up: twice the bands (half a band apart, same filter), a finer threshold for calling a band "beating", and a band on a reader stays longer.
        // By less than a fifth up a reader is worth less (what a plain band gets wrong on the weaker of two partials d Hz apart is (ratio - 1) d Hz) and costs
        // more (it plays its band up to a beat late until its next jump, which is a long way off): there a band leaves a reader whose beat has gone after 3
        // checks, as shifting down.
        const bool small = up && ratio - 1.0 < 0.45;
        dropIn = up ? 0.001 : 0.004; dropOut = up ? 0.00025 : dropIn; nOut = up && ! small ? 8 : 3;
        // in both directions: a beat slower than 50 ms has to hold before a reader takes it (and a plain band's phase advance is smoothed over 5 ms: analyzeFrame).
        // Shifting up by less than a fifth that starts sooner: wherever the reader would put right less than 10 Hz, from 20 ms on.
        slj = (small ? std::min (std::max (100.0 * (ratio - 1.0), 20.0), 50.0) : 50.0) * sr / 1000.0; nls = std::min (std::max ((int) (slj / H) - lmin + 2, 3), nl);
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
        t = 0;
        clearFrames();
        std::fill (onsBuf.begin(), onsBuf.end(), 0.0);
        ef = 0.0; pkh = 0.0; onsJ = 0;
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

        if (flag && t >= wait) { POLYPITCH_STAGE_START (sAttacks); onAttack(); }
        if (pendSwitch >= 0 && t >= pendSwitch)
        {
            POLYPITCH_STAGE_START (sAttacks);
            // the bands take over from the direct reader: all at one position, where they are the same signal
            for (int k = 0; k < kS; ++k) emit (k, t, 0, pendDelay, 0.0, k, 0, 1);
            pendSwitch = -1;
        }
        if ((t % hop) == 0 && pendSwitch < 0) control();
        if (pend >= 0 && t >= pend) { POLYPITCH_STAGE_START (sAttacks); phaseReset(); }

        // the bands. Between frames a plain band's loudness and phase run in straight lines, and a reader's carrier turns
        // at a fixed rate, so each stretch is a spinning vector that is set exactly once per frame (and when it changes)
        const bool tick = ((t - 1) % H) == 0;
        POLYPITCH_STAGE_START (tick ? sLoopFrameStart : sLoop);
        if (tick)                                                 // a new frame is in: every vector is set afresh, all the sines and cosines in one go
        {
            nRd = 0;
            for (int k = 1; k < kmax; ++k)
            {
                const size_t i = (size_t) k; vectorAngles (k, mode[i], st0[i], sd0[i], psi[i], lead[i], tPh[i], tSl[i], ramp[i], rdamp[i]);
                rai[i] = 0.0; rdirty[i] = 0; rframes[i].gen = -1;
                if (mode[i] == 0) rdList[(size_t) nRd++] = k;
            }
            if (kmax > 1) { sincosMany (tPh.data() + 1, rc.data() + 1, rs.data() + 1, kmax - 1); sincosMany (tSl.data() + 1, rdc.data() + 1, rds.data() + 1, kmax - 1); }
            for (int n = 0; n < nRd; ++n) { const size_t i = (size_t) rdList[(size_t) n]; rdc[i] = stC[i]; rds[i] = stS[i]; }
            for (int n = 0; n < nFd; ++n)                       // and the old stretches still fading out
            {
                const size_t i = (size_t) fdList[(size_t) n]; vectorAngles ((int) i, omode[i], ot0[i], od0[i], opsi[i], olead[i], tPh[(size_t) n], tSl[(size_t) n], oramp[i], ordamp[i]);
                orai[i] = 0.0; ordirty[i] = 0;
            }
            if (nFd > 0) { sincosMany (tPh.data(), tC.data(), tS.data(), nFd); sincosMany (tSl.data(), tDc.data(), tDs.data(), nFd); }
            for (int n = 0; n < nFd; ++n)
            {
                const size_t i = (size_t) fdList[(size_t) n]; orc[i] = tC[(size_t) n]; ors[i] = tS[(size_t) n];
                if (omode[i] == 1) { ordc[i] = tDc[(size_t) n]; ords[i] = tDs[(size_t) n]; } else { ordc[i] = stC[i]; ords[i] = stS[i]; }
            }
            dirty = false;
        }
        else if (dirty)                                           // between frames: only the stretches that have just changed
        {
            nRd = 0;
            for (int k = 1; k < kmax; ++k)
            {
                const size_t i = (size_t) k;
                if (rdirty[i]) { setVector (k, mode[i], st0[i], sd0[i], psi[i], lead[i], rc[i], rs[i], rdc[i], rds[i], ramp[i], rdamp[i]); rai[i] = 0.0; rdirty[i] = 0; rframes[i].gen = -1; }
                if (mode[i] == 0) rdList[(size_t) nRd++] = k;
            }
            dirty = false;
        }
        // the readers: what each one reads at this sample
        if (nRd > 0)
        {
            const double qn = ((double) t - flr) / H, fln = std::floor (qn), fn = qn - fln; const int64_t im = (int64_t) fln;      // (the band's level now is taken here: readerGain)
            if (im != imSeen) { imSeen = im; ++rdGen; }
            for (int n = 0; n < nRd; ++n)
            {
                const int k = rdList[(size_t) n]; const size_t i = (size_t) k; ReaderFrames& c = rframes[i];
                const double p = st0[i] - sd0[i] + ratio * ((double) t - st0[i]), q = p / H, fl = std::floor (q), f = q - fl; const int64_t ip = (int64_t) fl;
                if (ip != c.i || c.gen != rdGen) fetchReader (k, ip, im, sjn[i], c);
                double zr = 0.0, zi = 0.0, g = 1.0;
                if (c.okZ)
                {
                    zr = c.zr[1] + 0.5 * f * (c.zr[2] - c.zr[0] + f * (2 * c.zr[0] - 5 * c.zr[1] + 4 * c.zr[2] - c.zr[3] + f * (3 * (c.zr[1] - c.zr[2]) + c.zr[3] - c.zr[0])));
                    zi = c.zi[1] + 0.5 * f * (c.zi[2] - c.zi[0] + f * (2 * c.zi[0] - 5 * c.zi[1] + 4 * c.zi[2] - c.zi[3] + f * (3 * (c.zi[1] - c.zi[2]) + c.zi[3] - c.zi[0])));
                }
                if (c.okG)
                {
                    const double ep = c.ep[0] + f * (c.ep[1] - c.ep[0]) - c.ep[2] - f * (c.ep[3] - c.ep[2]), en = c.en[0] + fn * (c.en[1] - c.en[0]) - c.en[2] - fn * (c.en[3] - c.en[2]);
                    if (ep > 1e-24 && en > 0.0) g = std::min (2.0, std::max (0.5, std::sqrt (en / ep)));
                }
                ramp[i] = g * zr; rai[i] = g * zi;
            }
        }
        // every band: one sample of its stretch (doubled: the band and its mirror image), then its vector turned on. The same few
        // sums for each band, with nothing to decide: the compiler takes several bands at a time.
        double y = 0.0;
        {
            double* POLYPITCH_RESTRICT c = rc.data(); double* POLYPITCH_RESTRICT sn = rs.data(); double* POLYPITCH_RESTRICT ar = ramp.data(); double* POLYPITCH_RESTRICT vo = bandV.data();
            const double* POLYPITCH_RESTRICT dc = rdc.data(); const double* POLYPITCH_RESTRICT ds = rds.data(); const double* POLYPITCH_RESTRICT ai = rai.data(); const double* POLYPITCH_RESTRICT da = rdamp.data();
            for (int k = 1; k < kmax; ++k)
            {
                const double cc = c[k], ss = sn[k], vk = 2.0 * (ar[k] * cc - ai[k] * ss);
                vo[k] = vk; y += vk; c[k] = cc * dc[k] - ss * ds[k]; sn[k] = cc * ds[k] + ss * dc[k]; ar[k] += da[k];
            }
        }
        // a band that has just changed: its old stretch fades out under the new one
        for (int n = 0; n < nFd; )
        {
            const int k = fdList[(size_t) n]; const size_t i = (size_t) k;
            if (ordirty[i]) { setVector (k, omode[i], ot0[i], od0[i], opsi[i], olead[i], orc[i], ors[i], ordc[i], ords[i], oramp[i], ordamp[i]); orai[i] = 0.0; ordirty[i] = 0; }
            if (omode[i] == 0) readerNow (k, ot0[i], od0[i], ojn[i], oramp[i], orai[i]);
            const double cc = orc[i], ss = ors[i], ov = 2.0 * (oramp[i] * cc - orai[i] * ss);
            orc[i] = cc * ordc[i] - ss * ords[i]; ors[i] = cc * ords[i] + ss * ordc[i]; oramp[i] += ordamp[i];
            const double g = fti[i] >= 0 ? fadeG[(size_t) (fti[i] + fade[i])] : 0.5 - 0.5 * std::cos (kPi * (double) fade[i] / (double) flen[i]);
            y += g * (ov - bandV[i]);
            if (--fade[i] == 0) fdList[(size_t) n] = fdList[(size_t) --nFd]; else ++n;
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
    static constexpr int KMAX = 1024, H = 32, NF = 512, FM = NF - 1, XN = 8192, ws = 2, M = 16, nIn = 3, xa = 48, NW = 256;
    static constexpr double fmax = 10000.0, efloor = 1e-5, rho = 0.7, cabs = 0.9, tol2 = 0.01, clock = 0.95;

    double sr = 0.0, semis = -12.0, ratio = 0.5, drift = 0.5, flr = 66.0, dpv = 33.0, dA0 = 44.1, twait = 1764.0, wmin = 529.2, dw = 0.0, pgain = 0.5, tauD = 529.0, dropIn = 0.004, dropOut = 0.004;
    double smA = 0.0, smE = 0.0, slj = 0.0, slq = 0.5;
    bool up = false, unity = false, usebr = true, prepared = false, configured = false, designed = false;
    int tau = 529, L = 3176, hopf = 4, hop = 128, Wn = 17, lmin = 3, lmax = 137, nl = 135, xf = 117, xfu = 117, hold = 529, settle = 1102, pre = 88, Wl = 16, kmax = 116, kS = 116, placeMode = 3, Kb = 512, over = 1, nOut = 3, kB = 116, nls = 135, nPar = 1;
    int fresh[2] = { 1, 1 };
    int64_t sumsFor[2] = { 0, 1 };
    std::vector<double> rc, rs, rdc, rds, ramp, rdamp, orc, ors, ordc, ords, oramp, ordamp, stC, stS;
    std::vector<double> rai, orai, bandV, fadeG, tPh, tSl, tC, tS, tDc, tDs;
    std::vector<int> rdList, fdList, fti;                    // the bands on readers; the bands in a cross-fade; where each band's fade is in fadeG
    int nRd = 0, nFd = 0, fadeLen[4] = { 1, 1, 1, 1 }, fadeOff[4] = { 0, 0, 0, 0 };
    bool dirty = true;
    // what a reader reads changes frame only every H / ratio samples: the frames it is between (and the energies its loudness is
    // scaled by) are kept here, per band, and fetched again when it moves on to the next frame, or when anything they hang on changes
    struct ReaderFrames { int64_t i = -1, gen = -1; double zr[4] = {}, zi[4] = {}, ep[4] = {}, en[4] = {}; bool okZ = false, okG = false; };
    std::vector<ReaderFrames> rframes;
    int64_t rdGen = 0, imSeen = -1;
    std::vector<char> rdirty, ordirty;
    struct Bank { std::vector<double> h, twC, twS; int K = 512, over = 1, tau = 0, L = 0; bool designed = false; };      // one band filter and its FFT twiddles
    Bank banks[4];                                                    // [0] shifting down; shifting up: [1] fast, [2] balanced, [3] clean. Made in prepare()
    int response = 0, loaded = -1;
    int bankFor() const { return up ? 1 + response : 0; }
    const double* h = nullptr; const double* twC = nullptr; const double* twS = nullptr;                // the ones in use
    std::vector<double> xring, fftRe, fftIm, Am, Um, Us, fsm, ebm, epm, Cm, lastAng, st0, sd0, psi, ot0, od0, opsi, J, Jc, Js, E0, cpair, wk, CO, onsBuf, lockC, lockS, mags;
    std::vector<float> Zr, Zi;
    std::vector<double> pvR, pvI, Lr, Li, Lpk, Ew, Ei;
    double fitT[M] = {}, fitC[M] = {}, fitS[M] = {}, fitEn[M] = {}, fitOr[M] = {}, fitOi[M] = {}, fitVr[M] = {}, fitVi[M] = {}, fitWr[M] = {}, fitWi[M] = {};      // what a fit works with (fitPrepare)
    bool fitOk[M] = {};
    int lockN = 0;
    std::vector<int> mode, omode, lead, olead, fade, flen, cin, cout, lcnt, want, on, sjn, ojn, scnt;
    std::vector<int64_t> evT, busy;
    int64_t t = 0, mNow = -1, wait = 0, quietUntil = 0, pendSwitch = -1, pend = -1, br0 = 0, br1 = 0, smHold = -1;
    double pendDelay = 0.0, ef = 0.0, pkh = 0.0, afc = 0.0, rel = 0.0, dirP = 0.0, dirPOld = 0.0, brW = 0.0, brW0 = 0.0;
    int onsD = 132, onsJ = 0, dirFade = 0;
    bool brActive = false, dirAct = false;

    static inline double wrap (double a) { return a - 2.0 * kPi * std::round (a / (2.0 * kPi)); }

    // cos and sin of n angles, several at a time. Where the machine multiplies and adds in one step (FP_FAST_FMA) the angle is
    // brought into -pi/4 .. pi/4 by taking out a whole number of quarter turns (pi/2 in three parts, so that nothing is lost
    // however large the angle) and a polynomial does the rest: within two units in the last place of the library's cos and sin
    // for angles up to 1e13. Elsewhere the library's own functions are used.
    static inline void sincosMany (const double* POLYPITCH_RESTRICT x, double* POLYPITCH_RESTRICT c, double* POLYPITCH_RESTRICT s, int n)
    {
       #if defined (FP_FAST_FMA) || defined (__FP_FAST_FMA)
        for (int i = 0; i < n; ++i)
        {
            const double v = x[i], kf = std::round (v * 0.6366197723675814);
            double r = std::fma (-kf, 1.5707963267948966, v); r = std::fma (-kf, 6.123233995736766e-17, r); r = std::fma (-kf, -1.4973849048591698e-33, r);
            const double z = r * r;
            const double ps = r + r * z * (-1.66666666666666324348e-01 + z * (8.33333333332248946124e-03 + z * (-1.98412698298579493134e-04 + z * (2.75573137070700676789e-06 + z * (-2.50507602534068634195e-08 + z * 1.58969099521155010221e-10)))));
            const double pc = 1.0 - 0.5 * z + z * z * (4.16666666666666019037e-02 + z * (-1.38888888888741095749e-03 + z * (2.48015872894767294178e-05 + z * (-2.75573143513906633035e-07 + z * (2.08757232129817482790e-09 + z * -1.13596475577881948265e-11)))));
            const int64_t q = (int64_t) kf; const bool odd = (q & 1) != 0;
            const double a = odd ? pc : ps, b = odd ? ps : pc;
            s[i] = (q & 2) != 0 ? -a : a; c[i] = ((q + 1) & 2) != 0 ? -b : b;
        }
       #else
        for (int i = 0; i < n; ++i) { c[i] = std::cos (x[i]); s[i] = std::sin (x[i]); }
       #endif
    }

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
        std::fill (Lr.begin(), Lr.end(), 0.0); std::fill (Li.begin(), Li.end(), 0.0); std::fill (Lpk.begin(), Lpk.end(), 0.0); std::fill (Ew.begin(), Ew.end(), 0.0); std::fill (Ei.begin(), Ei.end(), 1e30);
        ++rdGen;
        const int64_t mNext = (t + H - 1) / H;                     // with no frames behind them the sums are right (zero) for the next frame of either kind
        for (int c = 0; c < 2; ++c) { fresh[c] = 1; sumsFor[c] = mNext + ((mNext ^ c) & 1); }
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
        nRd = 0; nFd = 0; dirty = true; ++rdGen;
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
        POLYPITCH_STAGE_START (sWindow);
        // band k centred at k sr / K:  Z_k = e^{-j w_k t} sum_n x[t - n] h[n] e^{+j w_k n}
        for (int q = 0; q < Kb; ++q)
        {
            double acc = 0.0;
            for (int n = q; n < L; n += Kb) acc += xring[(size_t) ((t - n) & (XN - 1))] * h[(size_t) n];
            fftRe[(size_t) q] = acc; fftIm[(size_t) q] = 0.0;
        }
        POLYPITCH_STAGE_NEXT (sTransform);
        fft (true);
        POLYPITCH_STAGE_NEXT (sFrameBands);
        const int64_t m = t / H; mNow = m; ++rdGen; const size_t col = (size_t) (m & FM);
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
            {
                // the band's energy over the comparison window that ends at this frame (Wn frames, every second one) as a running sum, and
                // one over it kept for the last NW frames: what the likeness curves are scaled by (control)
                const double qr = Zr[b + col + NF - 2 * Wn], qi = Zi[b + col + NF - 2 * Wn]; const size_t wb = (size_t) k * 2 * NW, wc = (size_t) (m & (NW - 1));
                double& w = Ew[(size_t) k * 2 + (size_t) (m & 1)]; w += (re * re + im * im) - (qr * qr + qi * qi);
                const double r = 1.0 / (std::max (w, 0.0) + 1e-30); Ei[wb + wc] = r; Ei[wb + wc + NW] = r;
            }
        }
        POLYPITCH_STAGE_NEXT (sCurves);
        if ((int) (m & 1) < nPar) slideCurves (m, (int) (m & 1), (t % hop) == 0);
    }

    // The likeness curves are built as the frames arrive. Band k's curve at lag l is, before scaling, a sum over the comparison
    // window (Wn frames, every second one, ending at the frame m of the check):
    //     sum over j = 0 .. Wn - 1 of  z[m - 2j] conj (z[m - 2j - l])
    // The check two frames later needs the same sum with one term more and one term less. So the sums are kept running, in double
    // precision (each term is a product of single-precision numbers and so exact): Lr / Li hold, for the frame `sumsFor`, the sum
    // without its newest term (j = 1 .. Wn - 1), which needs nothing of that frame. At a check control() adds the newest term
    // for the bands that are awake and scales. All the rest is done here, on a frame that has no check, so that it never falls
    // into the same 64 samples as the decisions; it is the same work whatever the bands hold.
    // A running sum keeps the rounding of everything that has passed through it, which is nothing beside a loud band and would be
    // something beside the same band 100 dB quieter. So a band is summed afresh, from the frames themselves, when its level has
    // fallen 40 dB below the highest it has been since the last time (at most 16 bands in one call, so that a sudden silence is
    // no lump of work either), and one band per call in turn whatever its level, so that nothing wrong can stay in a sum for good
    // (a number that is not a number, say).
    // Lr / Li [u] hold lag lmax - u, so that the loops run forwards through the frames.
    void slideCurves (int64_t m, int pc, bool check)
    {
        const int64_t target = check ? m : m + 2;                   // at a check the sums have to be those for this frame; otherwise for the next one
        if (sumsFor[pc] >= target) return;
        const int back = 2 * Wn - 2; int spare = 16;
        if (fresh[pc] >= kB) fresh[pc] = 1;
        for (int k = 1; k < kB; ++k)
        {
            const size_t kc = (size_t) pc * (size_t) kS + (size_t) k; double* sr = &Lr[kc * (size_t) nl]; double* si = &Li[kc * (size_t) nl];
            const double wNow = Ew[(size_t) k * 2 + (size_t) (m & 1)];                              // the band's level: its energy over the window that ends here
            bool afresh = k == fresh[pc];
            if (! afresh && wNow < 1e-4 * Lpk[kc] && spare > 0) { afresh = true; --spare; }
            if (afresh)
            {
                std::fill (sr, sr + nl, 0.0); std::fill (si, si + nl, 0.0);
                for (int j = 1; j < Wn; ++j)
                {
                    const float* pr = zrp (k, target - 2 * j); const float* pi = zip (k, target - 2 * j); const float* xr = pr - lmax; const float* xi = pi - lmax;
                    const double nr = pr[0], ni = pi[0];
                    for (int u = 0; u < nl; ++u) { const double a = xr[u], b = xi[u]; sr[u] += nr * a + ni * b; si[u] += ni * a - nr * b; }
                }
                const float* pr = zrp (k, m); const float* pi = zip (k, m);
                for (int q = 0; q < 2; ++q)                                                        // and its window energy, at this frame and the one before
                {
                    double w = 0.0;
                    for (int j = 0; j < Wn; ++j) { const double a = pr[-q - 2 * j], b = pi[-q - 2 * j]; w += a * a + b * b; }
                    const size_t wb = (size_t) k * 2 * NW, wc = (size_t) ((m - q) & (NW - 1)); const double r = 1.0 / (w + 1e-30);
                    Ew[(size_t) k * 2 + (size_t) ((m - q) & 1)] = w; Ei[wb + wc] = r; Ei[wb + wc + NW] = r;
                    if (q == 0) Lpk[kc] = w;
                }
                continue;
            }
            if (wNow > Lpk[kc]) Lpk[kc] = wNow;
            for (int64_t f = sumsFor[pc]; f < target; f += 2)                                          // frame f comes in, frame f - back goes out
            {
                const float* pr = zrp (k, f); const float* pi = zip (k, f);                           // p[-j] = frame f - j
                const double nr = pr[0], ni = pi[0], qr = pr[-back], qi = pi[-back];
                if (nr == 0.0 && ni == 0.0 && qr == 0.0 && qi == 0.0) continue;                       // (silence coming in and going out: nothing changes)
                const float* xr = pr - lmax; const float* xi = pi - lmax; const float* yr = xr - back; const float* yi = xi - back;
                for (int u = 0; u < nl; ++u)
                {
                    const double a = xr[u], b = xi[u], c = yr[u], d = yi[u]; double vr = sr[u], vi = si[u];
                    vr += nr * a; vr += ni * b; vr -= qr * c; vr -= qi * d; sr[u] = vr;
                    vi += ni * a; vi -= nr * b; vi -= qi * c; vi += qr * d; si[u] = vi;
                }
            }
        }
        ++fresh[pc]; sumsFor[pc] = target;
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

    // A stretch of band k's output is one of two things. Plain: the band's loudness as it happens, and as phase the band's own
    // + (ratio - 1) x its leader's. A reader: the band's envelope as it was d0 behind at t0, read on at `ratio`, on a carrier.
    // A plain stretch of band k (leader Ld, turn ps) at time tt: its loudness and its phase. False where there are no frames.
    inline bool plainAt (int k, double tt, double ps, int Ld, double& a, double& ph) const
    {
        const double p = tt - dpv, q = p / H, fl = std::floor (q); const int64_t i = (int64_t) fl; const double f = q - fl;
        if (! frameOk (i) || ! frameOk (i + 1)) return false;
        const size_t c0 = (size_t) (i & FM), c1 = (size_t) ((i + 1) & FM), bk = (size_t) k * NF, bl = (size_t) Ld * NF;
        a = Am[bk + c0] + f * (Am[bk + c1] - Am[bk + c0]); const double* ul = leadPhase();
        ph = Um[bk + c0] + f * (Um[bk + c1] - Um[bk + c0]) + wk[(size_t) k] * (p - tauD)
           + (ratio - 1.0) * (ul[bl + c0] + f * (ul[bl + c1] - ul[bl + c0]) + wk[(size_t) Ld] * (p - tauD)) + ps;
        return true;
    }

    // a stretch's spinning vector at sample t: where its phase stands (ph) and, for a plain stretch, how far it turns per sample
    // (sl; a reader's carrier turns at its own fixed rate: stC, stS), its loudness and how that changes per sample
    inline void vectorAngles (int k, int md, double t0, double d0, double ps, int Ld, double& ph, double& sl, double& amp, double& damp) const
    {
        if (md == 1)
        {
            const double p = (double) t - dpv, q = p / H, fl = std::floor (q); const int64_t i = (int64_t) fl; const double f = q - fl;
            if (! frameOk (i) || ! frameOk (i + 1)) { ph = 0.0; sl = 0.0; amp = 0.0; damp = 0.0; return; }
            const size_t c0 = (size_t) (i & FM), c1 = (size_t) ((i + 1) & FM), bk = (size_t) k * NF, bl = (size_t) Ld * NF;
            const double* lp = leadPhase();
            const double a0 = Am[bk + c0], a1 = Am[bk + c1], uk = Um[bk + c1] - Um[bk + c0], ul = lp[bl + c1] - lp[bl + c0];
            amp = a0 + f * (a1 - a0); damp = (a1 - a0) / H;
            ph = Um[bk + c0] + f * uk + wk[(size_t) k] * (p - tauD) + (ratio - 1.0) * (lp[bl + c0] + f * ul + wk[(size_t) Ld] * (p - tauD)) + ps;
            sl = uk / H + wk[(size_t) k] + (ratio - 1.0) * (ul / H + wk[(size_t) Ld]); return;
        }
        const double p = t0 - d0 + ratio * ((double) t - t0);
        ph = wk[(size_t) k] * (p - tauD) + ps; sl = 0.0; amp = 0.0; damp = 0.0;
    }

    // set one stretch's spinning vector exactly, for sample t
    inline void setVector (int k, int md, double t0, double d0, double ps, int Ld, double& c, double& sn, double& dc, double& ds, double& amp, double& damp) const
    {
        double ph, sl; vectorAngles (k, md, t0, d0, ps, Ld, ph, sl, amp, damp);
        c = std::cos (ph); sn = std::sin (ph);
        if (md == 1) { dc = std::cos (sl); ds = std::sin (sl); } else { dc = stC[(size_t) k]; ds = stS[(size_t) k]; }
    }

    // the frames a reader of band k is between when it stands in frame ip (zat), and the energies its loudness is scaled by (readerGain)
    void fetchReader (int k, int64_t ip, int64_t im, int Jn, ReaderFrames& c) const
    {
        c.i = ip; c.gen = rdGen;
        c.okZ = ! (ip < 1 || ip + 2 > mNow || ip - 1 <= mNow - NF);
        if (c.okZ)
        {
            const float* zr = &Zr[(size_t) k * 2 * NF]; const float* zi = &Zi[(size_t) k * 2 * NF];
            for (int j = 0; j < 4; ++j) { const size_t ix = (size_t) ((ip - 1 + j) & FM); c.zr[j] = zr[ix]; c.zi[j] = zi[ix]; }
        }
        c.okG = Jn > 0 && frameOk (ip - Jn) && frameOk (ip + 1) && frameOk (im - Jn) && frameOk (im + 1);
        if (c.okG)
        {
            const double* cm = &Cm[(size_t) k * NF]; auto at = [cm] (int64_t i) { return cm[(size_t) (i & FM)]; };
            c.ep[0] = at (ip); c.ep[1] = at (ip + 1); c.ep[2] = at (ip - Jn); c.ep[3] = at (ip - Jn + 1);
            c.en[0] = at (im); c.en[1] = at (im + 1); c.en[2] = at (im - Jn); c.en[3] = at (im - Jn + 1);
        }
    }

    // what a reader reads at this sample: the band's envelope where the reader is, scaled to the band's level now
    inline void readerNow (int k, double t0, double d0, int Jn, double& ar, double& ai) const
    {
        const double p = t0 - d0 + ratio * ((double) t - t0); double zr, zi; zat (k, p, zr, zi);
        const double g = readerGain (k, p, (double) t, Jn); ar = g * zr; ai = g * zi;
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
        rdirty[i] = 1; dirty = true;
        if (fade[i] == 0 && k >= 1 && k < kmax) fdList[(size_t) nFd++] = k;
        mode[i] = md; st0[i] = (double) te; sd0[i] = d; psi[i] = ps; lead[i] = Ld; sjn[i] = Jn; flen[i] = std::max (flenNew, 1); fade[i] = flen[i]; evT[i] = te;
        fti[i] = -1;
        for (int q = 0; q < 4; ++q) if (flen[i] == fadeLen[q]) { fti[i] = fadeOff[q]; break; }
    }

    // ---- fitting a new stretch of band k to what is running. Everything is compared at M moments of the output, t - j step
    // (not just at the splice, where a beat's quiet moment can mislead).

    // band k's stretch (md, t0, d0, ps, Ld, Jn) at those moments, as complex values. A plain stretch is worked out moment by
    // moment. A reader's carrier turns by the same angle from each moment to the next, so one sine and cosine do for all of them.
    void moments (int k, int md, double t0, double d0, double ps, int Ld, int Jn, double step, double* vr, double* vi) const
    {
        if (md == 1)
        {
            for (int j = 0; j < M; ++j)
            {
                double a, ph;
                if (plainAt (k, (double) t - j * step, ps, Ld, a, ph)) { vr[j] = a * std::cos (ph); vi[j] = a * std::sin (ph); } else { vr[j] = 0.0; vi[j] = 0.0; }
            }
            return;
        }
        const double w = wk[(size_t) k], ph = w * (t0 - d0 + ratio * ((double) t - t0) - tauD) + ps, dph = -w * ratio * step, dc = std::cos (dph), ds = std::sin (dph);
        double c = std::cos (ph), s = std::sin (ph);
        for (int j = 0; j < M; ++j)
        {
            const double tt = (double) t - j * step, p = t0 - d0 + ratio * (tt - t0), g = readerGain (k, p, tt, Jn); double zr, zi; zat (k, p, zr, zi);
            vr[j] = g * (zr * c - zi * s); vi[j] = g * (zr * s + zi * c);
            const double c2 = c * dc - s * ds; s = c * ds + s * dc; c = c2;
        }
    }

    // What stays the same wherever a new reader of band k (its beat jn frames long) is tried: the moments, the turn of its carrier
    // from the first moment to each of them, the band's level now at each (a reader's loudness is scaled to it: readerGain), and
    // the band's own running stretch.
    void fitPrepare (int k, int jn, double step)
    {
        POLYPITCH_STAGE_START (sPlacement);
        const size_t kk = (size_t) k; const double dph = -wk[kk] * ratio * step, dc = std::cos (dph), ds = std::sin (dph); double c = 1.0, s = 0.0;
        const double* cm = &Cm[kk * NF]; auto at = [cm] (int64_t i) { return cm[(size_t) (i & FM)]; };
        for (int j = 0; j < M; ++j)
        {
            const double tt = (double) t - j * step, qn = (tt - flr) / H, fln = std::floor (qn), fn = qn - fln; const int64_t im = (int64_t) fln;
            fitT[j] = tt; fitC[j] = c; fitS[j] = s; fitOk[j] = frameOk (im - jn) && frameOk (im + 1);
            fitEn[j] = fitOk[j] ? at (im) + fn * (at (im + 1) - at (im)) - at (im - jn) - fn * (at (im - jn + 1) - at (im - jn)) : 0.0;
            const double c2 = c * dc - s * ds; s = c * ds + s * dc; c = c2;
        }
        moments (k, mode[kk], st0[kk], sd0[kk], psi[kk], lead[kk], sjn[kk], step, fitOr, fitOi);
    }

    // What a new reader is placed against: the band's own running stretch plus the neighbours' output, each neighbour turned to
    // the phase the two bands have between them in the input (over the last WlP frames) and weighted by what they have in common.
    void fitNeighbours (int k, double step, int WlP)
    {
        POLYPITCH_STAGE_START (sPlacement);
        const int64_t m0 = t / H; const int nbs[2] = { k - 1, k + 1 }; WlP = std::min (WlP, NF - 4);
        for (; lockN < WlP; ++lockN) { lockC[(size_t) lockN] = lockC[(size_t) (lockN - Kb / H)]; lockS[(size_t) lockN] = lockS[(size_t) (lockN - Kb / H)]; }     // (the turn between two bands comes round every Kb / H frames)
        for (int j = 0; j < M; ++j) { pvR[(size_t) j] = fitOr[j]; pvI[(size_t) j] = fitOi[j]; }
        for (int q = 0; q < 2; ++q)
        {
            const int a = nbs[q];
            if (a < 1 || a >= kmax || m0 - WlP < 0) continue;
            const float* ar_ = zrp (a, m0); const float* ai_ = zip (a, m0); const float* br_ = zrp (k, m0); const float* bi_ = zip (k, m0);
            const double sg = a < k ? 1.0 : -1.0; double sr_ = 0.0, si_ = 0.0, ea = 1e-30, eb = 1e-30;
            for (int j = 0; j < WlP; ++j)
            {
                const double zar = ar_[-j], zai = ai_[-j], zbr = br_[-j], zbi = bi_[-j], pr = zar * zbr + zai * zbi, pi_ = zai * zbr - zar * zbi, c = lockC[(size_t) j], sn = sg * lockS[(size_t) j];
                sr_ += pr * c - pi_ * sn; si_ += pr * sn + pi_ * c; ea += zar * zar + zai * zai; eb += zbr * zbr + zbi * zbi;
            }
            const double nrm = std::sqrt (sr_ * sr_ + si_ * si_);
            if (! (nrm > 1e-30)) continue;
            const double cw = nrm / std::sqrt (ea * eb), ir = sr_ / nrm * cw, ii = si_ / nrm * cw;
            if (ir == 0.0 && ii == 0.0) continue;
            const size_t aa = (size_t) a; moments (a, mode[aa], st0[aa], sd0[aa], psi[aa], lead[aa], sjn[aa], step, fitVr, fitVi);
            for (int j = 0; j < M; ++j) { pvR[(size_t) j] += fitVr[j] * ir + fitVi[j] * ii; pvI[(size_t) j] += fitVi[j] * ir - fitVr[j] * ii; }      // (n conj(I))
        }
    }

    // A new reader of band k, d behind now, against a reference (wr, wi: already turned back by the carrier's turn at each
    // moment): the sum over the moments of reference x conj (the reader's value there, without its carrier). Its size says how
    // well the two line up; fitTurn makes the turn out of it.
    inline void fitTry (int k, int jn, double d, const double* wr, const double* wi, double& yr, double& yi) const
    {
        const double* cm = &Cm[(size_t) k * NF]; auto at = [cm] (int64_t i) { return cm[(size_t) (i & FM)]; }; double sr_ = 0.0, si_ = 0.0;
        for (int j = 0; j < M; ++j)
        {
            const double p = (double) t - d + ratio * (fitT[j] - (double) t); double zr, zi, g = 1.0; zat (k, p, zr, zi);
            if (fitOk[j])
            {
                const double qp = p / H, flp = std::floor (qp), fp = qp - flp; const int64_t ip = (int64_t) flp;
                if (frameOk (ip - jn) && frameOk (ip + 1))
                {
                    const double ep = at (ip) + fp * (at (ip + 1) - at (ip)) - at (ip - jn) - fp * (at (ip - jn + 1) - at (ip - jn)), en = fitEn[j];
                    if (ep > 1e-24 && en > 0.0) g = std::min (2.0, std::max (0.5, std::sqrt (en / ep)));
                }
            }
            sr_ += g * (wr[j] * zr + wi[j] * zi); si_ += g * (wi[j] * zr - wr[j] * zi);
        }
        yr = sr_; yi = si_;
    }

    inline double fitTurn (int k, double d, double yr, double yi) const
    {
        const double th = wk[(size_t) k] * ((double) t - d - tauD), c = std::cos (th), s = std::sin (th);                // the carrier at the first moment
        return std::atan2 (yi * c - yr * s, yr * c + yi * s);
    }

    // where to put band k's reader (within dn0 + lo .. dn0 + hi, nc trials and one more in between) and with what turn: where it
    // lines up best with what fitNeighbours laid out
    void fitSearch (int k, int jn, double dn0, double lo, double hi, int nc, double& bd, double& bp)
    {
        POLYPITCH_STAGE_START (sPlacement);
        for (int j = 0; j < M; ++j) { const double pr = pvR[(size_t) j], pi_ = pvI[(size_t) j]; fitWr[j] = pr * fitC[j] + pi_ * fitS[j]; fitWi[j] = pi_ * fitC[j] - pr * fitS[j]; }
        double best = -1.0, byr = 0.0, byi = 0.0; bd = dn0; int bi0 = 0; bool any = false;
        nc = std::min (nc, 30);
        for (int ci_ = 0; ci_ <= nc; ++ci_)
        {
            double dn;
            if (ci_ < nc) dn = dn0 + (nc > 1 ? lo + (hi - lo) * ci_ / (nc - 1) : 0.0);
            else                                                 // once more, between the grid points: where a parabola through the best three peaks
            {
                if (nc < 3 || bi0 == 0 || bi0 == nc - 1) break;
                const double den = mags[(size_t) bi0 - 1] - 2.0 * mags[(size_t) bi0] + mags[(size_t) bi0 + 1];
                if (den > -1e-30) break;
                dn = bd + 0.5 * (mags[(size_t) bi0 - 1] - mags[(size_t) bi0 + 1]) / den * (hi - lo) / (nc - 1);
            }
            double yr, yi; fitTry (k, jn, dn, fitWr, fitWi, yr, yi);
            const double mag = yr * yr + yi * yi; mags[(size_t) ci_] = mag;
            if (ci_ < nc) { if (mag > best) { best = mag; bd = dn; byr = yr; byi = yi; bi0 = ci_; any = true; } }
            else if (mag >= best) { bd = dn; byr = yr; byi = yi; any = true; }
        }
        bp = any ? fitTurn (k, bd, byr, byi) : 0.0;
    }

    // the turn that lines a new reader of band k, dn behind now, up with the band's own running stretch
    double fitAlign (int k, int jn, double dn)
    {
        POLYPITCH_STAGE_START (sPlacement);
        for (int j = 0; j < M; ++j) { fitWr[j] = fitOr[j] * fitC[j] + fitOi[j] * fitS[j]; fitWi[j] = fitOi[j] * fitC[j] - fitOr[j] * fitS[j]; }
        double yr, yi; fitTry (k, jn, dn, fitWr, fitWi, yr, yi);
        return fitTurn (k, dn, yr, yi);
    }

    // the turn that lines a new plain stretch of band k (leading itself) up with the band's running stretch
    double alignPlain (int k, double step)
    {
        POLYPITCH_STAGE_START (sPlacement);
        const size_t kk = (size_t) k; double cr = 0.0, ci = 0.0;
        moments (k, mode[kk], st0[kk], sd0[kk], psi[kk], lead[kk], sjn[kk], step, fitOr, fitOi); moments (k, 1, (double) t, 0.0, 0.0, k, 0, step, fitVr, fitVi);
        for (int j = 0; j < M; ++j) { cr += fitOr[j] * fitVr[j] + fitOi[j] * fitVi[j]; ci += fitOi[j] * fitVr[j] - fitOr[j] * fitVi[j]; }
        return std::atan2 (ci, cr);
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
        // the lowest point from the fall on, and the highest peak after it (two lags abreast, so that neither search waits on itself)
        double cmin = c[n - 1] < c[id] ? c[n - 1] : c[id], g = -1.0;
        {
            double m1 = cmin, g1 = -1.0; int i = id + 1;
            for (; i + 1 < n - 1; i += 2)
            {
                const double a = c[i - 1], b = c[i], d = c[i + 1], e = c[i + 2];
                if (b < cmin) cmin = b;
                if (d < m1) m1 = d;
                if (b > a && b >= d && b > g) g = b;
                if (d > b && d >= e && d > g1) g1 = d;
            }
            for (; i < n - 1; ++i) { const double b = c[i]; if (b < cmin) cmin = b; if (b > c[i - 1] && b >= c[i + 1] && b > g) g = b; }
            if (m1 < cmin) cmin = m1;
            if (g1 > g) g = g1;
        }
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
        POLYPITCH_STAGE_START (sCurves);
        double emax = 0.0;
        for (int k = 1; k < kmax; ++k)
        {
            const float* zr = zrp (k, m0); const float* zi = zip (k, m0); double e = 0.0;
            for (int j = 0; j < Wn; ++j) { const double a = zr[-j * ws], b = zi[-j * ws]; e += a * a + b * b; }
            E0[(size_t) k] = e; if (e > emax) emax = e;
        }
        on[0] = 0;
        for (int k = 1; k < kmax; ++k) on[(size_t) k] = (E0[(size_t) k] > efloor * emax && E0[(size_t) k] > 1e-13) ? 1 : 0;
        // how alike each band's newest stretch and the one `lag` frames earlier are (a common turn and gain allowed): the running
        // sums (slideCurves) with the newest frame's term added, scaled by the energies of the two stretches
        const size_t pc = (size_t) (m0 & 1) < (size_t) nPar ? (size_t) (m0 & 1) : 0;
        for (int k = 1; k < kmax; ++k)
        {
            if (! on[(size_t) k]) continue;
            const double* sr = &Lr[(pc * (size_t) kS + (size_t) k) * (size_t) nl]; const double* si = &Li[(pc * (size_t) kS + (size_t) k) * (size_t) nl];
            const float* pr = zrp (k, m0); const float* pi = zip (k, m0); const float* xr = pr - lmax; const float* xi = pi - lmax; const double nr = pr[0], ni = pi[0];
            const double* ri = &Ei[(size_t) k * 2 * NW + (size_t) (m0 & (NW - 1)) + NW] - lmax;     // ri[u] = 1 / the window energy lmax - u frames ago
            double* c = &CO[(size_t) k * (size_t) nl] + (nl - 1); const double r0 = 1.0 / (E0[(size_t) k] + 1e-30), rHi = 10.0 * r0, rLo = 0.1 * r0;
            for (int u = 0; u < nl; ++u)
            {
                const double a = xr[u], b = xi[u], r1 = ri[u]; double cr = sr[u], ci = si[u];
                cr += nr * a; cr += ni * b; ci += ni * a; ci -= nr * b;
                const double v = std::sqrt ((cr * cr + ci * ci) * (r0 * r1));
                c[-u] = (r1 > rHi || r1 < rLo) ? 0.0 : v;                                            // (0: the two stretches more than 10 dB apart in level)
            }
        }
        POLYPITCH_STAGE_NEXT (sDecisions);
        // the turn of phase between two neighbouring bands at each of the last frames (the followers below and fitNeighbours use it)
        lockN = std::max (Wl, Kb / H);
        for (int j = 0; j < lockN; ++j) { const double ang = -dw * ((double) (m0 - j) * H - tauD); lockC[(size_t) j] = std::cos (ang); lockS[(size_t) j] = std::sin (ang); }
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
                        fitPrepare (k, jnk, stp); fitNeighbours (k, stp, std::max ((int) std::nearbyint (Jk / H), 8));
                        fitSearch (k, jnk, dn, 0.0, Jk * 15.0 / 16.0, 16, dn, ps);
                        fitSearch (k, jnk, dn, -Jk / 24.0, Jk / 24.0, 5, dn, ps);
                        if (dn < flr) dn = flr;
                    }
                    else { fitPrepare (k, jnk, stp); ps = fitAlign (k, jnk, dn); }
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
                const double ps = alignPlain (k, std::max (J[i], wmin) / ratio / M);
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
                        fitPrepare (k, jnk, stp); fitNeighbours (k, stp, std::max ((int) std::nearbyint (J[i] / H), 8));
                        fitSearch (k, jnk, dn, lo, J[i] / 8.0, 9, dq, ps);
                        if (pgain < 1.0) { dn = dn + pgain * (dq - dn); ps = fitAlign (k, jnk, dn); }
                        else dn = dq;
                    }
                    else { fitPrepare (k, jnk, stp); ps = fitAlign (k, jnk, dn); }
                    emit (k, t, 0, dn, ps, k, jnk, xfu); busy[i] = t + xfu;
                }
            }
        }
        // shares of one partial in neighbouring bands: the weaker band follows the stronger one's phase
        if (m0 - Wl - 1 < 0) return;
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

#undef POLYPITCH_STAGE_START
#undef POLYPITCH_STAGE_NEXT
#undef POLYPITCH_RESTRICT
