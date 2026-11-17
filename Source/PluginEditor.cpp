#include "PluginEditor.h"

using namespace xfx::ui;

namespace
{
juce::String noteName (float midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int m = (int) std::lround (midi);
    if (m < 0 || m > 127) return "--";
    return juce::String (names[m % 12]) + juce::String (m / 12 - 1);
}
constexpr int kW = 1120, kH = 700, kHeader = 92, kPad = 14;
}

//==============================================================================
ModuleCard::ModuleCard (juce::AudioProcessorValueTreeState& s, const juce::String& t, const juce::String& sub,
                        const juce::String& powerId, const juce::String& powerText)
    : apvts (s), title (t), subtitle (sub)
{
    power.setButtonText (powerText);
    power.setClickingTogglesState (true);
    addAndMakeVisible (power);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, powerId, power);
    power.onStateChange = [this] { repaint(); };
}

void ModuleCard::addKnob (const juce::String& id, const juce::String& label, bool big)
{
    Knob k;
    k.s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    k.s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    k.s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.l = std::make_unique<juce::Label> (juce::String(), label.toUpperCase());
    k.l->setJustificationType (juce::Justification::centred);
    k.l->setFont (juce::FontOptions (11.0f, juce::Font::bold));
    k.big = big;
    addAndMakeVisible (*k.s);
    addAndMakeVisible (*k.l);
    k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *k.s);
    knobs.push_back (std::move (k));
}

void ModuleCard::addChoice (const juce::String& id, const juce::String& label)
{
    Choice c;
    c.c = std::make_unique<juce::ComboBox>();
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
        c.c->addItemList (pc->choices, 1);
    c.l = std::make_unique<juce::Label> (juce::String(), label.toUpperCase());
    c.l->setFont (juce::FontOptions (10.5f, juce::Font::bold));
    addAndMakeVisible (*c.c);
    addAndMakeVisible (*c.l);
    c.a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *c.c);
    choices.push_back (std::move (c));
}

void ModuleCard::setStatus (const juce::String& s, bool hot)
{
    if (s != status || hot != statusHot) { status = s; statusHot = hot; repaint(); }
}

void ModuleCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = power.getToggleState();
    g.setGradientFill (juce::ColourGradient (on ? Colours::cardHi : Colours::card, r.getX(), r.getY(),
                                             Colours::card.darker (0.3f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (on ? Colours::purple.withAlpha (0.85f) : Colours::edge);
    g.drawRoundedRectangle (r, 12.0f, on ? 1.8f : 1.0f);
    if (on)
    {
        g.setColour (Colours::purple.withAlpha (0.12f));
        g.drawRoundedRectangle (r.expanded (0.0f).reduced (-2.0f), 14.0f, 3.0f);
    }

    g.setColour (on ? Colours::text : Colours::text.withAlpha (0.7f));
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), 16, 10, getWidth() - 120, 22, juce::Justification::left);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (subtitle, 16, 31, power.getX() - 24, 16, juce::Justification::left, true);

    auto statusArea = juce::Rectangle<int> (16, getHeight() - 26, getWidth() - 32, 18);
    g.setColour (statusHot ? Colours::pink : Colours::dim.withAlpha (0.9f));
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText (status, statusArea, juce::Justification::left);
    if (footer.isNotEmpty())
    {
        g.setColour (Colours::dim);
        g.setFont (juce::FontOptions (10.5f));
        g.drawFittedText (footer, 16, getHeight() - 58, getWidth() - 32, 30, juce::Justification::centredLeft, 2);
    }
}

void ModuleCard::resized()
{
    const bool wide = power.getButtonText().isNotEmpty();
    power.setBounds (getWidth() - (wide ? 150 : 70), 12, wide ? 136 : 56, wide ? 34 : 24);

    auto area = getLocalBounds().reduced (12, 0).withTrimmedTop (54).withTrimmedBottom (footer.isNotEmpty() ? 62 : 30);
    if (! choices.empty())
    {
        auto row = area.removeFromTop (44);
        const int w = row.getWidth() / (int) choices.size();
        for (auto& c : choices)
        {
            auto cell = row.removeFromLeft (w).reduced (4, 0);
            c.l->setBounds (cell.removeFromTop (16));
            c.c->setBounds (cell.removeFromTop (24));
        }
        area.removeFromTop (4);
    }
    if (knobs.empty()) return;

    int bigCount = 0;
    for (auto& k : knobs) bigCount += k.big ? 1 : 0;
    const int smallCount = (int) knobs.size() - bigCount;
    const int perRow = smallCount > 4 ? (smallCount + 1) / 2 : juce::jmax (1, smallCount);
    const int rows = smallCount > 0 ? (smallCount + perRow - 1) / perRow : 0;

    auto bigArea = bigCount > 0 ? area.removeFromLeft (area.getWidth() * (smallCount > 0 ? 45 : 100) / 100) : juce::Rectangle<int>();
    int smallIdx = 0;
    for (auto& k : knobs)
    {
        juce::Rectangle<int> cell;
        if (k.big) cell = bigArea;
        else
        {
            const int rowH = area.getHeight() / juce::jmax (1, rows);
            const int row = smallIdx / perRow, col = smallIdx % perRow;
            const int inThisRow = juce::jmin (perRow, smallCount - row * perRow);
            const int cw = area.getWidth() / perRow;
            const int offset = (perRow - inThisRow) * cw / 2;
            cell = { area.getX() + offset + col * cw, area.getY() + row * rowH, cw, rowH };
            ++smallIdx;
        }
        if (! k.big && bigCount > 0) // keep side knobs smaller than the macro
            cell = cell.withSizeKeepingCentre (cell.getWidth(), juce::jmin (cell.getHeight(), 120));
        k.l->setBounds (cell.removeFromTop (14));
        k.s->setBounds (cell.reduced (k.big ? 0 : 2));
    }
}

