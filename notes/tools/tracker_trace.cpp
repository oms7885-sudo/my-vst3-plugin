#define private public
#include "/home/claude/my-vst3-plugin/BassSynth/Source/SynthEngine.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main (int argc, char** argv)
{
    FILE* f = std::fopen (argv[1], "rb"); std::vector<float> x; float v; while (std::fread (&v, 4, 1, f) == 1) x.push_back (v); std::fclose (f);
    const double t0 = std::atof (argv[2]), t1 = std::atof (argv[3]); const int step = argc > 4 ? std::atoi (argv[4]) : 2;
    bsyn::Params p; bsyn::Engine e; e.prepare (44100); e.setParams (p);
    uint32_t last = 0; double lastOsc = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        e.process (x[i]); const double t = i / 44100.0;
        if (e.triggers() != last) { last = e.triggers(); if (t >= t0 && t < t1) std::printf ("%.3f  PICK  prevOwn %.1f\n", t, e.prevOwn); }
        if (t >= t0 && t < t1 && i % (44 * step) == 0)
            std::printf ("%.3f  first %6.1f whole %6.1f wind %.2f own %5.1f | taken %6.1f cut %6.1f | run %d | osc %.1f Hz\n", t, e.dbgF1, e.dbgFw, e.dbgWind, e.ownF, e.fundHz, e.fcN, e.fRun, (e.dbgOsc - lastOsc) / (44.0 * step / 44100.0)), lastOsc = e.dbgOsc;
    }
}
