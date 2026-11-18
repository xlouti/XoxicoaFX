#pragma once
// Factory + user presets.
// User presets live in   <user app data>/Audio/Presets/Xoxicoa/Xoxicoa FX/*.xfxpreset
//   Mac:   ~/Library/Audio/Presets/Xoxicoa/Xoxicoa FX/
//   Win:   %APPDATA%\Audio\Presets\Xoxicoa\Xoxicoa FX\     Linux: ~/.config/Audio/Presets/...
// A preset is the full parameter state as XML. "preview_on" is never restored.
// All methods are message-thread only.
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/RageStutter.h"

class PresetManager
{
public:
    struct Entry { juce::String name; bool factory = true; int factoryIndex = -1; juce::File file; };

    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) { refresh(); }

    static juce::File userDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Audio").getChildFile ("Presets").getChildFile ("Xoxicoa").getChildFile ("Xoxicoa FX");
    }
    static constexpr const char* extension = ".xfxpreset";

    void refresh()
    {
        entries.clear();
        for (int i = 0; i < (int) factory().size(); ++i)
            entries.push_back ({ factory()[(size_t) i].name, true, i, {} });
        auto dir = userDirectory();
        if (dir.isDirectory())
        {
            auto files = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
            files.sort();
            for (auto& f : files) entries.push_back ({ f.getFileNameWithoutExtension(), false, -1, f });
        }
    }

    const std::vector<Entry>& getEntries() const { return entries; }
    int getNumFactory() const { return (int) factory().size(); }
    juce::String getCurrentName() const { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n.isEmpty() ? juce::String ("Init") : n; }
    int getCurrentIndex() const
    {
        for (int i = 0; i < (int) entries.size(); ++i) if (entries[(size_t) i].name == currentName) return i;
        return -1;
    }
    bool isCurrentUserPreset() const { const int i = getCurrentIndex(); return i >= 0 && ! entries[(size_t) i].factory; }

    bool load (int index)
    {
        if (index < 0 || index >= (int) entries.size()) return false;
        const auto& e = entries[(size_t) index];
        if (e.factory)
        {
            resetToDefaults();
            for (auto& [id, v] : factory()[(size_t) e.factoryIndex].values) setReal (id, v);
        }
        else
        {
            auto xml = juce::XmlDocument::parse (e.file);
            if (xml == nullptr || ! xml->hasTagName ("XoxicoaFXPreset") || xml->getFirstChildElement() == nullptr) return false;
            auto tree = juce::ValueTree::fromXml (*xml->getFirstChildElement());
            if (! tree.isValid()) return false;
            resetToDefaults();
            for (auto* p : apvts.processor.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    auto child = tree.getChildWithProperty ("id", rp->getParameterID());
                    if (child.isValid() && rp->getParameterID() != "preview_on")
                        rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
                }
        }
        setReal ("preview_on", 0.0f);
        currentName = e.name;
        return true;
    }

    bool loadNext (int dir)
    {
        if (entries.empty()) return false;
        const int n = (int) entries.size();
        const int cur = getCurrentIndex();
        return load (cur < 0 ? 0 : ((cur + dir) % n + n) % n);
    }

    bool save (const juce::String& rawName)
    {
        const auto name = juce::File::createLegalFileName (rawName.trim());
        if (name.isEmpty()) return false;
        for (auto& f : factory()) if (name.equalsIgnoreCase (f.name)) return false;   // can't overwrite factory
        auto dir = userDirectory();
        if (! dir.createDirectory()) return false;
        juce::XmlElement root ("XoxicoaFXPreset");
        root.setAttribute ("name", name);
        root.setAttribute ("version", "1.2");
        auto state = apvts.copyState();
        if (auto xml = state.createXml()) root.addChildElement (xml.release());
        if (! root.writeTo (dir.getChildFile (name + extension))) return false;
        currentName = name;
        refresh();
        return true;
    }

    bool removeCurrent()
    {
        const int i = getCurrentIndex();
        if (i < 0 || entries[(size_t) i].factory) return false;
        if (! entries[(size_t) i].file.deleteFile()) return false;
        refresh();
        currentName = "Init";
        return true;
    }

