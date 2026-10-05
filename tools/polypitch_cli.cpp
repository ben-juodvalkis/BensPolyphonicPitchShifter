// Offline harness: raw float32 mono in -> raw float32 mono out.
//   polypitch_cli in.f32 out.f32 <sampleRate> <semitones> [mix 0..100] [tone 0..100] [response=0|1|2]
// With only four arguments it runs the bare engine (shifted sound only, no tone curve) - what the tests compare with
// the Python reference. With mix and tone it runs the whole processor, as the plug-in and the Max object do.
// response=N (anywhere after the semitones) sets the response shifting up: 0 fast (the default), 1 balanced, 2 clean.
#include "../engine/PolyPitchProcessor.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <cstring>
int main (int argc, char** argv)
{
    int response = 0;
    for (int i = 5; i < argc; ++i)
        if (std::strncmp (argv[i], "response=", 9) == 0) { response = std::atoi (argv[i] + 9); for (int j = i; j + 1 < argc; ++j) argv[j] = argv[j + 1]; --argc; --i; }
    if (argc < 5) { std::fprintf (stderr, "usage: polypitch_cli in.f32 out.f32 sampleRate semitones [mix tone] [response=0|1|2]\n"); return 1; }
    FILE* f = std::fopen (argv[1], "rb"); if (! f) { std::fprintf (stderr, "cannot open %s\n", argv[1]); return 1; }
    std::fseek (f, 0, SEEK_END); const long n = std::ftell (f) / 4; std::fseek (f, 0, SEEK_SET);
    std::vector<float> x ((size_t) n), y ((size_t) n);
    if (std::fread (x.data(), 4, (size_t) n, f) != (size_t) n) { std::fprintf (stderr, "short read\n"); return 1; }
    std::fclose (f);
    const double sr = std::atof (argv[3]), st = std::atof (argv[4]);
    const auto t0 = std::chrono::steady_clock::now(); double readers = 0.0; int bands = 0; bool designed = false;
    if (argc >= 7)
    {
        polypitch::Processor p; p.setSemitones ((int) st); p.setMix (std::atof (argv[5]) / 100.0); p.setTone (std::atof (argv[6]) / 100.0); p.setResponse (response); p.prepare (sr);
        for (long i = 0; i < n; i += 512) p.process (x.data() + i, (const float*) nullptr, y.data() + i, (float*) nullptr, (int) std::min (512L, n - i));
        bands = p.getEngine().getNumBands(); designed = p.getEngine().usingDesignedFilter();
    }
    else
    {
        polypitch::Engine e; e.setSemitones (st); e.setResponse (response); e.prepare (sr); long rd = 0;
        for (long i = 0; i < n; ++i) { y[(size_t) i] = (float) e.processSample (x[(size_t) i]); if ((i & 127) == 0) rd += e.readersNow(); }
        readers = (double) rd / (n / 128.0); bands = e.getNumBands(); designed = e.usingDesignedFilter();
    }
    const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::fprintf (stderr, "%ld samples in %.2f s = %.1f%% of real time at %.0f Hz; designed filter: %s; %d bands, %.1f on readers on average\n", n, dt, 100.0 * dt / (n / sr), sr, designed ? "yes" : "no", bands, readers);
    f = std::fopen (argv[2], "wb"); std::fwrite (y.data(), 4, (size_t) n, f); std::fclose (f);
    return 0;
}
