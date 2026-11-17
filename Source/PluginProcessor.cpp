#include "PluginProcessor.h"
#include "PluginEditor.h"

XoxicoaFXAudioProcessor::XoxicoaFXAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "XoxicoaFX", Param::createLayout())
{
    p = { raw ("ghost_on"), raw ("ghost_pitch"), raw ("ghost_formant"), raw ("ghost_delay"), raw ("ghost_width"), raw ("ghost_mix"),
          raw ("haze_on"), raw ("haze_depth"), raw ("haze_rate"), raw ("haze_key"), raw ("haze_scale"), raw ("haze_tone"), raw ("haze_tape"),
          raw ("stutter_on"), raw ("stutter_bars"), raw ("stutter_random"), raw ("stutter_mix"), raw ("stutter_seed"),
          raw ("phone_on"), raw ("phone_amount"), raw ("phone_bright"), raw ("phone_tone"),
          raw ("mosh_on"), raw ("mosh_macro"), raw ("mosh_follow"),
          raw ("preview_on"), raw ("preview_crunch"), raw ("output_gain") };
}

bool XoxicoaFXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void XoxicoaFXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    chunkSize = juce::jlimit (64, 2048, samplesPerBlock > 0 ? samplesPerBlock : 512);
    workL.assign ((size_t) chunkSize, 0.0f);
    workR.assign ((size_t) chunkSize, 0.0f);

    ghost.prepare (fs);
    haze.prepare (fs);
    stutter.prepare (fs);
    phone.prepare (fs);
    mosh.prepare (fs, chunkSize);
    tiktok.prepare (fs);
    limiter.prepare (fs);
    outGainS.prepare (fs, 30.0);

    // Snap every smoother to the current knob positions so a fresh start
    // (or an export) never glides in from stale values.
    applyParams (isNonRealtime());
    resetModules();

    latency = haze.getLatencySamples() + mosh.getLatencySamples();
    setLatencySamples (latency);
    bypassDelay.assign ((size_t) (latency + 1) * 2, 0.0f);
    bypassPos = 0;
}

void XoxicoaFXAudioProcessor::resetModules()
{
    ghost.reset(); haze.reset(); stutter.reset(); phone.reset(); mosh.reset(); tiktok.reset(); limiter.reset();
    outGainS.snap (outGainS.target);
    std::fill (bypassDelay.begin(), bypassDelay.end(), 0.0f);
    bypassPos = 0;
}

void XoxicoaFXAudioProcessor::reset()
{
    applyParams (isNonRealtime());
    resetModules();
}

bool XoxicoaFXAudioProcessor::applyParams (bool offline)
{
    const bool previewOn = p.prOn->load() > 0.5f && ! offline;   // never prints in an offline export
    ghost.setParams (p.ghostOn->load() > 0.5f, (int) std::lround (p.ghostPitch->load()), p.ghostFormant->load(),
                     p.ghostDelay->load(), p.ghostWidth->load(), p.ghostMix->load());
    haze.setParams (p.hazeOn->load() > 0.5f, p.hazeDepth->load(), p.hazeRate->load(),
                    (int) std::lround (p.hazeKey->load()), (int) std::lround (p.hazeScale->load()),
                    p.hazeTone->load(), p.hazeTape->load());
    stutter.setParams (p.stOn->load() > 0.5f, p.stBars->load() > 0.5f ? 8 : 4, p.stRandom->load(),
                       p.stMix->load(), (int) std::lround (p.stSeed->load()));
    phone.setParams (p.phOn->load() > 0.5f, p.phAmount->load(), p.phBright->load(), p.phTone->load());
    mosh.setParams (p.moOn->load() > 0.5f, p.moMacro->load(), p.moFollow->load());
    tiktok.setParams (previewOn, p.prCrunch->load());
    if (offline) tiktok.forceOff();   // no fade-out tail of the preview in an export either
    outGainS.setTarget (xfx::dbToGain (p.outGain->load()));
    return previewOn;
}

void XoxicoaFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int nIn = mainIn.getNumChannels(), nOut = mainOut.getNumChannels();
    if (nOut == 0 || total == 0) return;

    const float* scBase[2] = { nullptr, nullptr };
    if (getBusCount (true) > 1)
        if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0) scBase[0] = sc.getReadPointer (0);
            if (sc.getNumChannels() > 1) scBase[1] = sc.getReadPointer (1);
        }

    // Transport for Rage Stutter
    bool valid = false, playing = false;
    double bpm = 120.0, ppq = 0.0;
    int num = 4, den = 4;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; valid = true; }
            if (auto ts = pos->getTimeSignature()) { num = ts->numerator; den = ts->denominator; }
            playing = pos->getIsPlaying();
        }

    const bool offline = isNonRealtime();
    const bool previewOn = applyParams (offline);

    float pk[2] = { 0, 0 };
    double sq[2] = { 0, 0 };
    for (int start = 0; start < total; start += chunkSize)
    {
        const int n = std::min (chunkSize, total - start);
        float* L = workL.data();
        float* R = workR.data();
        if (nIn > 0)
        {
            std::copy_n (mainIn.getReadPointer (0) + start, n, L);
            std::copy_n (mainIn.getReadPointer (nIn > 1 ? 1 : 0) + start, n, R);
        }
        else { std::fill_n (L, n, 0.0f); std::fill_n (R, n, 0.0f); }

        // Audio reaching the stutter is already delayed by Plugg Haze, so look up the
        // song position that audio belongs to; fills then line up after host PDC.
        const double qps = bpm / 60.0 / fs;
        stutter.setTransport (valid, playing, bpm, ppq + (double) (start - haze.getLatencySamples()) * qps, num, den);
        processChunk (L, R, scBase[0] ? scBase[0] + start : nullptr, scBase[1] ? scBase[1] + start : nullptr, n);

        if (nOut == 1)
        {
            auto* o = mainOut.getWritePointer (0) + start;
            for (int i = 0; i < n; ++i) o[i] = 0.5f * (L[i] + R[i]);
        }
        else
        {
            std::copy_n (L, n, mainOut.getWritePointer (0) + start);
            std::copy_n (R, n, mainOut.getWritePointer (1) + start);
            for (int c = 2; c < nOut; ++c) mainOut.clear (c, start, n);
        }
        for (int i = 0; i < n; ++i)
        {
            pk[0] = std::max (pk[0], std::abs (L[i])); pk[1] = std::max (pk[1], std::abs (R[i]));
            sq[0] += (double) L[i] * L[i]; sq[1] += (double) R[i] * R[i];
        }
    }

    auto& m = meters;
    m.peakL = pk[0]; m.peakR = pk[1];
    m.rmsL = (float) std::sqrt (sq[0] / total); m.rmsR = (float) std::sqrt (sq[1] / total);
    m.limiterDb = limiter.takeGainReductionDb();
    m.hz808 = phone.getDetectedHz();
    m.hazeMidi = haze.getDetectedMidi(); m.hazeBias = haze.getBiasSemis();
    m.kick = mosh.getKickEnvelope();
    m.bpm = (float) bpm; m.playing = playing && valid;
    m.barInCycle = stutter.getBarInCycle(); m.fill = stutter.isFillActive();
    m.previewLive = previewOn; m.offline = offline;
}

void XoxicoaFXAudioProcessor::processChunk (float* L, float* R, const float* scL, const float* scR, int n)
{
    ghost.process (L, R, n);      // 1. Ghost Double
    haze.process (L, R, n);       // 2. Plugg Haze
    stutter.process (L, R, n);    // 3. Rage Stutter
    phone.process (L, R, n);      // 4. 808 Phone-Proof
    mosh.process (L, R, scL, scR, n); // 5. Mosh Meter
    for (int i = 0; i < n; ++i) { const float g = outGainS.next(); L[i] *= g; R[i] *= g; }
    tiktok.process (L, R, n);     // 6. TikTok Check (monitor preview only)
    limiter.process (L, R, n);    // safety ceiling -0.3 dBFS
}

void XoxicoaFXAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Host bypass: pass audio through, delayed by the reported latency so timing stays aligned.
    auto out = getBusBuffer (buffer, false, 0);
    const int nCh = std::min (2, out.getNumChannels());
    const int len = latency + 1;
    if (latency <= 0 || bypassDelay.size() < (size_t) len * 2) return;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (int c = 0; c < nCh; ++c)
        {
            float* line = bypassDelay.data() + (size_t) c * (size_t) len;
            float* s = out.getWritePointer (c) + i;
            const float delayed = line[(bypassPos + 1) % len];
            line[bypassPos] = *s;
            *s = delayed;
        }
        bypassPos = (bypassPos + 1) % len;
    }
}

void XoxicoaFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void XoxicoaFXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    // TikTok preview always comes back OFF after loading a project or preset.
    if (auto* pr = apvts.getParameter ("preview_on")) pr->setValueNotifyingHost (0.0f);
}

juce::AudioProcessorEditor* XoxicoaFXAudioProcessor::createEditor() { return new XoxicoaFXAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new XoxicoaFXAudioProcessor(); }
