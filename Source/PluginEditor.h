#pragma once
#include "PluginProcessor.h"
#include "ui/XfxLookAndFeel.h"

/** One module "card": title, ON switch, knobs, dropdowns and a status line. */
class ModuleCard : public juce::Component
{
public:
    ModuleCard (juce::AudioProcessorValueTreeState& state, const juce::String& title, const juce::String& subtitle,
                const juce::String& powerId, const juce::String& powerText = {});

    void addKnob (const juce::String& paramId, const juce::String& label, bool big = false);
    void addChoice (const juce::String& paramId, const juce::String& label);
    void setStatus (const juce::String& s, bool hot);
    juce::ToggleButton& getPower() { return power; }
    void setFooter (const juce::String& s) { footer = s; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob   { std::unique_ptr<juce::Slider> s; std::unique_ptr<juce::Label> l; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a; bool big; };
    struct Choice { std::unique_ptr<juce::ComboBox> c; std::unique_ptr<juce::Label> l; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> a; };

    juce::AudioProcessorValueTreeState& apvts;
    juce::String title, subtitle, status, footer;
    bool statusHot = false;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::vector<Knob> knobs;
    std::vector<Choice> choices;
};

/** Stereo output meter, dBFS scale. */
class LevelMeter : public juce::Component
{
public:
    void setLevels (float pl, float pr, float rl, float rr, float limDb);
    void paint (juce::Graphics&) override;
private:
    float peak[2] { 0, 0 }, rms[2] { 0, 0 }, hold[2] { 0, 0 }, lim = 0;
};

class XoxicoaFXAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit XoxicoaFXAudioProcessorEditor (XoxicoaFXAudioProcessor&);
    ~XoxicoaFXAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    XoxicoaFXAudioProcessor& proc;
    xfx::ui::LookAndFeel lnf;
    ModuleCard ghost, haze, stutter, phone, mosh, tiktok;
    LevelMeter meter;
    juce::Slider outKnob;
    juce::Label outLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAtt;
    bool previewBlink = false;
    int blinkCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XoxicoaFXAudioProcessorEditor)
};
