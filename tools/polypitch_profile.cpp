// Where the engine's time goes: per stage, on average and in the heaviest 64-sample blocks.
//   polypitch_profile in.f32 <sampleRate> <semitones> [response 0|1|2] [quality 0|1|2]
// Built with -DPOLYPITCH_PROFILE, which makes the engine keep the time of each stage (the timers themselves cost about
// half a percent of a core). polypitch_load is the plain measurement; this one says where to look.
#ifndef POLYPITCH_PROFILE
 #error "build with -DPOLYPITCH_PROFILE (scripts/build.sh tools does)"
#endif
#include "../engine/PolyPitchEngine.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <numeric>
int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage: polypitch_profile in.f32 sampleRate semitones [response] [quality]\n"); return 1; }
    FILE* f = std::fopen (argv[1], "rb"); if (! f) return 1;
    std::fseek (f, 0, SEEK_END); const long n = std::ftell (f) / 4; std::fseek (f, 0, SEEK_SET);
    std::vector<float> x ((size_t) n); if (std::fread (x.data(), 4, (size_t) n, f) != (size_t) n) return 1; std::fclose (f);
    const double sr = std::atof (argv[2]); polypitch::Engine e; e.setSemitones (std::atof (argv[3])); e.setResponse (argc > 4 ? std::atoi (argv[4]) : 0); e.setQuality (argc > 5 ? std::atoi (argv[5]) : 0); e.prepare (sr);
    typedef polypitch::Engine E; const int NS = E::numStages;
    const char* names[E::numStages] = { "window", "transform", "per-band work at a frame", "likeness curves", "decisions", "  of which: fitting readers", "attacks", "band loop, frame start", "band loop, other samples" };
    std::vector<double> bt; std::vector<std::vector<double>> bs; double acc = 0.0, prev[E::numStages] = {}; long readers = 0;
    for (long i = 0; i + 64 <= n; i += 64)
    {
        const double t0 = E::stageNow();
        for (int j = 0; j < 64; ++j) acc += e.processSample (x[(size_t) (i + j)]);
        bt.push_back (E::stageNow() - t0); readers += e.readersNow();
        std::vector<double> d ((size_t) NS); for (int q = 0; q < NS; ++q) { d[(size_t) q] = e.stageTime[q] - prev[q]; prev[q] = e.stageTime[q]; } bs.push_back (d);
    }
    const double audio = (double) bt.size() * 64.0 / sr, slot = 64.0 / sr; double tot = 0.0; for (double v : bt) tot += v;
    std::vector<size_t> idx (bt.size()); std::iota (idx.begin(), idx.end(), 0); std::sort (idx.begin(), idx.end(), [&] (size_t a, size_t b) { return bt[a] > bt[b]; });
    const size_t top = std::max<size_t> (bt.size() / 100, 1);      // the heaviest 1 % of the 64-sample blocks
    double topT = 0.0; std::vector<double> topS ((size_t) NS, 0.0); for (size_t q = 0; q < top; ++q) { topT += bt[idx[q]]; for (int s = 0; s < NS; ++s) topS[(size_t) s] += bs[idx[q]][(size_t) s]; }
    std::printf ("%+g semitones, response %d, quality %d, %.0f Hz: %d bands, %.0f on readers on average\n", std::atof (argv[3]), e.getResponse(), e.getQuality(), sr, e.getNumBands(), (double) readers / (double) bt.size());
    std::printf ("   %-28s %12s %32s\n", "stage", "average", "in the heaviest 1 % of blocks");
    for (int s = 0; s < NS; ++s) std::printf ("   %-28s %8.2f %% of a core %14.1f %% of the time slot\n", names[s], 100.0 * e.stageTime[s] / audio, 100.0 * topS[(size_t) s] / top / slot);
    std::printf ("   %-28s %8.2f %% of a core %14.1f %% of the time slot\n", "all", 100.0 * tot / audio, 100.0 * topT / top / slot);
    std::printf ("   64-sample blocks (%.2f ms each): median %.0f %% of the slot, 99th percentile %.0f %%, 99.9th %.0f %%, heaviest %.0f %%%s\n", slot * 1000.0,
                 100.0 * bt[idx[bt.size() / 2]] / slot, 100.0 * bt[idx[bt.size() / 100]] / slot, 100.0 * bt[idx[bt.size() / 1000]] / slot, 100.0 * bt[idx[0]] / slot, acc == 12345.678 ? "!" : "");
    return 0;
}
