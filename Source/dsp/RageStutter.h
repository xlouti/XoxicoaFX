#pragma once
// RAGE STUTTER: automatic glitch fills in the last beat of every 4 or 8 bars.
//
// Tempo + position come from the host playhead. The fill beat is split into four
// 1/16 steps; each step is Pass, 1/16 repeat, 1/32 repeat, Reverse or Tape-stop.
// The pattern is generated from a seed made of (bar number, Pattern knob), so the
// same bar always gets the same fill. Repeats start on the most recent detected
// transient so slices land on hits. This is a deterministic algorithm.
#include "Common.h"

namespace xfx
{
class RageStutter
{
public:
    enum Step : int { Pass = 0, Rep16, Rep32, Reverse, TapeStop };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int n = nextPow2 ((int) (fs * 4.0));
        histL.assign ((size_t) n, 0.0f);
        histR.assign ((size_t) n, 0.0f);
        mask = n - 1;
        fast.prepare (fs, 0.3, 8.0);
        slow.prepare (fs, 25.0, 180.0);
        fadeLen = std::max (16, (int) (0.0015 * fs));
        gateRamp.prepare (fadeLen);
        mixS.prepare (fs, 30.0);
        holdoff = (int64_t) (0.06 * fs);
        reset();
    }

    void reset() noexcept
    {
        std::fill (histL.begin(), histL.end(), 0.0f);
        std::fill (histR.begin(), histR.end(), 0.0f);
        fast.reset(); slow.reset();
        pos = 0; curBar = kNoBar; curStep = -1;
        lastTransient = -1000000; tIdx = 0;
        for (auto& t : transients) t = -1000000;
        gateRamp.value = 0.0f;
        mixS.snap (mixS.target);
        fillActive = false;
    }

    void setParams (bool on, int barsEvery, float randomness01, float mix01, int seed) noexcept
    {
        enabled = on;
        bars = std::max (1, barsEvery);
        randomness = clampf (randomness01, 0.0f, 1.0f);
        mixS.setTarget (mix01);
        seedParam = (uint32_t) seed;
    }

    void setTransport (bool valid, bool isPlaying, double bpmIn, double ppqStart, int num, int den) noexcept
    {
        transportOk = valid && isPlaying && bpmIn > 1.0;
        bpm = bpmIn; ppq0 = ppqStart;
        tsNum = std::max (1, num); tsDen = std::max (1, den);
    }

    bool isFillActive() const noexcept { return fillActive; }
    int  getBarInCycle() const noexcept { return barInCycle; }

    void process (float* L, float* R, int n) noexcept
    {
        const double qps = bpm / 60.0 / fs;                   // quarter notes per sample
        const double barLen = tsNum * 4.0 / tsDen, beatLen = 4.0 / tsDen, stepLenQ = beatLen / 4.0;
        bool anyFill = false;

        for (int i = 0; i < n; ++i)
        {
            const float dryL = L[i], dryR = R[i];
            histL[(size_t) (pos & mask)] = dryL;
            histR[(size_t) (pos & mask)] = dryR;
            detectTransient (0.5f * (dryL + dryR));

            bool inFill = false;
            if (enabled && transportOk)
            {
                const double ppq = ppq0 + i * qps;
                const auto bar = (int64_t) std::floor (ppq / barLen + 1.0e-10);
                const double inBar = ppq - (double) bar * barLen;
                barInCycle = (int) (((bar % bars) + bars) % bars);
                if (ppq >= 0.0 && barInCycle == bars - 1 && inBar >= barLen - beatLen - 1.0e-10) // no fills in pre-roll
                {
                    inFill = true;
                    const double fp = inBar - (barLen - beatLen);
                    const int step = std::min (3, (int) std::floor (fp / stepLenQ + 1.0e-10));
                    if (bar != curBar)
                    {
                        curBar = bar;
                        makePattern (bar);
                        fillEnd = pos + (int64_t) std::llround ((beatLen - fp) / qps);
                        beatSamples = (int64_t) std::llround (beatLen / qps);
                        curStep = -1;
                    }
                    if (step != curStep)
                    {
                        const double stepEndQ = (step + 1) * stepLenQ;
                        beginStep (step, std::max<int64_t> (32, (int64_t) std::llround ((stepEndQ - fp) / qps)));
                    }
                }
            }
            if (! inFill) { curBar = kNoBar; curStep = -1; }
            anyFill |= inFill;

            const float a = gateRamp.next (inFill);
            const float mix = mixS.next();
            if (a <= 0.0f) { ++pos; continue; }

            float fxL = 0.0f, fxR = 0.0f;
            if (curStep >= 0) renderStep (dryL, dryR, fxL, fxR);
            const float m = mix * a;
            L[i] = dryL * (1.0f - m) + fxL * m;
            R[i] = dryR * (1.0f - m) + fxR * m;
            ++pos;
        }
        fillActive = anyFill;
    }

private:
    static constexpr int64_t kNoBar = INT64_MIN;

    void detectTransient (float x) noexcept
    {
        const float f = fast.process (x), s = slow.process (x);
        if (f > 1.8f * s + 0.003f && pos - lastTransient > holdoff)
        {
            lastTransient = pos;
            transients[(size_t) (tIdx++ & 31)] = pos;
        }
    }

