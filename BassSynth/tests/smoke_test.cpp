// Plays synthetic plucked notes through the engine and checks the output is sane:
// sound during notes, silence between them, no NaN/Inf, at several sample rates; and that the
// contour peaks (overshoot, build-up on quick re-picks) behave as documented in SynthEngine.h.
#include "../Source/SynthEngine.h"
#include <cstdint>
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

// Contour peaks: a single note peaks at Params::overshoot; quick re-picks build the peak up, never
// past Params::overshootMax; after a pause the next note is back at the single-note peak.
static int contours (double sr)
{
    const double f = 82.4;
    std::vector<double> starts;
    double t0 = 0.2;
    for (int k = 0; k < 6; ++k) { starts.push_back (t0); t0 += 0.11; }
    starts.push_back (t0 + 1.2);
    const int n = (int) ((t0 + 2.0) * sr);
    std::vector<float> x ((size_t) n, 0.0f);
    for (size_t k = 0; k < starts.size(); ++k)
    {
        const int a = (int) (starts[k] * sr), b = k + 1 < starts.size() ? (int) (starts[k + 1] * sr) : n;
        for (int i = a; i < b; ++i)
        {
            const double t = (i - a) / sr, env = std::exp (-t * 12.0) * std::min (1.0, (i - a) / (0.002 * sr));
            x[(size_t) i] = (float) (0.4 * env * (std::sin (6.283185307 * f * t) + 0.4 * std::sin (6.283185307 * 2 * f * t)));
        }
    }
    bsyn::Engine e; e.prepare (sr);
    bsyn::Params p; e.setParams (p);
    std::vector<double> peakA, peakF;
    uint32_t last = 0;
    for (int i = 0; i < n; ++i)
    {
        e.process (x[(size_t) i]);
        if (e.triggers() != last) { last = e.triggers(); peakA.push_back (0); peakF.push_back (0); }
        if (! peakA.empty())
        {
            peakA.back() = std::max (peakA.back(), e.loudnessContour());
            peakF.back() = std::max (peakF.back(), e.filterContour());
        }
    }
    bool ok = peakA.size() == starts.size();
    if (ok)
    {
        const double one = p.overshoot, top = p.overshootMax;
        ok = ok && std::abs (peakA[0] - one) < 0.01 && std::abs (peakF[0] - one) < 0.01;      // single note
        for (size_t k = 1; k < 6; ++k)                                                          // quick hits build up
            ok = ok && peakA[k] > peakA[k - 1] && peakA[k] <= top + 1e-6 && peakF[k] >= peakF[k - 1] - 1e-9 && peakF[k] <= top + 1e-6;
        ok = ok && peakA[3] > one + 0.1;                                                        // clearly higher by the 4th hit
        ok = ok && std::abs (peakA[6] - one) < 0.01 && std::abs (peakF[6] - one) < 0.01;      // after a pause: back to normal
    }
    std::printf ("sr %.0f: contour peaks (loudness)", sr);
    for (double v : peakA) std::printf (" %.3f", v);
    std::printf ("  %s\n", ok ? "ok" : "WRONG");
    return ok ? 0 : 1;
}

int main()
{
    int fail = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 }) fail |= run (sr);
    for (double sr : { 44100.0, 96000.0 }) fail |= contours (sr);
    std::printf (fail ? "FAIL\n" : "OK\n");
    return fail;
}
