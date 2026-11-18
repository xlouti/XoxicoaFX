#pragma once
// SIDECHAIN PUMP: ducking envelope, either tempo-synced (1/4, 1/8, 1/2 from the host
// song position) or triggered by kicks (same low-band envelope idea as the Mosh Meter's
// kick follower: lowpass ~140 Hz, fast vs slow envelope onset, 80 ms hold-off).
//
// Envelope after each trigger:  gain = 1 - depth * (1 - x)^k,  x = t / releaseTime.
//   Release: sync = 10..100 % of the note length, kick = 40..600 ms.
//   Shape:   k = 0.6 (slow, "breathing") .. 4 (snappy, fast recovery).
// The gain is smoothed with a 1.5 ms one-pole, so the duck attack never clicks.
// Kick mode listens to the external sidechain when one is connected, otherwise the input.
// The sidechain is delayed by the plugin's internal latency so ducks land on the kick.
#include "Common.h"

namespace xfx
{
class SidechainPump
{
public:
    enum Mode : int { Quarter = 0, Eighth, Half, Kick };

    void prepare (double sampleRate, int maxScDelay)
    {
        fs = sampleRate;
        lowA.set (Biquad::Type::lowpass, fs, 140.0, 0.707);
        lowB = lowA;
        fastEnv.prepare (fs, 1.0, 40.0);
        slowEnv.prepare (fs, 40.0, 300.0);
        scL.prepare (maxScDelay + 16); scR.prepare (maxScDelay + 16);
        gainCoef = msToCoef (fs, 1.5);
        onS.prepare (fs, 20.0); depthS.prepare (fs, 20.0);
        holdoff = (int64_t) (0.08 * fs);
        reset();
    }

    void reset() noexcept
    {
        lowA.reset(); lowB.reset(); fastEnv.reset(); slowEnv.reset(); scL.reset(); scR.reset();
        gain = 1.0f; sinceKick = 1 << 30; sinceTrigger = 1 << 30; phase01 = 1.0f;
        onS.snap (onS.target); depthS.snap (depthS.target);
    }

    void setParams (bool on, int modeIn, float depth01, float release01, float shape01) noexcept
    {
        onS.setTarget (on ? 1.0f : 0.0f);
        mode = std::min (std::max (modeIn, 0), (int) Kick);
        depthS.setTarget (clampf (depth01, 0.0f, 1.0f));
        release = clampf (release01, 0.0f, 1.0f);
        k = 0.6f + 3.4f * clampf (shape01, 0.0f, 1.0f);
    }

    void setTransport (bool valid, bool isPlaying, double bpmIn, double ppqStart) noexcept
    {
        transportOk = valid && isPlaying && bpmIn > 1.0;
        bpm = std::max (1.0, bpmIn); ppq0 = ppqStart;
    }
    void setSidechainDelay (int samples) noexcept { scDelay = std::max (0, samples); }

    /** 0..1 position in the current duck (1 = fully recovered), for the curve display. */
    float getPhase() const noexcept { return phase01; }
    float getGain() const noexcept { return gain; }
    /** Duck curve for display: x in 0..1 of the release, returns gain. */
    static float curve (float x, float depth, float kk) noexcept
    {
        return 1.0f - depth * std::pow (1.0f - clampf (x, 0.0f, 1.0f), kk);
    }
    float getShapeK() const noexcept { return k; }

    void process (float* L, float* R, const float* extL, const float* extR, int n) noexcept
    {
        const double qps = bpm / 60.0 / fs;
        const double periodQ = mode == Eighth ? 0.5 : mode == Half ? 2.0 : 1.0;
        const double relSyncQ = periodQ * (0.1 + 0.9 * release);
        const double relKickS = 0.040 * std::pow (15.0, (double) release) * fs;
        bool active = onS.target > 0.0f || onS.value > 1.0e-4f;

        for (int i = 0; i < n; ++i)
        {
            // kick detector (always runs so it is primed when switched to Kick mode)
            const float mIn = extL ? (extR ? 0.5f * (extL[i] + extR[i]) : extL[i]) : 0.5f * (L[i] + R[i]);
            float m = mIn;
            if (extL)   // external sidechain arrives early by the plugin latency: delay it
            {
                scL.push (mIn);
                m = scDelay > 0 ? scL.tap (scDelay) : mIn;
            }
            const float lb = lowB.process (lowA.process (m));
            const float f = fastEnv.process (lb), s = slowEnv.process (lb);
            ++sinceKick;
            bool kick = false;
            if (f > 1.6f * s + 0.01f && sinceKick > holdoff) { kick = true; sinceKick = 0; }

            const float on = onS.next(), depth = depthS.next();
            if (! active) continue;

            float target = 1.0f;
            if (mode == Kick)
            {
                if (kick) sinceTrigger = 0; else ++sinceTrigger;
                const float x = (float) ((double) sinceTrigger / relKickS);
                phase01 = clampf (x, 0.0f, 1.0f);
                target = curve (x, depth, k);
            }
            else if (transportOk)
            {
                const double ppq = ppq0 + i * qps;
                const double inP = ppq - std::floor (ppq / periodQ) * periodQ;
                const float x = (float) (inP / relSyncQ);
                phase01 = clampf (x, 0.0f, 1.0f);
                target = curve (x, depth, k);
            }
            else phase01 = 1.0f;

            target = 1.0f + on * (target - 1.0f);
            gain = target + gainCoef * (gain - target);
            L[i] *= gain;
            R[i] *= gain;
        }
    }

private:
    double fs = 48000.0, bpm = 120.0, ppq0 = 0.0;
    Biquad lowA, lowB;
    EnvFollower fastEnv, slowEnv;
    DelayLine scL, scR;
    Smoother onS, depthS;
    float gain = 1.0f, gainCoef = 0.99f, release = 0.5f, k = 2.3f, phase01 = 1.0f;
    int mode = Quarter, scDelay = 0;
    bool transportOk = false;
    int64_t sinceKick = 1 << 30, sinceTrigger = 1 << 30, holdoff = 3840;
};
} // namespace xfx
