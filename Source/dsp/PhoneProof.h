#pragma once
// 808 PHONE-PROOF: makes an 808 audible on phone speakers.
//
// 1. Isolate the low band (< 160 Hz) and track its pitch (YIN, 28-180 Hz).
// 2. Normalise the band to a unit-level wave and run it through Chebyshev
//    polynomials, which turn a sine at f0 into exact harmonics 2*f0 ... 7*f0.
// 3. Each harmonic goes through a bandpass tuned to n*f0 (follows the note), so
//    only clean harmonics survive. If no confident pitch is found, the untuned
//    harmonics are used instead.
// 4. Harmonics are band-limited to ~150 Hz .. 0.7-2.2 kHz and added on top.
//    The original sub is never cut or changed.
#include "PitchTracker.h"

namespace xfx
{
class PhoneProof
{
public:
    static constexpr int numH = 6; // harmonics 2..7

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        tracker.prepare (fs, 28.0f, 180.0f, 3000.0f, 220.0f);
        lowA.set (Biquad::Type::lowpass, fs, 160.0, 0.5412);
        lowB.set (Biquad::Type::lowpass, fs, 160.0, 1.3066);
        hp1.set (Biquad::Type::highpass, fs, 150.0, 0.707);
        hp2.set (Biquad::Type::highpass, fs, 90.0, 0.707);
        env.prepare (fs, 2.0, 90.0);
        wetS.prepare (fs, 30.0);
        lastTone = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        tracker.reset();
        lowA.reset(); lowB.reset(); hp1.reset(); hp2.reset(); toneLp.reset(); env.reset();
        for (auto& b : bp) b.reset();
        logF0 = std::log (50.0f); conf = 0.0f; counter = 0;
        wetS.snap (wetS.target);
        updateFilters();
    }

    void setParams (bool on, float amount01, float bright01, float tone01) noexcept
    {
        wetS.setTarget (on ? 2.0f * amount01 * amount01 : 0.0f);
        const float b = bright01;
        w[0] = 1.00f - 0.60f * b; // 2nd
        w[1] = 0.85f - 0.30f * b; // 3rd
        w[2] = 0.35f + 0.45f * b; // 4th
        w[3] = 0.20f + 0.60f * b; // 5th
        w[4] = 0.05f + 0.45f * b; // 6th
        w[5] = 0.00f + 0.35f * b; // 7th
        if (tone01 != lastTone)
        {
            lastTone = tone01;
            toneLp.set (Biquad::Type::lowpass, fs, 700.0 * std::pow (3.1428, (double) tone01), 0.707);
        }
    }

    float getDetectedHz() const noexcept { return conf > 0.5f ? std::exp (logF0) : 0.0f; }

    void process (float* L, float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            if (--counter <= 0) { counter = 32; updateFilters(); }

            const float m = 0.5f * (L[i] + R[i]);
            tracker.push (m);
            const float lb = lowB.process (lowA.process (m));
            const float e = env.process (lb);
            const float x = e > 1.0e-4f ? clampf (lb / (e * 1.02f), -1.0f, 1.0f) : 0.0f;

            const float x2 = x * x, x3 = x2 * x, x4 = x2 * x2, x5 = x4 * x, x6 = x3 * x3, x7 = x6 * x;
            const float T[numH] = {
                2.0f * x2 - 1.0f,
                4.0f * x3 - 3.0f * x,
                8.0f * x4 - 8.0f * x2 + 1.0f,
                16.0f * x5 - 20.0f * x3 + 5.0f * x,
                32.0f * x6 - 48.0f * x4 + 18.0f * x2 - 1.0f,
                64.0f * x7 - 112.0f * x5 + 56.0f * x3 - 7.0f * x
            };
            float tuned = 0.0f, untuned = 0.0f;
            for (int h = 0; h < numH; ++h)
            {
                const float v = w[h] * T[h];
                untuned += v;
                tuned += bp[h].process (v);
            }
            float harm = e * (conf * tuned + (1.0f - conf) * untuned * 0.6f);
            harm = toneLp.process (hp2.process (hp1.process (harm)));

            const float g = wetS.next();
            L[i] += g * harm;
            R[i] += g * harm;
        }
    }

private:
    void updateFilters() noexcept
    {
        const float target = tracker.isVoiced() ? tracker.getConfidence() : 0.0f;
        if (tracker.isVoiced())
            logF0 += 0.3f * (std::log (clampf (tracker.getHz(), 28.0f, 180.0f)) - logF0);
        conf += 0.15f * (target - conf);
        const double f0 = std::exp ((double) logF0);
        for (int h = 0; h < numH; ++h)
            bp[h].set (Biquad::Type::bandpass, fs, f0 * (h + 2), 3.0);
    }

    double fs = 48000.0;
    PitchTracker tracker;
    Biquad lowA, lowB, hp1, hp2, toneLp, bp[numH];
    EnvFollower env;
    Smoother wetS;
    float w[numH] { 1, 0.8f, 0.5f, 0.4f, 0.2f, 0.1f };
    float logF0 = 3.9f, conf = 0.0f, lastTone = -1.0f;
    int counter = 0;
};
} // namespace xfx
