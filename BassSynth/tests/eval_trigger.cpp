// Evaluates the note detector (pick triggering) on annotated real bass lines:
// IDMT-SMT-BASS-SINGLE-TRACKS (Fraunhofer IDMT, CC BY-NC-ND 4.0), 17 lines with note annotations.
// Also checks that the oscillators run at the annotated pitch, with and without fundamental tracking.
//   eval_trigger <data dir> <out dir>
// <data dir>/single/audio/NNN.wav, <data dir>/single/misc/notes_csv/NNN_note_parameters.csv
// Writes <out dir>/report.txt, and per line a block-feature file (NNN.feat, float32 rows, 1 ms each)
// plus the annotated note starts (NNN.gt) for tuning the detector offline. No audio is written.
#include "../Source/SynthEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static bool readWav (const char* path, std::vector<float>& out, double& rateOut)
{
    FILE* f = std::fopen (path, "rb");
    if (! f) return false;
    char id[4]; uint32_t sz;
    if (std::fread (id, 1, 4, f) != 4 || std::fread (&sz, 4, 1, f) != 1 || std::fread (id, 1, 4, f) != 4) { std::fclose (f); return false; }
    uint16_t fmt = 0, ch = 0, bits = 0; uint32_t rate = 0;
    std::vector<uint8_t> data;
    while (std::fread (id, 1, 4, f) == 4 && std::fread (&sz, 4, 1, f) == 1)
    {
        if (! std::memcmp (id, "fmt ", 4))
        {
            std::vector<uint8_t> b (sz); if (std::fread (b.data(), 1, sz, f) != sz) break;
            std::memcpy (&fmt, &b[0], 2); std::memcpy (&ch, &b[2], 2); std::memcpy (&rate, &b[4], 4); std::memcpy (&bits, &b[14], 2);
            if (fmt == 0xFFFE && sz >= 26) std::memcpy (&fmt, &b[24], 2);
        }
        else if (! std::memcmp (id, "data", 4)) { data.resize (sz); if (std::fread (data.data(), 1, sz, f) != sz) break; }
        else std::fseek (f, (long) (sz + (sz & 1)), SEEK_CUR);
    }
    std::fclose (f);
    if (data.empty() || ch == 0) return false;
    rateOut = rate;
    const int bps = bits / 8; const size_t frames = data.size() / (size_t) (bps * ch);
    out.assign (frames, 0.0f);
    for (size_t i = 0; i < frames; ++i)
    {
        double acc = 0;
        for (int c = 0; c < ch; ++c)
        {
            const uint8_t* p = &data[(i * ch + (size_t) c) * (size_t) bps];
            double v = 0;
            if (fmt == 3 && bits == 32) { float x; std::memcpy (&x, p, 4); v = x; }
            else if (bits == 16) v = (int16_t) (p[0] | p[1] << 8) / 32768.0;
            else if (bits == 24) { int32_t x = (p[0] << 8) | (p[1] << 16) | ((int32_t) p[2] << 24); v = (x >> 8) / 8388608.0; }
            else if (bits == 32) { int32_t x; std::memcpy (&x, p, 4); v = x / 2147483648.0; }
            acc += v;
        }
        out[i] = (float) (acc / ch);
    }
    return true;
}

struct Gt { double on, off; int pitch; std::string ps, es; };

static std::vector<Gt> readCsv (const fs::path& p)
{
    std::vector<Gt> v; std::ifstream f (p); std::string line;
    while (std::getline (f, line))
    {
        std::vector<std::string> t; std::stringstream ss (line); std::string tok;
        while (std::getline (ss, tok, ',')) t.push_back (tok);
        if (t.size() < 7) continue;
        try { v.push_back ({ std::stod (t[0]), std::stod (t[1]), std::atoi (t[2].c_str()), t[5], t[6] }); } catch (...) {}
    }
    return v;
}

struct Stat { int notes = 0, miss = 0, extra = 0; std::vector<double> lat; };

