// Benchmark for BassTracker using Karplus-Strong plucked-string bass lines.
// Reports accuracy and note-on latency. Can also run on a WAV file:
//   bench                      -> synthetic scenarios
//   bench file.wav             -> prints the detected notes for a real recording
#include "../Source/BassTracker.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <cstdlib>

using namespace b2m;

static const char* kNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
static std::string nn (int n) { return std::string (kNames[n % 12]) + std::to_string (n / 12 - 1); }
static double hz (double n) { return 440.0 * std::pow (2.0, (n - 69.0) / 12.0); }

// ---------------------------------------------------------------- synth
struct Target { int note; double start; double end; bool pluck; };

struct KS
{
    std::vector<double> buf; int w = 0; double delay = 100, lpState = 0, loss = 0.996, bright = 0.5;
    explicit KS (double fs) : buf ((size_t) (fs / 20.0) + 8, 0.0) {}
    void setNote (double fs, int note) { delay = fs / hz (note) - 0.5; }  // -0.5: loop filter delay
    void pluck (std::mt19937& rng, double amp, double pos)
    {
        std::uniform_real_distribution<double> u (-1.0, 1.0);
        const int L = (int) delay;
        // noise burst shaped by pick position (comb) -> realistic harmonic balance
        std::vector<double> ex ((size_t) L);
        for (auto& v : ex) v = u (rng);
        const int pd = std::max (1, (int) (pos * L));
        for (int i = 0; i < L; ++i)
        {
            double v = ex[(size_t) i] - (i >= pd ? ex[(size_t) (i - pd)] : 0.0);
            const size_t idx = (size_t) ((w - L + i + (int) buf.size() * 4) % (int) buf.size());
            buf[idx] += amp * v * 0.5;
        }
    }
    double tick()
    {
        const double rp = w - delay;
        int i0 = (int) std::floor (rp);
        double fr = rp - i0;
        const int N = (int) buf.size();
        double a = buf[(size_t) ((i0 % N + N) % N)], b = buf[(size_t) (((i0 + 1) % N + N) % N)];
        double y = a + (b - a) * fr;
        lpState = lpState + bright * (y - lpState);           // loop lowpass
        buf[(size_t) (w % N)] = lpState * loss;
        w = (w + 1) % N;
        return y;
    }
};

struct Scenario
{
    std::string name;
    std::vector<Target> targets;
    std::vector<float> audio;
};

static Scenario makeScenario (const std::string& name, double fs, unsigned seed, int lo, int hi,
                              double minDur, double maxDur, double legatoProb, double repeatProb,
                              double noiseDb, double gapProb)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    Scenario sc; sc.name = name;

    double t = 0.3; int prev = -1;
    while (t < 20.0)
    {
        int note;
        bool pluck = true;
        if (prev >= 0 && u (rng) < repeatProb) note = prev;
        else note = lo + (int) (u (rng) * (hi - lo + 1));
        if (prev >= 0 && note != prev && std::abs (note - prev) <= 4 && u (rng) < legatoProb) pluck = false;
        const double dur = minDur + u (rng) * (maxDur - minDur);
        double end = t + dur;
        const bool gap = u (rng) < gapProb;
        if (gap) end -= std::min (0.08, dur * 0.4);
        sc.targets.push_back ({ note, t, end, pluck });
        t += dur;
        prev = note;
    }

    const int N = (int) ((t + 0.5) * fs);
    sc.audio.assign ((size_t) N, 0.0f);
    KS ks (fs);
    std::normal_distribution<double> nz (0.0, std::pow (10.0, noiseDb / 20.0));
    size_t ti = 0;
    bool muted = true;
    for (int i = 0; i < N; ++i)
    {
        const double tt = i / fs;
        while (ti < sc.targets.size() && tt >= sc.targets[ti].start)
        {
            auto& tg = sc.targets[ti];
            ks.setNote (fs, tg.note);
            ks.loss = 0.9985;
            if (tg.pluck)
            {
                ks.bright = 0.35 + 0.4 * u (rng);
                ks.pluck (rng, 0.25 + 0.6 * u (rng), 0.08 + 0.2 * u (rng));
            }
            muted = false;
            ++ti;
        }
        // mute at end of note (if followed by a gap or last note)
        const size_t cur = ti == 0 ? 0 : ti - 1;
        if (ti > 0 && ! muted && tt >= sc.targets[cur].end)
        {
            ks.loss = 0.3; // fast damping (per loop pass)
            muted = true;
        }
        sc.audio[(size_t) i] = (float) (ks.tick() * 0.6 + nz (rng));
    }
    return sc;
}

// ---------------------------------------------------------------- evaluation
struct Ev { NoteEvent e; double t; };

