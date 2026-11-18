// In-process audit harness (links the plugin sources directly).
//  1. Rage Stutter: renders the 160 BPM loop through the module for several patterns,
//     writes WAVs and checks: output differs from input, finite, no clicks (sample-to-
//     sample jumps beyond what the dry loop itself contains), dry when stopped.
//  2. Parameter audit: for every parameter, renders min / mid / max with only its module
//     on and reports level change, difference-signal level and per-octave-band spectral change.
// usage: XoxicoaFXAudit <outDir>
#include "PluginProcessor.h"
#include "TestLoop.h"
#include <map>
#include <array>

using namespace testloop;

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what) { std::printf ("    [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (! ok) ++failures; }

struct PH : juce::AudioPlayHead
{
    juce::int64 sample = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (kBpm); p.setIsPlaying (playing); p.setTimeInSamples (sample);
        p.setPpqPosition ((double) sample / kFs * kBpm / 60.0); p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

struct Jumps { float maxD1 = 0, maxD2 = 0; };
Jumps jumps (const juce::AudioBuffer<float>& b)
{
    Jumps j;
    for (int c = 0; c < 2; ++c)
    {
        auto* x = b.getReadPointer (c);
        for (int i = 2; i < b.getNumSamples(); ++i)
        {
            j.maxD1 = std::max (j.maxD1, std::abs (x[i] - x[i - 1]));
            j.maxD2 = std::max (j.maxD2, std::abs (x[i] - 2 * x[i - 1] + x[i - 2]));
        }
    }
    return j;
}

/** Energy (dB) in 10 octave bands, 31.5 Hz .. 16 kHz, averaged over 4096-point frames. */
std::array<double, 10> octaveBands (const juce::AudioBuffer<float>& b)
{
    constexpr int order = 12, N = 1 << order;
    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> win (N, juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> buf (2 * N);
    std::array<double, 10> e {};
    for (int s = 0; s + N <= b.getNumSamples(); s += N)
    {
        for (int i = 0; i < N; ++i) buf[(size_t) i] = 0.5f * (b.getSample (0, s + i) + b.getSample (1, s + i));
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        win.multiplyWithWindowingTable (buf.data(), N);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int k = 1; k < N / 2; ++k)
        {
            const double f = k * kFs / N;
            const int band = (int) std::floor (std::log2 (f / 22.3));
            if (band >= 0 && band < 10) e[(size_t) band] += (double) buf[(size_t) k] * buf[(size_t) k];
        }
    }
    for (auto& v : e) v = 10.0 * std::log10 (v + 1e-20);
    return e;
}

/** Largest per-band level change between two renders, ignoring bands 60 dB below the loudest. */
double bandDelta (const std::array<double, 10>& a, const std::array<double, 10>& b)
{
    const double top = *std::max_element (a.begin(), a.end());
    double d = 0;
    for (size_t i = 0; i < a.size(); ++i) if (a[i] > top - 60.0) d = std::max (d, std::abs (a[i] - b[i]));
    return d;
}

//==============================================================================
void stutterTests (const juce::AudioBuffer<float>& loop, const juce::File& dir)
{
    std::printf ("\n=== RAGE STUTTER (module only, 160 BPM, 8 bars, mode Every Bar, mix 100%%) ===\n");
    const auto inJ = jumps (loop);
    std::printf ("input: max |d1| %.4f, max |d2| %.4f, rms %.1f dBFS\n", inJ.maxD1, inJ.maxD2, db (stats (loop).rms));
    writeWav (loop, dir.getChildFile ("00_input_loop.wav"));

    auto render = [&] (int pattern, int mode, bool playing, float rnd = 0.0f)
    {
        xfx::RageStutter st;
        st.prepare (kFs);
        st.setParams (true, mode, pattern, rnd, 1.0f);
        juce::AudioBuffer<float> out (loop);
        const double qps = kBpm / 60.0 / kFs;
        for (int s = 0; s < out.getNumSamples(); s += kBlock)
        {
            const int n = std::min (kBlock, out.getNumSamples() - s);
            st.setTransport (true, playing, kBpm, s * qps, 4, 4);
            st.process (out.getWritePointer (0) + s, out.getWritePointer (1) + s, n);
        }
        return out;
    };

    const auto& lib = xfx::RageStutter::library();
    auto find = [&] (const char* nm) { for (int i = 0; i < (int) lib.size(); ++i) if (juce::String (lib[(size_t) i].name) == nm) return i; return -1; };
    const char* picks[] = { "Half-Time", "Beat Flip (2-1-4-3)", "Reverse Last Beat", "Rage Roll Buildup", "Gated Chop 1/16", "Tape Stop End", "1/8 Retrigger Roll", "Triplet Skip (3-3-3)" };
    const int barS = (int) std::lround (4 * 60.0 / kBpm * kFs);
    for (auto* nm : picks)
    {
        const int idx = find (nm);
        auto out = render (idx, xfx::RageStutter::EveryBar, true);
        const auto s = stats (out);
        // bar 1 is mostly dry (sources in the previous bar need history), so measure bars 2..8
        const double rel = db (diffRms (out, loop, barS, out.getNumSamples()) / stats (loop, barS).rms);
        const auto j = jumps (out);
        std::printf ("\n[%2d %s]\n    diff vs input %.1f dB (rel. to input rms) | peak %.2f dBFS | max|d1| %.4f (x%.2f of input) | max|d2| %.4f (x%.2f)\n",
                     idx, nm, rel, db (s.peak), j.maxD1, j.maxD1 / inJ.maxD1, j.maxD2, j.maxD2 / inJ.maxD2);
        check (s.finite, "finite (no NaN/Inf)");
        check (rel > -12.0, "audibly rearranges the groove (difference > -12 dB)");
        check (j.maxD1 <= inJ.maxD1 * 1.25f, "no step discontinuities beyond the dry loop's own (|d1| <= 1.25x input)");
        check (j.maxD2 <= inJ.maxD2 * 1.25f, "no clicks beyond the dry loop's own (|d2| <= 1.25x input)");
        writeWav (out, dir.getChildFile (juce::String::formatted ("%02d_", idx) + juce::File::createLegalFileName (nm).replaceCharacter (' ', '_') + ".wav"));
    }

    {
        std::printf ("\n[all %d patterns, every bar]\n", (int) lib.size());
        int ok = 0; std::vector<juce::AudioBuffer<float>> outs;
        float worst1 = 0, worst2 = 0;
        for (int p = 0; p < (int) lib.size(); ++p)
        {
            auto out = render (p, xfx::RageStutter::EveryBar, true);
            const auto j = jumps (out);
            worst1 = std::max (worst1, j.maxD1 / inJ.maxD1); worst2 = std::max (worst2, j.maxD2 / inJ.maxD2);
            const double rel = db (diffRms (out, loop, barS, out.getNumSamples()) / stats (loop, barS).rms);
            std::printf ("    %2d %-24s diff %6.1f dB  d1 x%.2f  d2 x%.2f  %s\n", p, lib[(size_t) p].name, rel, j.maxD1 / inJ.maxD1, j.maxD2 / inJ.maxD2, stats (out).finite ? "finite" : "NAN!");
            ok += (stats (out).finite && rel > -20.0) ? 1 : 0;
            outs.push_back (std::move (out));
        }
        double minPair = 1e9;
        for (size_t a = 0; a < outs.size(); ++a) for (size_t b = a + 1; b < outs.size(); ++b)
            minPair = std::min (minPair, db (diffRms (outs[a], outs[b], barS, outs[a].getNumSamples()) / stats (loop, barS).rms));
        std::printf ("    most similar pair of patterns differs by %.1f dB\n", minPair);
        check (ok == (int) lib.size(), "every pattern is finite and changes the loop");
        check (minPair > -20.0, "every pattern sounds different from every other (pairwise diff > -20 dB)");
        check (worst1 <= 1.25f && worst2 <= 1.25f, juce::String::formatted ("click check across all patterns (worst d1 x%.2f, d2 x%.2f)", worst1, worst2));
    }

    {
        // Click test on a pure 220 Hz sine (amplitude 0.5): its own max step is 0.0144, a 4 ms crossfade
        // between unrelated phases adds at most ~0.006, a hard cut would jump up to 1.0.
        juce::AudioBuffer<float> sine (2, loop.getNumSamples());
        for (int i = 0; i < sine.getNumSamples(); ++i)
        {
            const float v = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / kFs);
            sine.setSample (0, i, v); sine.setSample (1, i, v);
        }
        const float own = jumps (sine).maxD1;
        float worst = 0; int worstP = 0;
        for (int p = 0; p < (int) lib.size(); ++p)
        {
            for (int mode : { (int) xfx::RageStutter::EveryBar, (int) xfx::RageStutter::LastBeatOf4 })
            {
                xfx::RageStutter st; st.prepare (kFs); st.setParams (true, mode, p, 0.0f, 1.0f);
                juce::AudioBuffer<float> out (sine);
                for (int s = 0; s < out.getNumSamples(); s += kBlock)
                {
                    const int n = std::min (kBlock, out.getNumSamples() - s);
                    st.setTransport (true, true, kBpm, s * kBpm / 60.0 / kFs, 4, 4);
                    st.process (out.getWritePointer (0) + s, out.getWritePointer (1) + s, n);
                }
                const float j = jumps (out).maxD1;
                if (j > worst) { worst = j; worstP = p; }
            }
        }
        std::printf ("\n[sine click test, all patterns, 2 modes]\n    sine's own max step %.4f, worst output step %.4f (%s)\n", own, worst, lib[(size_t) worstP].name);
        check (worst < 0.03f, "no clicks: worst sample-to-sample step < 0.03 on a 0.5-amplitude sine");
    }

    {
        std::printf ("\n[modes / transport]\n");
        auto stopped = render (find ("Beat Flip (2-1-4-3)"), xfx::RageStutter::EveryBar, false);
        check (diffRms (stopped, loop, 0, loop.getNumSamples()) == 0.0, "transport stopped -> bit-exact dry");
        auto fill4 = render (find ("Beat Flip (2-1-4-3)"), xfx::RageStutter::FillOf4, true);
        const double in3 = db (diffRms (fill4, loop, 0, 3 * barS) + 1e-12), in4 = db (diffRms (fill4, loop, 3 * barS, 4 * barS));
        std::printf ("    Fill of 4: change in bars 1-3 %.1f dBFS, bar 4 %.1f dBFS\n", in3, in4);
        check (in3 < -100.0 && in4 > -30.0, "Fill mode only touches the last bar of 4");
        auto lb = render (find ("Half-Time"), xfx::RageStutter::LastBeatOf4, true);
        const int beatS = barS / 4;
        const double pre = db (diffRms (lb, loop, 0, 3 * barS + 3 * beatS - 64) + 1e-12), last = db (diffRms (lb, loop, 3 * barS + 3 * beatS, 4 * barS));
        std::printf ("    Last Beat: change before beat 4 of bar 4 %.1f dBFS, in it %.1f dBFS\n", pre, last);
        check (pre < -100.0 && last > -30.0, "Last Beat mode only touches the last beat of every 4th bar");
        auto r1 = render (0, xfx::RageStutter::EveryBar, true, 1.0f), r2 = render (0, xfx::RageStutter::EveryBar, true, 1.0f);
        auto r0 = render (0, xfx::RageStutter::EveryBar, true, 0.0f);
        check (diffRms (r1, r2, 0, loop.getNumSamples()) == 0.0, "randomness is deterministic (same render twice)");
        check (db (diffRms (r1, r0, barS, loop.getNumSamples()) / stats (loop, barS).rms) > -15.0, "randomness 100% swaps in other patterns");
        writeWav (r1, dir.getChildFile ("randomness_100.wav"));
    }
}

//==============================================================================
struct Render { juce::AudioBuffer<float> buf; bool finite; float peak; };

Render renderProc (const juce::AudioBuffer<float>& loop, const std::map<juce::String, float>& set, bool offline = false)
{
    XoxicoaFXAudioProcessor proc;
    PH ph; proc.setPlayHead (&ph);
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : set) proc.apvts.getParameter (id)->setValueNotifyingHost (v);
    proc.setNonRealtime (offline);
    proc.prepareToPlay (kFs, kBlock);
    juce::AudioBuffer<float> out (loop);
    juce::MidiBuffer midi;
    for (int s = 0; s < out.getNumSamples(); s += kBlock)
    {
        const int n = std::min (kBlock, out.getNumSamples() - s);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, s, n);
        ph.sample = s;
        proc.processBlock (view, midi);
    }
    const auto st = stats (out);
    return { std::move (out), st.finite, st.peak };
}

