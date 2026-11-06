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
#include <array>

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
        else if (k == "reqon") tr.requireOnset = v != 0;
        else if (k == "rise") tr.retrigRiseDb = v;
        else if (k == "legato") tr.legatoRangeDb = v;
        else if (k == "sub") tr.subFactor = v;
        else if (k == "amb") tr.ambigSemis = v;
        else if (k == "ambwait") { tr.ambigWaitMs = v; }
        else if (k == "bias") tr.attackBias = v;
        else if (k == "fast") tr.fastLockDip = v;
        else if (k == "glide") tr.glideSemis = v;
        else if (k == "margin") tr.onsetMarginDb = v;
        else if (k == "tail") tr.tailDb = v;
        else if (k == "rel") tr.onsetRelDb = v;
        else if (k == "reldecay") tr.relDecayDbPerSec = v;
        else if (k == "drop") tr.retrigDropDb = v;
        else if (k == "octleg") tr.octLegatoExtra = (int) v;
        else if (k == "relgate") tr.relGateDb = v;
        else if (k == "est") tr.estimator = (int) v;
        else if (k == "mpmk") tr.mpmK = v;
        else if (k == "clar") tr.mpmClarityScale = v;
        else if (k == "biasms") tr.attackBiasMs = v;
    }
}

static thread_local std::vector<double> tlOnsetAtNoteOn;   // tracker's own onset time for each note-on

static std::vector<Ev> runCfg (const std::vector<float>& a, double fs, const Config& c,
                               std::vector<std::pair<double, float>>* freqs = nullptr,
                               std::vector<float>* levels = nullptr)
{
    tlOnsetAtNoteOn.clear();
    BassTracker tr; applyExtra (tr, c); tr.prepare (fs); tr.setParams (c.p);
    std::vector<Ev> ev;
    const int block = freqs ? 64 : 256;
    for (int pos = 0; pos < (int) a.size(); pos += block)
    {
        const int n = std::min (block, (int) a.size() - pos);
        tr.process (a.data() + pos, n, [&] (const NoteEvent& e)
        {
            ev.push_back ({ e, (pos + e.sampleOffset) / fs });
            tlOnsetAtNoteOn.push_back (e.isNoteOn ? tr.getLastOnsetSeconds() : -1.0);
        });
        if (freqs) freqs->push_back ({ (pos + n) / fs, tr.getLastFreq() });
        if (levels) levels->push_back (tr.getLastLevelDb());
    }
    return ev;
}

// ------------------------------------------------------------------ single notes
struct NoteFile { std::string path, ps, es; int pitch; };

