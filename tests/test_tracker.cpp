// Offline test of BassTracker with a synthetic bass line.
#include "../Source/BassTracker.h"
#include <cstdio>
#include <random>

using namespace b2m;

struct Seg { int note; double start, dur; bool legatoFromPrev; };

static double midiToHz (double n) { return 440.0 * std::pow (2.0, (n - 69.0) / 12.0); }

int main (int argc, char** argv)
{
    const double fs = argc > 1 ? std::atof (argv[1]) : 48000.0;
    // note, start, duration, legato (pitch changes without a new pluck)
    std::vector<Seg> segs = {
        { 28, 0.20, 0.50, false }, // E1
        { 33, 0.80, 0.40, false }, // A1
        { 38, 1.30, 0.40, false }, // D2
        { 43, 1.80, 0.30, false }, // G2
        { 43, 2.10, 0.30, false }, // G2 again (re-pluck while ringing)
        { 23, 2.60, 0.60, false }, // B0 (5-string)
        { 52, 3.40, 0.30, false }, // E3
        { 33, 3.90, 0.30, false }, // A1 ...
        { 35, 4.20, 0.30, true  }, // ... hammer-on to B1
    };

    const double total = 4.9;
    const int N = (int) (total * fs);
    std::vector<float> sig ((size_t) N, 0.0f);
    std::mt19937 rng (1);
    std::normal_distribution<float> noise (0.0f, 0.0015f);

    double phase = 0.0, env = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double t = i / fs;
        const Seg* cur = nullptr;
        for (auto& s : segs) if (t >= s.start && t < s.start + s.dur) cur = &s;
        double y = 0.0;
        if (cur)
        {
            const double tl = t - cur->start;
            if (! cur->legatoFromPrev && tl < 1.0 / fs) env = 0.0;   // fresh pluck
            const double attack = cur->legatoFromPrev ? 1.0 : std::min (1.0, tl / 0.003);
            const double target = 0.5 * attack * std::exp (-tl * 2.5);
            env = cur->legatoFromPrev ? env * 0.99995 : target;
            const double f = midiToHz (cur->note);
            phase += 2.0 * 3.14159265358979323846 * f / fs;
            // bass-like spectrum: strong 2nd harmonic, decaying upper partials
            y = env * (0.6 * std::sin (phase) + 0.8 * std::sin (2 * phase)
                     + 0.35 * std::sin (3 * phase) + 0.2 * std::sin (4 * phase)
                     + 0.1 * std::sin (5 * phase));
            // short damping at the end of each note
            const double rem = cur->start + cur->dur - t;
            if (rem < 0.01) y *= rem / 0.01;
        }
        sig[(size_t) i] = (float) y + noise (rng);
    }

    BassTracker tr;
    tr.prepare (fs);
    tr.setParams (TrackerParams {});

    struct Ev { NoteEvent e; double time; };
    std::vector<Ev> evs;
    const int block = 256;
    for (int pos = 0; pos < N; pos += block)
    {
        const int n = std::min (block, N - pos);
        tr.process (sig.data() + pos, n, [&] (const NoteEvent& e) { evs.push_back ({ e, (pos + e.sampleOffset) / fs }); });
    }

    const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    std::vector<int> ons;
    for (auto& v : evs)
    {
        std::printf ("%7.3f s  %s %-3s%d  vel %d\n", v.time, v.e.isNoteOn ? "ON " : "off",
                     names[v.e.note % 12], v.e.note / 12 - 1, v.e.velocity);
        if (v.e.isNoteOn) ons.push_back (v.e.note);
    }

    // Expect these note-ons in order
    std::vector<int> expected = { 28, 33, 38, 43, 43, 23, 52, 33, 35 };
    bool ok = ons == expected;

    // Onset latency for each plucked note
    double maxLat = 0;
    size_t k = 0;
    for (auto& v : evs)
        if (v.e.isNoteOn && k < segs.size())
            maxLat = std::max (maxLat, v.time - segs[k++].start);
    std::printf ("\nmax note-on latency: %.1f ms   latency estimate: %d samples\n", maxLat * 1000.0, tr.getLatencySamples());
    std::printf (ok ? "PASS\n" : "FAIL (note sequence mismatch)\n");
    return ok ? 0 : 1;
}
