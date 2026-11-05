// Evaluates BassTracker on the IDMT-SMT-Bass datasets (Fraunhofer IDMT, CC BY-NC-ND 4.0).
//   eval_idmt <data dir> [config ...]
// <data dir>/notes  : IDMT-SMT-BASS (single notes, file name gives string + fret)
// <data dir>/single : IDMT-SMT-BASS-SINGLE-TRACKS (17 bass lines with note annotations)
// A config is "key=value,key=value" (see applyConfig). Several configs are compared side by side.
#define main bench_main
#include "bench.cpp"
#undef main

#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>

namespace fs = std::filesystem;

struct Config
{
    std::string name = "default";
    TrackerParams p;
    std::map<std::string, float> extra;   // tracker tuning members
};

static Config parseConfig (const std::string& s)
{
    Config c; c.name = s.empty() ? "default" : s;
    std::stringstream ss (s); std::string kv;
    while (std::getline (ss, kv, ','))
    {
        auto eq = kv.find ('=');
        if (eq == std::string::npos) continue;
        const std::string k = kv.substr (0, eq); const float v = std::stof (kv.substr (eq + 1));
        if (k == "thr") c.p.thresholdDb = v;
        else if (k == "yt") c.p.yinThreshold = v;
        else if (k == "st") c.p.stableFrames = (int) v;
        else if (k == "rt") c.p.retriggerDb = v;
        else c.extra[k] = v;
    }
    return c;
}

static void applyExtra (BassTracker& tr, const Config& c)
{
    for (auto& [k, v] : c.extra)
    {
        if (k == "hff") tr.hfFactor = v;
        else if (k == "wmin") tr.wMinFactor = v;
        else if (k == "oct") tr.confirmOctave = v != 0;
        else if (k == "skip") tr.skipAfterOnset = (int) v;
        else if (k == "mute") tr.muteDropDb = v;
        else if (k == "hyst") tr.hysteresis = v;
    }
}

static std::vector<Ev> runCfg (const std::vector<float>& a, double fs, const Config& c)
{
    BassTracker tr; tr.prepare (fs); tr.setParams (c.p); applyExtra (tr, c);
    std::vector<Ev> ev;
    const int block = 256;
    for (int pos = 0; pos < (int) a.size(); pos += block)
    {
        const int n = std::min (block, (int) a.size() - pos);
        tr.process (a.data() + pos, n, [&] (const NoteEvent& e) { ev.push_back ({ e, (pos + e.sampleOffset) / fs }); });
    }
    return ev;
}

// ------------------------------------------------------------------ single notes
struct NoteFile { std::string path, ps, es; int pitch; };

struct NoteStats
{
    int files = 0, correct = 0, octave = 0, wrong = 0, missed = 0, glitch = 0, deadFired = 0, deadFiles = 0;
    std::vector<double> lat;
    void add (const NoteStats& o)
    {
        files += o.files; correct += o.correct; octave += o.octave; wrong += o.wrong; missed += o.missed;
        glitch += o.glitch; deadFired += o.deadFired; deadFiles += o.deadFiles;
        lat.insert (lat.end(), o.lat.begin(), o.lat.end());
    }
};

static std::vector<NoteFile> listNotes (const fs::path& dir, int stride)
{
    std::vector<NoteFile> v;
    if (! fs::exists (dir)) return v;
    std::vector<fs::path> paths;
    for (auto& e : fs::recursive_directory_iterator (dir))
        if (e.path().extension() == ".wav") paths.push_back (e.path());
    std::sort (paths.begin(), paths.end());
    static const int open[] = { 28, 33, 38, 43 };
    int k = 0;
    for (auto& p : paths)
    {
        // BS_1_EQ_3_PS_FS_ES_BEQ_1_10.wav  -> ... PS_<ps> ES_<es> <string> <fret>
        std::vector<std::string> t; std::stringstream ss (p.stem().string()); std::string tok;
        while (std::getline (ss, tok, '_')) t.push_back (tok);
        if (t.size() < 10) continue;
        const int str = std::atoi (t[t.size() - 2].c_str()), fret = std::atoi (t.back().c_str());
        if (str < 1 || str > 4) continue;
        NoteFile nf { p.string(), t[5], t[7], open[str - 1] + fret };
        if (nf.es == "HA") continue;                      // harmonics: not needed
        if (k++ % stride) continue;
        v.push_back (nf);
    }
    return v;
}

