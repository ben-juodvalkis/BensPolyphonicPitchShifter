// PolyPitch processor: the whole effect around the engine, with no host or framework dependencies.
// Used as is by the plug-in (plugin/) and by the Max object (max/).
//
//   - the inputs are summed to mono for the shifter (the shifted sound is mono, sent to every output);
//   - Tone: an interval-dependent pair of high shelves on the shifted sound (an octave down loses brightness, an
//     octave up gains harshness; at 100 % the curve restores the balance, at 0 % it is flat);
//   - Mix: both signals at full level at 50 %; below that the quieter side falls away on a dB curve
//     (0 % = dry only, 100 % = shifted only);
//   - Response (shifting up only): fast, balanced or clean (attacks about 8, 12 or 16 ms late, each step cleaner);
//   - Quality: full, lite or eco (lite does less, for about four fifths of the CPU shifting down and under three fifths shifting
//     up; eco about half of full's shifting down and a third shifting up);
//   - a change of Semitones, Quality, or Response while shifting up is a cross-fade: the engine has to start its bands over
//     for it, which clicks, so a second engine starts over with the new setting while the first plays on, and the shifted
//     sound fades from one to the other (each with its own tone curve). A change that comes during a fade waits for it to end,
//     and only the latest one is kept;
//   - the dry signal is never delayed, and no latency is reported.
//
// Copyright (c) 2026 Ben Juodvalkis. MIT License (see LICENSE).
#pragma once
#include "PolyPitchEngine.h"

