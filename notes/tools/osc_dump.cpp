// Dump the oscillators' running phase (cycles) and the tracking cutoff for a mono float32 file.
// The engine header is given on the compiler command line (-DENGINE=...), so variants can be compared.
#include ENGINE
#include <cstdio>
#include <cstdlib>
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
        if (k == "fundTrack") p.fundTrack = val > 0.5f;
#ifdef EXTRA
        EXTRA
#endif
    }
    bsyn::Engine eng; eng.prepare (44100); eng.setParams (p);
    std::vector<double> o (x.size() * 6);
    for (size_t i = 0; i < x.size(); ++i) { eng.process (x[i]); o[6*i]=eng.probeOscPhase();o[6*i+1]=eng.probeTrackHz();o[6*i+2]=eng.probeFundHz();o[6*i+3]=(double)eng.triggers();o[6*i+4]=eng.probeFirstHz();o[6*i+5]=eng.probeWholeHz();}
    FILE* w = std::fopen (argv[2], "wb"); std::fwrite (o.data(), 8, o.size(), w); std::fclose (w);
    return 0;
}
