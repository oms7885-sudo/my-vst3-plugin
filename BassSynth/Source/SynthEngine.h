// Bass DI -> Moog-style bass synth engine (DI modulation version).
// No pitch tracking: the DI waveform itself is clipped, folded and octave-shifted,
// then runs through a nonlinear ladder filter and RC envelopes triggered by picks.
// Header-only, no JUCE dependency, so it can be tested offline (see tests/).
// This is a sample-by-sample port of the browser prototype ("Bass Ladder").
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace bsyn
{

struct Params
{
    // input / setup
    float inGainDb = 5.0f;
    bool  comp = true;
    float cTh = -30.0f, cRat = 8.0f;   // dB, ratio
    float gateDb = -45.0f;
    float pickSens = 2.0f;
    float tail = 0.4f;                 // 0 = off
    float subLP = 120.0f;              // Hz, octave-down detector
    float hyst = 0.15f;
    // shaper
    float tone = 250.0f;               // Hz, low-pass before shaping
    float shapeAmt = 0.7f, foldAmt = 0.4f;
    float detune = 7.0f, drift = 2.0f; // cents
    // mixer
    float sawLvl = 0.8f, satLvl = 0.4f, foldLvl = 0.0f, subLvl = 0.5f, upLvl = 0.0f, cleanLvl = 0.0f, detLvl = 0.35f;
    float drive = 0.45f;
    // filter
    float cutoff = 160.0f, res = 0.3f, envAmt = 3.2f, keytrk = 0.5f;
    float fA = 3.0f, fD = 320.0f, fS = 0.1f;          // ms, ms, 0..1
    // loudness
    float aA = 1.5f, aD = 900.0f, aS = 0.75f, aR = 90.0f;
    float dyn = 0.35f;
    // output
    float outGainDb = 0.0f;
    float dry = 0.0f;
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hpA = std::exp (-TAU * 30.0 / sr);
        cAt = k (0.002); cRl = k (0.15);
        agA = k (0.002); agR = k (0.08);
        fA_ = k (0.001); fR_ = k (0.03); sA_ = k (0.04); sR_ = k (0.2);
        tfA = k (0.003); tfR = k (0.01);
        ehL  = (int) std::lround (0.015 * sr);
        ehL2 = (int) std::lround (0.025 * sr);
        pkR = std::exp (-1.0 / (1.5 * sr));
        openN = (int) std::lround (0.0015 * sr);
        hold = 0.06 * sr;
        upA = k (0.008);
        ktA = k (0.03);
        lpCoef (250.0, cs);
        dcA = std::exp (-TAU * 15.0 / sr);
        reset();
        haveCoefs = false;
    }

    void reset()
    {
        std::fill (std::begin (z), std::end (z), 0.0);
        std::fill (std::begin (zd), std::end (zd), 0.0);
        std::fill (std::begin (eh), std::end (eh), 0.0f);
        std::fill (std::begin (zs), std::end (zs), 0.0);
        std::fill (std::begin (hs1), std::end (hs1), 0.0);
        std::fill (std::begin (hs2), std::end (hs2), 0.0);
        hpY = hpX = cEnv = agc = agd = eF = eS = 0;
        gateOn = false; lastOn = -1e9; pk = notePk = 0; openCnt = 0; tfS = 1;
        s = -1; subv = 1; lastEdge = -1; zf = 0; upM = 0; per = sr / 55.0; wnP = 0;
        hD1 = hD2 = 0; phP = 0; ktf = 110; thU = thD = 0;
        dr = 0; drT = 0; seed = 777;
        y1 = y2 = y3 = y4 = 0; l1 = l2 = l3 = l4 = 0;
        fe = ae = 0; fs = as = 0;
        dcX = dcY = 0;
        n = 0;
    }

    // Call once per block. Recomputes the parameter-dependent coefficients.
    void setParams (const Params& p)
    {
        if (! haveCoefs || p.tone != P.tone)   lpCoef (p.tone, c);
        if (! haveCoefs || p.subLP != P.subLP) lpCoef (p.subLP, cd);
        haveCoefs = true;
        P = p;
        mk = -p.cTh * (1.0 - 1.0 / p.cRat) * 0.5;
        thr = std::pow (10.0, p.gateDb / 20.0);
        sG = 1.0 + p.shapeAmt * 14.0; sN = 1.0 / th (sG * 0.8);
        fG = PI * (0.6 + p.foldAmt * 3.0);
        wX = std::min (1.0, p.detune / 2.0) * p.detLvl;
        dg = 1.0 + p.drive * 6.0;
        kRes = 4.1 * p.res;
        cfA = k (p.fA / 3000.0); cfD = k (p.fD / 3000.0);
        caA = k (p.aA / 3000.0); caD = k (p.aD / 3000.0); caR = k (p.aR / 3000.0);
        inGain = std::pow (10.0, p.inGainDb / 20.0);
        outGain = std::pow (10.0, p.outGainDb / 20.0);
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
        const double hp = hpA * (hpY + x - hpX); hpX = x; hpY = hp;

        // tone filter before shaping, then level-normalise
        const double vv = biquad2 (hp, c, z);
        const double av = std::abs (vv); agc += (av - agc) * (av > agc ? agA : agR);
        const double yn = vv / (agc + 1e-4);

        // darker copy only for the octave-down flip-flop
        const double w = biquad2 (hp, cd, zd);
        const double aw = std::abs (w); agd += (aw - agd) * (aw > agd ? agA : agR);
        const double wn = w / (agd + 1e-4);

        // level detection + pick gate
        const double ax = std::abs (hp);
        eF += (ax - eF) * (ax > eF ? fA_ : fR_);
        eS += (eF - eS) * (eF > eS ? sA_ : sR_);
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
        if (onset)
        {
            notePk = eF; lastOn = (double) i; fs = 1; as = 1;
        }

        // ---- DI modulation ----
        const double sat  = th (yn * sG) * sN;          // clipping: soft -> near square
        const double fold = std::sin (yn * fG);         // wavefolder
        if (s < 0 && wn > p.hyst && (lastEdge < 0 || (double) (i - lastEdge) > 0.4 * per))
        {
            s = 1; subv = -subv;
            if (lastEdge >= 0)
            {
                const double d = (double) (i - lastEdge);
                if (d > sr / 1200.0 && d < sr / 20.0) { per = d; zf += (sr / d - zf) * 0.3; }
            }
            lastEdge = i;
        }
        else if (s > 0 && wn < -p.hyst) s = -1;
        wnP = wn;

        // saw from the DI's instantaneous phase (Hilbert pair on a fixed 250 Hz copy):
        // follows the waveform at once, no period measuring
        const double uu = biquad2 (hp, cs, zs);
        double hI, hQ; hilbert (uu, hs1, hD1, hI, hQ);
        const double ph = std::atan2 (-hQ, hI) / TAU + 0.5;
        double dph = ph - phP; if (dph < -0.5) dph += 1; else if (dph > 0.5) dph -= 1;
        phP = ph;
        const double dt = std::min (std::max (dph, 1e-4), 0.05);
        const double saw = 2 * ph - 1 - polyblep (ph, dt);
        if (gateOn && dph > 0) ktf += (std::min (std::max (sr * dph, 30.0), 600.0) - ktf) * ktA;
        const double sub = subv * std::abs (sat);       // octave down
        const double ra = std::abs (sat); upM += (ra - upM) * upA;
        const double up = (ra - upM) * 2.0;             // octave up
        const double cln = th (yn * 0.9);               // the DI waveform itself
        double m = (p.satLvl * sat + p.foldLvl * fold + p.subLvl * sub + p.upLvl * up + p.cleanLvl * cln
                    + p.sawLvl * saw * std::min (1.0, agd * 40.0)) * 0.55;

        // detune voices: the modulated DI frequency-shifted up and down (no delay, so no flam)
        if (wX > 0)
        {
            if (--drT <= 0) { drT = 2048; dr += (rnd() - dr) * 0.15; }
            double mI, mQ; hilbert (m, hs2, hD2, mI, mQ);
            const double fUp = ktf * (std::pow (2.0, (p.detune + p.drift * dr) / 1200.0) - 1);
            const double fDn = ktf * (1 - std::pow (2.0, -(p.detune - p.drift * dr) / 1200.0));
            thU += TAU * fUp / sr; if (thU > TAU) thU -= TAU;
            thD += TAU * fDn / sr; if (thD > TAU) thD -= TAU;
            const double vU = mI * std::cos (thU) - mQ * std::sin (thU);
            const double vD = mI * std::cos (thD) + mQ * std::sin (thD);
            m = (mI + wX * 0.8 * (vU + vD)) / (1 + wX * 0.9);
        }
        m = th (m * dg) / std::sqrt (dg) * 1.6;

        // nonlinear 4-pole ladder, 2x oversampled
        double fc = p.cutoff * std::pow (2.0, p.envAmt * fe)
                  * std::pow (std::min (8.0, std::max (0.125, ktf / 110.0)), (double) p.keytrk);
        fc = std::min (fc, sr * 0.45);
        // zero-delay-feedback ladder: the resonance feedback is solved within the same sample,
        // so the peak stays as strong at 5 kHz as at 150 Hz, like the analog ladder; tanh at the input and each stage
        const double gz = std::tan (PI * fc / (2 * sr)), Gz = gz / (1 + gz), G2 = Gz * Gz, G4 = G2 * G2;
        for (int o = 0; o < 2; ++o)
        {
            const double est = (G4 * m + (Gz * G2 * l1 + G2 * l2 + Gz * l3 + l4) / (1 + gz)) / (1 + kRes * G4);
            double a = th (m - kRes * est), v, yy;
            v = (th (a) - l1) * Gz; yy = v + l1; l1 = yy + v; a = yy;
            v = (th (a) - l2) * Gz; yy = v + l2; l2 = yy + v; a = yy;
            v = (th (a) - l3) * Gz; yy = v + l3; l3 = yy + v; a = yy;
            v = (th (a) - l4) * Gz; yy = v + l4; l4 = yy + v; y4 = yy;
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
        const double yv = y4 * (1 + p.res * 0.9) * ae * dynF * (p.tail > 0 ? tfS * tfS : 1.0);
        const double dc = yv - dcX + dcA * dcY; dcX = yv; dcY = dc;

        lastFreq = (gateOn && zf > 0 && i - lastEdge < sr / 25.0) ? zf : 0.0;
        if (onset) trigCount++;
        return (float) (th (dc * outGain) + p.dry * input);
    }

    // for the editor display
    double currentFreq() const { return lastFreq; }
    bool   gateIsOpen() const  { return gateOn; }
    uint32_t triggers() const  { return trigCount; }

private:
    static constexpr double TAU = 6.283185307179586, PI = 3.141592653589793;
    static constexpr int EHM = 4095;


    static double th (double q) { return q > 3 ? 1 : q < -3 ? -1 : q * (27 + q * q) / (27 + 9 * q * q); }
    double k (double t) const { return 1.0 - std::exp (-1.0 / (std::max (t, 1e-5) * sr)); }
    double rnd()
    {
        seed = (seed * 1103515245u + 12345u) & 0x7fffffffu;
        return (double) seed / (double) 0x7fffffff * 2 - 1;
    }
    void lpCoef (double f, double* cc) const
    {
        const double w = TAU * f / sr, cw = std::cos (w), a = std::sin (w) / (2 * 0.7071), a0 = 1 + a;
        cc[0] = (1 - cw) / 2 / a0; cc[1] = (1 - cw) / a0; cc[2] = (1 - cw) / 2 / a0;
        cc[3] = -2 * cw / a0;      cc[4] = (1 - a) / a0;
    }
    static double biquad2 (double in, const double* cc, double* st)
    {
        double q = in;
        for (int k2 = 0; k2 < 2; ++k2)
        {
            double* s4 = st + k2 * 4;
            const double y = cc[0] * q + cc[1] * s4[0] + cc[2] * s4[1] - cc[3] * s4[2] - cc[4] * s4[3];
            s4[1] = s4[0]; s4[0] = q; s4[3] = s4[2]; s4[2] = y; q = y;
        }
        return q;
    }
    static double polyblep (double t, double dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1; }
        if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
        return 0;
    }
    // 90-degree allpass pair (Niemitalo). st holds 32 values, dl is the one-sample delay of path A.
    static void hilbert (double x, double* st, double& dl, double& outI, double& outQ)
    {
        static constexpr double HA[4] = { 0.6923878 * 0.6923878, 0.9360654322959 * 0.9360654322959,
                                          0.9882295226860 * 0.9882295226860, 0.9987488452737 * 0.9987488452737 };
        static constexpr double HB[4] = { 0.4021921162426 * 0.4021921162426, 0.8561710882420 * 0.8561710882420,
                                          0.9722909545651 * 0.9722909545651, 0.9952884791278 * 0.9952884791278 };
        double a = x, b = x;
        for (int q = 0; q < 4; ++q)
        {
            double* o = st + q * 4;
            const double ya = HA[q] * (a + o[2]) - o[0];
            o[0] = o[1]; o[1] = a; o[2] = o[3]; o[3] = ya; a = ya;
            double* o2 = st + 16 + q * 4;
            const double yb = HB[q] * (b + o2[2]) - o2[0];
            o2[0] = o2[1]; o2[1] = b; o2[2] = o2[3]; o2[3] = yb; b = yb;
        }
        outI = dl; dl = a; outQ = b;
    }

    Params P;
    bool haveCoefs = false;
    double sr = 44100;
    int64_t n = 0;

    // coefficients
    double hpA = 0, cAt = 0, cRl = 0, agA = 0, agR = 0, fA_ = 0, fR_ = 0, sA_ = 0, sR_ = 0, tfA = 0, tfR = 0;
    double pkR = 0, hold = 0, upA = 0, ktA = 0, dcA = 0;
    int ehL = 0, ehL2 = 0, openN = 0;
    double c[5] {}, cd[5] {}, cs[5] {};
    double mk = 0, thr = 0, sG = 1, sN = 1, fG = 0, wX = 0, dg = 1, kRes = 0;
    double cfA = 0, cfD = 0, caA = 0, caD = 0, caR = 0, inGain = 1, outGain = 1;

    // state
    double z[8] {}, zd[8] {}, zs[8] {};
    double hs1[32] {}, hs2[32] {}; double hD1 = 0, hD2 = 0, phP = 0, ktf = 110, thU = 0, thD = 0;
    float eh[4096] {};
    double hpY = 0, hpX = 0, cEnv = 0, agc = 0, agd = 0, eF = 0, eS = 0;
    bool gateOn = false; double lastOn = -1e9, pk = 0, notePk = 0; int openCnt = 0; double tfS = 1;
    int s = -1, subv = 1; int64_t lastEdge = -1; double zf = 0, upM = 0, per = 800, wnP = 0;
    double dr = 0; int drT = 0; uint32_t seed = 777;
    double y1 = 0, y2 = 0, y3 = 0, y4 = 0, l1 = 0, l2 = 0, l3 = 0, l4 = 0;
    double fe = 0, ae = 0; int fs = 0, as = 0;
    double dcX = 0, dcY = 0;
    double lastFreq = 0;
    uint32_t trigCount = 0;
};

} // namespace bsyn
