#pragma once
// RAGE STUTTER v1.1: a host-synced beat slicer that rearranges the groove.
//
// How it works
// ------------
// * Every incoming sample is written to a long stereo history buffer, indexed by an
//   absolute sample counter. The host PPQ tells us where each sample sits in the bar.
// * A bar is a grid of 16 steps (1/16 notes in 4/4). A pattern is a list of segments;
//   each segment covers some steps and says what to play there:
//     Fwd  - play a source step forward (optionally as a retrigger roll of 1/8, 1/16,
//            1/32 or 1/64 slices)
//     Rev  - play a source span backwards
//     Half - play from the source at half speed (classic half-time, an octave down)
//     Tape - tape stop: speed ramps 1 -> 0 over the segment, then fades out
//     Mute - silence (gated chops, drop-outs)
//   Steps no segment covers are dry.
// * Where the source comes from (no lookahead is possible in a live plugin):
//     - if every sample the segment reads lies at or before "now", it reads the
//       CURRENT bar (e.g. "repeat beat 1" during beat 3);
//     - otherwise it reads the same position in the PREVIOUS bar (exactly one bar of
//       time ago). In a looping beat that is the same material, so "beat 4 reversed"
//       or "beat 2 and 1 swapped" work without latency.
//   Sources that would need audio from before the transport started fall back to dry.
// * Click-free: every segment change is a 4 ms linear crossfade between two voices
//   (the outgoing voice keeps playing its material during the fade). Roll repeats
//   crossfade each slice into the continuation of the previous one. No transient
//   snapping and no slices shorter than 1/16 except deliberate 1/32 and 1/64 rolls.
// * Transport stopped, or module off: the output fades back to dry.
#include "Common.h"
#include <string>

namespace xfx
{
class RageStutter
{
public:
    enum SegMode : int { Fwd = 0, Rev, Half, Tape, Mute };
    enum Mode : int { EveryBar = 0, Every2Bars, FillOf4, FillOf8, LastBeatOf4 };

    struct Seg { int start, len; int src; int mode; int roll64; float gain; };
    struct Pattern { const char* name; std::vector<Seg> segs; };

