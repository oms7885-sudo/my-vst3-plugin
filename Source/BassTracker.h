// BassTracker.h
// Monophonic bass pitch tracker (YIN) + note state machine.
// No JUCE dependency, so it can be unit-tested on its own.
#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <functional>
#include <array>
#ifdef B2M_DEBUG
 #include <cstdio>
#endif

namespace b2m
{

struct NoteEvent
{
    bool isNoteOn = false;
    int note = 0;
    int velocity = 0;   // 1..127 for note-on
    int sampleOffset = 0;
};

struct TrackerParams
{
    float thresholdDb   = -40.0f; // gate: below this the bass is "silent"
    float yinThreshold  = 0.15f;  // pitch tolerance (lower = stricter)
    int   lowestNote    = 23;     // B0 (5-string low B)
    int   highestNote   = 67;     // G4
    int   transpose     = 0;      // semitones
    int   stableFrames  = 3;      // frames (~2.7 ms each) a pitch must hold before note-on
    float retriggerDb   = 6.0f;   // level jump that counts as a new pluck (0 = off)
    float velocitySens  = 0.8f;   // 0 = fixed velocity 100, 1 = full dynamics
};

class Biquad
{
public:
    void setLowpass (double fs, double fc, double q)  { design (fs, fc, q, true); }
    void setHighpass (double fs, double fc, double q) { design (fs, fc, q, false); }
    void reset() { z1 = z2 = 0.0; }

    inline float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

private:
    void design (double fs, double fc, double q, bool lowpass)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * fc / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double a0 = 1.0 + alpha;
        if (lowpass)
        {
            b0 = (1.0 - cw) * 0.5 / a0;
            b1 = (1.0 - cw) / a0;
            b2 = b0;
        }
        else
        {
            b0 = (1.0 + cw) * 0.5 / a0;
            b1 = -(1.0 + cw) / a0;
            b2 = b0;
        }
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - alpha) / a0;
    }

    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

class BassTracker
{
public:
    static constexpr double minFreqHz = 28.0;  // just below low B (30.9 Hz)
    static constexpr double maxFreqHz = 700.0;
    double windowHz = 45.0;  // window length = 1/windowHz seconds

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        decim = std::max (1, (int) std::lround (fs / 11025.0));
        fsd = fs / decim;

        hp.setHighpass (fs, 20.0, 0.707);
        lp1.setLowpass (fs, 1000.0, 0.541);
        lp2.setLowpass (fs, 1000.0, 1.307);

        tauMax = (int) std::ceil (fsd / minFreqHz) + 2;
        tauMin = std::max (2, (int) std::floor (fsd / maxFreqHz));
        window = (int) std::lround (fsd / windowHz);   // YIN integration window (shorter = faster)
        gateLen = (int) std::lround (fsd * 0.035);      // >= one period of low B
        onsetLen = (int) std::lround (fsd * 0.008);
        hfHistLen = (int) std::lround (fsd * 0.045);
        hfRecent = std::max (2, (int) std::lround (fsd * 0.004));
        refractory = (int) std::lround (fsd * 0.035);
        onsetStartWindow = (int) std::lround (fsd * 0.25);
        ambigMaxWait = (int) std::lround (fsd * ambigWaitMs * 0.001);
        attackBiasLen = (int) std::lround (fsd * attackBiasMs * 0.001);
        hfHp1.setHighpass (fs, 1500.0, 0.541);
        hfHp2.setHighpass (fs, 1500.0, 1.307);
        hfRelease = std::exp (-1.0 / (0.004 * fs));
        bufLen = window + tauMax + 2;
        hop = std::max (8, (int) std::lround (fsd * 0.0027)); // ~2.7 ms
        retrigTimeoutFrames = std::max (4, (int) std::lround (0.15 * fsd / hop));

        ring.assign ((size_t) bufLen, 0.0f);
        lin.assign ((size_t) bufLen, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        hfHist.assign ((size_t) hfHistLen, -140.0f);
        reset();
    }