// Does the oscillator run at the annotated pitch? Judged per note from the cycles it ran between
// 50 ms after the note's start and its end (at most 450 ms), and in 40 ms slices within that span.
struct Pitch
{
    int notes = 0, ok = 0, octUp = 0, between = 0, low = 0; double slices = 0, slicesOk = 0;
    void add (const std::vector<double>& osc, double sr, double on, double off, int midi)
    {
        const double t1 = on + 0.05, t2 = std::min (off - 0.01, on + 0.45);
        if (t2 - t1 < 0.06) return;
        const size_t i1 = (size_t) (t1 * sr), i2 = std::min (osc.size() - 1, (size_t) (t2 * sr));
        if (i2 <= i1) return;
        const double ref = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
        const double r = (osc[i2] - osc[i1]) / ((double) (i2 - i1) / sr) / ref;
        ++notes;
        if (r > 0.94 && r < 1.06) ++ok; else if (r > 1.88 && r < 2.12) ++octUp; else if (r <= 0.94) ++low; else ++between;
        const size_t w = (size_t) (0.04 * sr), h = (size_t) (0.02 * sr);
        for (size_t a = i1; a + w <= i2; a += h)
        {
            const double rs = (osc[a + w] - osc[a]) / 0.04 / ref;
            slices += 1; if (rs > 0.94 && rs < 1.06) slicesOk += 1;
        }
    }
    void merge (const Pitch& o) { notes += o.notes; ok += o.ok; octUp += o.octUp; between += o.between; low += o.low; slices += o.slices; slicesOk += o.slicesOk; }
    std::string text() const
    {
        char b[200];
        std::snprintf (b, sizeof b, "on pitch %3d/%3d (%.0f%%), octave up %d, between %d, low %d; time on pitch %.0f%%",
                       ok, notes, notes ? 100.0 * ok / notes : 0.0, octUp, between, low, slices > 0 ? 100.0 * slicesOk / slices : 0.0);
        return b;
    }
};