    static const std::vector<Pattern>& library()
    {
        // roll64: slice length in 1/64 notes (8 = 1/8, 4 = 1/16, 2 = 1/32, 1 = 1/64), 0 = none.
        static const std::vector<Pattern> lib = {
            { "Beat 1 Repeat",          { { 4, 4, 0, Fwd, 0, 1 }, { 8, 4, 0, Fwd, 0, 1 }, { 12, 4, 0, Fwd, 0, 1 } } },
            { "Half-Time",              { { 0, 16, 0, Half, 0, 1 } } },
            { "Half-Time Back Half",    { { 8, 8, 8, Half, 0, 1 } } },
            { "Skip 2 & 4",             { { 4, 4, 0, Fwd, 0, 1 }, { 12, 4, 8, Fwd, 0, 1 } } },
            { "Beat Flip (2-1-4-3)",    { { 0, 4, 4, Fwd, 0, 1 }, { 4, 4, 0, Fwd, 0, 1 }, { 8, 4, 12, Fwd, 0, 1 }, { 12, 4, 8, Fwd, 0, 1 } } },
            { "Beat Shuffle (1-3-2-4)", { { 4, 4, 8, Fwd, 0, 1 }, { 8, 4, 4, Fwd, 0, 1 } } },
            { "Half-Bar Repeat",        { { 8, 8, 0, Fwd, 0, 1 } } },
            { "Reverse Last Beat",      { { 12, 4, 12, Rev, 0, 1 } } },
            { "Reverse 2 & 4",          { { 4, 4, 4, Rev, 0, 1 }, { 12, 4, 12, Rev, 0, 1 } } },
            { "Reverse Back Half",      { { 8, 8, 8, Rev, 0, 1 } } },
            { "Reverse Swell",          { { 8, 8, 0, Rev, 0, 1 } } },
            { "1/8 Retrigger Roll",     { { 8, 8, 8, Fwd, 8, 1 } } },
            { "1/16 Roll Last Beat",    { { 12, 4, 12, Fwd, 4, 1 } } },
            { "1/32 Snare Roll",        { { 12, 4, 8, Fwd, 2, 1 } } },
            { "Rage Roll Buildup",      { { 8, 4, 8, Fwd, 4, 0.75f }, { 12, 2, 8, Fwd, 2, 0.88f }, { 14, 2, 8, Fwd, 1, 1.0f } } },
            { "Machine Gun 1/64",       { { 14, 2, 12, Fwd, 1, 0.9f } } },
            { "Downbeat Stutter",       { { 0, 4, 0, Fwd, 4, 1 }, { 8, 4, 8, Fwd, 4, 1 } } },
            { "Triplet Skip (3-3-3)",   { { 3, 3, 0, Fwd, 0, 1 }, { 6, 3, 0, Fwd, 0, 1 }, { 9, 3, 0, Fwd, 0, 1 } } },
            { "8th Repeat 2 & 4",       { { 4, 2, 0, Fwd, 0, 1 }, { 6, 2, 0, Fwd, 0, 1 }, { 12, 2, 8, Fwd, 0, 1 }, { 14, 2, 8, Fwd, 0, 1 } } },
            { "Plugg Skip",             { { 2, 1, 0, Fwd, 0, 1 }, { 6, 1, 4, Fwd, 0, 1 }, { 10, 2, 8, Fwd, 0, 1 }, { 15, 1, 12, Fwd, 0, 1 } } },
            { "Gated Chop 1/16",        { { 1, 1, 0, Mute, 0, 0 }, { 3, 1, 0, Mute, 0, 0 }, { 5, 1, 0, Mute, 0, 0 }, { 7, 1, 0, Mute, 0, 0 },
                                          { 9, 1, 0, Mute, 0, 0 }, { 11, 1, 0, Mute, 0, 0 }, { 13, 1, 0, Mute, 0, 0 }, { 15, 1, 0, Mute, 0, 0 } } },
            { "Stop-Start",             { { 6, 2, 0, Mute, 0, 0 }, { 14, 2, 0, Mute, 0, 0 } } },
            { "Drop Silence",           { { 12, 4, 0, Mute, 0, 0 } } },
            { "Tape Stop End",          { { 12, 4, 12, Tape, 0, 1 } } },
            { "Tape Stop Half",         { { 8, 8, 8, Tape, 0, 1 } } },
            { "Half-Time Into Stop",    { { 0, 12, 0, Half, 0, 1 }, { 12, 4, 6, Tape, 0, 1 } } },
        };
        return lib;
    }
    static int numPatterns() { return (int) library().size(); }
    static constexpr int kDefaultPattern = 14; // Rage Roll Buildup

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int n = nextPow2 ((int) (fs * 16.0));
        histL.assign ((size_t) n, 0.0f);
        histR.assign ((size_t) n, 0.0f);
        mask = n - 1;
        bufLen = n;
        fadeLen = std::max (32, (int) std::lround (0.004 * fs));
        mixS.prepare (fs, 20.0);
        reset();
    }

    void reset() noexcept
    {
        std::fill (histL.begin(), histL.end(), 0.0f);
        std::fill (histR.begin(), histR.end(), 0.0f);
        pos = 0; validFrom = 0; wasRunning = false;
        cachedBar = kNoBar; curKey = kDryKey;
        cur = Voice{}; old = Voice{}; xf = 1.0f;
        mixS.snap (mixS.target);
        fillActive = false;
    }

    void setParams (bool on, int modeIn, int patternIn, float randomness01, float mix01) noexcept
    {
        enabled = on;
        const int m = std::min (std::max (modeIn, 0), (int) LastBeatOf4);
        const int p = std::min (std::max (patternIn, 0), numPatterns() - 1);
        const float r = clampf (randomness01, 0.0f, 1.0f);
        if (m != mode || p != patternIdx || std::abs (r - randomness) > 1.0e-6f) cachedBar = kNoBar; // rebuild the bar map
        mode = m; patternIdx = p; randomness = r;
        mixS.setTarget (clampf (mix01, 0.0f, 1.0f));
    }

    void setTransport (bool valid, bool isPlaying, double bpmIn, double ppqStart, int num, int den) noexcept
    {
        transportOk = valid && isPlaying && bpmIn > 1.0;
        bpm = std::max (1.0, bpmIn); ppq0 = ppqStart;
        tsNum = std::max (1, num); tsDen = std::max (1, den);
    }

    bool isFillActive() const noexcept { return fillActive; }
    int  getBarInCycle() const noexcept { return barInCycle; }
    int  getCycleLength() const noexcept { return mode == EveryBar ? 1 : mode == Every2Bars ? 2 : mode == FillOf8 ? 8 : 4; }
    /** Pattern of the most recent bar that had (or will have) a fill. */
    int  getActivePattern() const noexcept { return activePattern; }
    int  getCurrentStep() const noexcept { return lastStep; }