    void reset()
    {
        hp.reset(); lp1.reset(); lp2.reset(); hfHp1.reset(); hfHp2.reset();
        std::fill (ring.begin(), ring.end(), 0.0f);
        std::fill (hfHist.begin(), hfHist.end(), -140.0f);
        hfEnv = 0.0f; hfPos = 0; decimTotal = 0; lastOnsetAt = -1000000; pendingOnset = false;
        writePos = 0; decimCounter = 0; hopCounter = 0; filled = bufLen;   // the zeroed buffer is just silence
        activeNote = -1; candidateNote = -1; candidateCount = 0;
        quietCount = 0; framesSinceOn = 0; retrigPending = false; prevLevelDb = -140.0f;
        lastFreq = 0.0f; lastLevelDb = -120.0f;
    }

    void setParams (const TrackerParams& p) { params = p; }

    /** Processes a mono block. Calls emit() for every MIDI event with its sample offset. */
    template <typename Emit>
    void process (const float* in, int numSamples, Emit&& emit)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float xin = hp.process (in[i]);
            float x = lp2.process (lp1.process (xin));

            // Pick/finger transient detector (energy above 1.5 kHz, instant attack)
            const float h = std::abs (hfHp2.process (hfHp1.process (xin)));
            hfEnv = std::max (h, (float) (hfEnv * hfRelease));

            if (++decimCounter < decim)
                continue;
            decimCounter = 0;
            ++decimTotal;
            detectTransient();

            ring[(size_t) writePos] = x;
            writePos = (writePos + 1) % bufLen;
            if (filled < bufLen) ++filled;

            if (++hopCounter >= hop)
            {
                hopCounter = 0;
                if (filled >= bufLen)
                    analyse (i, emit);
            }
        }
    }

    /** Sends a note-off for any held note (e.g. on bypass / transport stop). */
    template <typename Emit>
    void allNotesOff (int sampleOffset, Emit&& emit)
    {
        if (activeNote >= 0)
            emit (NoteEvent { false, activeNote, 0, sampleOffset });
        activeNote = -1;
        candidateNote = -1;
        candidateCount = 0;
    }

    int   getActiveNote() const   { return activeNote; }
    float getLastFreq() const     { return lastFreq; }
    float getLastLevelDb() const  { return lastLevelDb; }
    float getOnsetHfMinusLevel() const { return onsetHfMinusLevel; }
    double getLastOnsetSeconds() const { return (double) lastOnsetAt / fsd; }
    int   getLatencySamples() const { return (window + tauMax) * decim / 2; }