private:
    struct Factory { const char* name; std::vector<std::pair<juce::String, float>> values; };

    static float pattern (const char* nm)
    {
        const auto& lib = xfx::RageStutter::library();
        for (int i = 0; i < (int) lib.size(); ++i) if (juce::String (lib[(size_t) i].name) == nm) return (float) i;
        return (float) xfx::RageStutter::kDefaultPattern;
    }

    // Values are real (denormalised) parameter values. Anything not listed = default.
    static const std::vector<Factory>& factory()
    {
        static const std::vector<Factory> f = {
            { "Init", {} },
            { "Glokk Mosh", { { "mosh_on", 1 }, { "mosh_macro", 0.72f }, { "mosh_follow", 0.6f },
                              { "phone_on", 1 }, { "phone_amount", 0.35f },
                              { "pump_on", 1 }, { "pump_mode", 0 }, { "pump_depth", 0.35f }, { "pump_release", 0.45f } } },
            { "Plugg Wash", { { "haze_on", 1 }, { "haze_depth", 0.45f }, { "haze_rate", 0.6f }, { "haze_tone", 0.55f }, { "haze_tape", 0.35f },
                              { "ghost_on", 1 }, { "ghost_pitch", 12 }, { "ghost_mix", 0.22f }, { "ghost_width", 0.85f }, { "ghost_delay", 45 } } },
            { "TikTok Ready", { { "phone_on", 1 }, { "phone_amount", 0.45f }, { "phone_bright", 0.6f },
                                { "mosh_on", 1 }, { "mosh_macro", 0.25f }, { "mosh_follow", 0.3f }, { "output_gain", -1.0f } } },
            { "Rage Roll Fill", { { "stutter_on", 1 }, { "stutter_mode", 2 }, { "stutter_pattern", pattern ("Rage Roll Buildup") }, { "stutter_mix", 1 },
                                  { "mosh_on", 1 }, { "mosh_macro", 0.45f } } },
            { "Half-Time Drop", { { "stutter_on", 1 }, { "stutter_mode", 3 }, { "stutter_pattern", pattern ("Half-Time Into Stop") }, { "stutter_mix", 1 },
                                  { "haze_on", 1 }, { "haze_depth", 0.2f }, { "haze_tape", 0.3f } } },
            { "Ghost Lead", { { "ghost_on", 1 }, { "ghost_pitch", -12 }, { "ghost_formant", -3 }, { "ghost_delay", 18 }, { "ghost_width", 0.7f }, { "ghost_mix", 0.45f },
                              { "haze_on", 1 }, { "haze_depth", 0.2f } } },
            { "808 Phone Punch", { { "phone_on", 1 }, { "phone_amount", 0.6f }, { "phone_bright", 0.65f }, { "phone_tone", 0.6f },
                                   { "mosh_on", 1 }, { "mosh_macro", 0.3f }, { "mosh_follow", 0.8f } } },
            { "Pump It", { { "pump_on", 1 }, { "pump_mode", 0 }, { "pump_depth", 0.75f }, { "pump_release", 0.6f }, { "pump_shape", 0.6f } } },
            { "Kick Duck", { { "pump_on", 1 }, { "pump_mode", 3 }, { "pump_depth", 0.6f }, { "pump_release", 0.35f }, { "pump_shape", 0.7f } } },
            { "Stutter Chop", { { "stutter_on", 1 }, { "stutter_mode", 1 }, { "stutter_pattern", pattern ("Gated Chop 1/16") }, { "stutter_random", 0.3f } } },
            { "Dark Tape", { { "haze_on", 1 }, { "haze_tone", 0.3f }, { "haze_tape", 0.7f }, { "haze_depth", 0.25f },
                             { "ghost_on", 1 }, { "ghost_formant", -6 }, { "ghost_mix", 0.3f } } },
        };
        return f;
    }

    void resetToDefaults()
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) rp->setValueNotifyingHost (rp->getDefaultValue());
    }
    void setReal (const juce::String& id, float v)
    {
        if (auto* rp = apvts.getParameter (id)) rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Entry> entries;
    juce::String currentName { "Init" };
};