//==============================================================================
void LevelMeter::setLevels (float pl, float pr, float rl, float rr, float limDb)
{
    const float p[2] = { pl, pr }, r[2] = { rl, rr };
    for (int c = 0; c < 2; ++c)
    {
        peak[c] = juce::jmax (p[c], peak[c] * 0.85f);
        rms[c]  = juce::jmax (r[c], rms[c] * 0.9f);
        hold[c] = juce::jmax (p[c], hold[c] * 0.97f);
    }
    lim = juce::jmin (limDb, lim * 0.85f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::card);
    g.fillRoundedRectangle (r, 6.0f);
    auto toY = [] (float lin, float h) { const float db = juce::jlimit (-60.0f, 0.0f, xfx::gainToDb (lin)); return h * (1.0f - (db + 60.0f) / 60.0f); };
    auto bars = r.reduced (6.0f, 6.0f).withTrimmedBottom (14.0f);
    const float bw = (bars.getWidth() - 4.0f) * 0.5f;
    for (int c = 0; c < 2; ++c)
    {
        auto b = juce::Rectangle<float> (bars.getX() + c * (bw + 4.0f), bars.getY(), bw, bars.getHeight());
        g.setColour (Colours::bg);
        g.fillRoundedRectangle (b, 3.0f);
        const float yp = toY (peak[c], b.getHeight()), yr = toY (rms[c], b.getHeight()), yh = toY (hold[c], b.getHeight());
        g.setGradientFill (juce::ColourGradient (Colours::pink, b.getX(), b.getY(), Colours::violet, b.getX(), b.getBottom(), false));
        g.setOpacity (0.45f);
        g.fillRect (b.withTrimmedTop (yp));
        g.setOpacity (1.0f);
        g.fillRect (b.withTrimmedTop (yr));
        g.setColour (Colours::text);
        g.fillRect (b.getX(), b.getY() + yh, b.getWidth(), 1.5f);
    }
    g.setColour (lim < -0.5f ? Colours::pink : Colours::dim);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (lim < -0.1f ? "LIM " + juce::String (lim, 1) : "SAFE", r.removeFromBottom (16.0f), juce::Justification::centred);
}