    void makePattern (int64_t bar) noexcept
    {
        Rng rng (hash32 ((uint32_t) bar * 2654435761u ^ (seedParam * 40503u + 0x51ED27u)) | 1u);
        static constexpr int defaults[4] = { Pass, Rep16, Rep32, Reverse };
        bool anyFx = false;
        for (int s = 0; s < 4; ++s)
        {
            const float coin = rng.uniform(), pick = rng.uniform(); // always draw both: deterministic
            int t = defaults[s];
            if (coin < randomness)
            {
                if      (pick < 0.22f) t = Pass;
                else if (pick < 0.47f) t = Rep16;
                else if (pick < 0.72f) t = Rep32;
                else if (pick < 0.87f) t = Reverse;
                else                   t = (s >= 1) ? TapeStop : Rep32;
            }
            pattern[s] = t;
            anyFx |= (t != Pass);
        }
        if (! anyFx) pattern[3] = Rep32;
        for (int s = 1; s < 4; ++s) if (pattern[s - 1] == TapeStop) pattern[s] = TapeStop; // tape stop runs to the end
    }

    void beginStep (int step, int64_t lenSamples) noexcept
    {
        const int prevType = curStep >= 0 ? pattern[curStep] : Pass;
        curStep = step;
        const int type = pattern[step];
        const bool continuingTape = (type == TapeStop && prevType == TapeStop && step > 0);
        if (! continuingTape)
        {
            stepStart = pos;
            stepLen = lenSamples;
        }
        stepFadeIn  = (type != Pass) ? ! continuingTape : (prevType != Pass);
        // the last step always fades out, because the fill hands back to the dry signal
        const bool nextIsFx = step < 3 ? pattern[step + 1] != Pass : true;
        stepFadeOut = (type != Pass) || nextIsFx;

        if (type == Rep16 || type == Rep32)
        {
            sliceLen = std::max<int64_t> (16, type == Rep16 ? lenSamples : lenSamples / 2);
            // newest transient that leaves a whole slice in the past, within one beat
            int64_t src = stepStart - sliceLen;
            int64_t bestT = INT64_MIN;
            for (auto t : transients)
                if (t <= stepStart - sliceLen && t >= stepStart - beatSamples && t > bestT) bestT = t;
            if (bestT != INT64_MIN) src = bestT;
            srcStart = src;
        }
        if (type == TapeStop && ! continuingTape)
        {
            tapePos = (double) pos;
            tapeLen = (double) std::max<int64_t> (32, fillEnd - pos);
        }
    }

    inline float edge (int64_t k, int64_t len, bool fin, bool fout) const noexcept
    {
        float e = 1.0f;
        if (fin && k < fadeLen) e = (float) k / (float) fadeLen;
        if (fout && k > len - fadeLen) e = std::min (e, std::max (0.0f, (float) (len - k) / (float) fadeLen));
        return e;
    }

    inline void renderStep (float dryL, float dryR, float& oL, float& oR) noexcept
    {
        const int type = pattern[curStep];
        const int64_t k = pos - stepStart;
        switch (type)
        {
            case Pass:
            {
                const float e = edge (k, stepLen, stepFadeIn, stepFadeOut);
                oL = dryL * e; oR = dryR * e; break;
            }
            case Rep16:
            case Rep32:
            {
                const int64_t kk = k % sliceLen;
                const int64_t idx = srcStart + kk;
                const float e = edge (kk, sliceLen, true, true) * edge (k, stepLen, true, true);
                oL = histL[(size_t) (idx & mask)] * e;
                oR = histR[(size_t) (idx & mask)] * e;
                break;
            }
            case Reverse:
            {
                const int64_t idx = stepStart - 1 - (k % std::max<int64_t> (1, stepLen));
                const float e = edge (k, stepLen, true, true);
                oL = histL[(size_t) (idx & mask)] * e;
                oR = histR[(size_t) (idx & mask)] * e;
                break;
            }
            case TapeStop:
            default:
            {
                const double rate = std::max (0.0, 1.0 - (double) k / tapeLen);
                const auto ip = (int64_t) std::floor (tapePos);
                const float f = (float) (tapePos - (double) ip);
                const float l0 = histL[(size_t) (ip & mask)], l1 = histL[(size_t) ((ip + 1) & mask)];
                const float r0 = histR[(size_t) (ip & mask)], r1 = histR[(size_t) ((ip + 1) & mask)];
                const float e = edge (k, (int64_t) tapeLen, true, true);
                oL = (l0 + f * (l1 - l0)) * e;
                oR = (r0 + f * (r1 - r0)) * e;
                tapePos = std::min (tapePos + rate, (double) pos - 1.0); // never read the future
                break;
            }
        }
    }

    double fs = 48000.0, bpm = 120.0, ppq0 = 0.0;
    int tsNum = 4, tsDen = 4, bars = 4;
    bool enabled = false, transportOk = false, fillActive = false;
    float randomness = 0.35f;
    uint32_t seedParam = 1;
    Smoother mixS;
    LinearRamp gateRamp;

    std::vector<float> histL, histR;
    int64_t mask = 0, pos = 0;
    EnvFollower fast, slow;
    int64_t lastTransient = -1000000, holdoff = 2880;
    int64_t transients[32] {};
    uint32_t tIdx = 0;

    int64_t curBar = kNoBar, fillEnd = 0, beatSamples = 1, stepStart = 0, stepLen = 1, sliceLen = 1, srcStart = 0;
    int pattern[4] { Pass, Rep16, Rep32, Reverse };
    int curStep = -1, fadeLen = 72, barInCycle = 0;
    bool stepFadeIn = true, stepFadeOut = true;
    double tapePos = 0.0, tapeLen = 1.0;
};
} // namespace xfx
