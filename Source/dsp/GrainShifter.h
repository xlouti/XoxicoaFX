#pragma once
// Delay-line pitch shifter with two read heads that sweep through a short
// window and crossfade (classic "rotating tape head" shifter).
#include "Common.h"

namespace xfx
{
class GrainShifter
{
public:
    /** xfade in (0, 0.5]: 0.5 = smooth sine crossfades, smaller = longer clean stretches. */
    void prepare (double fs, float windowMs, float maxBaseDelayMs, float xfadeFraction)
    {
        W = (float) (windowMs * 0.001 * fs);
        xf = clampf (xfadeFraction, 0.02f, 0.5f);
        dl.prepare ((int) (W + maxBaseDelayMs * 0.001 * fs) + 16);
        reset (0.0f);
    }
    void reset (float startPhase) noexcept { dl.reset(); phase = startPhase; }

    inline float process (float x, float ratio, float baseDelaySamples) noexcept
    {
        dl.push (x);
        phase += (1.0f - ratio) / W;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        const float w1 = weight (phase);
        const float d = std::max (1.0f, baseDelaySamples);
        return w1 * dl.read (d + phase * W) + (1.0f - w1) * dl.read (d + p2 * W);
    }

private:
    // w(p) + w(p + 0.5) == 1 and w(0) == 0, so the head that jumps is always silent.
    inline float weight (float p) const noexcept
    {
        if (p < xf)              return ramp (p / xf);
        if (p < 0.5f)            return 1.0f;
        if (p < 0.5f + xf)       return 1.0f - ramp ((p - 0.5f) / xf);
        return 0.0f;
    }
    static inline float ramp (float t) noexcept { return 0.5f - 0.5f * std::cos (kPi * t); }

    DelayLine dl;
    float W = 2048.0f, xf = 0.5f, phase = 0.0f;
};
} // namespace xfx
