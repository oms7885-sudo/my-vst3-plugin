// Bass DI -> Moog-style bass synth engine.
// Header-only, no JUCE dependency, so it can be tested offline (see tests/render_test.cpp).
// This is a sample-by-sample port of the browser prototype ("Bass Ladder Synth").
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace bsyn
{

struct Params
{
    // input
    float inGainDb = 5.0f;
    bool  comp = true;
    float cTh = -30.0f, cRat = 8.0f, cA = 2.0f, cR = 150.0f;   // dB, ratio, ms, ms
    float trackLP = 120.0f;                                    // Hz
    float hyst = 0.15f;
    float gateDb = -45.0f;
    float pickSens = 2.0f;
    bool  retrig = false;      // false = trigger on picks only
    bool  noiseMute = true;
    float tail = 0.4f;         // 0 = off
    // oscillators
    float sq = 0.5f, sub = 0.5f, saw = 0.8f;
    bool  sawIntegrate = false; // comparison mode (rough integrated saw)
    float detune = 7.0f;        // cents
    float drift = 3.0f;         // cents
    float pll = 0.25f;
    // mixer
    float drive = 0.45f, dyn = 0.35f;
    // ladder filter
    float cutoff = 160.0f, res = 0.3f, envAmt = 3.2f, keytrk = 0.5f;
    // envelopes (ms, sustain 0..1)
    float fA = 3.0f, fD = 320.0f, fS = 0.1f;
    float aA = 1.5f, aD = 900.0f, aS = 0.75f, aR = 90.0f;
    // output
    float outGainDb = 0.0f;
    float dry = 0.0f;           // 0..1 dry DI blended into the output
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hpA = std::exp (-TAU * 30.0 / sr);
        agA = k (0.002); agR = k (0.08);
        fA_ = k (0.001); fR_ = k (0.03); sA_ = k (0.04); sR_ = k (0.2);
        tfA = k (0.003); tfR = k (0.01);
        nGa = k (0.004); nGr = k (0.012);
        ehL  = (int) std::lround (0.015 * sr);
        ehL2 = (int) std::lround (0.025 * sr);
        pkR = std::exp (-1.0 / (1.5 * sr));
        openN = (int) std::lround (0.0015 * sr);
        hold = 0.06 * sr;
        sawK = 1.0 - std::exp (-1.0 / (0.0007 * sr));
        dcA = std::exp (-TAU * 15.0 / sr);
        caF = k (0.012 / 3.0);
        reset();
        lastP = Params(); lastP.trackLP = -1.0f;   // force coefficient update
    }

    void reset()
    {
        std::fill (std::begin (z), std::end (z), 0.0);
        std::fill (std::begin (eh), std::end (eh), 0.0f);
        std::fill (std::begin (rb), std::end (rb), 0.0f);
        hpY = hpX = 0; cEnv = 0; agc = 0; eF = eS = 0;
        gateOn = false; lastOn = -1e9; pk = 0; notePk = 0; openCnt = 0;
        conf = 1; nG = 1; tfS = 1;
        s = -1; subv = 1; lastEdge = -1; period = sr / 55.0; haveP = false; pend = 0; lastD = 0;
        stable = false; noteF = 0; sawG = 0; lastSync = -1e9;
        ph1 = 0; ph2 = 0.33; ph3 = 0.71; ramp = 0; integDC = 0;
        dr = 0; drT = 0; seed = 777;
        y1 = y2 = y3 = y4 = 0;
        fe = 0; fs = 0; ae = 0; as = 0;
        dcX = dcY = 0;
        n = 0;
    }

    // Call once per block (cheap). Recomputes parameter-dependent coefficients.
    void setParams (const Params& p)
    {
        P = p;
        if (p.trackLP != lastP.trackLP) lpCoef (p.trackLP);
        cAt = k (p.cA / 1000.0); cRl = k (p.cR / 1000.0);
        mk = -p.cTh * (1.0 - 1.0 / p.cRat) * 0.5;
        thr = std::pow (10.0, p.gateDb / 20.0);
        det = std::pow (2.0, p.detune / 1200.0);
        wX = std::min (1.0, p.detune / 2.0);
        dg = 1.0 + p.drive * 6.0;
        kRes = 4.1 * p.res;
        cfA = k (p.fA / 1000.0 / 3.0); cfD = k (p.fD / 1000.0 / 3.0);
        caA = k (p.aA / 1000.0 / 3.0); caD = k (p.aD / 1000.0 / 3.0); caR = k (p.aR / 1000.0 / 3.0);
        inGain = std::pow (10.0, p.inGainDb / 20.0);
        outGain = std::pow (10.0, p.outGainDb / 20.0);
        lastP = p;
    }

    float process (float input)
    {
        const Params& p = P;
        const int64_t i = n++;
        const double x0 = input * inGain;

        // input compressor (soft knee, feed-forward)
        double x = x0;
        if (p.comp)
        {
            const double a0 = std::abs (x0);
            cEnv += (a0 - cEnv) * (a0 > cEnv ? cAt : cRl);
            const double ov = 20.0 * std::log10 (cEnv + 1e-9) - p.cTh, sl = 1.0 - 1.0 / p.cRat;
            double gr = 0;
            if (ov > 3) gr = ov * sl; else if (ov > -3) gr = (ov + 3) * (ov + 3) / 12.0 * sl;
            x = x0 * std::pow (10.0, (mk - gr) / 20.0);
        }
        // 30 Hz high-pass
        const double hp = hpA * (hpY + x - hpX); hpX = x; hpY = hp;
        // tracking low-pass (two biquads = 4th order) + AGC
        double v = hp;
        for (int st = 0; st < 2; ++st)
        {
            double* q = z + st * 4;
            const double y = c[0] * v + c[1] * q[0] + c[2] * q[1] - c[3] * q[2] - c[4] * q[3];
            q[1] = q[0]; q[0] = v; q[3] = q[2]; q[2] = y; v = y;
        }
        const double av = std::abs (v); agc += (av - agc) * (av > agc ? agA : agR);
        const double yn = v / (agc + 1e-4);
        rb[i & (RB - 1)] = (float) v;

        // periodicity confidence: real notes repeat every period, noise doesn't
        if ((i & 127) == 0)
        {
            const double T = haveP ? period : (lastD > 0 ? lastD : sr / 80.0);
            conf = std::max ({ acAt (T, i), acAt (2 * T, i), acAt (T / 2, i) * 0.9 });
        }

        // envelope followers
        const double ax = std::abs (hp);
        eF += (ax - eF) * (ax > eF ? fA_ : fR_);
        eS += (eF - eS) * (eF > eS ? sA_ : sR_);

        // adaptive gate: opens only on a pick (sharp rise), thresholds follow the playing level
        pk = eF > pk ? eF : pk * pkR;
        if (gateOn && eF > notePk) notePk = eF;
        const double openT = std::max (thr, pk * 0.3), closeT = std::max (thr * 0.6, notePk * 0.16);
        bool onset = false;
        eh[i & EHM] = (float) eF;
        const double rise = eF / (std::min (eh[(i - ehL) & EHM], eh[(i - ehL2) & EHM]) + 1e-6);
        if (! gateOn && eF > openT && rise > 2.0)
        {
            if (++openCnt > openN) { gateOn = true; onset = true; openCnt = 0; }
        }
        else openCnt = 0;
        if (gateOn && ! onset && eF < closeT) { gateOn = false; fs = 3; as = 3; }
        else if (gateOn && ! onset && rise > p.pickSens && eF > std::max (thr * 2, pk * 0.3) && i - lastOn > hold) onset = true;

        // noise mute: fade out sounds that don't repeat at a pitch
        {
            const double tgt = (! p.noiseMute || i - lastOn < 0.015 * sr) ? 1.0
                             : std::min (1.0, std::max (0.0, (conf - 0.45) / 0.3));
            nG += (tgt - nG) * (tgt > nG ? nGa : nGr);
        }
        if (onset)
        {
            notePk = eF;
            lastOn = (double) i; lastSync = -1e9; haveP = false; pend = 0; stable = false; noteF = 0;
            fs = 1; as = 1;
        }

        // Schmitt trigger -> square, sub, period measurement
        bool edge = false;
        if (s < 0 && yn > p.hyst) { s = 1; edge = true; }
        else if (s > 0 && yn < -p.hyst) s = -1;
        if (edge)
        {
            subv = -subv;
            if (lastEdge >= 0)
            {
                const double d = (double) (i - lastEdge);
                if (d > sr / 1200.0 && d < sr / 24.0)
                {
                    stable = lastD > 0 && std::abs (d / lastD - 1) < 0.05; lastD = d;
                    if (! haveP) { period = d; haveP = true; }
                    else
                    {
                        const double r = d / period;
                        if (r > 0.7 && r < 1.4)
                        {
                            if (stable && std::abs (r - 1) > 0.04) period = d;
                            else period += (d - period) * 0.5;
                        }
                        else if (pend > 0 && std::abs (d / pend - 1) < 0.12) { period = d; pend = 0; }
                        else pend = d;
                    }
                }
            }
            lastEdge = i;
            if (p.retrig && stable && haveP)
            {
                const double fNow = sr / lastD;
                if (noteF > 0 && std::abs (12.0 * std::log2 (fNow / noteF)) > 0.7 && i - lastOn > 0.04 * sr)
                { lastOn = (double) i; fs = 1; as = 1; }
                noteF = fNow;
            }
            // hard-sync the saws right after a pick so all oscillators start together, then soft lock
            const bool atkSync = i - lastOn < 0.03 * sr && i - lastSync > 0.6 * period;
            const double kp = atkSync ? 1.0 : p.pll;
            if (atkSync) lastSync = (double) i;
            double e = ph1; if (e > 0.5) e -= 1; ph1 -= e * kp; if (ph1 < 0) ph1 += 1;
            if (atkSync) { ph2 = 0; ph3 = 0; }
            ramp = 0;
        }

        // drift
        if (--drT <= 0) { drT = 2048; dr += (rnd() - dr) * 0.15; }
        const double fq = sr / period;
        const double inc = (1.0 / period) * std::pow (2.0, (p.drift * dr) / 1200.0) * (1 + rnd() * p.drift * 2e-5);

        // three synced saws
        ph1 += inc; if (ph1 >= 1) ph1 -= 1;
        ph2 += inc * det; if (ph2 >= 1) ph2 -= 1;
        ph3 += inc / det; if (ph3 >= 1) ph3 -= 1;
        double saw;
        if (! p.sawIntegrate)
            saw = (sw (ph1, inc) + wX * 0.75 * (sw (ph2, inc * det) + sw (ph3, inc / det))) / (1 + 1.5 * wX);
        else
        {
            ramp += 80.0 / sr; const double r = std::min (ramp, 3.0);
            integDC += (r - integDC) * 0.0005; saw = (r - integDC) * 1.2;
        }
        sawG += ((gateOn ? 1.0 : 0.0) - sawG) * sawK;
        double m = (p.sq * s + p.sub * subv + p.saw * saw * sawG) * 0.55;
        m = th (m * dg) / std::sqrt (dg) * 1.6;

        // nonlinear 4-pole ladder, 2x oversampled
        double fc = p.cutoff * std::pow (2.0, p.envAmt * fe)
                  * std::pow (std::min (8.0, std::max (0.125, fq / 110.0)), (double) p.keytrk);
        fc = std::min (fc, sr * 0.45);
        const double g = 1.0 - std::exp (-TAU * fc / (2 * sr));
        for (int o = 0; o < 2; ++o)
        {
            const double u = th (m - kRes * y4);
            const double t1 = th (y1), t2 = th (y2), t3 = th (y3);
            y1 += g * (u - t1); y2 += g * (t1 - t2); y3 += g * (t2 - t3); y4 += g * (t3 - th (y4));
        }

        // RC envelopes
        if (fs == 1) { fe += (1.3 - fe) * cfA; if (fe >= 1) { fe = 1; fs = 2; } }
        else if (fs == 2) fe += (p.fS - fe) * cfD;
        else if (fs == 3) fe += (0 - fe) * cfD;
        if (as == 1) { ae += (1.3 - ae) * caA; if (ae >= 1) { ae = 1; as = 2; } }
        else if (as == 2) ae += (p.aS - ae) * caD;
        else if (as == 3) ae += (0 - ae) * caR;

        const double dynF = (1 - p.dyn) + p.dyn * std::min (1.0, eF * 2.2);
        const double tf = std::min (1.0, eF / (notePk * p.tail + 1e-6));
        tfS += (tf - tfS) * (tf > tfS ? tfA : tfR);
        const double yv = y4 * (1 + p.res * 0.9) * ae * dynF * nG * (p.tail > 0 ? tfS * tfS : 1.0);
        const double dc = yv - dcX + dcA * dcY; dcX = yv; dcY = dc;

        lastFreq = (gateOn && haveP) ? fq : 0.0;
        const double wet = th (dc * outGain);
        return (float) (wet + p.dry * input);
    }

    // for the editor display
    double currentFreq() const { return lastFreq; }
    bool   gateIsOpen() const  { return gateOn; }

private:
    static constexpr double TAU = 6.283185307179586;
    static constexpr int RB = 8192, EHM = 4095;

    static double th (double v) { return v > 3 ? 1 : v < -3 ? -1 : v * (27 + v * v) / (27 + 9 * v * v); }
    static double polyblep (double t, double dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1; }
        if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
        return 0;
    }
    double sw (double ph, double dt) const
    {
        constexpr double ck = 0.7;
        static const double cn = 1 - std::exp (-ck);
        return 2 * ((1 - std::exp (-ck * ph)) / cn) - 1 - polyblep (ph, dt);
    }
    double k (double t) const { return 1.0 - std::exp (-1.0 / (std::max (t, 1e-5) * sr)); }
    double rnd()
    {
        seed = (seed * 1103515245u + 12345u) & 0x7fffffffu;
        return (double) seed / (double) 0x7fffffff * 2 - 1;
    }
    void lpCoef (double f)
    {
        const double w = TAU * f / sr, cw = std::cos (w), a = std::sin (w) / (2 * 0.7071), a0 = 1 + a;
        c[0] = (1 - cw) / 2 / a0; c[1] = (1 - cw) / a0; c[2] = (1 - cw) / 2 / a0;
        c[3] = -2 * cw / a0;      c[4] = (1 - a) / a0;
    }
    double acAt (double Td, int64_t i) const
    {
        const int T = (int) std::lround (Td);
        if (T < 8 || T > RB / 3) return 0;
        const int W = std::min (T, 2048);
        double xy = 0, xx = 0, yy = 0;
        for (int q = 0; q < W; ++q)
        {
            const double a = rb[(i - q) & (RB - 1)], b = rb[(i - q - T) & (RB - 1)];
            xy += a * b; xx += a * a; yy += b * b;
        }
        return xy / std::sqrt (xx * yy + 1e-12);
    }

    Params P, lastP;
    double sr = 44100;
    int64_t n = 0;

    // coefficients
    double hpA = 0, agA = 0, agR = 0, fA_ = 0, fR_ = 0, sA_ = 0, sR_ = 0, tfA = 0, tfR = 0, nGa = 0, nGr = 0;
    double pkR = 0, hold = 0, sawK = 0, dcA = 0, caF = 0;
    int ehL = 0, ehL2 = 0, openN = 0;
    double c[5] {};
    double cAt = 0, cRl = 0, mk = 0, thr = 0, det = 1, wX = 0, dg = 1, kRes = 0;
    double cfA = 0, cfD = 0, caA = 0, caD = 0, caR = 0, inGain = 1, outGain = 1;

    // state
    double z[8] {};
    float eh[4096] {};
    float rb[RB] {};
    double hpY = 0, hpX = 0, cEnv = 0, agc = 0, eF = 0, eS = 0;
    bool gateOn = false; double lastOn = -1e9, pk = 0, notePk = 0; int openCnt = 0;
    double conf = 1, nG = 1, tfS = 1;
    int s = -1, subv = 1; int64_t lastEdge = -1;
    double period = 800, pend = 0, lastD = 0, noteF = 0, sawG = 0, lastSync = -1e9;
    bool haveP = false, stable = false;
    double ph1 = 0, ph2 = 0.33, ph3 = 0.71, ramp = 0, integDC = 0;
    double dr = 0; int drT = 0; uint32_t seed = 777;
    double y1 = 0, y2 = 0, y3 = 0, y4 = 0;
    double fe = 0, ae = 0; int fs = 0, as = 0;
    double dcX = 0, dcY = 0;
    double lastFreq = 0;
};

} // namespace bsyn
