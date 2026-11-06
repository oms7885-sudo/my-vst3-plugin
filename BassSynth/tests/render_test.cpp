// Offline check: renders a raw float32 mono file through the engine.
// usage: render_test in.f32 out.f32 sampleRate inGainDb outGainDb drift
#include "../Source/SynthEngine.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main (int argc, char** argv)
{
    if (argc < 7) { std::printf ("usage: render_test in.f32 out.f32 sr inDb outDb drift\n"); return 1; }
    FILE* f = std::fopen (argv[1], "rb"); if (! f) return 1;
    std::vector<float> x; float v; while (std::fread (&v, 4, 1, f) == 1) x.push_back (v); std::fclose (f);
    bsyn::Engine e; e.prepare (std::atof (argv[3]));
    bsyn::Params p; p.inGainDb = (float) std::atof (argv[4]); p.outGainDb = (float) std::atof (argv[5]); p.drift = (float) std::atof (argv[6]);
    std::vector<float> y (x.size());
    for (size_t i = 0; i < x.size(); ++i) { if ((i & 511) == 0) e.setParams (p); y[i] = e.process (x[i]); }
    FILE* o = std::fopen (argv[2], "wb"); std::fwrite (y.data(), 4, y.size(), o); std::fclose (o);
    return 0;
}
