// Plays synthetic plucked notes through the engine and checks the output is sane:
// sound during notes, silence between them, no NaN/Inf, at several sample rates.
#include "../Source/SynthEngine.h"
#include <cstdio>
#include <cmath>
#include <vector>

static int run (double sr)
{
    const double notes[] = { 41.2, 55.0, 82.4, 110.0, 73.4 };
    const int noteLen = (int) (0.5 * sr), gap = (int) (0.3 * sr);
    std::vector<float> x;
    for (double f : notes)
    {
        for (int i = 0; i < noteLen; ++i)
        {
            const double t = i / sr;
            const double env = std::exp (-t * 3.0) * std::min (1.0, i / (0.002 * sr));
            x.push_back ((float) (0.4 * env * (std::sin (6.283185307 * f * t) + 0.4 * std::sin (6.283185307 * 2 * f * t))));
        }
        for (int i = 0; i < gap; ++i) x.push_back (0.0f);
    }
    bsyn::Engine e; e.prepare (sr);
    bsyn::Params p; e.setParams (p);
    double onE = 0, offE = 0; long onN = 0, offN = 0; int bad = 0;
    const int per = noteLen + gap;
    for (size_t i = 0; i < x.size(); ++i)
    {
        if ((i & 255) == 0) e.setParams (p);
        const float y = e.process (x[i]);
        if (! std::isfinite (y)) ++bad;
        const long pos = (long) (i % (size_t) per);
        if (pos > (long) (0.02 * sr) && pos < (long) (0.3 * sr)) { onE += y * y; ++onN; }
        else if (pos > noteLen + (long) (0.2 * sr)) { offE += y * y; ++offN; }
    }
    const double onR = std::sqrt (onE / onN), offR = std::sqrt (offE / offN);
    std::printf ("sr %.0f: rms during notes %.4f, between notes %.5f, non-finite %d\n", sr, onR, offR, bad);
    return (bad == 0 && onR > 0.02 && offR < onR * 0.05) ? 0 : 1;
}

int main()
{
    int fail = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 }) fail |= run (sr);
    std::printf (fail ? "FAIL\n" : "OK\n");
    return fail;
}
