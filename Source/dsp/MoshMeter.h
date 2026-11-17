#pragma once
// MOSH METER: a single macro for saturation -> clipping -> stereo width.
// Grit can follow the kick envelope (external mono/stereo sidechain if present,
// otherwise a lowpassed copy of the main input).
//
// Saturation + clipping run at 4x sample rate (JUCE IIR oversampler). Its integer
// latency is always compensated against a dry delay and reported to the host,
// including when bypassed, so automation doesn't change project timing.
#include "Common.h"
#include <juce_dsp/juce_dsp.h>

namespace xfx
{
class MoshMeter
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        fs = sampleRate;
        over = std::make_unique<juce::dsp::Oversampling<float>> (
            2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        over->initProcessing ((size_t) maxBlockSize);
        latency = (int) std::lround (over->getLatencyInSamples());
        dryL.prepare (latency + 16); dryR.prepare (latency + 16);
        low.set (Biquad::Type::lowpass, fs, 140.0, 0.707);
        env.prepare (fs, 2.0, 110.0);
        mixS.prepare (fs, 25.0);
        macroS.prepare (fs * 4.0, 40.0);
        kickS.prepare (fs, 40.0);
        dry.resize ((size_t) maxBlockSize * 2);
        follow.resize ((size_t) maxBlockSize);
        reset();
    }

    int getLatencySamples() const noexcept { return latency; }

    void reset() noexcept
    {
        if (over) over->reset();
        low.reset(); env.reset(); dryL.reset(); dryR.reset();
        mixS.snap (mixS.target); macroS.snap (macroS.target); kickS.snap (kickS.target);
        lastWidth = 1.0f; lastEnv = 0.0f;
    }

    void setParams (bool on, float macro01, float kick01) noexcept
    {
        mixS.setTarget (on ? 1.0f : 0.0f);
        macroS.setTarget (macro01);
        kickS.setTarget (kick01);
    }

    float getKickEnvelope() const noexcept { return lastEnv; }

    /** scL/scR may be null. Arrays must have n samples; n <= prepared maxBlockSize. */
    void process (float* L, float* R, const float* scL, const float* scR, int n) noexcept
    {
        if (! over) return;
        float* dL = dry.data();
        float* dR = dry.data() + (dry.size() / 2);
        float maxE = 0;

        for (int i = 0; i < n; ++i)
        {
            const float m = scL ? (scR ? 0.5f * (scL[i] + scR[i]) : scL[i]) : 0.5f * (L[i] + R[i]);
            const float e = env.process (low.process (m));
            const float k = kickS.next();
            follow[(size_t) i] = k * clampf (4.0f * e, 0.0f, 1.0f);
            maxE = std::max (maxE, e);
            dryL.push (L[i]); dryR.push (R[i]);
            dL[i] = latency > 0 ? dryL.tap (latency) : L[i];
            dR[i] = latency > 0 ? dryR.tap (latency) : R[i];
        }
        lastEnv = maxE;

        float* chans[] = { L, R };
        juce::dsp::AudioBlock<float> base (chans, 2, (size_t) n);
        auto up = over->processSamplesUp (base);
        auto* uL = up.getChannelPointer (0);
        auto* uR = up.getChannelPointer (1);
        const size_t upN = up.getNumSamples();

        for (size_t j = 0; j < upN; ++j)
        {
            const float m = macroS.next();
            const float f = follow[std::min ((size_t) n - 1, j / 4)];
            const float drive = dbToGain (30.0f * m + 10.0f * m * f);
            const float wet = clampf (m * 2.0f, 0.0f, 1.0f);
            const float norm = 1.0f / (1.0f + 2.2f * m);
            const float ceil = 1.0f - 0.45f * m;
            const float trim = 1.0f / (1.0f + 0.25f * m);
            const float satL = std::tanh (drive * uL[j]) * norm;
            const float satR = std::tanh (drive * uR[j]) * norm;
            const float cL = clampf (satL, -ceil, ceil) * trim;
            const float cR = clampf (satR, -ceil, ceil) * trim;
            uL[j] += wet * (cL - uL[j]);
            uR[j] += wet * (cR - uR[j]);
            lastWidth = 1.0f + 0.9f * m;
        }
        over->processSamplesDown (base);

        for (int i = 0; i < n; ++i)
        {
            const float w = mixS.next();
            const float mid = 0.5f * (L[i] + R[i]), side = 0.5f * (L[i] - R[i]) * lastWidth;
            const float fxL = mid + side, fxR = mid - side;
            L[i] = dL[i] + w * (fxL - dL[i]);
            R[i] = dR[i] + w * (fxR - dR[i]);
        }
    }

private:
    double fs = 48000.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> over;
    int latency = 0;
    DelayLine dryL, dryR;
    Biquad low;
    EnvFollower env;
    Smoother mixS, macroS, kickS;
    std::vector<float> dry, follow;
    float lastWidth = 1.0f, lastEnv = 0.0f;
};
} // namespace xfx
