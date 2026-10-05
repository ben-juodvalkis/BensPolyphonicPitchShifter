// PolyPitch processor: the whole effect around the engine, with no host or framework dependencies.
// Used as is by the plug-in (plugin/) and by the Max object (max/).
//
//   - the inputs are summed to mono for the shifter (the shifted sound is mono, sent to every output);
//   - Tone: an interval-dependent pair of high shelves on the shifted sound (an octave down loses brightness, an
//     octave up gains harshness; at 100 % the curve restores the balance, at 0 % it is flat);
//   - Mix: both signals at full level at 50 %; below that the quieter side falls away on a dB curve
//     (0 % = dry only, 100 % = shifted only);
//   - Response (shifting up only): fast, balanced or clean (attacks about 8, 12 or 16 ms late, each step cleaner);
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
        engine.setSemitones ((double) semis); engine.setResponse (response); engine.prepare (sr);
        gsm = 1.0 - std::exp (-1.0 / (0.010 * sr));
        updateTone(); clearState();
    }

    void reset() { engine.reset(); clearState(); }

    void setSemitones (int st)       { st = std::max (-12, std::min (12, st)); if (st != semis) { semis = st; engine.setSemitones ((double) st); updateTone(); } }
    void setMix (double zeroToOne)   { mix = std::max (0.0, std::min (1.0, zeroToOne)); gwTarget = mixGain (mix); gdTarget = mixGain (1.0 - mix); }
    void setTone (double zeroToOne)  { const double v = std::max (0.0, std::min (1.0, zeroToOne)); if (v != tone) { tone = v; updateTone(); } }
    void setResponse (int r)         { r = std::max (0, std::min (2, r)); if (r != response) { response = r; engine.setResponse (r); } }      // shifting up only: 0 = fast, 1 = balanced, 2 = clean
    int getSemitones() const { return semis; }
    const Engine& getEngine() const { return engine; }

    // n samples from inL / inR to outL / outR (in place is fine). inR and outR may be null: mono.
    template <typename T>
    void process (const T* inL, const T* inR, T* outL, T* outR, int n)
    {
        if (first) { gWet = gwTarget; gDry = gdTarget; first = false; }
        for (int i = 0; i < n; ++i)
        {
            const double l = finiteOrZero ((double) inL[i]), r = inR != nullptr ? finiteOrZero ((double) inR[i]) : l, x = inR != nullptr ? 0.5 * (l + r) : l;
            const double wet = engine.processSample (x);
            const double a = b0 * wet + s1; s1 = b1 * wet - a1 * a + s2; s2 = b2 * wet - a2 * a;           // first shelf
            const double w = c0 * a + u1;   u1 = c1 * a - e1 * w + u2;   u2 = c2 * a - e2 * w;             // second shelf
            gWet += gsm * (gwTarget - gWet); gDry += gsm * (gdTarget - gDry);
            outL[i] = (T) (l * gDry + w * gWet);
            if (outR != nullptr) outR[i] = (T) (r * gDry + w * gWet);
        }
    }

private:
    Engine engine;
    double sr = 44100.0, mix = 1.0, tone = 1.0, gwTarget = 1.0, gdTarget = 0.0, gWet = 1.0, gDry = 0.0, gsm = 0.0;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, c0 = 1, c1 = 0, c2 = 0, e1 = 0, e2 = 0, s1 = 0, s2 = 0, u1 = 0, u2 = 0;
    int semis = -12, response = 0;
    bool first = true;

    void clearState() { s1 = s2 = u1 = u2 = 0.0; first = true; gwTarget = mixGain (mix); gdTarget = mixGain (1.0 - mix); }

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
        const double pi = 3.14159265358979323846;
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * pi * f0 / sampleRate, c = std::cos (w), al = std::sin (w) / 2.0 * 1.4142135623730951, q = 2.0 * std::sqrt (A) * al;
        const double n0 = (A + 1) - (A - 1) * c + q;
        o0 = A * ((A + 1) + (A - 1) * c + q) / n0; o1 = -2 * A * ((A - 1) + (A + 1) * c) / n0; o2 = A * ((A + 1) + (A - 1) * c - q) / n0;
        p1 = 2 * ((A - 1) - (A + 1) * c) / n0; p2 = ((A + 1) - (A - 1) * c - q) / n0;
    }

    // the tone curve, per interval: shifting down, +11 dB above 1 kHz less 15 dB above 6.8 kHz at a full octave;
    // shifting up, -5 dB above 1 kHz at a full octave; in proportion for smaller shifts
    void updateTone()
    {
        const double ratio = std::pow (2.0, semis / 12.0);
        double f1 = 1052.1135, g1 = 11.19060 * (1.0 - ratio) / 0.5 * tone, f2 = 6809.1133, g2 = -14.87473 * (1.0 - ratio) / 0.5 * tone;
        if (ratio > 1.0) { f1 = 1033.0498; g1 = -5.29473 * (ratio - 1.0) * tone; g2 = 0.0; }
        shelf (sr, f1, g1, b0, b1, b2, a1, a2); shelf (sr, f2, g2, c0, c1, c2, e1, e2);
    }
};

} // namespace polypitch
