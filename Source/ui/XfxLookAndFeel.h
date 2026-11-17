#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace xfx::ui
{
namespace Colours
{
    const juce::Colour bg      { 0xff07040b };
    const juce::Colour card    { 0xff130c1c };
    const juce::Colour cardHi  { 0xff1c1129 };
    const juce::Colour edge    { 0xff2c1d40 };
    const juce::Colour purple  { 0xffa64dff };
    const juce::Colour violet  { 0xff6a2cff };
    const juce::Colour pink    { 0xffff3df2 };
    const juce::Colour text    { 0xffe9defa };
    const juce::Colour dim     { 0xff8a7aa3 };
    const juce::Colour red     { 0xffff3355 };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, Colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, Colours::dim);
        setColour (juce::ComboBox::backgroundColourId, Colours::cardHi);
        setColour (juce::ComboBox::outlineColourId, Colours::edge);
        setColour (juce::ComboBox::textColourId, Colours::text);
        setColour (juce::ComboBox::arrowColourId, Colours::purple);
        setColour (juce::PopupMenu::backgroundColourId, Colours::card);
        setColour (juce::PopupMenu::textColourId, Colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::violet);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float thick = juce::jmax (3.0f, r * 0.13f);
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool on = s.isEnabled();

        // knob body
        g.setGradientFill (juce::ColourGradient (Colours::cardHi.brighter (0.15f), c.x, c.y - r,
                                                 Colours::bg, c.x, c.y + r, false));
        g.fillEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f);
        g.setColour (Colours::edge);
        g.drawEllipse (c.x - r * 0.72f, c.y - r * 0.72f, r * 1.44f, r * 1.44f, 1.2f);

        // track
        juce::Path track;
        track.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0, startAngle, endAngle, true);
        g.setColour (Colours::edge);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0, startAngle, angle, true);
        g.setGradientFill (juce::ColourGradient (Colours::violet, c.x - r, c.y, Colours::pink, c.x + r, c.y, false));
        if (! on) g.setColour (Colours::dim.withAlpha (0.4f));
        g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // pointer
        juce::Path ptr;
        ptr.addRoundedRectangle (-thick * 0.35f, -r * 0.68f, thick * 0.7f, r * 0.38f, thick * 0.3f);
        g.setColour (on ? Colours::text : Colours::dim);
        g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        const bool danger = b.getProperties().contains ("danger");
        const auto accent = danger ? Colours::red : Colours::purple;
        g.setColour (on ? accent.withAlpha (0.9f) : Colours::cardHi);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? accent.brighter (0.4f) : (hover ? Colours::purple.withAlpha (0.6f) : Colours::edge));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.3f);
        g.setColour (on ? juce::Colours::white : Colours::dim);
        g.setFont (juce::FontOptions (juce::jmin (13.0f, r.getHeight() * 0.55f), juce::Font::bold));
        g.drawText (b.getButtonText().isNotEmpty() ? b.getButtonText() : juce::String (on ? "ON" : "OFF"),
                    r, juce::Justification::centred);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (juce::FontOptions (11.5f));
        l->setColour (juce::Label::textColourId, Colours::text.withAlpha (0.8f));
        return l;
    }
};
} // namespace xfx::ui