struct NoteStats
{
    int files = 0, correct = 0, octave = 0, wrong = 0, missed = 0, glitch = 0, deadFired = 0, deadFiles = 0, settled = 0;
    std::vector<double> lat, onsetErr, detLat, firstVoiced, firstRight;   // latency; tracker onset - true onset; note-on - tracker onset
    std::map<int, std::vector<double>> latByString;
    std::map<std::string, std::vector<double>> cents;   // per bass: tuning deviation of plain notes
    std::map<int, int> diffs;                            // first-note error in semitones
    std::map<std::string, int> glitchKinds;              // "interval@timebucket" -> count
    std::map<std::string, std::array<std::vector<double>, 5>> glide;  // per style: cents vs time after onset
    void add (const NoteStats& o)
    {
        files += o.files; correct += o.correct; settled += o.settled; octave += o.octave; wrong += o.wrong; missed += o.missed;
        glitch += o.glitch; deadFired += o.deadFired; deadFiles += o.deadFiles;
        lat.insert (lat.end(), o.lat.begin(), o.lat.end());
        onsetErr.insert (onsetErr.end(), o.onsetErr.begin(), o.onsetErr.end());
        firstVoiced.insert (firstVoiced.end(), o.firstVoiced.begin(), o.firstVoiced.end());
        firstRight.insert (firstRight.end(), o.firstRight.begin(), o.firstRight.end());
        detLat.insert (detLat.end(), o.detLat.begin(), o.detLat.end());
        for (auto& [k, v] : o.latByString) latByString[k].insert (latByString[k].end(), v.begin(), v.end());
        for (auto& [k, v] : o.cents) cents[k].insert (cents[k].end(), v.begin(), v.end());
        for (auto& [k, v] : o.diffs) diffs[k] += v;
        for (auto& [k, v] : o.glitchKinds) glitchKinds[k] += v;
        for (auto& [k, arr] : o.glide) for (int w = 0; w < 5; ++w) glide[k][(size_t) w].insert (glide[k][(size_t) w].end(), arr[(size_t) w].begin(), arr[(size_t) w].end());
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
        // two naming schemes:  BS_1_EQ_1_FS_NO_<s>_<f>   and   BS_1_EQ_1_PS_FS_ES_BEQ_<s>_<f>
        if (t.size() < 8) continue;
        const bool longForm = t.size() >= 10 && t[4] == "PS";
        const std::string ps = longForm ? t[5] : t[4], es = longForm ? t[7] : t[5];
        const int str = std::atoi (t[t.size() - 2].c_str()), fret = std::atoi (t.back().c_str());
        if (str < 1 || str > 4) continue;
        NoteFile nf { p.string(), ps, es, open[str - 1] + fret };
        if (nf.es == "HA") continue;                      // harmonics: not needed
        // this session reads a steady semitone off its labels on ~half the files (labeling / tuning issue)
        if (p.filename().string().rfind ("BS_3_EQ_1_FS_NO", 0) == 0) continue;
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

static bool gVerbose = false;

static NoteStats evalNoteFile (const NoteFile& nf, const Config& c)
{
    NoteStats s;
    std::vector<float> a; double fs;
    if (! readWav (nf.path.c_str(), a, fs)) return s;
    std::vector<std::pair<double, float>> fr;
    std::vector<float> lv;
    auto ev = runCfg (a, fs, c, (nf.es == "NO" || gVerbose) ? &fr : nullptr, gVerbose ? &lv : nullptr);
    if (nf.es == "DN")
    {
        ++s.deadFiles;
        for (auto& e : ev) if (e.e.isNoteOn) { ++s.deadFired; break; }
        return s;
    }
    ++s.files;
    const double on = estimateOnset (a, fs);
    if (! fr.empty())
    {
        std::vector<double> cs;
        for (auto& [t, f] : fr)
            if (f > 0 && t > on + 0.1 && t < on + 0.6)
            {
                double c2 = 1200.0 * std::log2 (f / hz (nf.pitch));
                if (std::abs (c2) < 150) cs.push_back (c2);
            }
        if (cs.size() > 5) s.cents[fs::path (nf.path).filename().string().substr (0, 4)].push_back (pct (cs, 0.5));
        for (auto& [t, f] : fr) if (f > 0 && t > on) { s.firstVoiced.push_back ((t - on) * 1000.0); break; }
        for (auto& [t, f] : fr)
            if (f > 0 && t > on && std::abs (1200.0 * std::log2 (f / hz (nf.pitch))) < 50) { s.firstRight.push_back ((t - on) * 1000.0); break; }
        static const double edges[] = { 0.02, 0.04, 0.07, 0.12, 0.25, 0.5 };
        for (int w = 0; w < 5; ++w)
        {
            std::vector<double> c3;
            for (auto& [t, f] : fr)
                if (f > 0 && t > on + edges[w] && t <= on + edges[w + 1])
                {
                    const double c2 = 1200.0 * std::log2 (f / hz (nf.pitch));
                    if (std::abs (c2) < 150) c3.push_back (c2);
                }
            if (! c3.empty()) s.glide[nf.ps][(size_t) w].push_back (pct (c3, 0.5));
        }
    }
    const Ev* first = nullptr;
    for (auto& e : ev) if (e.e.isNoteOn) { first = &e; break; }
    if (! first) { ++s.missed; return s; }
    const int d = first->e.note - nf.pitch;
    if (d != 0) ++s.diffs[d];
    {
        // which note is sounding 150 ms after the first note-on (did it settle on the right pitch?)
        int cur = -1;
        for (auto& e : ev) { if (e.t > first->t + 0.15) break; if (e.e.isNoteOn) cur = e.e.note; else if (e.e.note == cur) cur = -1; }
        if (cur == nf.pitch) ++s.settled;
    }
    if (d == 0)
    {
        ++s.correct; s.lat.push_back ((first->t - on) * 1000.0);
        const double trOn = tlOnsetAtNoteOn[(size_t) (first - ev.data())];
        s.onsetErr.push_back ((trOn - on) * 1000.0);
        s.detLat.push_back ((first->t - trOn) * 1000.0);
        s.latByString[(nf.pitch >= 43) ? 4 : (nf.pitch >= 38) ? 3 : (nf.pitch >= 33) ? 2 : 1].push_back ((first->t - on) * 1000.0);
    }
    else if (d % 12 == 0) ++s.octave;
    else ++s.wrong;
    if (gVerbose && d != 0)
    {
        std::string seq;
        for (auto& e : ev) if (e.e.isNoteOn) seq += nn (e.e.note) + "@" + std::to_string ((int) ((e.t - on) * 1000)) + " ";
        std::printf ("    NOTE %s want %s : %s\n", fs::path (nf.path).filename().string().c_str(), nn (nf.pitch).c_str(), seq.c_str());
    }
    // glitches: any further note-on that is not the target pitch (bends/slides may move +-2)
    const int tol = (nf.es == "SLU" || nf.es == "SLD") ? 12 : (nf.es.rfind ("BE", 0) == 0 || nf.es.rfind ("VI", 0) == 0) ? 2 : 0;
    for (auto& e : ev)
        if (e.e.isNoteOn && &e != first && std::abs (e.e.note - nf.pitch) > tol)
        {
            ++s.glitch;
            const double dt = e.t - on;
            const char* b = dt < 0 ? "pre" : dt < 0.1 ? "<100ms" : dt < 0.5 ? "<500ms" : "late";
            const int iv = e.e.note - nf.pitch;
            const std::string ivs = std::abs (iv) == 1 || std::abs (iv) == 2 ? "+-1/2" : std::abs (iv) % 12 == 0 ? "oct" : std::abs (iv) == 7 || std::abs (iv) == 19 ? "5th" : "other";
            ++s.glitchKinds[ivs + "@" + b];
        }
    if (gVerbose && s.glitch > 0 && ! lv.empty())
    {
        float pk = -140; for (float l : lv) pk = std::max (pk, l);
        std::string seq;
        for (auto& e : ev)
        {
            const size_t idx = std::min (lv.size() - 1, (size_t) (e.t * fs / 64));
            char buf[64]; std::snprintf (buf, sizeof buf, "%s%s@%d(%.0fdB) ", e.e.isNoteOn ? "+" : "-", nn (e.e.note).c_str(), (int) ((e.t - on) * 1000), lv[idx]);
            seq += buf;
        }
        std::printf ("    GLITCH %s want %s peak %.0fdB len %.1fs : %s\n", fs::path (nf.path).filename().string().c_str(), nn (nf.pitch).c_str(), pk, a.size() / fs, seq.c_str());
    }
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
    if (verbose)
        for (size_t k = 0; k < ev.size(); ++k)
        {
            if (! ev[k].e.isNoteOn || ignore[k] || used[k]) continue;
            // nearest annotated note before this detection
            const GtNote* prev = nullptr;
            for (auto& g : gt) if (g.on <= ev[k].t - off + 0.05) prev = &g;
            std::printf ("    %s %7.3f EXTRA %-4s  (last gt: %s @%.3f-%.3f %s/%s)\n", wav.stem().string().c_str(), ev[k].t, nn (ev[k].e.note).c_str(),
                         prev ? nn (prev->pitch).c_str() : "-", prev ? prev->on : 0.0, prev ? prev->off : 0.0, prev ? prev->ps.c_str() : "", prev ? prev->es.c_str() : "");
        }
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
    gVerbose = verbose;
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
            std::string top;
            std::vector<std::pair<int, int>> dv; for (auto& [k, v] : s.diffs) dv.push_back ({ v, k });
            std::sort (dv.rbegin(), dv.rend());
            for (size_t i = 0; i < dv.size() && i < 4; ++i) top += (dv[i].second > 0 ? "+" : "") + std::to_string (dv[i].second) + ":" + std::to_string (dv[i].first) + " ";
            std::printf ("  %-18s %6d %6.1f %5d %6d %5d %7d  %5.1f / %5.1f   %s\n", name.c_str(), s.files, 100.0 * s.correct / s.files,
                         s.octave, s.wrong, s.missed, s.glitch, pct (s.lat, 0.5), pct (s.lat, 0.9), top.c_str());
        };
        for (auto& [g, s] : groups) row (g, s);
        row ("ALL", all);
        std::printf ("  correct pitch 150 ms after note-on: %.1f%%\n", all.files ? 100.0 * all.settled / all.files : 0.0);
        {
            std::vector<std::pair<int, std::string>> gk; for (auto& [k, v] : all.glitchKinds) gk.push_back ({ v, k });
            std::sort (gk.rbegin(), gk.rend());
            std::printf ("  glitch kinds:"); for (auto& [v, k] : gk) std::printf ("  %s:%d", k.c_str(), v); std::printf ("\n");
        }
        std::printf ("  latency split: tracker onset - audio onset med %.1f ms (p10 %.1f, p90 %.1f); note-on - tracker onset med %.1f ms (p90 %.1f)\n",
                     pct (all.onsetErr, 0.5), pct (all.onsetErr, 0.1), pct (all.onsetErr, 0.9), pct (all.detLat, 0.5), pct (all.detLat, 0.9));
        std::printf ("  pitch tracker: first voiced frame med %.1f ms, first frame within 50c of target med %.1f ms (p90 %.1f)\n",
                     pct (all.firstVoiced, 0.5), pct (all.firstRight, 0.5), pct (all.firstRight, 0.9));
        std::printf ("  latency by pitch range (E1-D#2 / A1.. / D2.. / G2..):");
        for (auto& [k, v] : all.latByString) std::printf ("  %.1f", pct (v, 0.5));
        std::printf ("\n");
        std::printf ("  pitch glide (median cents | %% beyond +-50c) at 20-40, 40-70, 70-120, 120-250, 250-500 ms:\n");
        for (auto& [k, arr] : all.glide)
        {
            std::printf ("    %s:", k.c_str());
            for (int w = 0; w < 5; ++w)
            {
                int o = 0; for (double x : arr[(size_t) w]) if (std::abs (x) > 50) ++o;
                std::printf ("  %+4.0f|%2.0f%%", pct (arr[(size_t) w], 0.5), arr[(size_t) w].empty() ? 0.0 : 100.0 * o / arr[(size_t) w].size());
            }
            std::printf ("\n");
        }
        {
            // semitone errors by recording session (bass / pickup setting / style)
            std::map<std::string, std::pair<int, int>> sess;   // key -> (errors +-1, files)
            for (size_t i = 0; i < notes.size(); ++i)
            {
                if (notes[i].es != "NO") continue;
                const std::string fn = fs::path (notes[i].path).filename().string();
                const std::string key = fn.substr (0, 9) + "_" + notes[i].ps;
                auto& e = sess[key];
                ++e.second;
                for (auto& [d, n] : per[i].diffs) if (std::abs (d) == 1) e.first += n;
            }
            std::string line;
            for (auto& [k, v] : sess) if (v.first >= 3) line += k + ":" + std::to_string (v.first) + "/" + std::to_string (v.second) + "  ";
            std::printf ("  +-1 semitone errors by session: %s\n", line.c_str());
        }
        for (auto& [b, v] : all.cents)
        {
            int out = 0; for (double x : v) if (std::abs (x) > 35) ++out;
            std::printf ("  tuning %s: median %+.0f cents, p10 %+.0f, p90 %+.0f, >35c off: %.0f%%\n", b.c_str(), pct (v, 0.5), pct (v, 0.1), pct (v, 0.9), 100.0 * out / v.size());
        }

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