//==============================================================================
XoxicoaFXAudioProcessorEditor::XoxicoaFXAudioProcessorEditor (XoxicoaFXAudioProcessor& pr)
    : AudioProcessorEditor (&pr), proc (pr),
      ghost   (pr.apvts, "Ghost Double",    "dark pitched shadow copy, delayed + spread", "ghost_on"),
      haze    (pr.apvts, "Plugg Haze",      "key-aware tape wobble, saturation, roll-off", "haze_on"),
      stutter (pr.apvts, "Rage Stutter",    "auto glitch fill on the last beat",            "stutter_on"),
      phone   (pr.apvts, "808 Phone-Proof", "pitch-locked harmonics so 808s hit on phones", "phone_on"),
      mosh    (pr.apvts, "Mosh Meter",      "clean -> full chaos. grit pumps with the kick", "mosh_on"),
      tiktok  (pr.apvts, "TikTok Check",    "phone + loudness preview",    "preview_on", "PREVIEW OFF")
{
    setLookAndFeel (&lnf);

    ghost.addKnob ("ghost_pitch", "Pitch");
    ghost.addKnob ("ghost_formant", "Formant");
    ghost.addKnob ("ghost_delay", "Delay");
    ghost.addKnob ("ghost_width", "Width");
    ghost.addKnob ("ghost_mix", "Mix");

    haze.addChoice ("haze_key", "Key");
    haze.addChoice ("haze_scale", "Scale");
    haze.addKnob ("haze_depth", "Depth");
    haze.addKnob ("haze_rate", "Rate");
    haze.addKnob ("haze_tone", "Tone");
    haze.addKnob ("haze_tape", "Tape");

    stutter.addChoice ("stutter_bars", "Fill every");
    stutter.addKnob ("stutter_random", "Random");
    stutter.addKnob ("stutter_mix", "Mix");
    stutter.addKnob ("stutter_seed", "Pattern");

    phone.addKnob ("phone_amount", "Amount");
    phone.addKnob ("phone_bright", "Brightness");
    phone.addKnob ("phone_tone", "Tone");

    mosh.addKnob ("mosh_macro", "Mosh", true);
    mosh.addKnob ("mosh_follow", "Kick-Follow");

    tiktok.getPower().getProperties().set ("danger", true);
    tiktok.addKnob ("preview_crunch", "Crunch");
    tiktok.setFooter ("Preview only. Auto-muted in offline export; turn OFF before realtime recording.");

    for (auto* c : { &ghost, &haze, &stutter, &phone, &mosh, &tiktok }) addAndMakeVisible (*c);
    addAndMakeVisible (meter);

    outKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
    outKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (outKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (pr.apvts, "output_gain", outKnob);
    outLabel.setText ("OUTPUT", juce::dontSendNotification);
    outLabel.setJustificationType (juce::Justification::centred);
    outLabel.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    addAndMakeVisible (outLabel);

    setSize (kW, kH);
    startTimerHz (30);
}

XoxicoaFXAudioProcessorEditor::~XoxicoaFXAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void XoxicoaFXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);
    auto header = getLocalBounds().removeFromTop (kHeader).toFloat();
    g.setGradientFill (juce::ColourGradient (Colours::violet.withAlpha (0.35f), 0, 0, Colours::bg, 0, header.getBottom(), false));
    g.fillRect (header);

    g.setGradientFill (juce::ColourGradient (Colours::pink, 24, 0, Colours::purple, 340, 0, false));
    g.setFont (juce::FontOptions (40.0f, juce::Font::bold));
    g.drawText ("XOXICOA FX", 24, 14, 360, 46, juce::Justification::left);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("GHOST  >  HAZE  >  STUTTER  >  808  >  MOSH  >  OUT   |   rage + pluggnb multi-fx", 26, 60, 640, 18,
                juce::Justification::left);

    const auto& m = proc.meters;
    juce::String transport = m.playing.load() ? juce::String (m.bpm.load(), 1) + " BPM" : "STOPPED";
    g.setColour (Colours::text.withAlpha (0.8f));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (transport, kW - 420, 20, 150, 18, juce::Justification::right);

    if (m.previewLive.load() || proc.apvts.getRawParameterValue ("preview_on")->load() > 0.5f)
    {
        auto banner = juce::Rectangle<float> ((float) kW - 420, 44, 150, 30);
        g.setColour (previewBlink ? Colours::red : Colours::red.darker (0.6f));
        g.fillRoundedRectangle (banner, 6.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (m.offline.load() ? "PREVIEW (MUTED)" : "PREVIEW", banner, juce::Justification::centred);
    }
}

void XoxicoaFXAudioProcessorEditor::resized()
{
    meter.setBounds (kW - 74, 10, 56, kHeader - 16);
    outKnob.setBounds (kW - 160, 12, 74, 62);
    outLabel.setBounds (kW - 160, 72, 74, 14);

    const int cw = (kW - kPad * 4) / 3, ch = (kH - kHeader - kPad * 2) / 2;
    ModuleCard* order[] = { &ghost, &haze, &stutter, &phone, &mosh, &tiktok };
    for (int i = 0; i < 6; ++i)
        order[i]->setBounds (kPad + (i % 3) * (cw + kPad), kHeader + (i / 3) * (ch + kPad), cw, ch);
}

void XoxicoaFXAudioProcessorEditor::timerCallback()
{
    const auto& m = proc.meters;
    meter.setLevels (m.peakL.load(), m.peakR.load(), m.rmsL.load(), m.rmsR.load(), m.limiterDb.load());

    const float hz = m.hz808.load();
    phone.setStatus (hz > 0.0f ? "808 NOTE  " + noteName (xfx::midiFromHz (hz)) + "  (" + juce::String (hz, 1) + " Hz)"
                               : "808 NOTE  --  (waiting for low end)", hz > 0.0f);

    const float hm = m.hazeMidi.load();
    if (hm >= 0.0f)
    {
        const int cents = (int) std::lround (m.hazeBias.load() * 100.0f);
        haze.setStatus ("LEAD  " + noteName (hm) + (cents == 0 ? "  in key" : "  leaning " + juce::String (cents > 0 ? "+" : "") + juce::String (cents) + " c"), cents != 0);
    }
    else haze.setStatus ("LEAD  --", false);

    if (! m.playing.load()) stutter.setStatus ("press play in FL - fills follow the song position", false);
    else stutter.setStatus (m.fill.load() ? ">>> FILL <<<" : "bar " + juce::String (m.barInCycle.load() + 1) + " of cycle", m.fill.load());

    const float k = m.kick.load();
    mosh.setStatus ("KICK  " + juce::String::repeatedString ("|", juce::jlimit (0, 24, (int) (k * 96.0f))), k > 0.1f);

    const bool previewOn = proc.apvts.getRawParameterValue ("preview_on")->load() > 0.5f;
    tiktok.getPower().setButtonText (previewOn ? "PREVIEW ON" : "PREVIEW OFF");
    tiktok.setStatus (previewOn ? (m.offline.load() ? "muted during export" : "LISTENING AS TIKTOK - TURN OFF BEFORE EXPORT") : "off - your mix is untouched", previewOn);

    if (++blinkCount >= 12) { blinkCount = 0; previewBlink = ! previewBlink; }
    repaint (kW - 430, 0, 170, kHeader);
}
