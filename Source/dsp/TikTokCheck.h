#pragma once
// TIKTOK CHECK (monitoring only): phone speaker + platform loudness simulation.
// Mono fold -> phone speaker EQ -> heavy compression -> loudness push toward
// ~-14 dBFS RMS -> optional "crunch" (sample-rate + bit reduction) -> -1 dBFS limiter.
#include "Common.h"

namespace xfx
{
class TikTokCheck
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        hp1.set (Biquad::Type::highpass, fs, 280.0, 0.5412);
        hp2.set (Biquad::Type::highpass, fs, 280.0, 1.3066);
        pk1.set (Biquad::Type::peak, fs, 1200.0, 0.8, 2.0);
        pk2.set (Biquad::Type::peak, fs, 3000.0, 1.0, 4.5);
        lp1.set (Biquad::Type::lowpass, fs, 10000.0, 0.5412);
        lp2.set (Biquad::Type::lowpass, fs, 10000.0, 1.3066);
        compEnv.prepare (fs, 4.0, 120.0);
        msCoef = 1.0f - msToCoef (fs, 400.0);
        agcSlew = (float) (6.0 / fs);           // max 6 dB per second
        limRel = msToCoef (fs, 60.0);
        wetS.prepare (fs, 15.0);
        reset();
    }

    void reset() noexcept
    {
        for (auto* b : { &hp1, &hp2, &pk1, &pk2, &lp1, &lp2 }) b->reset();
        compEnv.reset();
        ms = 0.0f; agcDb = 0.0f; limG = 1.0f; srAcc = 0.0f; held = 0.0f;
        wetS.snap (wetS.target);
    }

    void setParams (bool previewOn, float crunch01) noexcept
    {
        wetS.setTarget (previewOn ? 1.0f : 0.0f);
        crunch = clampf (crunch01, 0.0f, 1.0f);
    }

    void forceOff() noexcept { wetS.snap (0.0f); }
    bool isAudible() const noexcept { return wetS.value > 1.0e-3f; }

    void process (float* L, float* R, int n) noexcept
    {
        const float srRatio = (float) ((32000.0 - 20000.0 * crunch) / fs);
        const float q = std::pow (2.0f, 13.0f - 5.0f * crunch);
        for (int i = 0; i < n; ++i)
        {
            const float w = wetS.next();
            if (w < 1.0e-5f && wetS.target == 0.0f) continue; // fully off: untouched

            float x = 0.5f * (L[i] + R[i]);
            x = lp2.process (lp1.process (pk2.process (pk1.process (hp2.process (hp1.process (x))))));

            // compressor: -24 dBFS threshold, 5:1
            const float eDb = gainToDb (compEnv.process (x));
            const float over = eDb + 24.0f;
            if (over > 0.0f) x *= dbToGain (-over * 0.8f);

            // loudness push toward ~-14 dBFS RMS (rough -14 LUFS feel)
            ms += msCoef * (x * x - ms);
            const float rmsDb = 10.0f * std::log10 (ms + 1.0e-12f);
            if (rmsDb > -60.0f)
            {
                const float target = clampf (-14.0f - rmsDb, -12.0f, 24.0f);
                agcDb += clampf (target - agcDb, -agcSlew, agcSlew);
            }
            x *= dbToGain (agcDb);

            if (crunch > 0.0f)
            {
                srAcc += srRatio;
                if (srAcc >= 1.0f) { srAcc -= 1.0f; held = x; }
                const float c = std::round (held * q) / q;
                x += crunch * (c - x);
            }

            // limiter, -1 dBFS ceiling
            const float ceil = 0.891f, a = std::abs (x);
            if (a * limG > ceil) limG = ceil / a;
            else limG = 1.0f + limRel * (limG - 1.0f);
            x = clampf (x * limG, -ceil, ceil);

            L[i] += w * (x - L[i]);
            R[i] += w * (x - R[i]);
        }
    }

private:
    double fs = 48000.0;
    Biquad hp1, hp2, pk1, pk2, lp1, lp2;
    EnvFollower compEnv;
    Smoother wetS;
    float ms = 0, msCoef = 0.0001f, agcDb = 0, agcSlew = 0.0001f, limG = 1, limRel = 0.999f;
    float crunch = 0, srAcc = 0, held = 0;
};

/** Always-on output safety: stereo-linked, zero-lookahead peak limiter + hard ceiling. */
class SafetyLimiter
{
public:
    void prepare (double fs) { rel = msToCoef (fs, 50.0); reset(); }
    void reset() noexcept { g = 1.0f; minG = 1.0f; }
    static constexpr float ceiling = 0.966f; // -0.3 dBFS

    void process (float* L, float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (! std::isfinite (l)) l = 0.0f;
            if (! std::isfinite (r)) r = 0.0f;
            const float peak = std::max (std::abs (l), std::abs (r));
            if (peak * g > ceiling) g = ceiling / peak;
            else g = 1.0f + rel * (g - 1.0f);
            minG = std::min (minG, g);
            L[i] = clampf (l * g, -ceiling, ceiling);
            R[i] = clampf (r * g, -ceiling, ceiling);
        }
    }
    /** Largest gain reduction since last call, in dB (<= 0). */
    float takeGainReductionDb() noexcept { const float d = gainToDb (minG); minG = 1.0f; return d; }

private:
    float g = 1.0f, rel = 0.999f, minG = 1.0f;
};
} // namespace xfx