static std::vector<Ev> run (const std::vector<float>& a, double fs, const TrackerParams& p, int block = 128)
{
    BassTracker tr; tr.prepare (fs); tr.setParams (p);
    if (const char* e = std::getenv ("HFF")) tr.hfFactor = (float) std::atof (e);
    if (const char* e = std::getenv ("WMIN")) tr.wMinFactor = (float) std::atof (e);
    if (const char* e = std::getenv ("OCT")) tr.confirmOctave = std::atoi (e) != 0;
    if (const char* e = std::getenv ("SKIP")) tr.skipAfterOnset = std::atoi (e);
    if (const char* e = std::getenv ("MUTE")) tr.muteDropDb = (float) std::atof (e);
    std::vector<Ev> ev;
    for (int pos = 0; pos < (int) a.size(); pos += block)
    {
        const int n = std::min (block, (int) a.size() - pos);
        tr.process (a.data() + pos, n, [&] (const NoteEvent& e) { ev.push_back ({ e, (pos + e.sampleOffset) / fs }); });
    }
    return ev;
}

struct Stats
{
    int targets = 0, correct = 0, octave = 0, wrong = 0, missed = 0, extra = 0;
    std::vector<double> latLow, latHigh, offLat;   // below / above A1 (MIDI 33)
};

static Stats evaluate (const Scenario& sc, const std::vector<Ev>& ev, bool verbose)
{
    Stats s;
    std::vector<bool> used (ev.size(), false);
    for (size_t i = 0; i < sc.targets.size(); ++i)
    {
        auto& tg = sc.targets[i];
        const double winEnd = std::min (i + 1 < sc.targets.size() ? sc.targets[i + 1].start : 1e9, tg.start + 0.15);
        ++s.targets;
        int firstIdx = -1;
        for (size_t k = 0; k < ev.size(); ++k)
            if (! used[k] && ev[k].e.isNoteOn && ev[k].t >= tg.start && ev[k].t < winEnd)
            {
                used[k] = true;
                if (firstIdx < 0) firstIdx = (int) k;
                else if (ev[k].e.note != tg.note) { /* later wrong note in window */ }
            }
        if (firstIdx < 0)
        {
            // a held note with the same pitch through a legato "change" isn't possible; count as missed
            ++s.missed;
            if (verbose) std::printf ("  MISS  %6.3f %s%s prev=%s\n", tg.start, nn (tg.note).c_str(), tg.pluck ? "" : " (legato)", i ? nn (sc.targets[i-1].note).c_str() : "-");
            continue;
        }
        const auto& e = ev[(size_t) firstIdx];
        const double lat = (e.t - tg.start) * 1000.0;
        if (e.e.note == tg.note)
        {
            ++s.correct;
            (tg.note < 33 ? s.latLow : s.latHigh).push_back (lat);
        }
        else if ((e.e.note - tg.note) % 12 == 0)
        {
            ++s.octave;
            if (verbose) std::printf ("  OCT   %6.3f want %s got %s\n", tg.start, nn (tg.note).c_str(), nn (e.e.note).c_str());
        }
        else
        {
            ++s.wrong;
            if (verbose) std::printf ("  WRONG %6.3f want %s got %s\n", tg.start, nn (tg.note).c_str(), nn (e.e.note).c_str());
        }
    }
    // note-off latency for notes followed by silence
    for (size_t i = 0; i < sc.targets.size(); ++i)
    {
        auto& tg = sc.targets[i];
        const double nextStart = i + 1 < sc.targets.size() ? sc.targets[i + 1].start : 1e9;
        if (nextStart - tg.end < 0.03) continue;
        for (auto& e : ev)
            if (! e.e.isNoteOn && e.t >= tg.end - 0.005 && e.t < nextStart) { s.offLat.push_back ((e.t - tg.end) * 1000.0); break; }
    }
    for (size_t k = 0; k < ev.size(); ++k)
        if (ev[k].e.isNoteOn && ! used[k])
        {
            ++s.extra;
            if (verbose) std::printf ("  EXTRA %6.3f %s\n", ev[k].t, nn (ev[k].e.note).c_str());
        }
    return s;
}

static double pct (std::vector<double> v, double q)
{
    if (v.empty()) return 0;
    std::sort (v.begin(), v.end());
    return v[(size_t) std::min ((double) v.size() - 1, q * (v.size() - 1))];
}

