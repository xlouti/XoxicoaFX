#pragma once
// PLUGG HAZE: key-aware tape wow/flutter + tape saturation + gentle lowpass.
//
// Pitch wobble comes from a modulated delay (like a real tape machine). The
// wobble waveform is skewed by the detected note: if the input sits below the
// nearest in-key note, the "sharp" half of the cycle is made long and gentle and
// the "flat" half short, so the ear hears the wobble leaning toward the scale
// tone (and vice versa). A bounded delay can't hold a constant offset, so this
// is a perceptual bias, not pitch correction.
#include "PitchTracker.h"

namespace xfx
{
class PluggHaze
{
public:
    static constexpr int numScales = 7;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        center = (int) std::lround (0.010 * fs);   // 10 ms -> constant, reported latency
        dlL.prepare (center * 2 + 16);
        dlR.prepare (center * 2 + 16);
        tracker.prepare (fs, 60.0f, 1200.0f, 12000.0f, 1800.0f);
        wetS.prepare (fs, 30.0); depthS.prepare (fs, 50.0); rateS.prepare (fs, 50.0); tapeS.prepare (fs, 50.0);
        skewCoef = msToCoef (fs, 90.0);
        dcL.set (Biquad::Type::highpass, fs, 15.0, 0.707);
        dcR = dcL;
        lastTone = -1.0f;
        reset();
    }

    int getLatencySamples() const noexcept { return center; }

    void reset() noexcept
    {
        dlL.reset(); dlR.reset(); tracker.reset();
        lpL.reset(); lpR.reset(); dcL.reset(); dcR.reset();
        phase = phase2 = fphase = 0.0;
        skew = skewTarget = 0.0f; counter = 0;
        wetS.snap (wetS.target); depthS.snap (depthS.target); rateS.snap (rateS.target); tapeS.snap (tapeS.target);
    }

    void setParams (bool on, float depth01, float rateHz, int key, int scale, float tone01, float tape01) noexcept
    {
        wetS.setTarget (on ? 1.0f : 0.0f);
        depthS.setTarget (depth01);
        rateS.setTarget (clampf (rateHz, 0.1f, 8.0f));
        tapeS.setTarget (tape01);
        keyRoot = ((key % 12) + 12) % 12;
        scaleIdx = std::min (std::max (scale, 0), numScales - 1);
        if (tone01 != lastTone)
        {
            lastTone = tone01;
            const double f = 2000.0 * std::pow (9.0, (double) tone01); // 2 kHz .. 18 kHz
            lpL.set (Biquad::Type::lowpass, fs, f, 0.6);
            lpR.set (Biquad::Type::lowpass, fs, f, 0.6);
        }
    }

    /** Detected note (MIDI, fractional) and bias toward key in semitones, for the UI. */
    float getDetectedMidi() const noexcept { return detectedMidi; }
    float getBiasSemis() const noexcept    { return biasSemis; }

    void process (float* L, float* R, int n) noexcept
    {
        const float ln2over1200 = 0.000577623f;
        for (int i = 0; i < n; ++i)
        {
            tracker.push (0.5f * (L[i] + R[i]));
            if (++counter >= 64) { counter = 0; updateBias(); }

            const float en   = wetS.next();
            const float dep  = depthS.next() * en;
            const float rate = rateS.next();
            const float tape = tapeS.next() * en;
            skew = skewTarget + skewCoef * (skew - skewTarget);

            phase += rate / fs;            phase -= std::floor (phase);
            phase2 += 0.37 * rate / fs;    phase2 -= std::floor (phase2);
            fphase += 7.3 / fs;            fphase -= std::floor (fphase);

            const float duty = 0.5f + 0.35f * skew;     // fraction of the cycle spent sharp
            const float p = (float) phase;
            const float g = p < duty ? std::cos (kPi * p / duty)
                                     : std::cos (kPi * (1.0f + (p - duty) / (1.0f - duty)));

            // Wow: up to ~30 cents; flutter: ~3 cents at 7.3 Hz.
            const float flA = (dep * 3.0f * ln2over1200) / (kTwoPi * 7.3f) * (float) fs;
            float A = (dep * 30.0f * ln2over1200) / (kTwoPi * rate) * (float) fs;
            A = std::min (A, ((float) center - 4.0f - flA) / 1.3f);
            const float D = (float) center
                            + A * g
                            + 0.3f * A * std::sin (kTwoPi * (float) phase2)
                            + flA * std::sin (kTwoPi * (float) fphase);

            dlL.push (L[i]); dlR.push (R[i]);
            float yL = dlL.read (D), yR = dlR.read (D);

            if (tape > 1.0e-4f)
            {
                const float k = 1.0f + 4.0f * tape, b = 0.12f * tape;
                const float tb = std::tanh (b), norm = 1.0f / (k * (1.0f - tb * tb));
                const float sL = (std::tanh (k * yL + b) - tb) * norm;
                const float sR = (std::tanh (k * yR + b) - tb) * norm;
                yL += dcL.process (sL - yL);    // only the added colour is DC-blocked
                yR += dcR.process (sR - yR);
            }
            else { dcL.process (0.0f); dcR.process (0.0f); }

            const float lL = lpL.process (yL), lR = lpR.process (yR);
            L[i] = yL + en * (lL - yL);
            R[i] = yR + en * (lR - yR);
        }
    }

private:
    void updateBias() noexcept
    {
        static constexpr int scales[numScales][12] = {
            { 1,0,1,1,0,1,0,1,1,0,1,0 }, // Minor
            { 1,0,1,0,1,1,0,1,0,1,0,1 }, // Major
            { 1,0,1,1,0,1,0,1,1,0,0,1 }, // Harmonic Minor
            { 1,0,0,1,0,1,0,1,0,0,1,0 }, // Minor Pentatonic
            { 1,0,1,0,1,0,0,1,0,1,0,0 }, // Major Pentatonic
            { 1,1,0,1,0,1,0,1,1,0,1,0 }, // Phrygian
            { 1,0,1,1,0,1,0,1,0,1,1,0 }, // Dorian
        };
        if (! tracker.isVoiced()) { skewTarget = 0.0f; biasSemis = 0.0f; detectedMidi = -1.0f; return; }

        const float m = midiFromHz (tracker.getHz());
        const float rel = m - (float) keyRoot;
        const int base = (int) std::floor (rel);
        float bestDist = 100.0f, best = rel;
        for (int c = base - 2; c <= base + 3; ++c)
        {
            if (! scales[scaleIdx][((c % 12) + 12) % 12]) continue;
            const float d = std::abs ((float) c - rel);
            if (d < bestDist) { bestDist = d; best = (float) c; }
        }
        biasSemis = best - rel;
        detectedMidi = m;
        skewTarget = clampf (biasSemis * 1.6f, -1.0f, 1.0f);
    }

    double fs = 48000.0;
    int center = 480;
    DelayLine dlL, dlR;
    PitchTracker tracker;
    Biquad lpL, lpR, dcL, dcR;
    Smoother wetS, depthS, rateS, tapeS;
    double phase = 0, phase2 = 0, fphase = 0;
    float skew = 0, skewTarget = 0, skewCoef = 0.99f, lastTone = -1.0f;
    float detectedMidi = -1.0f, biasSemis = 0.0f;
    int keyRoot = 0, scaleIdx = 0, counter = 0;
};
} // namespace xfx