static double estimateOnset (const std::vector<float>& a, double fs)
{
    float mx = 0; for (float x : a) mx = std::max (mx, std::abs (x));
    if (mx <= 0) return 0;
    size_t i = 0;
    while (i < a.size() && std::abs (a[i]) < 0.1f * mx) ++i;
    // walk back to where the attack starts (below 2 % of max for 2 ms)
    const size_t lookback = (size_t) (0.02 * fs);
    size_t j = i;
    size_t quiet = 0;
    while (j > 0 && i - j < lookback)
    {
        --j;
        if (std::abs (a[j]) < 0.02f * mx) { if (++quiet > (size_t) (0.002 * fs)) break; }
        else quiet = 0;
    }
    return (j + quiet) / fs;
}

static NoteStats evalNoteFile (const NoteFile& nf, const Config& c)
{
    NoteStats s;
    std::vector<float> a; double fs;
    if (! readWav (nf.path.c_str(), a, fs)) return s;
    auto ev = runCfg (a, fs, c);
    if (nf.es == "DN")
    {
        ++s.deadFiles;
        for (auto& e : ev) if (e.e.isNoteOn) { ++s.deadFired; break; }
        return s;
    }
    ++s.files;
    const double on = estimateOnset (a, fs);
    const Ev* first = nullptr;
    for (auto& e : ev) if (e.e.isNoteOn) { first = &e; break; }
    if (! first) { ++s.missed; return s; }
    const int d = first->e.note - nf.pitch;
    if (d == 0) { ++s.correct; s.lat.push_back ((first->t - on) * 1000.0); }
    else if (d % 12 == 0) ++s.octave;
    else ++s.wrong;
    // glitches: any further note-on that is not the target pitch (bends/slides may move +-2)
    const int tol = (nf.es == "SLU" || nf.es == "SLD") ? 12 : (nf.es.rfind ("BE", 0) == 0 || nf.es.rfind ("VI", 0) == 0) ? 2 : 0;
    for (auto& e : ev)
        if (e.e.isNoteOn && &e != first && std::abs (e.e.note - nf.pitch) > tol) ++s.glitch;
    return s;
}

// ------------------------------------------------------------------ bass lines
struct GtNote { double on, off; int pitch; std::string ps, es; };

struct LineStats
{
    int gt = 0, det = 0, tp = 0, octave = 0, wrongPitch = 0;
    std::vector<double> lat;
    void add (const LineStats& o)
    {
        gt += o.gt; det += o.det; tp += o.tp; octave += o.octave; wrongPitch += o.wrongPitch;
        lat.insert (lat.end(), o.lat.begin(), o.lat.end());
    }
};

static std::vector<GtNote> readCsv (const fs::path& p)
{
    std::vector<GtNote> v; std::ifstream f (p); std::string line;
    while (std::getline (f, line))
    {
        std::vector<std::string> t; std::stringstream ss (line); std::string tok;
        while (std::getline (ss, tok, ',')) t.push_back (tok);
        if (t.size() < 7) continue;
        v.push_back ({ std::stod (t[0]), std::stod (t[1]), std::atoi (t[2].c_str()), t[5], t[6] });
    }
    return v;
}