namespace polypitch
{

class Processor
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        cur = 0; fadePos = -1;
        for (int e = 0; e < 2; ++e) { eng[e].setSemitones ((double) semis); eng[e].setResponse (response); eng[e].setQuality (quality); eng[e].prepare (sr); }
        path[0].semis = semis; path[0].response = response; path[0].quality = quality;
        gsm = 1.0 - std::exp (-1.0 / (0.010 * sr));
        updateTone (path[0]); clearState();
    }

    // forget everything heard so far; a setting that was waiting for a fade takes effect at once
    void reset()
    {
        fadePos = -1;
        Path& p = path[cur];
        if (p.semis != semis || p.response != response || p.quality != quality)
        {
            eng[cur].setSemitones ((double) semis); eng[cur].setResponse (response); eng[cur].setQuality (quality);
            p.semis = semis; p.response = response; p.quality = quality; updateTone (p);
        }
        eng[cur].reset(); clearState();
    }

    void setSemitones (int st)       { semis = std::max (-12, std::min (12, st)); }
    void setMix (double zeroToOne)   { mix = std::max (0.0, std::min (1.0, zeroToOne)); gwTarget = mixGain (mix); gdTarget = mixGain (1.0 - mix); }
    void setTone (double zeroToOne)  { const double v = std::max (0.0, std::min (1.0, zeroToOne)); if (v != tone) { tone = v; updateTone (path[0]); updateTone (path[1]); } }
    void setResponse (int r)         { response = std::max (0, std::min (2, r)); }      // shifting up only: 0 = fast, 1 = balanced, 2 = clean
    void setQuality (int q)          { quality = std::max (0, std::min (2, q)); }       // 0 = full, 1 = lite, 2 = eco
    int getSemitones() const { return semis; }
    const Engine& getEngine() const { return eng[cur]; }

    // n samples from inL / inR to outL / outR (in place is fine). inR and outR may be null: mono.
    template <typename T>
    void process (const T* inL, const T* inR, T* outL, T* outR, int n)
    {
        if (first) { gWet = gwTarget; gDry = gdTarget; first = false; }
        follow();
        for (int i = 0; i < n; ++i)
        {
            const double l = finiteOrZero ((double) inL[i]), r = inR != nullptr ? finiteOrZero ((double) inR[i]) : l, x = inR != nullptr ? 0.5 * (l + r) : l;
            double w = path[cur].shelve (eng[cur].processSample (x));
            if (fadePos >= 0)
            {
                const int nx = 1 - cur; const double b = path[nx].shelve (eng[nx].processSample (x));
                if (fadePos >= holdLen)                                  // equal loudness: the two are different sounds, not one in two phases
                {
                    const double u = 0.5 * kPi * (double) (fadePos - holdLen) / (double) fadeLen;
                    w = std::cos (u) * w + std::sin (u) * b;
                }
                if (++fadePos >= holdLen + fadeLen) { cur = nx; fadePos = -1; follow(); }
            }
            gWet += gsm * (gwTarget - gWet); gDry += gsm * (gdTarget - gDry);
            outL[i] = (T) (l * gDry + w * gWet);
            if (outR != nullptr) outR[i] = (T) (r * gDry + w * gWet);
        }
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
    // When the fade starts, and how long it takes. Shifting down the new engine plays the input straight away (its attack path),
    // so the fade starts after a few milliseconds. Shifting up, a band with two partials in it is treated as plain until its beat
    // has gone by once, and a new engine has no beats behind it: for its first tenth of a second it comes out several dB quiet
    // and rough, as just after any attack. So shifting up the old engine plays on for that tenth of a second after the new one's
    // band filter (8, 12 or 16 ms by response) has filled. (On a steady chord: 4 ms there left a sag of 5 to 10 dB, 120 ms
    // none beyond what the chord does by itself.)
    static constexpr double holdDownMs = 4.0, fadeDownMs = 30.0, holdUpMs = 100.0, fadeUpMs = 40.0;

    // one engine's settings and tone curve, with the curve's running state
    struct Path
    {
        int semis = -12, response = 0, quality = 0;
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, c0 = 1, c1 = 0, c2 = 0, e1 = 0, e2 = 0, s1 = 0, s2 = 0, u1 = 0, u2 = 0;
        double shelve (double wet)
        {
            const double a = b0 * wet + s1; s1 = b1 * wet - a1 * a + s2; s2 = b2 * wet - a2 * a;           // first shelf
            const double w = c0 * a + u1;   u1 = c1 * a - e1 * w + u2;   u2 = c2 * a - e2 * w;             // second shelf
            return w;
        }
        void clear() { s1 = s2 = u1 = u2 = 0.0; }
    };

    Engine eng[2];
    Path path[2];
    int cur = 0, fadePos = -1, holdLen = 1, fadeLen = 1;
    double sr = 44100.0, mix = 1.0, tone = 1.0, gwTarget = 1.0, gdTarget = 0.0, gWet = 1.0, gDry = 0.0, gsm = 0.0;
    int semis = -12, response = 0, quality = 0;
    bool first = true;

    void clearState() { path[0].clear(); path[1].clear(); first = true; gwTarget = mixGain (mix); gdTarget = mixGain (1.0 - mix); }

    // Between fades: bring the playing engine to the settings asked for. A Response change while shifting down changes nothing
    // the engine does, so it is passed on as it is; anything else that differs starts the other engine over and fades to it.
    void follow()
    {
        if (fadePos >= 0) return;
        Path& p = path[cur];
        if (p.semis == semis && p.quality == quality && p.response == response) return;
        if (p.semis == semis && p.quality == quality && semis <= 0) { eng[cur].setResponse (response); p.response = response; return; }
        const int nx = 1 - cur; Path& q = path[nx];
        eng[nx].setSemitones ((double) semis); eng[nx].setResponse (response); eng[nx].setQuality (quality); eng[nx].reset();
        q.semis = semis; q.response = response; q.quality = quality; updateTone (q); q.clear();
        const bool upNow = semis > 0;
        holdLen = (int) std::lround ((upNow ? 8.0 + 4.0 * response + holdUpMs : holdDownMs) * sr / 1000.0);
        fadeLen = std::max ((int) std::lround ((upNow ? fadeUpMs : fadeDownMs) * sr / 1000.0), 1);
        fadePos = 0;
    }

    // an input sample that is not a number, or is infinite, counts as silence on the dry side too (the engine does the same for itself)
    static double finiteOrZero (double v) { return std::isfinite (v) ? v : 0.0; }

    // both at full level at 50 %, and below that the quieter side falls on a dB curve
    static double mixGain (double u)
    {
        if (u <= 0.0) return 0.0;
        double db = 0.0;
        if (u < 0.5)  db = -48.0 * (0.5 - u);
        if (u < 0.25) db = -26.4 + (u - 0.15) * 144.0;
        if (u < 0.15) db = -48.0 + (u - 0.10) * 432.0;
        if (u < 0.10) db = -120.0 + u * 720.0;
        return std::exp (db * 0.11512925464970229);
    }

    static void shelf (double sampleRate, double f0, double gainDb, double& o0, double& o1, double& o2, double& p1, double& p2)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPi * f0 / sampleRate, c = std::cos (w), al = std::sin (w) / 2.0 * 1.4142135623730951, q = 2.0 * std::sqrt (A) * al;
        const double n0 = (A + 1) - (A - 1) * c + q;
        o0 = A * ((A + 1) + (A - 1) * c + q) / n0; o1 = -2 * A * ((A - 1) + (A + 1) * c) / n0; o2 = A * ((A + 1) + (A - 1) * c - q) / n0;
        p1 = 2 * ((A - 1) - (A + 1) * c) / n0; p2 = ((A + 1) - (A - 1) * c - q) / n0;
    }

    // the tone curve, per interval: shifting down, +11 dB above 1 kHz less 15 dB above 6.8 kHz at a full octave;
    // shifting up, -5 dB above 1 kHz at a full octave; in proportion for smaller shifts
    void updateTone (Path& p)
    {
        const double ratio = std::pow (2.0, p.semis / 12.0);
        double f1 = 1052.1135, g1 = 11.19060 * (1.0 - ratio) / 0.5 * tone, f2 = 6809.1133, g2 = -14.87473 * (1.0 - ratio) / 0.5 * tone;
        if (ratio > 1.0) { f1 = 1033.0498; g1 = -5.29473 * (ratio - 1.0) * tone; g2 = 0.0; }
        shelf (sr, f1, g1, p.b0, p.b1, p.b2, p.a1, p.a2); shelf (sr, f2, g2, p.c0, p.c1, p.c2, p.e1, p.e2);
    }
};

} // namespace polypitch