void paramAudit (const juce::AudioBuffer<float>& loop, const juce::File& dir)
{
    std::printf ("\n=== PARAMETER AUDIT (min / mid / max, only that module on) ===\n");
    std::printf ("levelD = output RMS change vs min setting; diff = RMS of (render - min render) relative to min render;\n"
                 "band = largest change in any octave band (31 Hz-16 kHz), dB.\n"
                 "FLAG (too subtle) when |level| < 1 dB AND diff < -20 dB AND band < 1.5 dB.\n\n");
    struct Item { const char* module; const char* onId; const char* id; };
    const Item items[] = {
        { "Ghost", "ghost_on", "ghost_pitch" }, { "Ghost", "ghost_on", "ghost_formant" }, { "Ghost", "ghost_on", "ghost_delay" },
        { "Ghost", "ghost_on", "ghost_width" }, { "Ghost", "ghost_on", "ghost_mix" },
        { "Haze", "haze_on", "haze_depth" }, { "Haze", "haze_on", "haze_rate" }, { "Haze", "haze_on", "haze_key" },
        { "Haze", "haze_on", "haze_scale" }, { "Haze", "haze_on", "haze_tone" }, { "Haze", "haze_on", "haze_tape" },
        { "Stutter", "stutter_on", "stutter_mode" }, { "Stutter", "stutter_on", "stutter_pattern" },
        { "Stutter", "stutter_on", "stutter_random" }, { "Stutter", "stutter_on", "stutter_mix" },
        { "Phone", "phone_on", "phone_amount" }, { "Phone", "phone_on", "phone_bright" }, { "Phone", "phone_on", "phone_tone" },
        { "Mosh", "mosh_on", "mosh_macro" }, { "Mosh", "mosh_on", "mosh_follow" },
        { "Pump", "pump_on", "pump_mode" }, { "Pump", "pump_on", "pump_depth" }, { "Pump", "pump_on", "pump_release" }, { "Pump", "pump_on", "pump_shape" },
        { "Preview", "preview_on", "preview_crunch" }, { "Output", nullptr, "output_gain" },
    };
    const char* onIds[] = { "ghost_on", "haze_on", "stutter_on", "phone_on", "mosh_on", "pump_on", "preview_on" };

    // module on/off at defaults
    auto dry = renderProc (loop, {});
    for (auto* on : onIds)
    {
        auto r = renderProc (loop, { { on, 1.0f } });
        std::printf ("  %-12s ON vs OFF at defaults: diff %6.1f dB, level %+5.1f dB, peak %.2f dBFS %s\n", on,
                     db (diffRms (r.buf, dry.buf, 0, loop.getNumSamples()) / stats (dry.buf).rms),
                     db (stats (r.buf).rms) - db (stats (dry.buf).rms), db (r.peak), r.finite ? "" : "NAN!");
        if (! r.finite) ++failures;
    }
    std::printf ("\n  %-8s %-16s %9s %9s %9s %9s %9s %9s  %s\n", "module", "param", "lvl mid", "lvl max", "diff mid", "diff max", "band mid", "band max", "verdict");
    int flagged = 0;
    XoxicoaFXAudioProcessor paramInfo;   // only used to read parameter ranges / choice names
    for (auto& it : items)
    {
        std::map<juce::String, float> base;
        if (it.onId) base[it.onId] = 1.0f;
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (paramInfo.apvts.getParameter (it.id));
        // floats/ints: min, mid, max. Choices: first, middle, and the most different of all the others.
        std::vector<float> norms { 0.0f, 0.5f, 1.0f };
        const int nSteps = param ? param->getNumSteps() : 0;
        const bool isChoice = dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr;
        if (isChoice) { norms.clear(); for (int k = 0; k < nSteps; ++k) norms.push_back ((float) k / (float) juce::jmax (1, nSteps - 1)); }
        std::vector<Render> r;
        std::vector<std::array<double, 10>> bands;
        bool fin = true; float pk = 0;
        for (float nv : norms)
        {
            auto s = base; s[it.id] = nv;
            r.push_back (renderProc (loop, s));
            bands.push_back (octaveBands (r.back().buf));
            fin &= r.back().finite; pk = std::max (pk, r.back().peak);
        }
        const double ref = stats (r[0].buf).rms;
        auto metrics = [&] (size_t k, double& lv, double& df, double& bd) {
            lv = db (stats (r[k].buf).rms) - db (ref);
            df = db (diffRms (r[k].buf, r[0].buf, 0, loop.getNumSamples()) / ref);
            bd = bandDelta (bands[0], bands[k]); };
        size_t iMid = norms.size() / 2, iMax = norms.size() - 1;
        if (isChoice)
        {
            double best = -1e9;
            for (size_t k = 1; k < norms.size(); ++k) { double lv, df, bd; metrics (k, lv, df, bd); if (df > best) { best = df; iMax = k; } }
        }
        double l1, d1, b1, l2, d2, b2;
        metrics (iMid, l1, d1, b1); metrics (iMax, l2, d2, b2);
        const bool flag = std::max (std::abs (l1), std::abs (l2)) < 1.0 && std::max (d1, d2) < -20.0 && std::max (b1, b2) < 1.5;
        flagged += flag ? 1 : 0;
        if (! fin || pk > 1.0f) ++failures;
        std::printf ("  %-8s %-16s %+9.1f %+9.1f %9.1f %9.1f %9.1f %9.1f  %s%s%s\n", it.module, it.id, l1, l2, d1, d2, b1, b2,
                     flag ? "FLAG (too subtle)" : "ok", fin ? "" : " NAN!", pk > 1.0f ? " CLIP!" : "");
        if (isChoice && param) std::printf ("  %-8s   (choices: mid = '%s', max = most different = '%s')\n", "", param->getAllValueStrings()[(int) iMid].toRawUTF8(), param->getAllValueStrings()[(int) iMax].toRawUTF8());
        if (juce::String (it.id) == "mosh_macro" || juce::String (it.id) == "haze_depth" || juce::String (it.id) == "phone_amount")
            writeWav (r[iMax].buf, dir.getChildFile (juce::String ("audit_") + it.id + "_max.wav"));
    }
    std::printf ("\n  flagged parameters: %d\n", flagged);
    if (flagged > 0) ++failures;

    // everything on, worst case: finite + no clipping
    auto all = renderProc (loop, { { "ghost_on", 1 }, { "haze_on", 1 }, { "stutter_on", 1 }, { "phone_on", 1 }, { "mosh_on", 1 },
                                   { "mosh_macro", 1 }, { "phone_amount", 1 }, { "ghost_mix", 1 }, { "output_gain", 1 } });
    std::printf ("\n  all modules on, everything maxed, +6 dB out: peak %.2f dBFS\n", db (all.peak));
    check (all.finite && all.peak <= 0.967f, "worst case stays finite and under the -0.3 dBFS ceiling");
    writeWav (all.buf, dir.getChildFile ("audit_all_on_max.wav"));
    auto off = renderProc (loop, {}, true);
    check (off.finite, "offline render finite");
}
//==============================================================================
void pumpTests (const juce::AudioBuffer<float>& loop, const juce::File& dir)
{
    std::printf ("\n=== SIDECHAIN PUMP ===\n");
    // steady 220 Hz sine so the gain curve is visible directly in the envelope
    juce::AudioBuffer<float> sine (2, loop.getNumSamples());
    for (int i = 0; i < sine.getNumSamples(); ++i)
    {
        const float v = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / kFs);
        sine.setSample (0, i, v); sine.setSample (1, i, v);
    }
    auto run = [] (const juce::AudioBuffer<float>& in, int mode, float depth)
    {
        xfx::SidechainPump p; p.prepare (kFs, 0); p.setParams (true, mode, depth, 0.5f, 0.5f);
        juce::AudioBuffer<float> out (in);
        for (int s = 0; s < out.getNumSamples(); s += kBlock)
        {
            const int n = std::min (kBlock, out.getNumSamples() - s);
            p.setTransport (true, true, kBpm, s * kBpm / 60.0 / kFs);
            p.process (out.getWritePointer (0) + s, out.getWritePointer (1) + s, nullptr, nullptr, n);
        }
        return out;
    };
    const double beatS = 60.0 / kBpm * kFs;
    for (int mode = 0; mode < 3; ++mode)
    {
        auto out = run (sine, mode, 0.8f);
        const double periodS = beatS * (mode == 1 ? 0.5 : mode == 2 ? 2.0 : 1.0);
        // envelope just after the downbeat vs just before the next one, averaged over the render
        double early = 0, late = 0; int ne = 0, nl = 0;
        for (double t0 = periodS * 2; t0 + periodS < out.getNumSamples(); t0 += periodS)
        {
            for (int i = 0; i < 480; ++i) { early += std::abs (out.getSample (0, (int) t0 + 240 + i)); ++ne; }
            for (int i = 0; i < 480; ++i) { late += std::abs (out.getSample (0, (int) (t0 + periodS) - 720 + i)); ++nl; }
        }
        const double duck = db ((early / ne) / (late / nl));
        const float j = jumps (out).maxD1;
        std::printf ("  %s: level right after each beat vs before the next %.1f dB, max step %.4f (sine's own 0.0144)\n",
                     mode == 0 ? "1/4" : mode == 1 ? "1/8" : "1/2", duck, j);
        check (duck < -6.0, juce::String ("synced duck lands on the grid (") + (mode == 0 ? "1/4" : mode == 1 ? "1/8" : "1/2") + ")");
        check (j < 0.03f && stats (out).finite, "no clicks, finite");
        if (mode == 0) writeWav (out, dir.getChildFile ("pump_quarter_sine.wav"));
    }
    {
        auto out = run (loop, xfx::SidechainPump::Kick, 0.8f);
        // kicks at beats 0 and 2.5 of every bar: compare 40-80 ms after each kick with just before it
        double after = 0, before = 0;
        for (int bar = 1; bar < kBars; ++bar)
            for (double hit : { 0.0, 2.5 })
            {
                const int k0 = (int) ((bar * 4 + hit) * beatS);
                after += diffRms (out, loop, k0 + 1920, k0 + 3840);      // how much the loop was reduced there
                before += diffRms (out, loop, k0 - 2400, k0 - 480);
            }
        std::printf ("  Kick Trigger: reduction after kicks %.1f dBFS vs before kicks %.1f dBFS\n", db (after / 14), db (before / 14));
        check (db (after / 14) > db (before / 14) + 6.0, "kick mode ducks right after kicks");
        check (stats (out).finite, "finite");
        writeWav (out, dir.getChildFile ("pump_kick_loop.wav"));
    }
}