static LineStats evalLine (const fs::path& wav, const fs::path& csv, const Config& c, bool verbose)
{
    LineStats s;
    std::vector<float> a; double fs;
    if (! readWav (wav.string().c_str(), a, fs)) return s;
    auto gt = readCsv (csv);
    auto ev = runCfg (a, fs, c);

    // Annotation times are aligned to the score grid; estimate the constant offset of
    // detections first (median of nearest same-pitch onsets), then match with that offset.
    std::vector<double> offs;
    for (auto& g : gt)
    {
        if (g.es == "DN" || g.es == "HA") continue;
        double best = 1e9;
        for (auto& e : ev) if (e.e.isNoteOn && e.e.note == g.pitch && std::abs (e.t - g.on) < std::abs (best)) best = e.t - g.on;
        if (std::abs (best) < 0.15) offs.push_back (best);
    }
    const double off = offs.empty() ? 0.0 : pct (offs, 0.5);

    std::vector<bool> used (ev.size(), false);
    std::vector<bool> ignore (ev.size(), false);
    for (auto& g : gt)
    {
        const bool skip = g.es == "DN" || g.es == "HA";
        // detections attached to dead notes / harmonics are neither hits nor false alarms
        for (size_t k = 0; k < ev.size(); ++k)
            if (ev[k].e.isNoteOn && ! used[k] && ev[k].t >= g.on - 0.03 && ev[k].t < g.on + 0.1 + std::max (0.0, off))
            {
                if (skip) { ignore[k] = true; continue; }
            }
        if (skip) continue;
        ++s.gt;
        int hit = -1, cand = -1;
        for (size_t k = 0; k < ev.size(); ++k)
        {
            if (! ev[k].e.isNoteOn || used[k] || ignore[k]) continue;
            const double dt = ev[k].t - g.on - off;
            if (dt < -0.05 || dt > 0.08) continue;
            if (ev[k].e.note == g.pitch) { hit = (int) k; break; }
            if (cand < 0) cand = (int) k;
        }
        if (hit >= 0) { used[(size_t) hit] = true; ++s.tp; s.lat.push_back ((ev[(size_t) hit].t - g.on) * 1000.0); }
        else if (cand >= 0)
        {
            const int d = ev[(size_t) cand].e.note - g.pitch;
            if (d % 12 == 0) ++s.octave; else ++s.wrongPitch;
            if (verbose) std::printf ("    %s %7.3f want %-4s got %-4s (%s/%s)\n", wav.stem().string().c_str(), g.on, nn (g.pitch).c_str(), nn (ev[(size_t) cand].e.note).c_str(), g.ps.c_str(), g.es.c_str());
        }
        else if (verbose)
            std::printf ("    %s %7.3f miss %-4s (%s/%s)\n", wav.stem().string().c_str(), g.on, nn (g.pitch).c_str(), g.ps.c_str(), g.es.c_str());
    }
    for (size_t k = 0; k < ev.size(); ++k) if (ev[k].e.isNoteOn && ! ignore[k]) ++s.det;
    return s;
}

