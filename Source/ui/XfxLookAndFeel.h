#pragma once
// Xoxicoa FX v1.1 look: strictly monochrome (black / white / greys), flat panels,
// thin strokes. Also home of XfxKnob, the responsive rotary used everywhere.
#include <juce_gui_basics/juce_gui_basics.h>

namespace xfx::ui
{
namespace Colours
{
    const juce::Colour bg      { 0xff000000 };   // window
    const juce::Colour panel   { 0xff0b0b0b };   // module panel
    const juce::Colour panelOn { 0xff111111 };
    const juce::Colour well    { 0xff161616 };   // knob body, combo body
    const juce::Colour edge    { 0xff262626 };   // hairlines
    const juce::Colour edgeHi  { 0xff4a4a4a };
    const juce::Colour track   { 0xff2e2e2e };   // knob track
    const juce::Colour dim     { 0xff7a7a7a };   // secondary text
    const juce::Colour mid     { 0xffb0b0b0 };
    const juce::Colour text    { 0xfff2f2f2 };   // primary text / active
    const juce::Colour white   { 0xffffffff };
}

inline juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

/** Rotary knob: velocity-sensitive vertical/horizontal drag, Shift = fine, Alt/Cmd-click or
    double-click = default, mouse wheel (Shift = fine), value bubble on hover and drag. */
class XfxKnob : public juce::Slider
{
public:
    XfxKnob() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow)
    {
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setScrollWheelEnabled (true);
        setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { juce::Slider::mouseDown (e); return; }
        if ((e.mods.isAltDown() || e.mods.isCommandDown()) && isDoubleClickReturnEnabled())
        {
            setValue (getDoubleClickReturnValue(), juce::sendNotificationSync);
            return;
        }
        juce::Slider::mouseDown (e);   // starts the host gesture + popup bubble
        lastPos = e.position;
        lastTime = e.eventTime;
        dragProp = valueToProportionOfLength (getValue());
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        const auto d = e.position - lastPos;
        const float px = d.x - d.y;                                    // up / right = increase
        const double dt = juce::jmax (1.0, (double) (e.eventTime - lastTime).inMilliseconds());
        const double speed = std::abs (px) / dt;                      // pixels per ms
        double perPixel = 1.0 / 220.0;                                 // 220 px = full sweep at slow speed
        perPixel *= juce::jlimit (1.0, 3.0, 0.75 + speed * 0.9);       // fast flicks cover more ground
        if (e.mods.isShiftDown()) perPixel = 1.0 / 1600.0;             // fine
        dragProp = juce::jlimit (0.0, 1.0, dragProp + px * perPixel);
        setValue (proportionOfLengthToValue (dragProp), juce::sendNotificationSync);
        lastPos = e.position;
        lastTime = e.eventTime;
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const float delta = (std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : w.deltaY) * (w.isReversed ? -1.0f : 1.0f);
        if (delta == 0.0f) return;
        const double step = e.mods.isShiftDown() ? 0.004 : 0.03;
        auto p = valueToProportionOfLength (getValue()) + (delta > 0 ? step : -step) * juce::jmin (3.0f, std::abs (delta) * 4.0f + 0.5f);
        setValue (proportionOfLengthToValue (juce::jlimit (0.0, 1.0, p)), juce::sendNotificationSync);
    }