static double pct (std::vector<double> v, double q)
{
    if (v.empty()) return 0; std::sort (v.begin(), v.end());
    return v[std::min (v.size() - 1, (size_t) (q * (double) (v.size() - 1) + 0.5))];
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: eval_trigger <data dir> <out dir>\n"); return 1; }
    const fs::path root = argv[1], out = argv[2];
    fs::create_directories (out);
    FILE* rep = std::fopen ((out / "report.txt").string().c_str(), "w");
    auto say = [rep] (const std::string& s) { std::printf ("%s", s.c_str()); if (rep) std::fputs (s.c_str(), rep); };
    char buf[512];

    Stat all, fast, slow;
    // Oscillator pitch: the plugin's defaults, the oscillators as they were before fundamental tracking,
    // and a few settings of the tracker's internals. NS = lines without slap.
    struct Var { const char* name; void (*set) (bsyn::Params&); Pitch all, ns; };
    std::vector<Var> vars = {
        { "tracking on (defaults)", [] (bsyn::Params&) {} },
        { "tracking off",           [] (bsyn::Params& q) { q.fundTrack = false; } },
        { "ringing-note test off",  [] (bsyn::Params& q) { q.ftRing = false; } },
        { "again on",               [] (bsyn::Params& q) { q.ftAgain = true; } },
        { "grew x1.25",             [] (bsyn::Params& q) { q.ftOwn = 1.25f; } },
        { "grew x2.5",              [] (bsyn::Params& q) { q.ftOwn = 2.5f; } },
        { "filter at 1.35 x",       [] (bsyn::Params& q) { q.ftMul = 1.35f; } },
        { "filter at 0.9 x",        [] (bsyn::Params& q) { q.ftMul = 0.9f; } },
        { "dip depth 0.15",         [] (bsyn::Params& q) { q.ftDelta = 0.15f; } },
        { "periodic < 0.15",        [] (bsyn::Params& q) { q.ftThr = 0.15f; } },
        { "periodic < 0.4",         [] (bsyn::Params& q) { q.ftThr = 0.4f; } },
        { "hold 1 ms",              [] (bsyn::Params& q) { q.ftHold = 1; } },
        { "window 10 ms",           [] (bsyn::Params& q) { q.ftWin = 10.0f; } },
        { "window 20 ms",           [] (bsyn::Params& q) { q.ftWin = 20.0f; } },
        { "no lag correction",      [] (bsyn::Params& q) { q.ftComp = false; } },
        { "inertia off",            [] (bsyn::Params& q) { q.inertia = 0.0f; } },
        { "inertia 0.5",            [] (bsyn::Params& q) { q.inertia = 0.5f; } },
    };
    FILE* nf = std::fopen ((out / "notes.csv").string().c_str(), "w");
    if (nf) std::fprintf (nf, "line,on,off,midi,ref_hz,ps,es,r_on,r_off,found_ok,found_none,found_up,found_low,first_ok_ms,cut_ratio,wind,h1_db,h2_db,h3_db\n");
    int lines = 0;
    for (int n = 1; n <= 17; ++n)
    {
        char id[8]; std::snprintf (id, sizeof id, "%03d", n);
        const auto wav = root / "single" / "audio" / (std::string (id) + ".wav");
        const auto csv = root / "single" / "misc" / "notes_csv" / (std::string (id) + "_note_parameters.csv");
        std::vector<float> a; double sr = 44100;
        if (! fs::exists (wav) || ! fs::exists (csv) || ! readWav (wav.string().c_str(), a, sr)) continue;
        const auto gt = readCsv (csv);
        if (gt.empty()) continue;
        ++lines;

        // level: loudest peak at -6 dBFS, as the Input knob's help text asks for
        float pk = 1e-9f; for (float v : a) pk = std::max (pk, std::abs (v));
        const float g = 0.5f / pk;

        bsyn::Params p;                       // the plugin's defaults
        bsyn::Engine eng; eng.prepare (sr); eng.setParams (p);
        bsyn::Params q = p; q.comp = false;   // probe pass: uncompressed, for the feature dump
        bsyn::Engine prb; prb.prepare (sr); prb.setParams (q);
        std::vector<bsyn::Engine> ve (vars.size());
        std::vector<std::vector<double>> osc (vars.size(), std::vector<double> (a.size()));
        for (size_t v = 0; v < vars.size(); ++v) { bsyn::Params pv = p; vars[v].set (pv); ve[v].prepare (sr); ve[v].setParams (pv); }
        std::vector<float> fnd (a.size()), cut (a.size()), wnd (a.size());      // the default tracker's view

        const int BL = std::max (1, (int) std::lround (0.001 * sr));
        std::vector<int64_t> trig;
        std::vector<float> feat;
        uint32_t last = 0;
        // 30 Hz high-pass and a 1 kHz high-pass of the input for the block features
        const double hpA = std::exp (-6.283185307179586 * 30.0 / sr), hfA = std::exp (-6.283185307179586 * 1000.0 / sr);
        double hX = 0, hY = 0, fX = 0, fY = 0;
        double bMax = 0, mMax = 0, mMin = 1e9, dSum = 0, dMin = 1e9, dMax = -1e9, fMax = 0; int bc = 0; bool tb = false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            const float x = a[i] * g;
            eng.process (x);
            prb.process (x);
            for (size_t v = 0; v < vars.size(); ++v) { ve[v].process (x); osc[v][i] = ve[v].probeOscPhase(); }
            fnd[i] = (float) ve[0].probeFundHz(); cut[i] = (float) ve[0].probeTrackHz(); wnd[i] = (float) ve[0].probeWinding();
            if (eng.triggers() != last) { last = eng.triggers(); trig.push_back ((int64_t) i); tb = true; }
            const double x0 = x * std::pow (10.0, p.inGainDb / 20.0);
            const double hp = hpA * (hY + x0 - hX); hX = x0; hY = hp;
            const double hf = hfA * (fY + x0 - fX); fX = x0; fY = hf;
            bMax = std::max (bMax, std::abs (hp)); fMax = std::max (fMax, std::abs (hf));
            const double m = prb.probeMagnitude(), d = prb.probePhaseStep();
            mMax = std::max (mMax, m); mMin = std::min (mMin, m);
            dSum += d; dMin = std::min (dMin, d); dMax = std::max (dMax, d);
            if (++bc >= BL)
            {
                const float row[10] = { (float) bMax, (float) mMax, (float) mMin, (float) (dSum / bc), (float) dMin, (float) dMax,
                                        (float) fMax, (float) prb.probePeriod(), tb ? 1.0f : 0.0f, eng.gateIsOpen() ? 1.0f : 0.0f };
                feat.insert (feat.end(), row, row + 10);
                bc = 0; bMax = 0; mMax = 0; mMin = 1e9; dSum = 0; dMin = 1e9; dMax = -1e9; fMax = 0; tb = false;
            }
        }
        { FILE* f = std::fopen ((out / (std::string (id) + ".feat")).string().c_str(), "wb");
          if (f) { std::fwrite (feat.data(), sizeof (float), feat.size(), f); std::fclose (f); } }
        { FILE* f = std::fopen ((out / (std::string (id) + ".gt")).string().c_str(), "w");
          if (f) { std::fprintf (f, "# sr %.0f block %d\n", sr, BL);
                   for (auto& e : gt) std::fprintf (f, "%.5f %.5f %d %s %s\n", e.on, e.off, e.pitch, e.ps.c_str(), e.es.c_str()); std::fclose (f); } }

        // match: each annotated start takes the nearest unused trigger from 45 ms before to 60 ms after
        // (the annotations mark where the tone starts; the finger or pick touches the string up to ~40 ms earlier)
        Stat s; std::vector<char> used (trig.size(), 0);
        for (size_t k = 0; k < gt.size(); ++k)
        {
            const double on = gt[k].on * sr;
            const double ioi = k > 0 ? (gt[k].on - gt[k - 1].on) * 1000.0 : 1e9;
            int best = -1; double bd = 1e18;
            for (size_t j = 0; j < trig.size(); ++j)
            {
                const double d = (double) trig[j] - on;
                if (used[j] || d < -0.045 * sr || d > 0.06 * sr) continue;
                if (std::abs (d) < bd) { bd = std::abs (d); best = (int) j; }
            }
            Stat& grp = ioi < 150.0 ? fast : slow;
            ++s.notes; ++grp.notes;
            if (best < 0) { ++s.miss; ++grp.miss; }
            else { used[(size_t) best] = 1; const double l = ((double) trig[(size_t) best] - on) / sr * 1000.0; s.lat.push_back (l); grp.lat.push_back (l); }
        }
        for (char u : used) if (! u) ++s.extra;
        std::vector<double> iois; for (size_t k = 1; k < gt.size(); ++k) iois.push_back ((gt[k].on - gt[k - 1].on) * 1000.0);
        std::snprintf (buf, sizeof buf, "line %s (%s/%s, %.1fs, median gap %.0f ms): notes %3d  missed %3d  false %3d  latency med %.1f p90 %.1f ms\n",
                       id, gt[0].ps.c_str(), gt[0].es.c_str(), a.size() / sr, pct (iois, 0.5), s.notes, s.miss, s.extra, pct (s.lat, 0.5), pct (s.lat, 0.9));
        say (buf);
        all.notes += s.notes; all.miss += s.miss; all.extra += s.extra; all.lat.insert (all.lat.end(), s.lat.begin(), s.lat.end());

        bool slap = false;
        for (auto& e : gt) slap = slap || e.ps == "ST" || e.ps == "SP";
        for (size_t v = 0; v < vars.size(); ++v)
        {
            Pitch pv;
            for (auto& e : gt) pv.add (osc[v], sr, e.on, e.off, e.pitch);
            if (v < 2) say (std::string ("    oscillator, ") + vars[v].name + ": " + pv.text() + "\n");
            vars[v].all.merge (pv);
            if (! slap) vars[v].ns.merge (pv);
        }
        // one row per note: what the tracker saw (for looking into the notes that go wrong)
        for (auto& e : gt)
        {
            const double t1 = e.on + 0.05, t2 = std::min (e.off - 0.01, e.on + 0.45);
            if (t2 - t1 < 0.06 || ! nf) continue;
            const size_t i1 = (size_t) (t1 * sr), i2 = std::min (a.size() - 1, (size_t) (t2 * sr));
            if (i2 <= i1) continue;
            const double ref = 440.0 * std::pow (2.0, (e.pitch - 69) / 12.0), span = (double) (i2 - i1) / sr;
            int ok = 0, none = 0, up = 0, low = 0, cnt = 0; double firstOk = -1, cr = 0, wd = 0; int wn = 0;
            for (size_t i = (size_t) (e.on * sr); i < i2; i += 44)
            {
                const double r = fnd[i] / ref;
                if (firstOk < 0 && r > 0.94 && r < 1.06) firstOk = ((double) i / sr - e.on) * 1000.0;
                if (i < i1) continue;
                ++cnt; cr += cut[i] / ref;
                if (wnd[i] > 0) { wd += wnd[i]; ++wn; }
                if (fnd[i] <= 0) ++none; else if (r > 0.94 && r < 1.06) ++ok; else if (r >= 1.06) ++up; else ++low;
            }
            // level of the first three harmonics in the plugin's input (dB re full scale)
            double h[3];
            for (int k = 1; k <= 3; ++k)
            {
                double c = 0, q = 0; const double w = 6.283185307179586 * ref * k / sr;
                for (size_t i = i1; i < i2; ++i) { c += a[i] * g * std::cos (w * (double) (i - i1)); q += a[i] * g * std::sin (w * (double) (i - i1)); }
                h[k - 1] = 20.0 * std::log10 (2.0 * std::sqrt (c * c + q * q) / (double) (i2 - i1) + 1e-9);
            }
            std::fprintf (nf, "%s,%.3f,%.3f,%d,%.1f,%s,%s,%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.0f,%.2f,%.2f,%.1f,%.1f,%.1f\n",
                          std::string (id).c_str(), e.on, e.off, e.pitch, ref, e.ps.c_str(), e.es.c_str(),
                          (osc[0][i2] - osc[0][i1]) / span / ref, (osc[1][i2] - osc[1][i1]) / span / ref,
                          cnt ? (double) ok / cnt : 0.0, cnt ? (double) none / cnt : 0.0, cnt ? (double) up / cnt : 0.0, cnt ? (double) low / cnt : 0.0,
                          firstOk, cnt ? cr / cnt : 0.0, wn ? wd / wn : 0.0, h[0], h[1], h[2]);
        }
    }

    say ("\nOscillator pitch against the annotated notes, 50-450 ms into each note (all lines)\n");
    for (auto& v : vars) { char b2[64]; std::snprintf (b2, sizeof b2, "  %-24s", v.name); say (b2 + v.all.text() + "\n"); }
    say ("Oscillator pitch, lines without slap\n");
    for (auto& v : vars) { char b2[64]; std::snprintf (b2, sizeof b2, "  %-24s", v.name); say (b2 + v.ns.text() + "\n"); }
    if (nf) std::fclose (nf);
    if (rep) std::fclose (rep);
    return lines > 0 ? 0 : 2;
}
