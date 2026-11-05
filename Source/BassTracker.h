// BassTracker.h
// Monophonic bass pitch tracker (YIN) + note state machine.
// No JUCE dependency, so it can be unit-tested on its own.
#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <functional>

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
    float yinThreshold  = 0.20f;  // pitch tolerance (lower = stricter)
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
        bufLen = window + tauMax + 2;
        hop = std::max (8, (int) std::lround (fsd * 0.0027)); // ~2.7 ms

        ring.assign ((size_t) bufLen, 0.0f);
        lin.assign ((size_t) bufLen, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        reset();
    }

    void reset()
    {
        hp.reset(); lp1.reset(); lp2.reset();
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = 0; decimCounter = 0; hopCounter = 0; filled = 0;
        activeNote = -1; candidateNote = -1; candidateCount = 0;
        quietCount = 0; framesSinceOn = 0;
        lastFreq = 0.0f; lastLevelDb = -120.0f;
    }

    void setParams (const TrackerParams& p) { params = p; }

    /** Processes a mono block. Calls emit() for every MIDI event with its sample offset. */
    template <typename Emit>
    void process (const float* in, int numSamples, Emit&& emit)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float x = lp2.process (lp1.process (hp.process (in[i])));
            if (++decimCounter < decim)
                continue;
            decimCounter = 0;

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
    int   getLatencySamples() const { return (window + tauMax) * decim / 2; }

private:
    template <typename Emit>
    void analyse (int sampleOffset, Emit&& emit)
    {
        // Linearise ring buffer (oldest -> newest)
        for (int k = 0; k < bufLen; ++k)
            lin[(size_t) k] = ring[(size_t) ((writePos + k) % bufLen)];

        // Peak level over the last 35 ms (covers a full period of the lowest string)
        const float levelDb = toDb (peak (bufLen - gateLen, bufLen));
        lastLevelDb = levelDb;

        // Onset: peak of the last 8 ms vs. peak of the 35 ms before that.
        // A steady/decaying note can never jump up, a new pluck does.
        const float newPk = toDb (peak (bufLen - onsetLen, bufLen));
        const float oldPk = toDb (peak (bufLen - onsetLen - gateLen, bufLen - onsetLen));
        const bool onset = params.retriggerDb > 0.0f
                        && newPk - oldPk >= params.retriggerDb
                        && levelDb > params.thresholdDb;

        // Pitch (YIN)
        float freq = 0.0f;
        const bool gateOpen = levelDb > params.thresholdDb;
        if (gateOpen)
            freq = yin (lin.data() + (bufLen - window - tauMax));
        lastFreq = freq;

        int note = -1;
        if (freq > 0.0f)
        {
            const int raw = (int) std::lround (69.0 + 12.0 * std::log2 (freq / 440.0));
            if (raw >= params.lowestNote && raw <= params.highestNote)
                note = std::clamp (raw + params.transpose, 0, 127);
        }

        if (activeNote >= 0)
        {
            ++framesSinceOn;

            // Release: level falls below gate (with 3 dB hysteresis) for a few frames
            if (levelDb < params.thresholdDb - 3.0f)
            {
                if (++quietCount >= 3)
                {
                    emit (NoteEvent { false, activeNote, 0, sampleOffset });
                    activeNote = -1;
                    candidateNote = -1;
                    candidateCount = 0;
                }
                return;
            }
            quietCount = 0;

            // New pluck on top of a ringing note -> release, then re-detect
            if (onset && framesSinceOn > 6)
            {
                emit (NoteEvent { false, activeNote, 0, sampleOffset });
                activeNote = -1;
                candidateNote = -1;
                candidateCount = 0;
                return;
            }

            // Pitch change (slide / hammer-on / pull-off)
            if (note >= 0 && note != activeNote)
            {
                if (note == candidateNote) ++candidateCount;
                else { candidateNote = note; candidateCount = 1; }

                // Octave jumps are a typical detection error -> demand more evidence
                const int need = params.stableFrames + (std::abs (note - activeNote) == 12 ? 4 : 1);
                if (candidateCount >= need)
                {
                    emit (NoteEvent { false, activeNote, 0, sampleOffset });
                    activeNote = note;
                    framesSinceOn = 0;
                    emit (NoteEvent { true, activeNote, velocityFor (levelDb), sampleOffset });
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
        if (note < 0)
        {
            candidateNote = -1;
            candidateCount = 0;
            return;
        }

        if (note == candidateNote) ++candidateCount;
        else { candidateNote = note; candidateCount = 1; peakSinceCandidate = levelDb; }
        peakSinceCandidate = std::max (peakSinceCandidate, levelDb);

        if (candidateCount >= params.stableFrames)
        {
            activeNote = note;
            framesSinceOn = 0;
            quietCount = 0;
            emit (NoteEvent { true, activeNote, velocityFor (peakSinceCandidate), sampleOffset });
            candidateCount = 0;
        }
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

    // Returns frequency in Hz, or 0 if unvoiced.
    float yin (const float* x)
    {
        // Difference function
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            const float* a = x + tauMax;           // current window start
            const float* b = x + tauMax - tau;     // lagged window
            for (int j = 0; j < window; ++j)
            {
                const float d = a[j] - b[j];
                s += (double) d * d;
            }
            diff[(size_t) tau] = (float) s;
        }

        // Cumulative mean normalised difference
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += diff[(size_t) tau];
            diff[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
        }

        // Absolute threshold: first dip below threshold, then walk to its local minimum
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (diff[(size_t) tau] < params.yinThreshold)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
            return 0.0f;

        // Parabolic interpolation
        double t = best;
        if (best > 1 && best < tauMax)
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
    float lastFreq = 0.0f, lastLevelDb = -120.0f;
};

} // namespace b2m
