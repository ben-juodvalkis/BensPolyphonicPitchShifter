// How heavy and how uneven is the engine's work? Times every 64-sample block (what a small audio buffer has to fit in).
//   polypitch_load in.f32 <sampleRate> <semitones>
#include "../engine/PolyPitchEngine.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <algorithm>
int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage: polypitch_load in.f32 sampleRate semitones\n"); return 1; }
    FILE* f = std::fopen (argv[1], "rb"); if (! f) return 1;
    std::fseek (f, 0, SEEK_END); const long n = std::ftell (f) / 4; std::fseek (f, 0, SEEK_SET);
    std::vector<float> x ((size_t) n); if (std::fread (x.data(), 4, (size_t) n, f) != (size_t) n) return 1; std::fclose (f);
    const double sr = std::atof (argv[2]); polypitch::Engine e; e.setSemitones (std::atof (argv[3])); e.prepare (sr);
    std::vector<double> bt; double acc = 0.0, sum = 0.0;
    for (long i = 0; i + 64 <= n; i += 64)
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (int j = 0; j < 64; ++j) acc += e.processSample (x[(size_t) (i + j)]);
        bt.push_back (std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() * 1000.0);
    }
    std::sort (bt.begin(), bt.end()); for (double v : bt) sum += v;
    const double per = 64.0 / sr * 1000.0;
    std::printf ("%.1f%% of one core; 64-sample blocks (%.2f ms of audio each): mean %.3f ms, 99.9%% %.3f ms, worst %.3f ms (%.0f%% of the block)%s\n",
                 100.0 * sum / (bt.size() * per), per, sum / bt.size(), bt[(size_t) (bt.size() * 0.999)], bt.back(), 100.0 * bt.back() / per, acc == 12345.678 ? "!" : "");
    return 0;
}
