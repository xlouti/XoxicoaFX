// Offline render test: loads the BUILT Xoxicoa FX VST3, feeds a synthetic
// 160 BPM rage loop (kick + gliding 808 + detuned saw lead + hats) through
// each module, and checks output is finite, not silent, never above 0 dBFS,
// and that each module actually changes the sound where it should.
//
// usage: XoxicoaFXRenderTest <path/to/Xoxicoa FX.vst3> [outDir]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <map>

namespace
{
constexpr double kFs = 48000.0, kBpm = 160.0;
constexpr int kBars = 8, kBlock = 512;

struct TestPlayHead : juce::AudioPlayHead
{
    juce::int64 sample = 0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (kBpm);
        p.setIsPlaying (playing);
        p.setTimeInSamples (sample);
        p.setTimeInSeconds ((double) sample / kFs);
        p.setPpqPosition ((double) sample / kFs * kBpm / 60.0);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

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
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: %s <plugin.vst3> [outDir]\n", argv[0]); return 2; }
    const juce::File outDir (argc > 2 ? juce::String (argv[2]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    juce::VST3PluginFormat vst3;
    juce::OwnedArray<juce::PluginDescription> types;
    vst3.findAllTypesForFile (types, juce::String (argv[1]));
    if (types.isEmpty()) { std::printf ("FAIL: could not scan %s\n", argv[1]); return 1; }
    juce::String err;
    auto inst = vst3.createInstanceFromDescription (*types[0], kFs, kBlock, err);
    if (! inst) { std::printf ("FAIL: load error %s\n", err.toRawUTF8()); return 1; }
    std::printf ("Loaded: %s by %s, %d params\n", inst->getName().toRawUTF8(), types[0]->manufacturerName.toRawUTF8(), (int) inst->getParameters().size());

    std::map<juce::String, juce::AudioProcessorParameter*> params;
    for (auto* p : inst->getParameters()) params[p->getName (100)] = p;
    auto setP = [&] (const char* name, float norm) {
        auto it = params.find (name);
        if (it == params.end()) { std::printf ("  !! missing param '%s'\n", name); return; }
        it->second->setValueNotifyingHost (norm);
    };

    TestPlayHead ph;
    inst->setPlayHead (&ph);
    inst->prepareToPlay (kFs, kBlock);
    const int latency = inst->getLatencySamples();
    std::printf ("Reported latency: %d samples (%.2f ms)\n", latency, latency * 1000.0 / kFs);

    const auto dry = makeLoop();
    const int total = dry.getNumSamples();
    writeWav (dry, outDir.getChildFile ("00_input.wav"));

    auto render = [&] (const juce::AudioBuffer<float>& input, bool nonRealtime, bool randomBlocks) {
        inst->setNonRealtime (nonRealtime);
        {   // deliver queued parameter changes (VST3 sends them with the next process call)
            juce::AudioBuffer<float> prime (2, kBlock); prime.clear(); juce::MidiBuffer mm;
            ph.sample = 0; inst->processBlock (prime, mm);
        }
        inst->reset();
        juce::AudioBuffer<float> out (2, total);
        juce::AudioBuffer<float> blk (2, kBlock);
        juce::MidiBuffer midi;
        juce::Random r (99);
        for (int pos = 0; pos < total;)
        {
            const int n = std::min (total - pos, randomBlocks ? 1 + r.nextInt (kBlock) : kBlock);
            blk.setSize (2, n, false, false, true);
            for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, input, c, pos, n);
            ph.sample = pos;
            inst->processBlock (blk, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, blk, c, 0, n);
            pos += n;
        }
        return out;
    };
    auto defaults = [&] { for (auto* p : inst->getParameters()) p->setValueNotifyingHost (p->getDefaultValue()); };

    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::printf ("    [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (! ok) ++failures; };

    defaults();
    const auto off = render (dry, false, false);
    {
        // all modules off: output == input delayed by reported latency
        double sq = 0;
        for (int c = 0; c < 2; ++c) for (int i = latency; i < total; ++i) { const double d = off.getSample (c, i) - dry.getSample (c, i - latency); sq += d * d; }
        const double err = db (std::sqrt (sq / (2.0 * (total - latency))));
        std::printf ("\n[all_off] transparency error vs latency-aligned input: %.1f dBFS\n", err);
        check (err < -60.0, "all modules off is transparent (< -60 dB residual)");
    }
    writeWav (off, outDir.getChildFile ("01_all_off.wav"));

    const double beatS = 60.0 / kBpm * kFs;
    auto inFill = [&] (int i) { // input-timeline sample i is in the last beat of bar 4 or 8
        const double q = (i - latency) / beatS; const double inCycle = std::fmod (q, 16.0);
        return q >= 0 && inCycle >= 15.0; };

    struct Case { const char* file; const char* label; std::vector<std::pair<const char*, float>> set; };
    const std::vector<Case> cases = {
        { "02_ghost_double.wav", "Ghost Double (-12 st, mix 60%)", { { "Ghost Double On", 1 }, { "Ghost Mix", 0.6f } } },
        { "03_plugg_haze.wav",   "Plugg Haze (key F minor, depth 70%)", { { "Plugg Haze On", 1 }, { "Haze Depth", 0.7f }, { "Haze Key", 5.0f / 11.0f } } },
        { "04_rage_stutter.wav", "Rage Stutter (every 4 bars, mix 100%)", { { "Rage Stutter On", 1 }, { "Stutter Mix", 1.0f } } },
        { "05_808_phoneproof.wav", "808 Phone-Proof (amount 70%)", { { "808 Phone-Proof On", 1 }, { "808 Amount", 0.7f } } },
        { "06_mosh_meter.wav",   "Mosh Meter (mosh 80%, kick-follow 60%)", { { "Mosh Meter On", 1 }, { "Mosh", 0.8f }, { "Mosh Kick-Follow", 0.6f } } },
        { "07_tiktok_preview.wav", "TikTok Check PREVIEW (realtime)", { { "MONITOR PREVIEW (not for export)", 1 }, { "Preview Crunch", 0.4f } } },
        { "08_all_on.wav", "ALL modules on", { { "Ghost Double On", 1 }, { "Plugg Haze On", 1 }, { "Rage Stutter On", 1 }, { "808 Phone-Proof On", 1 }, { "Mosh Meter On", 1 }, { "Mosh", 0.6f } } },
    };

    std::map<juce::String, juce::AudioBuffer<float>> renders;
    for (const auto& cs : cases)
    {
        defaults();
        for (auto& [n, v] : cs.set) setP (n, v);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto out = render (dry, false, false);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        const auto st = stats (out);
        const double d = diffRms (out, off, 0, total);
        std::printf ("\n[%s]\n    peak %.2f dBFS | rms %.1f dBFS | change vs off %.1f dBFS | %.0fx realtime\n",
                     cs.label, db (st.peak), db (st.rms), db (d), (total / kFs * 1000.0) / ms);
        check (st.finite, "output finite (no NaN/Inf)");
        check (st.rms > 1e-4, "output not silent");
        check (st.peak <= 1.0f, "never above 0 dBFS (peak " + juce::String (db (st.peak), 2) + " dBFS)");
        check (d > 1e-4, "module audibly changes the signal");
        renders[cs.file] = out;
        writeWav (out, outDir.getChildFile (cs.file));
    }

    // module-specific checks
    std::printf ("\n[module checks]\n");
    {
        const auto& st = renders["04_rage_stutter.wav"];
        const int edge = (int) (0.10 * kFs); // exclude crossfades and the safety limiter release tail
        double inS = 0, outS = 0; int inN = 0, outN = 0;
        for (int i = 0; i < total; ++i)
        {
            double d = 0; for (int c = 0; c < 2; ++c) { const double x = st.getSample (c, i) - off.getSample (c, i); d += x * x; }
            if (inFill (i)) { inS += d; ++inN; } else if (! inFill (i - edge) && ! inFill (i + edge)) { outS += d; ++outN; }
        }
        const double inDb = db (std::sqrt (inS / std::max (1, inN))), outDb = db (std::sqrt (outS / std::max (1, outN)));
        std::printf ("    stutter change inside fill beats %.1f dBFS, outside %.1f dBFS\n", inDb, outDb);
        check (inDb > -40.0 && outDb < -60.0, "stutter only acts in the last beat of every 4th bar");
    }
    {
        const auto& pp = renders["05_808_phoneproof.wav"];
        const double midOff = bandRms (off, 180, 1500), midOn = bandRms (pp, 180, 1500);
        const double subOff = bandRms (off, 25, 70), subOn = bandRms (pp, 25, 70);
        std::printf ("    180-1500 Hz band: %.1f -> %.1f dB | 25-70 Hz sub: %.1f -> %.1f dB\n", db (midOff), db (midOn), db (subOff), db (subOn));
        check (db (midOn) - db (midOff) > 2.0, "808 harmonics added in the phone range (> +2 dB)");
        check (std::abs (db (subOn) - db (subOff)) < 0.5, "sub left intact (< 0.5 dB change)");
    }
    {
        const auto& tt = renders["07_tiktok_preview.wav"];
        double side = 0; for (int i = 0; i < total; ++i) side = std::max (side, (double) std::abs (tt.getSample (0, i) - tt.getSample (1, i)));
        std::printf ("    (preview fades in over ~15 ms when switched on; mono checked over the whole render)\n");
        const double low = bandRms (tt, 25, 120), lowOff = bandRms (off, 25, 120);
        std::printf ("    preview: max L-R difference %.6f | 25-120 Hz %.1f -> %.1f dB\n", side, db (lowOff), db (low));
        check (side < 1e-6, "preview is mono");
        check (db (lowOff) - db (low) > 12.0, "preview removes phone-inaudible lows (> 12 dB)");

        defaults(); setP ("MONITOR PREVIEW (not for export)", 1);
        const auto exported = render (dry, true, false);
        const double d = db (diffRms (exported, off, 0, total));
        std::printf ("    preview ON + offline export: difference to clean %.1f dBFS\n", d);
        check (d < -90.0, "preview never prints in an offline (export) render");
    }
    {
        defaults();
        for (auto& [n, v] : cases.back().set) setP (n, v);
        const auto a = render (dry, false, false);
        const auto b = render (dry, false, false);
        const auto c = render (dry, false, true);
        const double rep = diffRms (a, b, 0, total), blk = diffRms (a, c, 0, total);
        std::printf ("    all-on repeat render diff %s | random block sizes (1..512) diff %.1f dBFS\n",
                     rep == 0.0 ? "bit-identical" : juce::String (db (rep), 1).toRawUTF8(), db (blk));
        check (rep == 0.0, "deterministic: same input renders bit-identical");
        check (db (blk) < -60.0, "block-size independent (< -60 dB)");

        juce::AudioBuffer<float> hot (dry);
        hot.applyGain (juce::Decibels::decibelsToGain (18.0f));
        setP ("Output (dB)", 1.0f); // +6 dB
        const auto h = render (hot, false, false);
        const auto st = stats (h);
        std::printf ("    +18 dB hot input, all on, output +6 dB: peak %.2f dBFS\n", db (st.peak));
        check (st.finite && st.peak <= 0.9661f, "safety limiter holds -0.3 dBFS ceiling on hot input");
        writeWav (h, outDir.getChildFile ("09_hot_input_all_on.wav"));
    }
    {
        // state round trip: settings survive, preview comes back OFF
        defaults(); setP ("Mosh", 0.9f); setP ("MONITOR PREVIEW (not for export)", 1);
        juce::AudioBuffer<float> pb (2, kBlock); juce::MidiBuffer mm;
        pb.clear(); inst->processBlock (pb, mm);
        juce::MemoryBlock mb; inst->getStateInformation (mb);
        defaults(); pb.clear(); inst->processBlock (pb, mm);
        inst->setStateInformation (mb.getData(), (int) mb.getSize());
        pb.clear(); inst->processBlock (pb, mm);
        const float mosh = params["Mosh"]->getValue(), prev = params["MONITOR PREVIEW (not for export)"]->getValue();
        std::printf ("    state restore: Mosh=%.3f (expect 0.900), Preview=%.0f (expect 0)\n", mosh, prev);
        check (std::abs (mosh - 0.9f) < 0.01f && prev < 0.5f, "state saves/restores; preview forced off on load");
    }
    {
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.inputBuses.add (juce::AudioChannelSet::disabled());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        inst->releaseResources();
        const bool ok = inst->setBusesLayout (mono);
        inst->prepareToPlay (kFs, kBlock);
        defaults();
        for (auto& [n, v] : cases.back().set) setP (n, v);
        juce::AudioBuffer<float> mb (1, kBlock); juce::MidiBuffer midi; bool finite = true; float pk = 0;
        for (int pos = 0; pos + kBlock <= total; pos += kBlock)
        {
            mb.copyFrom (0, 0, dry, 0, pos, kBlock); ph.sample = pos;
            inst->processBlock (mb, midi);
            for (int i = 0; i < kBlock; ++i) { finite &= std::isfinite (mb.getSample (0, i)); pk = std::max (pk, std::abs (mb.getSample (0, i))); }
        }
        std::printf ("    mono layout accepted: %s, peak %.2f dBFS\n", ok ? "yes" : "no", db (pk));
        check (ok && finite && pk > 1e-3f && pk <= 1.0f, "mono in/out works");
    }

    inst->releaseResources();
    std::printf ("\n%s: %d failure(s). Renders in %s\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, outDir.getFullPathName().toRawUTF8());
    return failures == 0 ? 0 : 1;
}