    void mouseEnter (const juce::MouseEvent& e) override { juce::Slider::mouseEnter (e); repaint(); }
    void mouseExit (const juce::MouseEvent& e) override  { juce::Slider::mouseExit (e); repaint(); }

private:
    juce::Point<float> lastPos;
    juce::Time lastTime;
    double dragProp = 0.0;
};

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, Colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, Colours::edgeHi);
        setColour (juce::Label::textColourId, Colours::dim);
        setColour (juce::Label::textWhenEditingColourId, Colours::text);
        setColour (juce::Label::backgroundWhenEditingColourId, Colours::well);
        setColour (juce::Label::outlineWhenEditingColourId, Colours::edgeHi);
        setColour (juce::TextEditor::highlightColourId, Colours::edgeHi);
        setColour (juce::TextEditor::highlightedTextColourId, Colours::white);
        setColour (juce::CaretComponent::caretColourId, Colours::white);
        setColour (juce::ComboBox::backgroundColourId, Colours::well);
        setColour (juce::ComboBox::outlineColourId, Colours::edge);
        setColour (juce::ComboBox::textColourId, Colours::text);
        setColour (juce::ComboBox::arrowColourId, Colours::dim);
        setColour (juce::ComboBox::focusedOutlineColourId, Colours::edgeHi);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0e0e0e));
        setColour (juce::PopupMenu::textColourId, Colours::mid);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::text);
        setColour (juce::PopupMenu::highlightedTextColourId, Colours::bg);
        setColour (juce::BubbleComponent::backgroundColourId, Colours::text);
        setColour (juce::BubbleComponent::outlineColourId, Colours::text);
        setColour (juce::TooltipWindow::backgroundColourId, Colours::text);
        setColour (juce::TooltipWindow::textColourId, Colours::bg);
        setColour (juce::TextButton::buttonColourId, Colours::well);
        setColour (juce::TextButton::buttonOnColourId, Colours::text);
        setColour (juce::TextButton::textColourOffId, Colours::text);
        setColour (juce::TextButton::textColourOnId, Colours::bg);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff0e0e0e));
        setColour (juce::AlertWindow::textColourId, Colours::text);
        setColour (juce::AlertWindow::outlineColourId, Colours::edgeHi);
        setColour (juce::TextEditor::backgroundColourId, Colours::well);
        setColour (juce::TextEditor::textColourId, Colours::text);
        setColour (juce::TextEditor::outlineColourId, Colours::edgeHi);
        setColour (juce::TextEditor::focusedOutlineColourId, Colours::text);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? Colours::edgeHi : Colours::well);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (hover && b.isEnabled() ? Colours::edgeHi.brighter (0.4f) : Colours::edge);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return uiFont (juce::jmin (13.0f, (float) h * 0.5f), true); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float thick = juce::jlimit (2.0f, 4.0f, r * 0.085f);
        const float arcR = r - thick * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool on = s.isEnabled() && ! (bool) s.getProperties().getWithDefault ("moduleOff", false);
        const bool hot = s.isMouseOverOrDragging();

        // track + value arc (bipolar params fill from the centre)
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0, startAngle, endAngle, true);
        g.setColour (Colours::track);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (on ? Colours::text : Colours::dim);
        g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // body
        const float br = r - thick - juce::jmax (3.0f, r * 0.1f);
        g.setColour (Colours::well);
        g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
        g.setColour (hot ? Colours::edgeHi : Colours::edge);
        g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);

        // pointer
        juce::Path ptr;
        const float pw = juce::jmax (1.6f, br * 0.09f);
        ptr.addRoundedRectangle (-pw * 0.5f, -br * 0.92f, pw, br * 0.5f, pw * 0.5f);
        g.setColour (on ? Colours::white : Colours::dim);
        g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        const float rad = r.getHeight() * 0.5f;
        g.setColour (on ? Colours::text : Colours::bg);
        g.fillRoundedRectangle (r, rad);
        g.setColour (on ? Colours::text : (hover ? Colours::edgeHi.brighter (0.3f) : Colours::edgeHi));
        g.drawRoundedRectangle (r, rad, 1.0f);
        g.setColour (on ? Colours::bg : Colours::dim);
        g.setFont (uiFont (juce::jmin (11.5f, r.getHeight() * 0.5f), true));
        g.drawText (b.getButtonText().isNotEmpty() ? b.getButtonText() : juce::String (on ? "ON" : "OFF"),
                    r, juce::Justification::centred);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (Colours::well);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (box.isMouseOver (true) || box.hasKeyboardFocus (true) ? Colours::edgeHi : Colours::edge);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (Colours::dim);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (12.5f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getPopupMenuFont() override { return uiFont (13.0f); }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (Colours::edgeHi);
        g.drawRect (0, 0, w, h, 1);
    }

    // Labels: text only, no boxes (an outline appears only while a value is being typed in)
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (label.isBeingEdited()) { g.setColour (Colours::edgeHi); g.drawRect (label.getLocalBounds()); return; }
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f));
        g.setFont (getLabelFont (label));
        g.drawFittedText (label.getText(), getLabelBorderSize (label).subtractedFrom (label.getLocalBounds()),
                          label.getJustificationType(), 1, label.getMinimumHorizontalScale());
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (uiFont (12.0f));
        l->setColour (juce::Label::textColourId, Colours::text);
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        return l;
    }

    // value bubble (hover + drag): white chip, black text
    void drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body) override
    {
        g.setColour (Colours::text);
        g.fillRoundedRectangle (body, 3.0f);
    }
    juce::Font getSliderPopupFont (juce::Slider&) override { return uiFont (12.5f, true); }
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
    void setComponentEffectForBubbleComponent (juce::BubbleComponent& b) override { b.setComponentEffect (nullptr); }
};
} // namespace xfx::ui
