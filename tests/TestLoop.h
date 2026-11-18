// Shared synthetic 160 BPM rage loop + measurement helpers for the test harnesses.
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
namespace testloop {
constexpr double kFs = 48000.0, kBpm = 160.0;
constexpr int kBars = 8, kBlock = 512;
double polyBlep (double t, double dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

juce::AudioBuffer<float> makeLoop()
{
    const double beat = 60.0 / kBpm;
    const int total = (int) (kBars * 4 * beat * kFs);
    juce::AudioBuffer<float> b (2, total);
    b.clear();
    juce::Random rng (1234);
    const double bass[] = { 43.65, 43.65, 51.91, 38.89 };              // F1 F1 G#1 D#1
    const double lead[] = { 349.23, 415.30, 523.25, 622.25, 523.25, 415.30, 466.16, 392.00 }; // F minor
    for (int bar = 0; bar < kBars; ++bar)
    {
        for (double hit : { 0.0, 2.5 })                                // kick + 808
        {
            const int start = (int) ((bar * 4 + hit) * beat * kFs);
            const double f808 = bass[(bar * 2 + (hit > 0 ? 1 : 0)) % 4];
            double ph = 0, ph8 = 0;
            for (int i = 0; start + i < total && i < (int) (1.2 * kFs); ++i)
            {
                const double t = i / kFs;
                const double kf = 50.0 + 120.0 * std::exp (-t / 0.03);
                ph += kf / kFs;
                const double kick = std::sin (2 * juce::MathConstants<double>::pi * ph) * std::exp (-t / 0.18) * 0.55;
                const double glide = f808 * (1.0 + 0.5 * std::exp (-t / 0.04));
                ph8 += glide / kFs;
                const double s808 = std::tanh (1.6 * std::sin (2 * juce::MathConstants<double>::pi * ph8)) * 0.45
                                    * std::min (1.0, t / 0.004) * std::exp (-t / 0.9);
                for (int c = 0; c < 2; ++c) b.addSample (c, start + i, (float) (kick + s808));
            }
        }
        for (int n = 0; n < 8; ++n)                                    // 8th-note saw lead
        {
            const int start = (int) ((bar * 4 + n * 0.5) * beat * kFs);
            const double f = lead[(n + bar) % 8];
            const int len = (int) (0.5 * beat * kFs);
            for (int c = 0; c < 2; ++c)
            {
                const double fd = f * std::pow (2.0, (c ? -6.0 : 6.0) / 1200.0), dt = fd / kFs;
                double ph = c * 0.3;
                for (int i = 0; i < len && start + i < total; ++i)
                {
                    double saw = 2.0 * ph - 1.0 - polyBlep (ph, dt);
                    ph += dt; if (ph >= 1.0) ph -= 1.0;
                    const double env = std::min (1.0, i / 240.0) * std::min (1.0, (len - i) / 240.0) * std::exp (-(double) i / kFs / 0.25);
                    b.addSample (c, start + i, (float) (saw * env * 0.12));
                }
            }
        }
        for (int h = 0; h < 16; ++h)                                   // 16th hats
        {
            const int start = (int) ((bar * 4 + h * 0.25) * beat * kFs);
            double hp = 0, prev = 0;
            for (int i = 0; i < 2400 && start + i < total; ++i)
            {
                const double w = rng.nextDouble() * 2 - 1; hp = 0.6 * (hp + w - prev); prev = w;
                const float v = (float) (hp * std::exp (-i / 300.0) * 0.06);
                b.addSample (0, start + i, v); b.addSample (1, start + i, v * 0.9f);
            }
        }
    }
    const float pk = b.getMagnitude (0, total);
    b.applyGain (juce::Decibels::decibelsToGain (-3.0f) / pk);
    return b;
}

struct Stats { bool finite = true; float peak = 0; double rms = 0; };
Stats stats (const juce::AudioBuffer<float>& b, int from = 0, int to = -1)
{
    Stats s; if (to < 0) to = b.getNumSamples();
    double sq = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = from; i < to; ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v)) s.finite = false;
            s.peak = std::max (s.peak, std::abs (v));
            sq += (double) v * v;
        }
    s.rms = std::sqrt (sq / std::max (1, (to - from) * b.getNumChannels()));
    return s;
}
double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }

double diffRms (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int from, int to)
{
    double sq = 0;
    for (int c = 0; c < 2; ++c) for (int i = from; i < to; ++i) { const double d = a.getSample (c, i) - b.getSample (c, i); sq += d * d; }
    return std::sqrt (sq / std::max (1, 2 * (to - from)));
}

/** RMS of a band (simple RBJ bandpass x2), left+right mono. */
double bandRms (const juce::AudioBuffer<float>& b, double lo, double hi)
{
    auto design = [] (double f, bool high, double* c) {
        const double w0 = 2 * juce::MathConstants<double>::pi * f / kFs, cw = std::cos (w0), al = std::sin (w0) / (2 * 0.707);
        double b0 = high ? (1 + cw) / 2 : (1 - cw) / 2, b1 = high ? -(1 + cw) : 1 - cw, a0 = 1 + al;
        c[0] = b0 / a0; c[1] = b1 / a0; c[2] = b0 / a0; c[3] = -2 * cw / a0; c[4] = (1 - al) / a0; };
    double h[5], l[5]; design (lo, true, h); design (hi, false, l);
    double s[4][2] = {}; double sq = 0;
    auto run = [] (double* c, double (&z)[2], double x) { double y = c[0] * x + z[0]; z[0] = c[1] * x - c[3] * y + z[1]; z[1] = c[2] * x - c[4] * y; return y; };
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        double x = 0.5 * (b.getSample (0, i) + b.getSample (1, i));
        x = run (h, s[0], x); x = run (h, s[1], x); x = run (l, s[2], x); x = run (l, s[3], x);
        sq += x * x;
    }
    return std::sqrt (sq / b.getNumSamples());
}

void writeWav (const juce::AudioBuffer<float>& b, const juce::File& f)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    if (auto* w = wav.createWriterFor (os.get(), kFs, 2, 24, {}, 0)) { os.release(); std::unique_ptr<juce::AudioFormatWriter> wr (w); wr->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
}
} // namespace testloop
