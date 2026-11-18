#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/RageStutter.h"

// Parameter IDs are stable for saved FL projects. Never rename existing IDs.
namespace Param {
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout p;
    auto b = [&] (const char* id, const char* nm, bool v) { p.add (std::make_unique<AudioParameterBool> (ParameterID {id, 1}, nm, v)); };
    // unit: 0 = percent of a 0..1 range, otherwise a suffix such as " ms"
    auto f = [&] (const char* id, const char* nm, float lo, float hi, float step, float def, float skew = 1.0f, const char* unit = nullptr)
    {
        const String u (unit == nullptr ? "" : unit);
        auto attrs = AudioParameterFloatAttributes()
            .withStringFromValueFunction ([u] (float v, int) {
                if (u.isEmpty()) return String (juce::roundToInt (v * 100.0f)) + "%";
                return String (v, std::abs (v) < 10.0f ? 1 : 0) + u; })
            .withValueFromStringFunction ([u] (const String& t) {
                const float v = t.retainCharacters ("-.0123456789").getFloatValue();
                return u.isEmpty() ? v / 100.0f : v; });
        p.add (std::make_unique<AudioParameterFloat> (ParameterID {id, 1}, nm, NormalisableRange<float> (lo, hi, step, skew), def, attrs));
    };
    auto i = [&] (const char* id, const char* nm, int lo, int hi, int def) { p.add (std::make_unique<AudioParameterInt> (ParameterID {id, 1}, nm, lo, hi, def)); };
    auto c = [&] (const char* id, const char* nm, StringArray choices, int def) { p.add (std::make_unique<AudioParameterChoice> (ParameterID {id, 1}, nm, choices, def)); };

    b ("ghost_on", "Ghost Double On", false);
    i ("ghost_pitch", "Ghost Pitch (st)", -24, 12, -12);
    f ("ghost_formant", "Ghost Formant Colour", -12, 12, 0.1f, 0, 1.0f, " st");
    f ("ghost_delay", "Ghost Delay (ms)", 5, 80, 0.1f, 23, 0.6f, " ms");
    f ("ghost_width", "Ghost Width", 0, 1, 0.001f, 0.65f);
    f ("ghost_mix", "Ghost Mix", 0, 1, 0.001f, 0.4f);

    b ("haze_on", "Plugg Haze On", false);
    f ("haze_depth", "Haze Depth", 0, 1, 0.001f, 0.35f);
    f ("haze_rate", "Haze Rate (Hz)", 0.1f, 8.0f, 0.01f, 0.7f, 0.4f, " Hz");
    c ("haze_key", "Haze Key", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    c ("haze_scale", "Haze Scale", { "Minor", "Major", "Harm. Minor", "Minor Pent.", "Major Pent.", "Phrygian", "Dorian" }, 0);
    f ("haze_tone", "Haze Tone", 0, 1, 0.001f, 0.8f);
    f ("haze_tape", "Haze Tape", 0, 1, 0.001f, 0.25f);

    b ("stutter_on", "Rage Stutter On", false);
    // v1.1: "stutter_bars" (4/8) and "stutter_seed" (1-128) were retired; replaced by
    // "stutter_mode" and "stutter_pattern" (named patterns). Old IDs in saved projects are ignored.
    c ("stutter_mode", "Stutter Mode", { "Every Bar", "Every 2 Bars", "Fill: Last Bar of 4", "Fill: Last Bar of 8", "Last Beat of 4 Bars" }, 2);
    {
        StringArray names;
        for (auto& pat : xfx::RageStutter::library()) names.add (pat.name);
        c ("stutter_pattern", "Stutter Pattern", names, xfx::RageStutter::kDefaultPattern);
    }
    f ("stutter_random", "Stutter Randomness", 0, 1, 0.001f, 0.0f);
    f ("stutter_mix", "Stutter Mix", 0, 1, 0.001f, 1.0f);

    b ("phone_on", "808 Phone-Proof On", false);
    f ("phone_amount", "808 Amount", 0, 1, 0.001f, 0.45f);
    f ("phone_bright", "808 Brightness", 0, 1, 0.001f, 0.55f);
    f ("phone_tone", "808 Tone", 0, 1, 0.001f, 0.65f);

    b ("mosh_on", "Mosh Meter On", false);
    f ("mosh_macro", "Mosh", 0, 1, 0.001f, 0.35f);
    f ("mosh_follow", "Mosh Kick-Follow", 0, 1, 0.001f, 0.4f);

    // v1.2: Sidechain Pump
    b ("pump_on", "Pump On", false);
    c ("pump_mode", "Pump Mode", { "1/4", "1/8", "1/2", "Kick Trigger" }, 0);
    f ("pump_depth", "Pump Depth", 0, 1, 0.001f, 0.6f);
    f ("pump_release", "Pump Release", 0, 1, 0.001f, 0.5f);
    f ("pump_shape", "Pump Shape", 0, 1, 0.001f, 0.5f);

    // Monitoring-only switch: not automatable, never restored from a saved project.
    p.add (std::make_unique<AudioParameterBool> (ParameterID {"preview_on", 1}, "MONITOR PREVIEW (not for export)", false,
                                                 AudioParameterBoolAttributes().withAutomatable (false)));
    f ("preview_crunch", "Preview Crunch", 0, 1, 0.001f, 0.0f);

    f ("output_gain", "Output (dB)", -24, 6, 0.1f, 0, 1.0f, " dB");
    return p;
}
}
