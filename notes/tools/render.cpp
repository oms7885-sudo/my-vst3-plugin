// Render a mono float32 file through the current plugin engine with params given as key=value args.
#include "/home/claude/my-vst3-plugin/BassSynth/Source/SynthEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
int main (int argc, char** argv)
{
    if (argc < 3) return 1;
    FILE* f = std::fopen (argv[1], "rb"); if (! f) return 1;
    std::vector<float> x; float v; while (std::fread (&v, 4, 1, f) == 1) x.push_back (v); std::fclose (f);
    bsyn::Params p;
    for (int a = 3; a < argc; ++a)
    {
        std::string s (argv[a]); auto e = s.find ('='); if (e == std::string::npos) continue;
        std::string k = s.substr (0, e); float val = (float) std::atof (s.c_str() + e + 1);
#define P(n) if (k == #n) p.n = val;
        P(inGainDb) P(cTh) P(cRat) P(gateDb) P(pickSens) P(tail) P(subLP) P(hyst) P(tone) P(vintage) P(overshoot) P(overshootMax) P(resFade)
        P(osc1Range) P(osc1Wave) P(osc2Range) P(osc2Wave) P(osc3Range) P(osc3Wave) P(osc2Freq) P(osc3Freq)
        P(osc1Lvl) P(osc2Lvl) P(osc3Lvl) P(extLvl) P(noiseLvl) P(modMix) P(modAmt)
        P(cutoff) P(res) P(envAmt) P(fA) P(fD) P(fS) P(aA) P(aD) P(aS) P(dyn) P(outGainDb) P(dry)
        if (k == "osc3Ctl") p.osc3Ctl = val > 0.5f; if (k == "osc1On") p.osc1On = val > 0.5f; if (k == "osc2On") p.osc2On = val > 0.5f;
        if (k == "osc3On") p.osc3On = val > 0.5f; if (k == "extOn") p.extOn = val > 0.5f; if (k == "noiseOn") p.noiseOn = val > 0.5f;
        if (k == "noisePink") p.noisePink = val > 0.5f; if (k == "filterMod") p.filterMod = val > 0.5f;
        if (k == "keyTrk1") p.keyTrk1 = val > 0.5f; if (k == "keyTrk2") p.keyTrk2 = val > 0.5f; if (k == "decaySw") p.decaySw = val > 0.5f;
        if (k == "comp") p.comp = val > 0.5f;
        if (k == "fundTrack") p.fundTrack = val > 0.5f; if (k == "extFb") p.extFb = val > 0.5f; if (k == "inertia") p.inertia = val; if (k == "grain") p.grain = (int) val;
#undef P
    }
    bsyn::Engine eng; eng.prepare (44100); eng.setParams (p);
    std::vector<float> y (x.size());
    for (size_t i = 0; i < x.size(); ++i) { if ((i & 511) == 0) eng.setParams (p); y[i] = eng.process (x[i]); }
    FILE* o = std::fopen (argv[2], "wb"); std::fwrite (y.data(), 4, y.size(), o); std::fclose (o);
    return 0;
}
