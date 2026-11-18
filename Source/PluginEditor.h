#pragma once
#include "PluginProcessor.h"
#include "ui/XfxLookAndFeel.h"

/** One module panel: title, power switch, dropdowns, knobs, optional extra view and a status line. */
class ModuleCard : public juce::Component
{
public:
    ModuleCard (juce::AudioProcessorValueTreeState& state, const juce::String& number, const juce::String& title,
                const juce::String& subtitle, const juce::String& powerId, const juce::String& powerText = {});

    void addKnob (const juce::String& paramId, const juce::String& label, bool big = false);
    void addChoice (const juce::String& paramId, const juce::String& label, int widthWeight = 1);
    void setExtra (juce::Component* c, int height) { extra = c; extraH = height; if (c) addAndMakeVisible (*c); resized(); }
    void setStatus (const juce::String& s, bool hot);
    void setPopupParent (juce::Component* p);
    juce::ToggleButton& getPower() { return power; }
    void setFooter (const juce::String& s) { footer = s; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob   { std::unique_ptr<xfx::ui::XfxKnob> s; std::unique_ptr<juce::Label> l; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a; bool big; };
    struct Choice { std::unique_ptr<juce::ComboBox> c; std::unique_ptr<juce::Label> l; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> a; int weight; };

    juce::AudioProcessorValueTreeState& apvts;
    juce::String number, title, subtitle, status, footer;
    bool statusHot = false;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::vector<Knob> knobs;
    std::vector<Choice> choices;
    juce::Component* extra = nullptr;
    int extraH = 0;
};

/** 16-step map of the current Rage Stutter pattern, with a playhead. */
class PatternView : public juce::Component
{
public:
    void set (int patternIdx, int step, bool live);
    void paint (juce::Graphics&) override;
private:
    int pattern = 0, curStep = -1;
    bool isLive = false;
};

/** Sidechain Pump duck curve with a playhead dot. */
class PumpView : public juce::Component
{
public:
    void set (int mode, float depth, float release, float shapeK, float phase, bool on);
    void paint (juce::Graphics&) override;
private:
    int mode = 0; float depth = 0.6f, release = 0.5f, k = 2.3f, phase = 1.0f; bool isOn = false;
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
    void refreshPresetUi();
    void showPresetMenu();
    void savePresetDialog();
    void deletePresetDialog();

    ModuleCard ghost, haze, stutter, phone, mosh, pump, tiktok;
    PatternView patternView;
    PumpView pumpView;
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, presetName, presetSave { "SAVE" }, presetDelete { "DELETE" };
    LevelMeter meter;
    xfx::ui::XfxKnob outKnob;
    juce::Label outLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAtt;
    juce::TooltipWindow tooltips { this, 600 };
    bool previewBlink = false;
    int blinkCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XoxicoaFXAudioProcessorEditor)
};
