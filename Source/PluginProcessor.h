#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/GhostDouble.h"
#include "dsp/PluggHaze.h"
#include "dsp/RageStutter.h"
#include "dsp/PhoneProof.h"
#include "dsp/MoshMeter.h"
#include "dsp/TikTokCheck.h"

class XoxicoaFXAudioProcessor : public juce::AudioProcessor
{
public:
    XoxicoaFXAudioProcessor();
    ~XoxicoaFXAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Xoxicoa FX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor's timer (GUI thread). Written once per block.
    struct Meters
    {
        std::atomic<float> peakL { 0 }, peakR { 0 }, rmsL { 0 }, rmsR { 0 }, limiterDb { 0 };
        std::atomic<float> hz808 { 0 }, hazeMidi { -1 }, hazeBias { 0 }, kick { 0 }, bpm { 0 };
        std::atomic<int>   barInCycle { 0 };
        std::atomic<bool>  fill { false }, previewLive { false }, offline { false }, playing { false };
    } meters;

private:
    void processChunk (float* L, float* R, const float* scL, const float* scR, int n);
    bool applyParams (bool offline);
    void resetModules();
    std::atomic<float>* raw (const char* id) { return apvts.getRawParameterValue (id); }

    xfx::GhostDouble ghost;
    xfx::PluggHaze haze;
    xfx::RageStutter stutter;
    xfx::PhoneProof phone;
    xfx::MoshMeter mosh;
    xfx::TikTokCheck tiktok;
    xfx::SafetyLimiter limiter;
    xfx::Smoother outGainS;

    std::vector<float> workL, workR, bypassDelay;
    int chunkSize = 512, latency = 0, bypassPos = 0;
    double fs = 48000.0;

    struct P
    {
        std::atomic<float> *ghostOn, *ghostPitch, *ghostFormant, *ghostDelay, *ghostWidth, *ghostMix;
        std::atomic<float> *hazeOn, *hazeDepth, *hazeRate, *hazeKey, *hazeScale, *hazeTone, *hazeTape;
        std::atomic<float> *stOn, *stBars, *stRandom, *stMix, *stSeed;
        std::atomic<float> *phOn, *phAmount, *phBright, *phTone;
        std::atomic<float> *moOn, *moMacro, *moFollow;
        std::atomic<float> *prOn, *prCrunch, *outGain;
    } p {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XoxicoaFXAudioProcessor)
};
