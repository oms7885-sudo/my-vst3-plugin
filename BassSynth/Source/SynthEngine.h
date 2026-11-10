// Bass DI -> Moog Model D style bass synth engine.
// No pitch tracking: three oscillators are drawn straight from the DI's own phase, mixed like the
// Model D mixer, then run through a nonlinear ladder filter and RC contours triggered by picks.
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
    // ---- setup (plugin only: turning a bass DI into "keys") ----
    float inGainDb = 5.0f;
    bool  comp = true;
    float cTh = -30.0f, cRat = 8.0f;   // dB, ratio
    float gateDb = -45.0f;
    float pickSens = 2.0f;
    float tail = 0.4f;                 // 0 = off
    float subLP = 120.0f, hyst = 0.15f; // internal: period detector for the note detector (not on the panel)
    // internal Model D character (not on the panel):
    // Both contours peak above "maximum sustain" before the decay brings them back (the Model D punch).
    // What is documented: the attack ends at a fixed threshold while the sustain control tops out at
    // roughly 80-85 % of that peak (service manual: flip-flop reset at the threshold; clone makers and
    // Moog technicians: sustain "10" behaves like 8-8.5), so a single note peaks about 1.18-1.25 above
    // maximum sustain. Re-triggering before the contour has settled pushes each peak higher, levelling
    // off after three or four quick hits about 30 % above the normal peak (8 V -> 10.5 V on a clone).
    float overshoot = 1.22f;           // peak of a single note, relative to maximum sustain
    float overshootMax = 1.60f;        // highest peak quick re-triggering can build up to (1.22 x 1.31)
    float resFade = 150.0f;            // Hz: Emphasis weakens as the cutoff falls towards this (full bass at low cutoff)
    // Fundamental tracking (internal; off = the oscillators follow the phase of everything under 250 Hz).
    // The low band of a bass note holds its 2nd and 3rd harmonics too, so its phase does not advance
    // evenly and, when a harmonic is louder than the fundamental, runs two or three cycles per period:
    // a warped saw with extra teeth, or a note that sounds an octave up. With tracking, a low-pass
    // placed just above the fundamental leaves only the fundamental before the phase is read, which
    // gives an even ramp at one cycle per period. Where the fundamental is comes from the period of
    // the low band (PeriodFinder); the oscillators' phase itself is still read straight off the DI, so
    // nothing is delayed. The filter opens wide on every pick (the attack is as it was) and closes on
    // the fundamental as soon as one period of the new note has been seen.
    bool  fundTrack = true;
    float ftMul = 1.1f;                // the low-pass sits at this x the fundamental
    float ftThr = 0.25f, ftDelta = 0.05f;   // PeriodFinder: how periodic the band must be / how deep a dip must be
    float ftWin = 14.0f;               // ms: PeriodFinder's window
    bool  ftComp = true;               // take the low-pass's own phase lag back out
    bool  ftRing = true;               // tell the last note ringing on from this note's fundamental
    float ftOwn = 1.6f;                // ... by how much louder it must be after the pick than before
    bool  ftAgain = false;             // ... or by its being the last note's fundamental again (off: misfires on octave patterns)
    int   ftHold = 3;                  // ms a period must hold before the filter goes there
    float tone = 250.0f;               // Hz, low-pass on the External (DI) signal
    float vintage = 0.3f;              // 0 = stable reissue .. 1 = old unit: osc 2/3 drift on their own

    // ---- oscillators ----
    // Range: -1 = LO (osc 3 only), 0 = 32', 1 = 16' (the note you play), 2 = 8', 3 = 4', 4 = 2'.
    // Wave: 0 triangle, 1 triangle-saw (osc 3: reverse saw), 2 saw, 3 square, 4 wide pulse, 5 narrow pulse.
    int   osc1Range = 1, osc1Wave = 2, osc2Range = 1, osc2Wave = 2, osc3Range = 0, osc3Wave = 3;
    float osc2Freq = 0.0f, osc3Freq = 0.0f;   // semitones, -8..+8 (service manual: 14-17 semitones end to end)
    bool  osc3Ctl = true;                     // Osc 3 Control: on = follows the DI, off = fixed pitch (drone / LFO)

    // ---- mixer (Model D: a volume and an on/off switch per source) ----
    float osc1Lvl = 0.8f, osc2Lvl = 0.0f, osc3Lvl = 0.5f, extLvl = 0.0f, noiseLvl = 0.0f;
    bool  osc1On = true, osc2On = true, osc3On = true, extOn = false, noiseOn = false;
    bool  noisePink = false;

    // ---- modulation (filter only) ----
    float modMix = 0.0f;               // 0 = osc 3 .. 1 = noise
    float modAmt = 0.0f;               // the mod wheel
    bool  filterMod = false;

    // ---- filter ----
    float cutoff = 160.0f, res = 0.24f, envAmt = 3.2f;   // res 0.78 = self-oscillation (Model D dial 7.5-8)
    bool  keyTrk1 = false, keyTrk2 = true;    // 1/3 + 2/3 keyboard control
    float fA = 3.0f, fD = 320.0f, fS = 0.1f;  // ms, ms, 0..1

    // ---- loudness ----
    float aA = 1.5f, aD = 900.0f, aS = 0.75f;
    bool  decaySw = false;             // off: the sound stops when the note ends; on: it dies away over Decay
    float dyn = 0.35f;

    // ---- output ----
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
        relFast = k (0.004 / 3.0);
        fA_ = k (0.001); fR_ = k (0.03); sA_ = k (0.04); sR_ = k (0.2);
        tfA = k (0.003); tfR = k (0.01);
        ehL  = (int) std::lround (0.015 * sr);
        ehL2 = (int) std::lround (0.025 * sr);
        pkR = std::exp (-1.0 / (3.0 * sr));
        BL = std::max (1, (int) std::lround (0.001 * sr));
        openN = (int) std::lround (0.0015 * sr);
        hold = 0.05 * sr; fastGap = 0.07 * sr; velA = 1 - std::exp (-1.0 / (0.003 * sr));
        inPkR = std::exp (-1.0 / (0.3 * sr)); drA = k (0.8);
        ktA = k (0.03);
        fcA = k (0.004); fuA = k (0.008);
        decD = std::max (1, (int) std::lround (sr / 2000.0)); pfWin = -1;
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
                hpY = hpX = cEnv = agc = agd = eF = eS = 0;
        gateOn = false; lastOn = -1e9; pk = notePk = 0; tfS = 1; onP = 0; vel = velS = 1; runMax = 0; decayed = false;
        h0X = h0Y = 0; cbm = 0; bc = 0; bi = 0; PW = 13; pkP = notePkP = 0;
        std::fill (std::begin (bm), std::end (bm), 0.0f);
        std::fill (std::begin (phb), std::end (phb), 0.0f);
        s = -1; lastEdge = -1; zf = 0; per = sr / 55.0; wnP = 0;
        hD1 = 0; phP = 0; ktf = 110; cyc = 0; acc[0] = acc[1] = acc[2] = 0; o3free = 0;
        std::fill (std::begin (pk3), std::end (pk3), 0.0); inPk = 0;
        seed = 777; drT = 0; dw[0] = dw[1] = dwT[0] = dwT[1] = 0;
        y1 = y2 = y3 = y4 = 0; l1 = l2 = l3 = l4 = 0;
        fe = ae = 0; fs = as = 0; stF = stA = 0;
        pf.reset(); decS = 0; decI = 0; fcT = fcN = 250.0; fundHz = fundUse = fundS = 0; lagP = 0; ph2P = 0; hD2 = 0; cyc2 = 0;
        fCand = ownF = prevOwn = 0; fRun = 0; miss = 0;
        std::fill (std::begin (sv), std::end (sv), 0.0);
        std::fill (std::begin (hs2), std::end (hs2), 0.0);
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
        if (pfWin != p.ftWin) { pfWin = p.ftWin; pf.prepare (sr / decD, pfWin); }
        mk = -p.cTh * (1.0 - 1.0 / p.cRat) * 0.5;
        thr = std::pow (10.0, p.gateDb / 20.0);
        // Emphasis scale: the service manual wants self-oscillation to begin between 7.5 and 8 on the dial
        // and to carry on down to a cutoff under 300 Hz. With 5.2 it begins at 0.78 and (with resFade)
        // holds down to about 200 Hz.
        kRes = 5.2 * p.res;
        cfA = k (p.fA / 3000.0); cfD = k (p.fD / 3000.0);
        caA = k (p.aA / 3000.0); caD = k (p.aD / 3000.0);
        keyAmt = (p.keyTrk1 ? 1.0 / 3.0 : 0.0) + (p.keyTrk2 ? 2.0 / 3.0 : 0.0);
        inGain = std::pow (10.0, p.inGainDb / 20.0);
        outGain = std::pow (10.0, p.outGainDb / 20.0);
    }

    float process (float input)
    {
        const Params& p = P;
        const int64_t i = n++;
        const double x0 = input * inGain;
        { const double ai = std::abs (x0); inPk = ai > inPk ? ai : inPk * inPkR; }

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

        // level detection
        const double ax = std::abs (hp);
        eF += (ax - eF) * (ax > eF ? fA_ : fR_);
        eS += (eF - eS) * (eF > eS ? sA_ : sR_);

        // ---- note detector (on the uncompressed signal) ----
        // peak of |x| over the last 28 ms; a note starts when that peak jumps above the lowest peak of
        // the previous ~40 ms. Settings checked against 17 annotated real bass lines (tests/eval_trigger.cpp)
        const double hp0 = hpA * (h0Y + x0 - h0X); h0X = x0; h0Y = hp0;
        {
            const double a0 = std::abs (hp0);
            if (a0 > cbm) cbm = a0;
            if (++bc >= BL)
            {
                bc = 0;
                // the window always spans the longest period a bass plays (28 ms). Sizing it from the
                // measured period failed on real recordings: the period detector often locks to the 2nd
                // harmonic, the window then covers half a cycle, and the level seems to jump on every cycle
                PW = 28;
                bm[bi & 63] = (float) cbm; cbm = 0;
                double P_ = 0;
                for (int q = 0; q < PW; ++q) P_ = std::max (P_, (double) bm[(bi - q) & 63]);
                phb[bi & 63] = (float) P_;
                ++bi;
                // has the current note clearly died away since its peak?
                if (P_ > runMax) runMax = P_; else if (P_ < 0.7 * runMax) decayed = true;
            }
        }
        double Pn = cbm;
        for (int q = 0; q < PW - 1; ++q) Pn = std::max (Pn, (double) bm[(bi - 1 - q) & 63]);
        // look back only to after the last note's own attack has settled
        const int back = std::min (40, (int) std::floor ((i - lastOn) / BL) - PW - 2);
        double Mn = 1e9;
        for (int q = 5; q <= back; ++q) Mn = std::min (Mn, (double) phb[(bi - q) & 63]);
        const double refP = pkP;                       // level of the recent notes, before this one
        pkP = Pn > pkP ? Pn : pkP * pkR;
        if (gateOn && Pn > notePkP) notePkP = Pn;
        pk = eF > pk ? eF : pk * pkR;
        if (gateOn && eF > notePk) notePk = eF;

        bool onset = false;
        // a pick often sounds a small click first and the full note 20-50 ms later: when the level
        // jumps well above the click inside the hold time, restart the note right there (on the beat)
        const bool upgrade = gateOn && i - lastOn <= hold && i - lastOn > 15 * BL
                             && Pn > 2.5 * std::max (onP, (double) phb[(bi - 10) & 63]) && Pn > pkP * 0.3;
        // fast playing: a re-pick on a ringing string only lifts the level by about as much as the last
        // note had decayed (often 1.5-2x at 16ths). Accept a smaller jump (1.5x at Pick Sens 2) when
        // the previous note has clearly died away, it is at least 70 ms old, and the new level is
        // close to that note's own peak (so damping noise and small bounces don't count)
        const bool fastPick = decayed && i - lastOn > fastGap && Pn > (1 + (p.pickSens - 1) * 0.5) * Mn
                              && Pn > 0.6 * notePkP && Pn > std::max (thr * 1.5, pkP * 0.25);
        if (upgrade || fastPick || (Pn > p.pickSens * Mn && Pn > std::max (thr * 1.5, pkP * 0.25) && i - lastOn > hold)) { onset = true; gateOn = true; }
        else if (gateOn && Pn < std::max (thr, notePkP * 0.12)) { gateOn = false; fs = 3; as = 3; }
        if (onset)
        {
            // a pick that lands while the last attack's peak is still standing (not yet back at the
            // sustain level) lifts the next peak, a third of the way to the limit per hit
            // (the click-then-pluck restart is one note and does not count as another hit)
            if (! upgrade)
            {
                const double ov0 = std::max (1.0, (double) p.overshoot);
                const double room = std::max (0.0, (double) p.overshootMax - ov0);
                stF = stackUp (stF, fe, p.fS, ov0, room, fs == 1 || fs == 2);
                stA = stackUp (stA, ae, p.aS, ov0, room, as == 1 || as == 2);
            }
            notePk = eF; notePkP = Pn; lastOn = (double) i; fs = 1; as = 1; onP = Pn; runMax = Pn; decayed = false;
            refOn = upgrade ? refOn : refP;
        }

        // ---- period of the DI (used by the note detector's window) ----
        if (s < 0 && wn > p.hyst && (lastEdge < 0 || (double) (i - lastEdge) > 0.4 * per))
        {
            s = 1;
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
        dbgI = hI; dbgQ = hQ;
        const double ph = std::atan2 (-hQ, hI) / TAU + 0.5;
        double dph = ph - phP; if (dph < -0.5) dph += 1; else if (dph > 0.5) dph -= 1;
        if (ph - phP < -0.5) ++cyc;     // one DI cycle completed (counts cycles for the 32' range)
        phP = ph;
        dbgDph = dph;
        double phO = ph, dphO = dph;    // what the oscillators are drawn from
        int64_t cycO = cyc;
        if (p.fundTrack)
        {
            // where the fundamental is: the period of the low band, looked at once a millisecond
            if (onset)
            {
                pf.pick(); fcT = 250.0; fRun = 0; miss = 0;
                prevOwn = ownF; ownF = 0;
            }
            decS += uu;
            if (++decI >= decD)
            {
                pf.push (decS / decD, (double) cyc + ph); decS = 0; decI = 0;
                if (pf.n & 1)
                {
                    double f1 = 0, fw = 0, f = 0;
                    const bool got = pf.find (p.ftThr, p.ftDelta, 8, f1, fw);
                    dbgF1 = f1; dbgFw = fw; dbgWind = 0;
                    if (got)
                    {
                        f = fw;
                        // How many cycles the low band's phase runs in one such period: 1 when the fundamental
                        // is the loudest thing in the band. More means something an octave (or more) above is
                        // louder: a harmonic of this note - or a new note, with the last one ringing on
                        // underneath it. The fundamental of the note just picked got louder at the pick; a
                        // note left ringing did not. If it is not this note's, follow the loudest instead.
                        const double wd = pf.wind (fw);
                        dbgWind = wd;
                        if (p.ftRing && wd > 1.3)
                        {
                            bool own = (ownF > 0 && std::abs (fw / ownF - 1.0) < 0.08)
                                    || (p.ftAgain && prevOwn > 0 && std::abs (fw / prevOwn - 1.0) < 0.08);
                            if (! own) own = pf.grew (fw, p.ftOwn);
                            if (own) ownF = fw;
                            else
                            {
                                const double k = std::round (wd);
                                f = std::abs (wd - k) < 0.2 && k <= 4 ? fw * k : 0.0;
                            }
                        }
                        if (f > 0 && fRun > 0 && std::abs (f / fCand - 1.0) < 0.04) ++fRun; else fRun = f > 0 ? 1 : 0;
                        fCand = f;
                    }
                    else fRun = 0;
                    if (fRun >= p.ftHold)                    // it has held for a few milliseconds: take it
                    {
                        fundHz = f; fundUse = f; miss = 0;
                        fcT = std::min (250.0, std::max (36.0, (double) p.ftMul * f));
                    }
                    else
                    {
                        fundHz = 0;
                        if (++miss > 25) fcT = 250.0;        // nothing to go by for a while: back to the whole low band
                    }
                }
            }

            // 4-pole low-pass just above it, then the phase of what is left
            fcN += (fcT - fcN) * fcA;
            fundS += (fundUse - fundS) * fuA;
            static constexpr double KQ[2] = { 1.0 / 0.54119610, 1.0 / 1.3065630 };
            double v = hp;
            {
                double g = PI * fcN / sr; g *= 1.0 + g * g / 3.0;              // tan() for small angles
                for (int q = 0; q < 2; ++q)
                {
                    const double a1 = 1.0 / (1.0 + g * (g + KQ[q])), a2 = g * a1, a3 = g * a2;
                    const double v3 = v - sv[2 * q + 1], v1 = a1 * sv[2 * q] + a2 * v3, v2 = sv[2 * q + 1] + a2 * sv[2 * q] + a3 * v3;
                    sv[2 * q] = 2 * v1 - sv[2 * q]; sv[2 * q + 1] = 2 * v2 - sv[2 * q + 1]; v = v2;
                }
            }
            double nI, nQ; hilbert (v, hs2, hD2, nI, nQ);
            dbgN = std::sqrt (nI * nI + nQ * nQ);
            const double ph2 = std::atan2 (-nQ, nI) / TAU + 0.5;
            double d2 = ph2 - ph2P; if (d2 < -0.5) d2 += 1; else if (d2 > 0.5) d2 -= 1;
            if (ph2 - ph2P < -0.5) ++cyc2;
            ph2P = ph2;

            // The low-pass delays the fundamental by a known part of a cycle (more the closer it sits).
            // Adding that back keeps the oscillators in step with the DI wherever the filter is, and
            // keeps them from bending while it moves.
            double lag = 0;
            if (p.ftComp && fundS > 0)
            {
                const double r = fundS / fcN, q = 1.0 - r * r;
                lag = (std::atan2 (r * KQ[0], q) + std::atan2 (r * KQ[1], q)) / TAU;
            }
            const double un = (double) cyc2 + ph2 + lag, fl = std::floor (un);
            cycO = (int64_t) fl; phO = un - fl; dphO = d2 + (lag - lagP); lagP = lag;
        }
        const double dt = std::min (std::max (dphO, 1e-4), 0.05);
        dbgOsc = (double) cycO + phO;

        // ---- vintage drift: osc 2 and 3 each wander by a few cents on their own ----
        if (--drT <= 0)
        {
            drT = 2048;
            for (int q = 0; q < 2; ++q) { dwT[q] += (rnd() * 0.7 - dwT[q]) * 0.08; }
        }
        for (int q = 0; q < 2; ++q) dw[q] += (dwT[q] - dw[q]) * drA;
        const double cents2 = p.vintage * (4.0 * dw[0] + 1.2), cents3 = p.vintage * (4.0 * dw[1] - 0.9);

        // ---- oscillator bank: drawn from the DI's own phase (no pitch tracking) ----
        const double gAmp = std::min (1.0, agd * 40.0);
        const double o1 = p.osc1On && p.osc1Lvl > 0 ? oscWave (p.osc1Wave, false, oscPhase (p.osc1Range, phO, dt, cycO, 0.0, acc[0])) : 0.0;
        const double o2 = p.osc2On && p.osc2Lvl > 0 ? oscWave (p.osc2Wave, false, oscPhase (p.osc2Range, phO, dt, cycO, p.osc2Freq + cents2 / 100.0, acc[1])) : 0.0;
        const bool need3 = (p.osc3On && p.osc3Lvl > 0) || (p.filterMod && p.modAmt > 0 && p.modMix < 1);
        double o3 = 0;
        if (need3)
        {
            if (p.osc3Ctl)
                o3 = oscWave (p.osc3Wave, true, oscPhase (p.osc3Range, phO, dt, cycO, p.osc3Freq + cents3 / 100.0, acc[2]));
            else
            {
                // Osc 3 Control off: a fixed pitch set by Range, Frequency sweeps about 6 octaves
                const int r = std::clamp (p.osc3Range, -1, 4);
                const double base = r < 0 ? 2.0 : 32.703 * std::pow (2.0, (double) r);   // LO = 2 Hz, 32' = C1
                const double f3 = base * std::pow (2.0, p.osc3Freq / 8.0 * 3.0 + cents3 / 1200.0);
                const double d3 = f3 / sr;
                o3free += d3; o3free -= std::floor (o3free);
                o3 = oscWave (p.osc3Wave, true, { o3free, d3 });
            }
        }
        if (gateOn && dphO > 0) ktf += (std::min (std::max (sr * dphO, 30.0), 600.0) - ktf) * ktA;
        const double ext = th (yn * 0.9);               // External: the DI itself
        // noise source: white, or pink (Kellet's economy filter)
        const double wht = rnd();
        pk3[0] = 0.99765 * pk3[0] + wht * 0.0990460;
        pk3[1] = 0.96300 * pk3[1] + wht * 0.2965164;
        pk3[2] = 0.57000 * pk3[2] + wht * 1.0526913;
        const double pnk = (pk3[0] + pk3[1] + pk3[2] + wht * 0.1848) * 0.22;
        const double nzS = p.noisePink ? pnk : wht * 0.6;

        double m = ((p.osc1On ? p.osc1Lvl * o1 : 0.0) + (p.osc2On ? p.osc2Lvl * o2 : 0.0)
                    + (p.osc3On ? p.osc3Lvl * o3 : 0.0) + (p.noiseOn ? p.noiseLvl * nzS : 0.0)) * gAmp
                   + (p.extOn ? p.extLvl * ext : 0.0);
        // The mixer feeds the ladder directly, like the Model D: one oscillator at a moderate volume stays
        // fairly clean and lets the resonance speak, three at full overload the filter and flatten it.
        // (This level is 1/3.4 of the engine's earlier one: Moog cut the filter's input drive by that
        // much during production, R2 47k -> 160k, to reduce intermodulation distortion.)
        m = th (m * mixIn) * 0.832;

        // filter modulation: osc 3 and noise, mixed, scaled by the "wheel"
        const double modSrc = (1.0 - p.modMix) * o3 + p.modMix * pnk * 2.5;
        const double fMod = p.filterMod ? std::pow (2.0, p.modAmt * 3.0 * modSrc) : 1.0;

        // nonlinear 4-pole ladder, 2x oversampled
        double fc = p.cutoff * std::pow (2.0, p.envAmt * fe) * fMod
                  * std::pow (std::min (8.0, std::max (0.125, ktf / 110.0)), keyAmt);
        fc = std::min (fc, sr * 0.45);
        // zero-delay-feedback ladder: the resonance feedback is solved within the same sample,
        // so the peak stays as strong at 5 kHz as at 150 Hz, like the analog ladder; tanh at the input and each stage
        const double gz = std::tan (PI * fc / (2 * sr)), Gz = gz / (1 + gz), G2 = Gz * Gz, G4 = G2 * G2;
        // like the Minimoog, the resonance fades as the cutoff comes down, so low notes keep their body
        const double rf = p.resFade > 0 ? fc / std::sqrt (fc * fc + (double) p.resFade * p.resFade) : 1.0;
        const double kR = kRes * rf;
        for (int o = 0; o < 2; ++o)
        {
            const double est = (G4 * m + (Gz * G2 * l1 + G2 * l2 + Gz * l3 + l4) / (1 + gz)) / (1 + kR * G4);
            double a = th (m - kR * est), v, yy;
            v = (th (a) - l1) * Gz; yy = v + l1; l1 = yy + v; a = yy;
            v = (th (a) - l2) * Gz; yy = v + l2; l2 = yy + v; a = yy;
            v = (th (a) - l3) * Gz; yy = v + l3; l3 = yy + v; a = yy;
            v = (th (a) - l4) * Gz; yy = v + l4; l4 = yy + v; y4 = yy;
        }

        // RC envelopes
        // the attack runs past "maximum" to the overshoot peak, then the decay brings it back
        const double ov = std::max (1.0, (double) p.overshoot), ovF = ov + stF, ovA = ov + stA;
        if (fs == 1) { fe += (1.3 * ovF - fe) * cfA; if (fe >= ovF) { fe = ovF; fs = 2; } }
        else if (fs == 2) fe += (p.fS - fe) * cfD;
        else if (fs == 3) fe += (0 - fe) * (p.decaySw ? cfD : relFast);
        if (as == 1) { ae += (1.3 * ovA - ae) * caA; if (ae >= ovA) { ae = ovA; as = 2; } }
        else if (as == 2) ae += (p.aS - ae) * caD;
        else if (as == 3) ae += (0 - ae) * (p.decaySw ? caD : relFast);

        const double dynF = (1 - p.dyn) + p.dyn * std::min (1.0, eF * 2.2);
        const double tf = std::min (1.0, eF / (notePk * p.tail + 1e-6));
        tfS += (tf - tfS) * (tf > tfS ? tfA : tfR);
        // a pick much softer than the recent notes (a click before the real pluck, a ghost note) plays
        // softer; measured over the note's first 30 ms as its attack builds
        if (gateOn && i - lastOn < 30 * BL)
            vel = std::min (1.0, std::max (0.2, notePkP / (0.4 * refOn + 1e-9)));
        velS += (vel - velS) * velA;
        const double yv = y4 * ladderOut * (1 + 0.22 * kR) * ae * velS * dynF * (p.tail > 0 ? tfS * tfS : 1.0);
        const double dc = yv - dcX + dcA * dcY; dcX = yv; dcY = dc;

        lastFreq = (gateOn && zf > 0 && i - lastEdge < sr / 25.0) ? zf : 0.0;
        if (onset) trigCount++;
        return (float) (softLimit (dc * outGain) + p.dry * input);
    }

    // for the editor display
    double currentFreq() const { return lastFreq; }
    double inputPeak() const   { return inPk; }          // input level after Input Gain (1.0 = full scale)
    double filterContour() const { return fe; }
    // probes for offline analysis (tests/eval_trigger.cpp)
    double probeMagnitude() const { return std::sqrt (dbgI * dbgI + dbgQ * dbgQ); }   // level of the low band
    double probePhaseStep() const { return dbgDph; }                                  // cycles per sample
    double probePeriod() const    { return per; }                                     // samples
    double loudnessContour() const { return ae; }
    double probeTrackHz() const   { return fcN; }       // fundamental tracking: where the low-pass is
    double probeFundHz() const    { return fundHz; }    // ... and the fundamental it found (0: none just now)
    double probeNarrow() const    { return dbgN; }      // ... and the level of what it lets through
    double probeFirstHz() const   { return dbgF1; }     // PeriodFinder: the loudest part's period
    double probeWholeHz() const   { return dbgFw; }     // PeriodFinder: the whole sound's period
    double probeWinding() const   { return dbgWind; }   // low-band phase cycles per such period
    double probeOscPhase() const  { return dbgOsc; }    // cycles the oscillators have run (16' range)
    bool   gateIsOpen() const  { return gateOn; }
    uint32_t triggers() const  { return trigCount; }

private:
    static constexpr double TAU = 6.283185307179586, PI = 3.141592653589793;
    static constexpr int EHM = 4095;


    static double th (double q) { return q > 3 ? 1 : q < -3 ? -1 : q * (27 + q * q) / (27 + 9 * q * q); }
    // output stage: untouched up to 0.8 of full scale (a synth's output does not squash its own
    // attacks), then a soft knee that never passes full scale
    static double softLimit (double v)
    {
        const double a = std::abs (v);
        if (a <= 0.8) return v;
        const double r = 0.8 + 0.2 * std::tanh ((a - 0.8) / 0.2);
        return v < 0 ? -r : r;
    }
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
    // Peak build-up on quick re-triggering. `level` is the contour right now, `sustain` where its decay
    // is heading, `ov0` the single-note peak, `room` how far above that the peak may grow. The share of
    // the last peak still standing above the sustain level decides how much carries over; a contour
    // that has settled (or was released) starts again from the single-note peak.
    static double stackUp (double stack, double level, double sustain, double ov0, double room, bool sounding)
    {
        if (! sounding || room <= 0) return 0.0;
        const double span = ov0 + stack - sustain;
        const double carry = span > 1e-6 ? std::min (1.0, std::max (0.0, (level - sustain) / span)) : 0.0;
        return std::min (room, carry * (stack + room / 3.0));
    }

    // Times the last full cycle of a phase that is fed in step by step, and keeps the low end of the
    // rates of the last ~100 ms (one figure per millisecond).
    // Period of the low band: YIN's difference function on a 2 kHz copy, slid along one sample at a time.
    // d[t] = sum over the last W samples of (x[j] - x[j - t])^2, which is small when t is a whole number
    // of periods; that holds whichever harmonic is the loudest, which is what the phase alone cannot tell.
    struct PeriodFinder
    {
        static constexpr int N = 256, LMAX = 84, PRE = 96;
        double ring[N] {}, uw[N] {}, pre[PRE] {}, d[LMAX + 1] {}, fs = 2000.0;
        int64_t n = 0, on = 0;
        int W = 28, lmin = 6, lmax = 74, ref = 1;
        void prepare (double fsDec, double winMs)
        {
            fs = fsDec;
            W = std::min (64, std::max (8, (int) std::lround (winMs * 0.001 * fs)));
            lmin = std::max (2, (int) (fs / 320.0)); lmax = std::min (LMAX - 1, (int) std::ceil (fs / 27.0));
            reset();
        }
        void reset()
        {
            std::fill (std::begin (ring), std::end (ring), 0.0); std::fill (std::begin (uw), std::end (uw), 0.0);
            std::fill (std::begin (pre), std::end (pre), 0.0); std::fill (std::begin (d), std::end (d), 0.0);
            n = 0; on = 0; ref = 1;
        }
        // a new note: keep what the band was doing up to 4 ms before it (the attack is not in there yet)
        void pick()
        {
            on = n;
            for (int j = 0; j < PRE; ++j) pre[j] = ring[(n - 8 - PRE + j) & (N - 1)];
        }
        // x: the low band; phase: its running phase in cycles (not wrapped)
        void push (double x, double phase)
        {
            ring[n & (N - 1)] = x; uw[n & (N - 1)] = phase;
            const double xo = ring[(n - W) & (N - 1)];
            for (int t = 1; t <= lmax; ++t)
            {
                const double a = x - ring[(n - t) & (N - 1)], b = xo - ring[(n - W - t) & (N - 1)];
                d[t] += a * a - b * b;
            }
            double sum = 0;                                   // one lag summed afresh each time: rounding never builds up
            for (int j = 0; j < W; ++j) { const double a = ring[(n - j) & (N - 1)] - ring[(n - j - ref) & (N - 1)]; sum += a * a; }
            d[ref] = sum; if (++ref > lmax) ref = 1;
            ++n;
        }
        // cycles the band's phase has run over the last period of `hz`
        double wind (double hz) const
        {
            const double lag = fs / hz; const int l0 = (int) lag; const double fr = lag - l0;
            if (l0 + 2 >= N || n < l0 + 3) return 1.0;
            const double a = uw[(n - 1 - l0) & (N - 1)], b = uw[(n - 2 - l0) & (N - 1)];
            return uw[(n - 1) & (N - 1)] - (a + (b - a) * fr);
        }
        // level of the part of `buf` (len samples, oldest first) that swings at `hz`
        double level (const double* buf, int len, double hz) const
        {
            const double w = TAU * hz / fs, wn = TAU / (double) (len + 1);
            double c = 0, q = 0, sum = 0;
            for (int j = 0; j < len; ++j)
            {
                const double h = 0.5 - 0.5 * std::cos (wn * (j + 1)), v = buf[j] * h;
                c += v * std::cos (w * j); q += v * std::sin (w * j); sum += h;
            }
            return 2.0 * std::sqrt (c * c + q * q) / sum;
        }
        // Did the part at `hz` get louder with the last pick? Compares the same length of signal before the
        // pick and now (at least a period and a half of it, all of it after the pick).
        bool grew (double hz, double ratio) const
        {
            const int len = (int) std::min ((int64_t) PRE, n - on - 6);
            if (len < (int) (1.5 * fs / hz)) return false;
            double now[PRE];
            for (int j = 0; j < len; ++j) now[j] = ring[(n - len + j) & (N - 1)];
            return level (now, len, hz) > ratio * level (pre + (PRE - len), len, hz) + 1e-5;
        }
        // Looks for the period. `first` is the shortest lag at which the band repeats well (what the loudest
        // part of the sound is doing), `whole` the shortest at which it repeats about as well as it ever
        // does (the sound as a whole). The two differ when a harmonic is louder than the fundamental - or
        // when the last note is still ringing underneath a new one. False when the band is not periodic,
        // or when the stretch that would say so reaches back before the last pick.
        bool find (double thr, double delta, int guard, double& first, double& whole) const
        {
            double cm[LMAX + 2], run = 0, deep = 1e9;
            for (int t = 1; t <= lmax; ++t) { run += d[t]; cm[t] = run > 1e-18 ? d[t] * t / run : 1.0; }
            if (run < 1e-12) return false;
            for (int t = lmin; t < lmax; ++t) deep = std::min (deep, cm[t]);
            if (deep > thr) return false;
            int ta = 0, tb = 0;
            for (int t = lmin; t < lmax; ++t)
                if (cm[t] <= cm[t - 1] && cm[t] <= cm[t + 1])
                {
                    if (n - on < W + t + guard) break;
                    if (ta == 0 && cm[t] < thr) ta = t;
                    if (cm[t] <= deep + delta) { tb = t; break; }
                }
            if (ta == 0) return false;
            if (tb == 0) tb = ta;
            const auto hz = [&] (int t)
            {
                const double a = cm[t - 1], b = cm[t], c = cm[t + 1], den = a - 2 * b + c;
                return fs / ((double) t + (den > 1e-12 ? std::min (0.5, std::max (-0.5, 0.5 * (a - c) / den)) : 0.0));
            };
            first = hz (ta); whole = tb == ta ? first : hz (tb);
            return true;
        }
    };

    struct OscPh { double t, dt; };

    // phase of one oscillator: range shifts the DI's phase by octaves (counting DI cycles for the low
    // ranges); a Frequency offset or vintage drift runs a phase accumulator at the DI's rate times the
    // interval and runs free (like a real Moog), so the beating lands somewhere different on every note
    OscPh oscPhase (int range, double ph, double dt, int64_t cycles, float semi, double& accum) const
    {
        const int r = std::clamp (range, -1, 4);
        const int o = r < 0 ? -3 : r - 1;                         // LO = 3 octaves below 16', 32' = -1 .. 2' = +3
        double t, d;
        if (o < 0) { const int64_t kd = (int64_t) 1 << -o; t = (ph + (double) (cycles % kd)) / (double) kd; d = dt / (double) kd; }
        else       { const double mul = (double) (1 << o); t = ph * mul; t -= std::floor (t); d = dt * mul; }
        if (std::abs (semi) < 1e-3) { accum = t; return { t, d }; }
        const double ratio = std::pow (2.0, semi / 12.0);
        accum += d * ratio; accum -= std::floor (accum);
        return { accum, std::min (d * ratio, 0.1) };
    }

    static double oscWave (int w, bool osc3, OscPh q)
    {
        const double t = q.t, dt = std::max (q.dt, 1e-5);
        auto saw   = [&] { return 2 * t - 1 - polyblep (t, dt); };
        auto tri   = [&] { return 1 - 4 * std::abs (t - 0.5); };
        auto pulse = [&] (double pw)
        {
            double tp = t + (1 - pw); tp -= std::floor (tp);
            return (t < pw ? 1.0 : -1.0) + polyblep (t, dt) - polyblep (tp, dt);
        };
        switch (w)
        {
            case 0:  return tri();
            case 1:  return osc3 ? -saw() : 0.5 * (tri() + saw());
            case 2:  return saw();
            case 3:  return pulse (0.5);
            case 4:  return pulse (0.3);
            default: return pulse (0.15);
        }
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
    double velA = 0;
    double pkR = 0, hold = 0, ktA = 0, dcA = 0;
    int ehL = 0, ehL2 = 0, openN = 0;
    double c[5] {}, cd[5] {}, cs[5] {};
    double mk = 0, thr = 0, kRes = 0, keyAmt = 2.0 / 3.0, relFast = 0, inPkR = 0, drA = 0;
    static constexpr double mixIn = 0.6;        // mixer sum -> ladder input (was 2.035)
    static constexpr double ladderOut = 1.75;   // level after the ladder, set so the default patch is as loud as before
    double cfA = 0, cfD = 0, caA = 0, caD = 0, inGain = 1, outGain = 1;

    // state
    double z[8] {}, zd[8] {}, zs[8] {};
    int64_t cyc = 0; double acc[3] {};
    double hs1[32] {}; double hD1 = 0, phP = 0, ktf = 110, o3free = 0, pk3[3] {}, inPk = 0;
    float eh[4096] {};
    double hpY = 0, hpX = 0, cEnv = 0, agc = 0, agd = 0, eF = 0, eS = 0;
    double onP = 0, vel = 1, velS = 1, refOn = 0, runMax = 0, fastGap = 3087; bool decayed = false;
    bool gateOn = false; double lastOn = -1e9, pk = 0, notePk = 0; double tfS = 1;
    double h0X = 0, h0Y = 0, cbm = 0, pkP = 0, notePkP = 0; int bc = 0, BL = 44, PW = 13; int64_t bi = 0;
    float bm[64] {}, phb[64] {};
    int s = -1; int64_t lastEdge = -1; double zf = 0, per = 800, wnP = 0;
    int drT = 0; double dw[2] {}, dwT[2] {}; uint32_t seed = 777;
    double y1 = 0, y2 = 0, y3 = 0, y4 = 0, l1 = 0, l2 = 0, l3 = 0, l4 = 0;
    double fe = 0, ae = 0; int fs = 0, as = 0;
    // fundamental tracking
    PeriodFinder pf;
    double fcT = 250, fcN = 250, fcA = 0, fuA = 0, fundHz = 0, fundUse = 0, fundS = 0, lagP = 0, decS = 0, dbgN = 0;
    double sv[4] {}, hs2[32] {}, hD2 = 0, ph2P = 0, pfWin = -1;
    double dbgF1 = 0, dbgFw = 0, dbgWind = 0, fCand = 0, ownF = 0, prevOwn = 0;
    int64_t cyc2 = 0; int decD = 22, decI = 0, fRun = 0, miss = 0;
    double stF = 0, stA = 0;       // extra peak height built up by quick re-triggering (filter, loudness)
    double dcX = 0, dcY = 0;
    double lastFreq = 0, dbgI = 0, dbgQ = 0, dbgDph = 0, dbgOsc = 0;
    uint32_t trigCount = 0;
};

} // namespace bsyn
