#pragma once
// Monophonic pitch tracker (YIN algorithm) running on a lowpassed,
// decimated copy of the input. All memory is allocated in prepare().
#include "Common.h"

namespace xfx
{
class PitchTracker
{
public:
    void prepare (double fs, float minHz, float maxHz, float targetRate, float lowpassHz)
    {
        decim  = std::max (1, (int) std::lround (fs / targetRate));
        rate   = (float) (fs / decim);
        maxLag = (int) std::ceil (rate / minHz) + 2;
        minLag = std::max (2, (int) std::floor (rate / maxHz));
        win    = maxLag;
        frameLen = win + maxLag + 2;
        ringSize = nextPow2 (frameLen + 8);
        ring.assign ((size_t) ringSize, 0.0f);
        frame.assign ((size_t) frameLen, 0.0f);
        diff.assign ((size_t) maxLag + 2, 0.0f);
        hop = std::max (8, (int) (rate * 0.010f)); // ~10 ms analysis hop
        lp1.set (Biquad::Type::lowpass, fs, lowpassHz, 0.5412);
        lp2.set (Biquad::Type::lowpass, fs, lowpassHz, 1.3066);
        reset();
    }

    void reset() noexcept
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        lp1.reset(); lp2.reset();
        wpos = 0; decCount = 0; hopCount = 0;
        hz = 0.0f; conf = 0.0f; voiced = false;
    }

    inline void push (float x) noexcept
    {
        const float y = lp2.process (lp1.process (x));
        if (++decCount < decim) return;
        decCount = 0;
        ring[(size_t) wpos] = y;
        wpos = (wpos + 1) & (ringSize - 1);
        if (++hopCount >= hop) { hopCount = 0; analyse(); }
    }

    float getHz() const noexcept         { return hz; }
    float getConfidence() const noexcept { return conf; }
    bool  isVoiced() const noexcept      { return voiced; }

private:
    void analyse() noexcept
    {
        const int mask = ringSize - 1;
        const int start = (wpos - frameLen) & mask;
        double energy = 0.0;
        for (int i = 0; i < frameLen; ++i)
        {
            const float v = ring[(size_t) ((start + i) & mask)];
            frame[(size_t) i] = v;
            energy += (double) v * v;
        }
        if (std::sqrt (energy / frameLen) < 3.0e-4) { voiced = false; conf = 0.0f; return; }

        // YIN difference function + cumulative mean normalisation
        diff[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            float s = 0.0f;
            const float* a = frame.data();
            const float* b = frame.data() + tau;
            for (int j = 0; j < win; ++j) { const float d = a[j] - b[j]; s += d * d; }
            running += s;
            diff[(size_t) tau] = running > 0.0f ? s * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
        {
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxLag && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            float mn = 10.0f;
            for (int tau = minLag; tau < maxLag; ++tau)
                if (diff[(size_t) tau] < mn) { mn = diff[(size_t) tau]; best = tau; }
            if (best < 0 || mn > 0.4f) { voiced = false; conf = 0.0f; return; }
        }

        const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        const float den = a - 2.0f * b + c;
        const float shift = std::abs (den) > 1.0e-9f ? clampf (0.5f * (a - c) / den, -1.0f, 1.0f) : 0.0f;
        hz = rate / ((float) best + shift);
        conf = clampf (1.0f - b, 0.0f, 1.0f);
        voiced = conf > 0.6f;
    }

    std::vector<float> ring, frame, diff;
    Biquad lp1, lp2;
    int decim = 1, maxLag = 64, minLag = 2, win = 64, frameLen = 130, ringSize = 256, hop = 32;
    int wpos = 0, decCount = 0, hopCount = 0;
    float rate = 48000.0f, hz = 0.0f, conf = 0.0f;
    bool voiced = false;
};
} // namespace xfx
