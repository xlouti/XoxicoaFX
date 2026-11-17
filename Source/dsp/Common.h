#pragma once
// Small, allocation-free DSP helpers shared by all Xoxicoa FX modules.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace xfx
{
constexpr double kPiD   = 3.14159265358979323846;
constexpr float  kPi    = 3.14159265358979f;
constexpr float  kTwoPi = 6.28318530717959f;

inline float dbToGain (float db) noexcept   { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept    { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clampf (float x, float lo, float hi) noexcept { return std::min (hi, std::max (lo, x)); }
inline int   nextPow2 (int n) noexcept      { int p = 1; while (p < n) p <<= 1; return p; }
inline float msToCoef (double fs, double ms) noexcept
{
    return ms <= 0.0 ? 0.0f : (float) std::exp (-1.0 / (ms * 0.001 * fs));
}

/** RBJ-cookbook biquad, transposed direct form II. */
struct Biquad
{
    enum class Type { lowpass, highpass, bandpass, peak, lowShelf, highShelf };

    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void set (Type t, double fs, double f, double q, double gainDb = 0.0) noexcept
    {
        f = std::min (std::max (f, 5.0), fs * 0.48);
        q = std::max (q, 0.05);
        const double w0 = 2.0 * kPiD * f / fs, cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;

        switch (t)
        {
            case Type::lowpass:
                B0 = (1 - cw) * 0.5; B1 = 1 - cw; B2 = (1 - cw) * 0.5;
                A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case Type::highpass:
                B0 = (1 + cw) * 0.5; B1 = -(1 + cw); B2 = (1 + cw) * 0.5;
                A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case Type::bandpass: // 0 dB peak gain
                B0 = alpha; B1 = 0; B2 = -alpha;
                A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            case Type::peak:
                B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A;
                A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break;
            case Type::lowShelf:
            {
                const double sq = 2.0 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) - (A - 1) * cw + sq);
                B1 = 2 * A * ((A - 1) - (A + 1) * cw);
                B2 = A * ((A + 1) - (A - 1) * cw - sq);
                A0 = (A + 1) + (A - 1) * cw + sq;
                A1 = -2 * ((A - 1) + (A + 1) * cw);
                A2 = (A + 1) + (A - 1) * cw - sq; break;
            }
            case Type::highShelf:
            {
                const double sq = 2.0 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) + (A - 1) * cw + sq);
                B1 = -2 * A * ((A - 1) + (A + 1) * cw);
                B2 = A * ((A + 1) + (A - 1) * cw - sq);
                A0 = (A + 1) - (A - 1) * cw + sq;
                A1 = 2 * ((A - 1) - (A + 1) * cw);
                A2 = (A + 1) - (A - 1) * cw - sq; break;
            }
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
};

/** Peak envelope follower with separate attack / release. */
struct EnvFollower
{
    float env = 0, atk = 0, rel = 0;
    void prepare (double fs, double atkMs, double relMs) noexcept { atk = msToCoef (fs, atkMs); rel = msToCoef (fs, relMs); env = 0; }
    void reset() noexcept { env = 0; }
    inline float process (float x) noexcept
    {
        x = std::abs (x);
        const float c = x > env ? atk : rel;
        env = x + c * (env - x);
        return env;
    }
};

/** One-pole parameter smoother (exponential glide toward a target). */
struct Smoother
{
    float value = 0, target = 0, coef = 0;
    void prepare (double fs, double ms) noexcept { coef = msToCoef (fs, ms); }
    void setTarget (float t) noexcept { target = t; }
    void snap (float v) noexcept { value = target = v; }
    inline float next() noexcept { value = target + coef * (value - target); return value; }
};

/** Linear ramp 0..1 (used for click-free gates). */
struct LinearRamp
{
    float value = 0, step = 0.01f;
    void prepare (int rampSamples) noexcept { step = 1.0f / (float) std::max (1, rampSamples); }
    inline float next (bool up) noexcept
    {
        value = up ? std::min (1.0f, value + step) : std::max (0.0f, value - step);
        return value;
    }
};

inline float hermite (float f, float xm1, float x0, float x1, float x2) noexcept
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

/** Mono ring-buffer delay line with fractional (Hermite) reads. Delay must be >= 1. */
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int maxDelaySamples)
    {
        const int n = nextPow2 (maxDelaySamples + 8);
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) noexcept { w = (w + 1) & mask; buf[(size_t) w] = x; }
    inline float tap (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }
    inline float read (float d) const noexcept
    {
        d = std::max (1.0f, d);
        const int   i = (int) d;
        const float f = d - (float) i;
        return hermite (f, tap (i - 1), tap (i), tap (i + 1), tap (i + 2));
    }
};

/** Tiny deterministic RNG (xorshift32) + integer hash for per-bar seeds. */
struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed = 1) noexcept : s (seed ? seed : 0x9E3779B9u) {}
    inline uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
};

inline uint32_t hash32 (uint32_t x) noexcept
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

inline float midiFromHz (float hz) noexcept { return 69.0f + 12.0f * std::log2 (std::max (hz, 1.0f) / 440.0f); }
} // namespace xfx
