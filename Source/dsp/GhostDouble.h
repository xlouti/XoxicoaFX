#pragma once
// GHOST DOUBLE: a pitch-shifted, delayed, darkened shadow copy, spread in stereo.
#include "GrainShifter.h"

namespace xfx
{
class GhostDouble
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // 80 ms max delay * (1 + 1.2 width offset) = 176 ms -> 190 ms headroom
        shL.prepare (fs, 45.0f, 190.0f, 0.5f);
        shR.prepare (fs, 45.0f, 190.0f, 0.5f);
        mixS.prepare (fs, 20.0); delayS.prepare (fs, 25.0); widthS.prepare (fs, 20.0); ratioS.prepare (fs, 25.0);
        formantS.prepare (fs, 25.0);
        lastFormant = 1000.0f; // force filter update
        reset();
    }

    void reset() noexcept
    {
        shL.reset (0.0f); shR.reset (0.37f);
        for (auto* v : { &voiceL, &voiceR }) for (auto& b : *v) b.reset();
        mixS.snap (mixS.target); delayS.snap (delayS.target); widthS.snap (widthS.target); ratioS.snap (ratioS.target);
        formantS.snap (formantS.target); lastFormant = formantS.value; updateFilters(); counter = 0;
    }

    void setParams (bool on, int semitones, float formantSemis, float delayMs, float width01, float mix01) noexcept
    {
        mixS.setTarget (on ? mix01 : 0.0f);
        delayS.setTarget (clampf (delayMs, 5.0f, 80.0f));
        widthS.setTarget (width01);
        ratioS.setTarget (std::pow (2.0f, (float) semitones / 12.0f));
        formantS.setTarget (clampf (formantSemis, -12.0f, 12.0f));
    }

    void process (float* L, float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float fm = formantS.next();               // smoothed sweep, filters refreshed every 32 samples
            if (--counter <= 0) { counter = 32; if (std::abs (fm - lastFormant) > 0.005f) { lastFormant = fm; updateFilters(); } }
            const float g = mixS.next();
            const float ratio = ratioS.next();
            const float d = delayS.next() * (float) fs * 0.001f;
            const float w = widthS.next();
            const float m = 0.5f * (L[i] + R[i]);

            float vL = shL.process (m, ratio, d);
            float vR = shR.process (m, ratio, d * (1.0f + 1.2f * w));
            for (auto& b : voiceL) vL = b.process (vL);
            for (auto& b : voiceR) vR = b.process (vR);

            if (g < 1.0e-5f) continue; // shifter still runs so it is primed when switched on
            const float mid = 0.5f * (vL + vR), side = 0.5f * (vL - vR) * (2.4f * w);
            L[i] += g * 0.9f * (mid + side);
            R[i] += g * 0.9f * (mid - side);
        }
    }

private:
    void updateFilters() noexcept
    {
        // "Formant" = throat size of the ghost. It moves a resonant peak and tilts
        // the tone; it is a colour control, not true formant preservation.
        const float f = lastFormant;
        for (auto* v : { &voiceL, &voiceR })
        {
            (*v)[0].set (Biquad::Type::highpass, fs, 55.0, 0.707);              // keep the ghost off the sub
            (*v)[1].set (Biquad::Type::lowpass, fs, 4200.0, 0.707);             // darkened
            (*v)[2].set (Biquad::Type::lowShelf, fs, 300.0, 0.707, -0.75 * f);
            (*v)[3].set (Biquad::Type::peak, fs, 900.0 * std::pow (2.0, f / 8.0), 1.2, 8.0);
            (*v)[4].set (Biquad::Type::highShelf, fs, 2200.0, 0.707, 0.75 * f);
        }
    }

    double fs = 48000.0;
    GrainShifter shL, shR;
    Biquad voiceL[5], voiceR[5];
    Smoother mixS, delayS, widthS, ratioS, formantS;
    float lastFormant = 1000.0f;
    int counter = 0;
};
} // namespace xfx