private:
    template <typename Emit>
    void analyse (int sampleOffset, Emit&& emit)
    {
        // Linearise ring buffer (oldest -> newest)
        for (int k = 0; k < bufLen; ++k)
            lin[(size_t) k] = ring[(size_t) ((writePos + k) % bufLen)];

        // Peak level over the last 35 ms (covers a full period of the lowest string)
        // While a note is held, the gate only needs to span ~1.3 periods of that note
        // -> faster note-off on high notes.
        int gl = gateLen;
        if (activeNote >= 0)
        {
            const double f0 = 440.0 * std::pow (2.0, (activeNote - params.transpose - 69.0) / 12.0);
            gl = std::clamp ((int) std::lround (1.3 * fsd / f0), (int) std::lround (fsd * 0.008), gateLen);
        }
        const float levelDb = toDb (peak (bufLen - gl, bufLen));
        lastLevelDb = levelDb;

        // Onset: peak of the last 8 ms vs. peak of the 35 ms before that.
        // A steady/decaying note can never jump up, a new pluck does.
        const float newPk = toDb (peak (bufLen - onsetLen, bufLen));
        const float oldPk = toDb (peak (bufLen - onsetLen - gateLen, bufLen - onsetLen));
        // slowly decaying memory of how loud the playing has been (~6 dB/s)
        recentPeakDb = std::max (levelDb, recentPeakDb - (float) (relDecayDbPerSec * hop / fsd));
        const bool loudEnoughForOnset = onsetRelDb <= 0.0f || levelDb > recentPeakDb - onsetRelDb;
        // gate: the user's threshold, raised for hot inputs to stay a fixed distance
        // below the recent playing level
        gateDb = relGateDb > 0.0f ? std::max (params.thresholdDb, recentPeakDb - relGateDb) : params.thresholdDb;

        bool onset = false;
        if (params.retriggerDb > 0.0f && levelDb > gateDb + onsetMarginDb && loudEnoughForOnset)
        {
            if (pendingOnset)
                onset = true;
            else if (newPk - oldPk >= params.retriggerDb && decimTotal - lastOnsetAt > gateLen + onsetLen)
            {
                onset = true;
                // locate where the signal first rose above the previous peak
                const float thrAmp = std::pow (10.0f, oldPk / 20.0f);
                int k = std::max (0, bufLen - 2 * onsetLen);
                while (k < bufLen - 1 && std::abs (lin[(size_t) k]) <= thrAmp) ++k;
                lastOnsetAt = decimTotal - (bufLen - k);
            }
        }
        pendingOnset = false;
        onsetPeakDb = onset ? levelDb : std::max (onsetPeakDb, levelDb);
       #ifdef B2M_DEBUG
        if (onset) std::printf ("    [onset used %.3f lvl %.1f active %d]\n", decimTotal / fsd, levelDb, activeNote);
       #endif

        // Analyse only what was played after the last pluck: the ringing previous
        // note (or silence) before it would only blur / delay the new pitch.
        const long long age = decimTotal - lastOnsetAt - skipAfterOnset;
        const int avail = age < bufLen ? (int) std::max (0LL, age) : bufLen;

        // Pitch (YIN)
        float freq = 0.0f;
        const bool gateOpen = levelDb > gateDb;
        if (gateOpen)
            freq = estimator == 1 ? mpm (avail) : yin (avail);
        lastFreq = freq;

        int note = -1;
        bool ambiguous = false;
        if (freq > 0.0f)
        {
            double fm = 69.0 + 12.0 * std::log2 (freq / 440.0);
            // A hard pluck / slap pop starts sharp and settles within ~70 ms (measured on
            // real recordings: pops are +20..+40 cents early on). Lean towards the lower note then.
            if (decimTotal - lastOnsetAt < attackBiasLen)
                fm -= attackBias;
            // between two semitones (attack pitch glide of a hard pluck / slap pop, bends)
            ambiguous = std::abs (fm - std::round (fm)) > ambigSemis;
            // pitch still gliding (attack settling, slide into the note)?
            if (onset) fmCount = 0;
            fmHist[(size_t) (fmCount++ % 3)] = (float) fm;
            if (glideSemis > 0.0f && fmCount >= 2)
            {
                const int n = std::min (fmCount, 3);
                float lo = fmHist[0], hi = fmHist[0];
                for (int k = 1; k < n; ++k) { lo = std::min (lo, fmHist[(size_t) k]); hi = std::max (hi, fmHist[(size_t) k]); }
                if (hi - lo > glideSemis) ambiguous = true;
            }
            int raw = (int) std::lround (fm);
            // Hysteresis: vibrato / small bends / slightly out-of-tune strings stay on the held note
            if (activeNote >= 0 && std::abs (fm - (activeNote - params.transpose)) < hysteresis)
                raw = activeNote - params.transpose;
            if (raw >= params.lowestNote && raw <= params.highestNote)
                note = std::clamp (raw + params.transpose, 0, 127);
        }

        const float prevLevel = prevLevelDb;
        prevLevelDb = levelDb;

        if (activeNote >= 0)
        {
            ++framesSinceOn;
            notePeakDb = std::max (notePeakDb, levelDb);

            // Muting: the level dropped fast (natural decay is ~10-20 dB/s, a mute is
            // tens of dB within a few ms) -> release right away.
            float recentMax = -140.0f;
            for (float v : frameLevels) recentMax = std::max (recentMax, v);
            pushFrameLevel (levelDb);
            if (levelDb < recentMax - muteDropDb)
            {
                release (sampleOffset, emit);
                return;
            }

            // Tail release: far below the note's peak the pitch estimate drifts and noise
            // starts to matter -> end the note there.
            if (tailDb > 0.0f && levelDb < notePeakDb - tailDb)
            {
                release (sampleOffset, emit);
                return;
            }

            // Release: level falls below gate (with 3 dB hysteresis) for a few frames
            if (levelDb < gateDb - 3.0f)
            {
                if (++quietCount >= 3)
                    release (sampleOffset, emit);
                return;
            }
            quietCount = 0;

            // A transient on top of a ringing note: maybe a new pluck, maybe just the
            // finger touching the string to mute it. Keep the note and decide once the
            // pitch after the transient is known.
            const long long sinceOnset = decimTotal - lastOnsetAt;
            const int onsetIdx = sinceOnset < bufLen ? bufLen - (int) sinceOnset : 0;
            if (onset && framesSinceOn > 4)
            {
                retrigPending = true;
                // level just before the transient vs. level after it
                retrigPreLevel = onsetIdx > 0 ? toDb (peak (onsetIdx - gl, onsetIdx)) : prevLevel;
                retrigPeak = -140.0f;
                retrigFrames = 0;
                candidateNote = -1;
                candidateCount = 0;
            }

            if (retrigPending)
            {
                retrigPeak = std::max (retrigPeak, onsetIdx > 0 ? toDb (peak (std::max (onsetIdx, bufLen - gl), bufLen)) : levelDb);
                if (++retrigFrames > retrigTimeoutFrames)
                {
                    retrigPending = false;
                }
                else if (note >= 0)
                {
                    if (note == candidateNote) ++candidateCount;
                    else { candidateNote = note; candidateCount = 1; }

                    const int need = params.stableFrames + (std::abs (note - activeNote) == 12 ? 4 : 0);
                    if (candidateCount >= need)
                    {
                        retrigPending = false;
                        candidateCount = 0;
                        // same pitch: only a real re-pluck if the level actually rose.
                        // other pitch: a muting finger also makes a transient (and a brief
                        // odd pitch) but the level falls -> not a new note.
                        if ((note == activeNote && retrigPeak > retrigPreLevel + retrigRiseDb)
                            || (note != activeNote && retrigPeak > retrigPreLevel - retrigDropDb))
                            startNote (note, velocityFor (retrigPeak), levelDb, sampleOffset, emit);
                    }
                }
                return;
            }

            // Pitch change without a new pluck (slide / hammer-on / pull-off).
            // Ignored in the quiet tail of a note, where the pitch estimate drifts.
            // Also ignored while the level is falling fast: releasing the fretting finger at
            // the end of a note bends / drops the pitch briefly.
            if (note >= 0 && note != activeNote && levelDb > notePeakDb - legatoRangeDb
                && levelDb > recentMax - legatoFallDb)
            {
                if (note == candidateNote) ++candidateCount;
                else { candidateNote = note; candidateCount = 1; }

                // Octave jumps are a typical detection error -> demand more evidence
                const int need = params.stableFrames + (std::abs (note - activeNote) % 12 == 0 ? octLegatoExtra : 1);
                if (candidateCount >= need)
                {
                    startNote (note, velocityFor (levelDb), levelDb, sampleOffset, emit);
                    candidateCount = 0;
                }
            }
            else
            {
                candidateNote = -1;
                candidateCount = 0;
            }
            return;
        }

        // Idle: wait for a stable pitch
        if (onset)
        {
            candidateNote = -1;
            candidateCount = 0;
        }
        if (note < 0)
        {
            candidateNote = -1;
            candidateCount = 0;
            return;
        }

        // A note only starts on a pluck: noise, other strings ringing and the
        // decaying tail of a released note never start one.
        if (requireOnset && params.retriggerDb > 0.0f && decimTotal - lastOnsetAt > onsetStartWindow)
        {
            candidateNote = -1;
            candidateCount = 0;
            return;
        }

        // An attack whose pitch sits between two semitones is usually still gliding
        // into tune: give it a little time before committing (bounded wait).
        const bool waitForPitch = ambiguous && decimTotal - lastOnsetAt < ambigMaxWait;

        // a very clean periodicity counts double (faster lock on clean attacks)
        const int inc = (fastLockDip > 0.0f && lastDip < fastLockDip) ? 2 : 1;
        if (note == candidateNote) { if (! waitForPitch || candidateCount + inc < params.stableFrames) candidateCount += inc; }
        else { candidateNote = note; candidateCount = 1; peakSinceCandidate = levelDb; }
        peakSinceCandidate = std::max (peakSinceCandidate, levelDb);

        if (candidateCount >= params.stableFrames && ! waitForPitch)
        {
            // dead note / percussive thump: already collapsed by the time the pitch is known
            if (deadDropDb > 0.0f && levelDb < onsetPeakDb - deadDropDb)
            {
                candidateCount = 0;
                return;
            }
            startNote (note, velocityFor (peakSinceCandidate), levelDb, sampleOffset, emit);
            candidateCount = 0;
        }
    }

    template <typename Emit>
    void startNote (int note, int velocity, float levelDb, int sampleOffset, Emit&& emit)
    {
        if (activeNote >= 0)
            emit (NoteEvent { false, activeNote, 0, sampleOffset });
        activeNote = note;
        framesSinceOn = 0;
        quietCount = 0;
        notePeakDb = levelDb;
        retrigPending = false;
        frameLevels.fill (levelDb);
        emit (NoteEvent { true, activeNote, velocity, sampleOffset });
    }

    template <typename Emit>
    void release (int sampleOffset, Emit&& emit)
    {
        emit (NoteEvent { false, activeNote, 0, sampleOffset });
        activeNote = -1;
        candidateNote = -1;
        candidateCount = 0;
        retrigPending = false;
    }

    void detectTransient()
    {
        const float db = toDb (hfEnv);
        hfHist[(size_t) hfPos] = db;
        hfPos = (hfPos + 1) % hfHistLen;
        if (params.retriggerDb <= 0.0f || decimTotal - lastOnsetAt <= refractory)
            return;

        // Newest few ms vs. the loudest point of the ~40 ms before (covers a full
        // period of the lowest string, so the periodic ripple of a ringing note never fires)
        float newest = -140.0f, older = -140.0f;
        for (int k = 0; k < hfHistLen; ++k)
        {
            const int ageK = (hfPos - 1 - k + hfHistLen) % hfHistLen;  // index of sample k steps back
            const float v = hfHist[(size_t) ageK];
            if (k < hfRecent) newest = std::max (newest, v);
            else older = std::max (older, v);
        }
        if (newest - older >= hfFactor * params.retriggerDb && newest > gateDb - 30.0f
            && lastLevelDb > gateDb + onsetMarginDb)
        {
            pendingOnset = true;
            // place the onset where the transient actually crossed the threshold
            int back = 0;
            for (int k = hfRecent - 1; k >= 0; --k)
            {
                const int idx = (hfPos - 1 - k + hfHistLen) % hfHistLen;
                if (hfHist[(size_t) idx] - older >= hfFactor * params.retriggerDb) { back = k; break; }
            }
            lastOnsetAt = decimTotal - back;
            onsetHfMinusLevel = newest - lastLevelDb;
           #ifdef B2M_DEBUG
            std::printf ("    [hf onset %.3f  new %.1f old %.1f]\n", decimTotal / fsd, newest, older);
           #endif
        }
    }

    void pushFrameLevel (float db)
    {
        frameLevels[(size_t) framePos] = db;
        framePos = (framePos + 1) % (int) frameLevels.size();
    }

    float peak (int from, int to) const
    {
        float m = 0.0f;
        for (int k = std::max (0, from); k < to; ++k)
            m = std::max (m, std::abs (lin[(size_t) k]));
        return m;
    }

    static float toDb (float x) { return x > 1.0e-7f ? 20.0f * std::log10 (x) : -140.0f; }

    int velocityFor (float levelDb) const
    {
        if (params.velocitySens <= 0.0f)
            return 100;
        // Map gate..-6 dBFS onto 1..127
        const float top = -3.0f;
        float t = (levelDb - params.thresholdDb) / std::max (1.0f, top - params.thresholdDb);
        t = std::clamp (t, 0.0f, 1.0f);
        const float dyn = 20.0f + t * 107.0f;
        const float v = 100.0f + (dyn - 100.0f) * params.velocitySens;
        return std::clamp ((int) std::lround (v), 1, 127);
    }

    // McLeod pitch method (normalised square difference). Robust to the amplitude
    // decay inside the analysis window; picks the first "key maximum" within
    // mpmK of the highest one (avoids octave-down errors). Returns Hz or 0.
    float mpm (int avail)
    {
        const float* x = lin.data();
        const int wMinAbs = std::max (4, (int) std::lround (fsd * 0.002));

        int tauLimit = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const int n = std::min (window, avail - tau);
            if (n < std::max (wMinAbs, (int) (wMinFactor * tau)))
                break;
            double r = 0.0, m = 0.0;
            for (int j = bufLen - n; j < bufLen; ++j)
            {
                const float a = x[j], b = x[j - tau];
                r += (double) a * b;
                m += (double) a * a + (double) b * b;
            }
            diff[(size_t) tau] = m > 0.0 ? (float) (2.0 * r / m) : 0.0f;   // nsdf in [-1, 1]
            tauLimit = tau;
        }
        if (tauLimit <= tauMin + 2)
            return 0.0f;

        // key maxima: highest point of each positive lobe (after the first negative zero crossing)
        int keyTau[64]; float keyVal[64]; int nk = 0;
        int t = 1;
        while (t <= tauLimit && diff[(size_t) t] > 0.0f) ++t;      // leave the lag-0 lobe
        while (t <= tauLimit && nk < 64)
        {
            while (t <= tauLimit && diff[(size_t) t] <= 0.0f) ++t;
            if (t > tauLimit) break;
            int best = t;
            while (t <= tauLimit && diff[(size_t) t] > 0.0f)
            {
                if (diff[(size_t) t] > diff[(size_t) best]) best = t;
                ++t;
            }
            if (t > tauLimit && best >= tauLimit - 1) break;          // lobe cut off by the data edge
            if (best >= tauMin) { keyTau[nk] = best; keyVal[nk] = diff[(size_t) best]; ++nk; }
        }
        if (nk == 0)
            return 0.0f;

        float maxVal = 0.0f;
        for (int k = 0; k < nk; ++k) maxVal = std::max (maxVal, keyVal[k]);
        int best = -1;
        for (int k = 0; k < nk; ++k)
            if (keyVal[k] >= mpmK * maxVal) { best = keyTau[k]; break; }
        if (best < 0)
            return 0.0f;

        const float clarity = diff[(size_t) best];
        if (clarity < 1.0f - params.yinThreshold * mpmClarityScale)
            return 0.0f;
        lastDip = 1.0f - clarity;

        double tt = best;
        if (best > 1 && best < tauLimit)
        {
            const double s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
            const double den = 2.0 * (2.0 * s1 - s2 - s0);
            if (std::abs (den) > 1e-12)
                tt = best + (s2 - s0) / den;
        }
        return (float) (fsd / tt);
    }

    // YIN on the newest `avail` samples of lin (normalised per-lag window, so it can
    // answer as soon as ~one period has been played). Returns Hz, or 0 if not (yet) voiced.
    float yin (int avail)
    {
        const float* x = lin.data();
        const int wMinAbs = std::max (4, (int) std::lround (fsd * 0.002));

        int tauLimit = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const int n = std::min (window, avail - tau);
            if (n < std::max (wMinAbs, (int) (wMinFactor * tau)))
                break;
            double sum = 0.0;
            for (int j = bufLen - n; j < bufLen; ++j)
            {
                const float d = x[j] - x[j - tau];
                sum += (double) d * d;
            }
            diff[(size_t) tau] = (float) (sum / n);
            tauLimit = tau;
        }
        if (tauLimit <= tauMin + 2)
            return 0.0f;

        // Cumulative mean normalised difference
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauLimit; ++tau)
        {
            running += diff[(size_t) tau];
            diff[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
        }

       #ifdef B2M_DEBUG
        {
            int mi = tauMin; for (int t = tauMin; t <= tauLimit; ++t) if (diff[(size_t) t] < diff[(size_t) mi]) mi = t;
            std::printf ("      [yin avail %d tauLimit %d min %.3f at %d (%.1f Hz)]\n", avail, tauLimit, diff[(size_t) mi], mi, fsd / mi);
        }
       #endif
        // First dip below threshold, then walk to its local minimum
        int best = -1;
        for (int tau = tauMin; tau < tauLimit; ++tau)
        {
            if (diff[(size_t) tau] < params.yinThreshold)
            {
                while (tau + 1 <= tauLimit && diff[(size_t) tau + 1] < diff[(size_t) tau])
                    ++tau;
                if (tau >= tauLimit)
                    return 0.0f;   // still falling at the edge: need more data
                best = tau;
                break;
            }
        }
        if (best < 0)
            return 0.0f;

        // Sub-harmonic correction: if half (or a third) of the found period also
        // fits nearly as well, the shorter period is the real one (octave-down error).
        if (subFactor > 0.0f)
        {
            for (int k = 3; k >= 2; --k)
            {
                const int c = (int) std::lround ((double) best / k);
                if (c - 2 < tauMin) continue;
                int m = c;
                for (int t = c - 2; t <= c + 2; ++t) if (diff[(size_t) t] < diff[(size_t) m]) m = t;
                if (diff[(size_t) m] < std::min (0.4f, subFactor * diff[(size_t) best] + 0.05f))
                {
                    best = m;
                    break;
                }
            }
        }

        // Octave-up correction: with a weak fundamental (thumb slap) the waveform nearly
        // repeats at half the period; if the double lag fits much better, take it.
        if (octUpFactor > 0.0f && 2 * best + 2 <= tauLimit)
        {
            int m = 2 * best;
            for (int t = 2 * best - 2; t <= 2 * best + 2; ++t) if (diff[(size_t) t] < diff[(size_t) m]) m = t;
            const double fLow = fsd / (double) m;
            const double lowestHz = 440.0 * std::pow (2.0, (params.lowestNote - 0.5 - 69.0) / 12.0);
            if (fLow >= lowestHz && diff[(size_t) m] < octUpFactor * diff[(size_t) best])
                best = m;
        }

        // Octave-up guard: a strong 2nd harmonic dips at half the true period.
        // If the octave below is a playable note but its lag can't be checked yet, wait.
        // Slap attacks (very bright transient, weak fundamental) are prone to locking onto
        // the 2nd or 3rd harmonic: for those, wait until 2x / 3x the lag can be checked.
        const bool slapLike = slapHfDb > -100.0f && onsetHfMinusLevel > slapHfDb
                              && decimTotal - lastOnsetAt < bufLen;
        if (confirmOctave || slapLike)
        {
            const double lowestHz = 440.0 * std::pow (2.0, (params.lowestNote - 0.5 - 69.0) / 12.0);
            for (int k = 2; k <= (slapLike ? 3 : 2); ++k)
                if (fsd / ((double) k * best) >= lowestHz && k * best + 1 > tauLimit)
                    return 0.0f;
            // and if a longer lag fits nearly as well, it is the real period
            if (slapLike)
                for (int k = 3; k >= 2; --k)
                {
                    const int c = k * best;
                    if (c + 2 > tauLimit || fsd / (double) c < lowestHz) continue;
                    int m = c;
                    for (int t = c - 2; t <= c + 2; ++t) if (diff[(size_t) t] < diff[(size_t) m]) m = t;
                    if (diff[(size_t) m] < slapLongerFactor * diff[(size_t) best]) { best = m; break; }
                }
        }

        lastDip = diff[(size_t) best];

        // Parabolic interpolation
        double t = best;
        if (best > 1 && best < tauLimit)
        {
            const double s0 = diff[(size_t) best - 1], s1 = diff[(size_t) best], s2 = diff[(size_t) best + 1];
            const double den = 2.0 * (2.0 * s1 - s2 - s0);
            if (std::abs (den) > 1e-12)
                t = best + (s2 - s0) / den;
        }
        return (float) (fsd / t);
    }

    TrackerParams params;
    double fs = 44100.0, fsd = 11025.0;
    int decim = 4, tauMin = 16, tauMax = 443, window = 443, bufLen = 900, hop = 30;
    Biquad hp, lp1, lp2;
    std::vector<float> ring, lin, diff;
    int writePos = 0, decimCounter = 0, hopCounter = 0, filled = 0;

    int activeNote = -1, candidateNote = -1, candidateCount = 0, quietCount = 0, framesSinceOn = 0;
    float peakSinceCandidate = -120.0f;
    int gateLen = 385, onsetLen = 88;
    Biquad hfHp1, hfHp2;
    double hfRelease = 0.99;
    float hfEnv = 0.0f;
    std::vector<float> hfHist;
    int hfHistLen = 500, hfRecent = 48, hfPos = 0, refractory = 400;
    long long decimTotal = 0, lastOnsetAt = -1000000;
    bool pendingOnset = false;
    std::array<float, 16> frameLevels {};   // ~43 ms of frame levels
    int framePos = 0;
    float prevLevelDb = -140.0f, notePeakDb = -140.0f, retrigPreLevel = -140.0f, retrigPeak = -140.0f;
    bool retrigPending = false;
    int retrigFrames = 0, retrigTimeoutFrames = 55, onsetStartWindow = 2750, ambigMaxWait = 400, attackBiasLen = 800;