    void process (float* L, float* R, int n) noexcept
    {
        const double qps = bpm / 60.0 / fs;                   // quarter notes per sample
        const double barLen = tsNum * 4.0 / tsDen, stepQ = barLen / 16.0;
        const double barSamples = barLen / qps;
        const bool fits = barSamples * 2.0 + 4.0 * fs < (double) bufLen; // need one bar of lookbehind + margin
        const bool run = enabled && transportOk && fits;
        if (run && ! wasRunning) validFrom = pos;            // history is only valid from here on
        wasRunning = run;
        bool anyFill = false;

        for (int i = 0; i < n; ++i)
        {
            const float dryL = L[i], dryR = R[i];
            histL[(size_t) (pos & mask)] = dryL;
            histR[(size_t) (pos & mask)] = dryR;

            int64_t key = kDryKey;
            int segIdx = -1;
            double inBar = 0.0;
            if (run)
            {
                const double ppq = ppq0 + i * qps;
                if (ppq >= 0.0)                              // nothing in the pre-roll
                {
                    const auto bar = (int64_t) std::floor (ppq / barLen + 1.0e-10);
                    inBar = std::max (0.0, ppq - (double) bar * barLen);
                    const int step = std::min (15, (int) std::floor (inBar / stepQ + 1.0e-9));
                    if (bar != cachedBar) buildBar (bar);
                    segIdx = stepSeg[step];
                    lastStep = step;
                    if (segIdx >= 0) key = bar * 64 + segIdx;
                }
            }
            if (key != curKey)
            {
                curKey = key;
                startVoice (segIdx, inBar, stepQ, qps, barSamples);
            }
            anyFill |= (key != kDryKey);

            const float m = mixS.next();
            if (cur.type == VDry && xf >= 1.0f) { ++pos; continue; }   // exact dry, nothing to do

            float wL, wR;
            render (cur, wL, wR);
            if (xf < 1.0f)
            {
                float oL, oR;
                render (old, oL, oR);
                wL = oL + xf * (wL - oL);
                wR = oR + xf * (wR - oR);
                xf = std::min (1.0f, xf + 1.0f / (float) fadeLen);
            }
            L[i] = dryL + m * (wL - dryL);
            R[i] = dryR + m * (wR - dryR);
            ++pos;
        }
        fillActive = anyFill;
    }

private:
    static constexpr int64_t kNoBar = INT64_MIN;
    static constexpr int64_t kDryKey = INT64_MIN + 1;
    enum VType : int { VDry = 0, VFwd, VRoll, VRev, VHalf, VTape, VMute };

    struct Voice
    {
        int type = VDry;
        int64_t start = 0;          // absolute sample where the segment (virtually) began
        int64_t src = 0;            // absolute sample of the source start
        double len = 1.0, roll = 0.0;
        float gain = 1.0f;
    };

    void buildBar (int64_t bar) noexcept
    {
        cachedBar = bar;
        const int cyc = getCycleLength();
        barInCycle = (int) (((bar % cyc) + cyc) % cyc);
        for (auto& s : stepSeg) s = -1;
        bool active = barInCycle == cyc - 1;
        if (! active) { curPattern = -1; return; }

        int p = patternIdx;
        if (randomness > 0.0f && numPatterns() > 1)
        {
            const uint32_t h = hash32 ((uint32_t) bar * 2654435761u ^ ((uint32_t) patternIdx + 1u) * 0x9E3779B9u);
            const float u = (float) (h >> 8) * (1.0f / 16777216.0f);
            if (u < randomness)
                p = (patternIdx + 1 + (int) (hash32 (h ^ 0x5bd1e995u) % (uint32_t) (numPatterns() - 1))) % numPatterns();
        }
        curPattern = activePattern = p;
        const int firstStep = mode == LastBeatOf4 ? 12 : 0;
        const auto& segs = library()[(size_t) p].segs;
        for (int si = 0; si < (int) segs.size(); ++si)
            for (int s = std::max (segs[(size_t) si].start, firstStep); s < std::min (16, segs[(size_t) si].start + segs[(size_t) si].len); ++s)
                stepSeg[s] = si;
    }