// ------------------------------------------------------------------ main
template <typename Fn>
static void parallelFor (size_t n, Fn&& fn)
{
    std::atomic<size_t> next { 0 };
    const unsigned nt = std::max (1u, std::thread::hardware_concurrency());
    std::vector<std::thread> th;
    for (unsigned t = 0; t < nt; ++t)
        th.emplace_back ([&] { for (size_t i; (i = next++) < n;) fn (i); });
    for (auto& t : th) t.join();
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: eval_idmt <data dir> [config ...]\n"); return 1; }
    const fs::path root = argv[1];
    std::vector<Config> cfgs;
    for (int i = 2; i < argc; ++i) cfgs.push_back (parseConfig (argv[i]));
    if (cfgs.empty()) cfgs.push_back (parseConfig (""));
    const bool verbose = std::getenv ("VERBOSE") != nullptr;
    const int stride = std::getenv ("STRIDE") ? std::atoi (std::getenv ("STRIDE")) : 1;

    auto notes = listNotes (root / "notes", stride);
    std::vector<std::pair<fs::path, fs::path>> lines;
    for (int i = 1; i <= 17; ++i)
    {
        char id[8]; std::snprintf (id, sizeof id, "%03d", i);
        auto w = root / "single" / "audio" / (std::string (id) + ".wav");
        auto c = root / "single" / "misc" / "notes_csv" / (std::string (id) + "_note_parameters.csv");
        if (fs::exists (w) && fs::exists (c)) lines.push_back ({ w, c });
    }
    std::printf ("single notes: %zu files (harmonics excluded)   bass lines: %zu\n\n", notes.size(), lines.size());

    for (auto& c : cfgs)
    {
        std::printf ("=== config: %s\n", c.name.c_str());

        // single notes, grouped by plucking / expression style
        std::vector<NoteStats> per (notes.size());
        parallelFor (notes.size(), [&] (size_t i) { per[i] = evalNoteFile (notes[i], c); });
        std::map<std::string, NoteStats> groups; NoteStats all;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const std::string g = notes[i].es == "NO" ? "PS_" + notes[i].ps : "ES_" + notes[i].es;
            groups[g].add (per[i]); all.add (per[i]);
        }
        std::printf ("  single notes        files  corr%%   oct  wrong  miss  glitch  lat med/p90 ms\n");
        auto row = [] (const std::string& name, const NoteStats& s)
        {
            if (s.files == 0)
            {
                if (s.deadFiles) std::printf ("  %-18s %6d  dead notes that fired a note: %.1f%%\n", name.c_str(), s.deadFiles, 100.0 * s.deadFired / s.deadFiles);
                return;
            }
            std::printf ("  %-18s %6d %6.1f %5d %6d %5d %7d  %5.1f / %5.1f\n", name.c_str(), s.files, 100.0 * s.correct / s.files,
                         s.octave, s.wrong, s.missed, s.glitch, pct (s.lat, 0.5), pct (s.lat, 0.9));
        };
        for (auto& [g, s] : groups) row (g, s);
        row ("ALL", all);

        // bass lines
        std::vector<LineStats> ls (lines.size());
        parallelFor (lines.size(), [&] (size_t i) { ls[i] = evalLine (lines[i].first, lines[i].second, c, false); });
        LineStats tot;
        std::printf ("\n  bass line   notes  recall  precision  F1     oct  wrong\n");
        for (size_t i = 0; i < lines.size(); ++i)
        {
            auto& s = ls[i];
            const double r = s.gt ? (double) s.tp / s.gt : 0, p = s.det ? (double) s.tp / s.det : 0;
            std::printf ("  %-10s %6d  %5.1f%%   %5.1f%%   %5.1f%%  %4d %5d\n", lines[i].first.stem().string().c_str(), s.gt,
                         100 * r, 100 * p, (r + p) > 0 ? 200 * r * p / (r + p) : 0.0, s.octave, s.wrongPitch);
            tot.add (s);
        }
        const double r = tot.gt ? (double) tot.tp / tot.gt : 0, p = tot.det ? (double) tot.tp / tot.det : 0;
        std::printf ("  %-10s %6d  %5.1f%%   %5.1f%%   %5.1f%%  %4d %5d\n", "ALL", tot.gt, 100 * r, 100 * p,
                     (r + p) > 0 ? 200 * r * p / (r + p) : 0.0, tot.octave, tot.wrongPitch);
        std::printf ("  SUMMARY %-40s notes %.1f%%  lines F1 %.1f%%  note lat %.1f ms\n\n", c.name.c_str(),
                     all.files ? 100.0 * all.correct / all.files : 0.0, (r + p) > 0 ? 200 * r * p / (r + p) : 0.0, pct (all.lat, 0.5));

        if (verbose)
            for (auto& l : lines) evalLine (l.first, l.second, c, true);
    }
    return 0;
}