void presetTests (const juce::AudioBuffer<float>& loop)
{
    std::printf ("\n=== PRESETS ===\n");
    XoxicoaFXAudioProcessor proc;
    auto& pm = proc.presets;
    std::printf ("  user preset folder: %s\n", PresetManager::userDirectory().getFullPathName().toRawUTF8());
    const int nf = pm.getNumFactory();
    std::printf ("  factory presets (%d):", nf);
    for (int i = 0; i < nf; ++i) std::printf ("%s%s", i ? ", " : " ", pm.getEntries()[(size_t) i].name.toRawUTF8());
    std::printf ("\n");
    check (nf >= 10, "at least 10 factory presets");

    // every factory preset renders finite, unclipped and (except Init) audibly different from Init
    auto renderState = [&] (int idx)
    {
        XoxicoaFXAudioProcessor p;
        p.presets.load (idx);
        std::map<juce::String, float> set;
        for (auto* prm : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm)) set[rp->getParameterID()] = rp->getValue();
        return renderProc (loop, set);
    };
    auto init = renderState (0);
    bool allOk = true;
    for (int i = 1; i < nf; ++i)
    {
        auto r = renderState (i);
        const double d = db (diffRms (r.buf, init.buf, 0, loop.getNumSamples()) / stats (init.buf).rms);
        std::printf ("    %-16s diff vs Init %6.1f dB, peak %6.2f dBFS %s\n", pm.getEntries()[(size_t) i].name.toRawUTF8(), d, db (r.peak), r.finite ? "" : "NAN!");
        allOk &= r.finite && r.peak <= 0.967f && d > -20.0;
    }
    check (allOk, "every factory preset is finite, under the ceiling and audibly different from Init");

    // user preset round trip
    proc.apvts.getParameter ("mosh_on")->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter ("mosh_macro")->setValueNotifyingHost (0.81f);
    proc.apvts.getParameter ("preview_on")->setValueNotifyingHost (1.0f);
    const juce::String name = "zz Audit Test Preset";
    const bool saved = pm.save (name);
    const auto file = PresetManager::userDirectory().getChildFile (name + PresetManager::extension);
    check (saved && file.existsAsFile(), "save writes " + file.getFileName());
    pm.load (0);
    const float afterInit = proc.apvts.getParameter ("mosh_macro")->getValue();
    int idx = -1;
    for (int i = 0; i < (int) pm.getEntries().size(); ++i) if (pm.getEntries()[(size_t) i].name == name) idx = i;
    pm.load (idx);
    const float restored = proc.apvts.getParameter ("mosh_macro")->getValue();
    std::printf ("  mosh_macro: saved 0.810, after Init %.3f, after loading user preset %.3f; preview after load %.0f\n",
                 afterInit, restored, proc.apvts.getParameter ("preview_on")->getValue());
    check (std::abs (restored - 0.81f) < 0.002f && proc.apvts.getParameter ("preview_on")->getValue() < 0.5f,
           "user preset restores values, preview stays off");
    check (! pm.save ("Glokk Mosh"), "factory names can't be overwritten");
    pm.loadNext (+1); const auto n1 = pm.getCurrentName(); pm.loadNext (-1);
    check (pm.getCurrentName() == name && n1 != name, "next/previous step through the list");
    check (pm.removeCurrent() && ! file.existsAsFile(), "delete removes the user preset file");

    // preset name survives a project save/load
    pm.load (3);
    juce::MemoryBlock mb; proc.getStateInformation (mb);
    XoxicoaFXAudioProcessor proc2; proc2.setStateInformation (mb.getData(), (int) mb.getSize());
    check (proc2.presets.getCurrentName() == pm.getCurrentName(), "preset name is stored in the project");
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();
    const auto loop = makeLoop();
    const bool onlyStutter = argc > 2 && juce::String (argv[2]) == "stutter";
    const bool onlyAudit = argc > 2 && juce::String (argv[2]) == "audit";
    if (! onlyAudit) stutterTests (loop, dir);
    if (! onlyStutter) paramAudit (loop, dir);
    if (! onlyStutter) { pumpTests (loop, dir); presetTests (loop); }
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