public:
    bool requireOnset = true;
    float retrigRiseDb = 3.0f;
    float legatoRangeDb = 14.0f;
    float legatoFallDb = 4.0f;    // 100 = off
    float octUpFactor = 0.0f;     // 0 = off
    float deadDropDb = 15.0f;     // 0 = off
    float slapHfDb = -1000.0f;    // HF-minus-level at onset above which an attack counts as slap (-1000 = off)
    float slapLongerFactor = 0.0f;
    float onsetHfMinusLevel = -140.0f;
    float onsetPeakDb = -140.0f;
    float subFactor = 3.0f;      // sub-harmonic correction (0 = off)
    float ambigSemis = 0.25f;    // > 0.5 disables the ambiguity wait
    float ambigWaitMs = 60.0f;
    float attackBias = 0.15f;    // semitones
    float fastLockDip = 0.0f;    // 0 = off
    float glideSemis = 0.0f;     // 0 = off
    float onsetMarginDb = 0.0f;  // a pluck must be this much above the gate
    float tailDb = 0.0f;         // release when this far below the note's peak (0 = off)
    float onsetRelDb = 18.0f;    // a pluck must be within this of the recent playing level (0 = off)
    float recentPeakDb = -140.0f;
    float relDecayDbPerSec = 3.0f;
    float retrigDropDb = 6.0f;
    int octLegatoExtra = 20;
    float relGateDb = 30.0f;      // 0 = off
    int estimator = 0;            // 0 = YIN, 1 = MPM
    float mpmK = 0.9f;
    float mpmClarityScale = 1.0f;
    float gateDb = -40.0f;
    std::array<float, 3> fmHist {};
    int fmCount = 0;
    float lastDip = 1.0f;
    float attackBiasMs = 70.0f;
    float muteDropDb = 18.0f;
    float hysteresis = 0.75f;   // semitones
    float hfFactor = 1.0f;
    float wMinFactor = 0.4f;
    bool confirmOctave = false;
    int skipAfterOnset = 0;
private:
    float lastFreq = 0.0f, lastLevelDb = -120.0f;
};

} // namespace b2m