    void startVoice (int segIdx, double inBar, double stepQ, double qps, double barSamples) noexcept
    {
        Voice v;
        if (segIdx >= 0 && curPattern >= 0)
        {
            const auto& s = library()[(size_t) curPattern].segs[(size_t) segIdx];
            const double stepSamples = stepQ / qps;
            v.start = pos - (int64_t) std::llround ((inBar - s.start * stepQ) / qps);
            v.src = v.start + (int64_t) std::llround ((s.src - s.start) * stepSamples);
            v.len = std::max (1.0, s.len * stepSamples);
            v.gain = s.gain;
            // Can the whole segment be read from the current bar without reading the future?
            const bool needsPrev = s.mode == Rev ? (s.src + s.len > s.start) : (s.src > s.start);
            if (needsPrev) v.src -= (int64_t) std::llround (barSamples);
            switch (s.mode)
            {
                case Fwd:  v.type = s.roll64 > 0 ? VRoll : VFwd;
                           v.roll = std::max (16.0, std::round (s.roll64 * stepSamples / 4.0)); break;
                case Rev:  v.type = VRev;  break;
                case Half: v.type = VHalf; break;
                case Tape: v.type = VTape; break;
                default:   v.type = VMute; break;
            }
            if (v.type != VMute && v.src < validFrom) v = Voice{};   // no history yet: stay dry
        }
        old = cur;
        cur = v;
        xf = 0.0f;
    }

    inline void readAt (double idx, float& oL, float& oR) const noexcept
    {
        idx = std::min (idx, (double) pos);
        idx = std::max (idx, (double) (pos - bufLen + 8));
        const auto ip = (int64_t) std::floor (idx);
        const float f = (float) (idx - (double) ip);
        if (f <= 0.0f) { oL = histL[(size_t) (ip & mask)]; oR = histR[(size_t) (ip & mask)]; return; }
        auto at = [this] (const std::vector<float>& h, int64_t j) { return h[(size_t) (std::min (j, pos) & mask)]; };
        oL = hermite (f, at (histL, ip - 1), at (histL, ip), at (histL, ip + 1), at (histL, ip + 2));
        oR = hermite (f, at (histR, ip - 1), at (histR, ip), at (histR, ip + 1), at (histR, ip + 2));
    }

    inline void render (const Voice& v, float& oL, float& oR) const noexcept
    {
        const double k = (double) (pos - v.start);
        float g = v.gain;
        switch (v.type)
        {
            case VDry:  oL = histL[(size_t) (pos & mask)]; oR = histR[(size_t) (pos & mask)]; return;
            case VMute: oL = oR = 0.0f; return;
            case VFwd:  readAt ((double) v.src + k, oL, oR); break;
            case VRoll:
            {
                const double kk = std::fmod (std::max (0.0, k), v.roll);
                readAt ((double) v.src + kk, oL, oR);
                if (k >= v.roll && kk < fadeLen)            // crossfade out of the previous repeat
                {
                    float pL, pR;
                    readAt ((double) v.src + v.roll + kk, pL, pR);
                    const float t = (float) (kk / fadeLen);
                    oL = pL + t * (oL - pL); oR = pR + t * (oR - pR);
                }
                break;
            }
            case VRev:  readAt ((double) v.src + v.len - 1.0 - k, oL, oR); break;
            case VHalf: readAt ((double) v.src + 0.5 * k, oL, oR); break;
            case VTape:
            default:
            {
                const double kk = std::min (std::max (0.0, k), v.len);
                readAt ((double) v.src + kk - kk * kk / (2.0 * v.len), oL, oR);
                g *= (float) clampf ((float) ((v.len - k) / (0.3 * v.len)), 0.0f, 1.0f); // dies out like a real stop
                break;
            }
        }
        oL *= g; oR *= g;
    }

    double fs = 48000.0, bpm = 120.0, ppq0 = 0.0;
    int tsNum = 4, tsDen = 4;
    bool enabled = false, transportOk = false, fillActive = false, wasRunning = false;
    int mode = FillOf4, patternIdx = kDefaultPattern, curPattern = -1, activePattern = kDefaultPattern, barInCycle = 0;
    float randomness = 0.0f;
    Smoother mixS;

    std::vector<float> histL, histR;
    int64_t mask = 0, pos = 0, validFrom = 0, bufLen = 1;
    int64_t cachedBar = kNoBar, curKey = kDryKey;
    int stepSeg[16] {};
    Voice cur, old;
    float xf = 1.0f;
    int fadeLen = 192, lastStep = -1;
};
} // namespace xfx