// ---------------------------------------------------------------- wav reader (16/24/32-bit PCM, 32-bit float)
static bool readWav (const char* path, std::vector<float>& out, double& fs)
{
    FILE* f = std::fopen (path, "rb");
    if (! f) return false;
    char id[4]; uint32_t sz;
    std::fread (id, 1, 4, f); std::fread (&sz, 4, 1, f); std::fread (id, 1, 4, f);
    uint16_t fmt = 0, ch = 0, bits = 0; uint32_t rate = 0;
    std::vector<uint8_t> data;
    while (std::fread (id, 1, 4, f) == 4 && std::fread (&sz, 4, 1, f) == 1)
    {
        if (! std::memcmp (id, "fmt ", 4))
        {
            std::vector<uint8_t> b (sz); std::fread (b.data(), 1, sz, f);
            std::memcpy (&fmt, &b[0], 2); std::memcpy (&ch, &b[2], 2); std::memcpy (&rate, &b[4], 4); std::memcpy (&bits, &b[14], 2);
            if (fmt == 0xFFFE && sz >= 26) std::memcpy (&fmt, &b[24], 2);
        }
        else if (! std::memcmp (id, "data", 4)) { data.resize (sz); std::fread (data.data(), 1, sz, f); }
        else std::fseek (f, sz + (sz & 1), SEEK_CUR);
    }
    std::fclose (f);
    if (data.empty() || ch == 0) return false;
    fs = rate;
    const int bps = bits / 8; const size_t frames = data.size() / (bps * ch);
    out.assign (frames, 0.0f);
    for (size_t i = 0; i < frames; ++i)
    {
        double acc = 0;
        for (int c = 0; c < ch; ++c)
        {
            const uint8_t* p = &data[(i * ch + c) * bps];
            double v = 0;
            if (fmt == 3 && bits == 32) { float x; std::memcpy (&x, p, 4); v = x; }
            else if (bits == 16) v = (int16_t) (p[0] | p[1] << 8) / 32768.0;
            else if (bits == 24) { int32_t x = (p[0] << 8) | (p[1] << 16) | (p[2] << 24); v = (x >> 8) / 8388608.0; }
            else if (bits == 32) { int32_t x; std::memcpy (&x, p, 4); v = x / 2147483648.0; }
            acc += v;
        }
        out[i] = (float) (acc / ch);
    }
    return true;
}

int main (int argc, char** argv)
{
    TrackerParams p;
    if (const char* e = std::getenv ("ST")) p.stableFrames = std::atoi (e);
    if (const char* e = std::getenv ("YT")) p.yinThreshold = (float) std::atof (e);
    bool verbose = false;
    const char* wav = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (! std::strcmp (argv[i], "-v")) verbose = true;
        else wav = argv[i];
    }

    if (wav)
    {
        std::vector<float> a; double fs;
        if (! readWav (wav, a, fs)) { std::printf ("cannot read %s\n", wav); return 1; }
        auto ev = run (a, fs, p);
        for (auto& e : ev)
            std::printf ("%8.3f  %s %-4s vel %d\n", e.t, e.e.isNoteOn ? "ON " : "off", nn (e.e.note).c_str(), e.e.velocity);
        return 0;
    }

    const double fs = 48000.0;
    std::vector<Scenario> scs = {
        makeScenario ("open-ish low notes, slow", fs, 1, 23, 40, 0.30, 0.70, 0.0, 0.1, -80, 0.5),
        makeScenario ("mid register, medium",     fs, 2, 28, 52, 0.15, 0.40, 0.2, 0.2, -75, 0.3),
        makeScenario ("fast 16ths (~125 ms)",     fs, 3, 28, 50, 0.11, 0.14, 0.1, 0.3, -75, 0.2),
        makeScenario ("noisy input (-55 dB)",     fs, 4, 28, 50, 0.15, 0.40, 0.1, 0.2, -55, 0.3),
    };

    Stats tot;
    std::printf ("%-28s %5s %6s %4s %4s %4s %4s | lat ms low(<A1) med/p90  high med/p90\n",
                 "scenario", "notes", "corr%", "oct", "wrng", "miss", "xtra");
    for (auto& sc : scs)
    {
        auto ev = run (sc.audio, fs, p);
        if (verbose) std::printf ("[%s]\n", sc.name.c_str());
        auto s = evaluate (sc, ev, verbose);
        std::printf ("%-28s %5d %5.1f%% %4d %4d %4d %4d |   %5.1f/%5.1f      %5.1f/%5.1f\n", sc.name.c_str(), s.targets,
                     100.0 * s.correct / s.targets, s.octave, s.wrong, s.missed, s.extra,
                     pct (s.latLow, 0.5), pct (s.latLow, 0.9), pct (s.latHigh, 0.5), pct (s.latHigh, 0.9));
        tot.targets += s.targets; tot.correct += s.correct; tot.octave += s.octave; tot.wrong += s.wrong;
        tot.missed += s.missed; tot.extra += s.extra;
        tot.latLow.insert (tot.latLow.end(), s.latLow.begin(), s.latLow.end());
        tot.latHigh.insert (tot.latHigh.end(), s.latHigh.begin(), s.latHigh.end());
        tot.offLat.insert (tot.offLat.end(), s.offLat.begin(), s.offLat.end());
    }
    std::printf ("note-off latency after mute: median %.1f ms, p90 %.1f ms\n", pct (tot.offLat, 0.5), pct (tot.offLat, 0.9));
    std::printf ("%-28s %5d %5.1f%% %4d %4d %4d %4d |   %5.1f/%5.1f      %5.1f/%5.1f\n", "TOTAL", tot.targets,
                 100.0 * tot.correct / tot.targets, tot.octave, tot.wrong, tot.missed, tot.extra,
                 pct (tot.latLow, 0.5), pct (tot.latLow, 0.9), pct (tot.latHigh, 0.5), pct (tot.latHigh, 0.9));
    return 0;
}
