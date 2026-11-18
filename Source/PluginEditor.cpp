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
constexpr int kW = 1280, kH = 760, kHeader = 76, kPad = 12;
constexpr int kCardTop = 50, kStatusH = 30;

void setupKnobDefaults (XfxKnob& k, juce::RangedAudioParameter* p)
{
    if (p != nullptr)
        k.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}
}

//==============================================================================
ModuleCard::ModuleCard (juce::AudioProcessorValueTreeState& s, const juce::String& num, const juce::String& t,
                        const juce::String& sub, const juce::String& powerId, const juce::String& powerText)
    : apvts (s), number (num), title (t), subtitle (sub)
{
    power.setButtonText (powerText);
    power.setClickingTogglesState (true);
    addAndMakeVisible (power);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, powerId, power);
    power.onStateChange = [this]
    {
        for (auto& k : knobs) { k.s->getProperties().set ("moduleOff", ! power.getToggleState()); k.s->repaint(); }
        repaint();
    };
}

void ModuleCard::addKnob (const juce::String& id, const juce::String& label, bool big)
{
    Knob k;
    k.s = std::make_unique<XfxKnob>();
    k.s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    k.l = std::make_unique<juce::Label> (juce::String(), label.toUpperCase());
    k.l->setJustificationType (juce::Justification::centred);
    k.l->setFont (uiFont (10.5f, true));
    k.l->setColour (juce::Label::textColourId, Colours::dim);
    k.l->setInterceptsMouseClicks (false, false);
    k.big = big;
    addAndMakeVisible (*k.s);
    addAndMakeVisible (*k.l);
    k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *k.s);
    setupKnobDefaults (*k.s, apvts.getParameter (id));
    k.s->setTooltip ("Drag to change. Shift = fine. Double-click = default. Wheel works too.");
    k.s->getProperties().set ("moduleOff", ! power.getToggleState());
    knobs.push_back (std::move (k));
}

void ModuleCard::addChoice (const juce::String& id, const juce::String& label, int weight)
{
    Choice c;
    c.c = std::make_unique<juce::ComboBox>();
    if (auto* pc = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
        c.c->addItemList (pc->choices, 1);
    c.l = std::make_unique<juce::Label> (juce::String(), label.toUpperCase());
    c.l->setFont (uiFont (10.5f, true));
    c.l->setColour (juce::Label::textColourId, Colours::dim);
    c.l->setInterceptsMouseClicks (false, false);
    c.weight = juce::jmax (1, weight);
    addAndMakeVisible (*c.c);
    addAndMakeVisible (*c.l);
    c.a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *c.c);
    choices.push_back (std::move (c));
}

void ModuleCard::setPopupParent (juce::Component* p)
{
    for (auto& k : knobs) k.s->setPopupDisplayEnabled (true, true, p, 1200);
}

void ModuleCard::setStatus (const juce::String& s, bool hot)
{
    if (s != status || hot != statusHot) { status = s; statusHot = hot; repaint(); }
}

void ModuleCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = power.getToggleState();
    g.setColour (on ? Colours::panelOn : Colours::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? Colours::edgeHi : Colours::edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    if (on) { g.setColour (Colours::text); g.fillRect (r.getX() + 1.0f, r.getY() + 1.0f, 3.0f, (float) kCardTop - 14.0f); }

    g.setColour (Colours::dim);
    g.setFont (uiFont (10.5f, true));
    g.drawText (number, 16, 12, 24, 14, juce::Justification::left);
    g.setColour (on ? Colours::text : Colours::mid);
    g.setFont (uiFont (15.0f, true));
    g.drawText (title.toUpperCase(), 38, 9, power.getX() - 44, 20, juce::Justification::left);
    g.setColour (Colours::dim);
    g.setFont (uiFont (11.0f));
    g.drawText (subtitle, 38, 28, power.getX() - 44, 14, juce::Justification::left, true);

    g.setColour (Colours::edge);
    g.fillRect (12, kCardTop - 4, getWidth() - 24, 1);
    g.fillRect (12, getHeight() - kStatusH, getWidth() - 24, 1);

    g.setColour (statusHot ? Colours::text : Colours::dim);
    g.setFont (uiFont (11.0f, statusHot));
    g.drawText (status, 16, getHeight() - kStatusH + 1, getWidth() - 32, kStatusH - 2, juce::Justification::centredLeft, true);
    if (footer.isNotEmpty())
    {
        g.setColour (Colours::dim);
        g.setFont (uiFont (10.5f));
        g.drawFittedText (footer, 16, getHeight() - kStatusH - 40, getWidth() - 32, 34, juce::Justification::centredLeft, 2);
    }
}

void ModuleCard::resized()
{
    const bool wide = power.getButtonText().isNotEmpty();
    power.setBounds (getWidth() - (wide ? 124 : 62), 13, wide ? 110 : 48, 22);

    auto area = getLocalBounds().reduced (12, 0).withTrimmedTop (kCardTop + 6)
                    .withTrimmedBottom (kStatusH + 6 + (footer.isNotEmpty() ? 40 : 0));
    if (! choices.empty())
    {
        auto row = area.removeFromTop (44);
        int totalW = 0;
        for (auto& c : choices) totalW += c.weight;
        const int unit = row.getWidth() / juce::jmax (1, totalW);
        for (auto& c : choices)
        {
            auto cell = row.removeFromLeft (unit * c.weight).reduced (4, 0);
            c.l->setBounds (cell.removeFromTop (16));
            c.c->setBounds (cell.removeFromTop (24));
        }
        area.removeFromTop (6);
    }
    if (extra != nullptr)
    {
        extra->setBounds (area.removeFromBottom (extraH).reduced (4, 0));
        area.removeFromBottom (6);
    }
    if (knobs.empty()) return;

    int bigCount = 0;
    for (auto& k : knobs) bigCount += k.big ? 1 : 0;
    const int smallCount = (int) knobs.size() - bigCount;
    const int perRow = smallCount > 4 ? (smallCount + 1) / 2 : juce::jmax (1, smallCount);
    const int rows = smallCount > 0 ? (smallCount + perRow - 1) / perRow : 0;

    auto bigArea = bigCount > 0 ? area.removeFromLeft (area.getWidth() * (smallCount > 0 ? 50 : 100) / 100) : juce::Rectangle<int>();
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
        const int maxKnob = k.big ? 170 : 92;
        cell = cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), maxKnob + 10), juce::jmin (cell.getHeight(), maxKnob + 32));
        k.l->setBounds (cell.removeFromTop (14));
        k.s->setBounds (cell.reduced (2, 0));
    }
}

//==============================================================================
void PatternView::set (int p, int step, bool live)
{
    if (p != pattern || step != curStep || live != isLive) { pattern = p; curStep = step; isLive = live; repaint(); }
}

void PatternView::paint (juce::Graphics& g)
{
    const auto& lib = xfx::RageStutter::library();
    if (pattern < 0 || pattern >= (int) lib.size()) return;
    const auto& segs = lib[(size_t) pattern].segs;
    auto r = getLocalBounds().toFloat();
    const float gap = 2.0f, beatGap = 6.0f;
    const float cw = (r.getWidth() - 12 * gap - 3 * beatGap) / 16.0f;
    const float h = r.getHeight() - 12.0f;

    auto cellX = [&] (int s) { return r.getX() + s * (cw + gap) + (float) (s / 4) * (beatGap - gap); };
    for (int s = 0; s < 16; ++s)
    {
        int si = -1;
        for (int k = 0; k < (int) segs.size(); ++k)
            if (s >= segs[(size_t) k].start && s < segs[(size_t) k].start + segs[(size_t) k].len) si = k;
        auto cell = juce::Rectangle<float> (cellX (s), r.getY(), cw, h);
        juce::String tag;
        bool filled = false, outline = false;
        if (si < 0) { tag = ""; }
        else
        {
            const auto& sg = segs[(size_t) si];
            const int srcStep = sg.src + (s - sg.start);
            switch (sg.mode)
            {
                case xfx::RageStutter::Fwd:
                    filled = true;
                    tag = sg.roll64 > 0 ? (sg.roll64 == 8 ? "1/8" : sg.roll64 == 4 ? "1/16" : sg.roll64 == 2 ? "1/32" : "1/64")
                                        : juce::String (srcStep + 1);
                    break;
                case xfx::RageStutter::Rev:  filled = true; tag = "REV"; break;
                case xfx::RageStutter::Half: filled = true; tag = "1/2x"; break;
                case xfx::RageStutter::Tape: filled = true; tag = "STOP"; break;
                default:                     outline = true; tag = "--"; break;
            }
        }
        const bool atHead = isLive && s == curStep;
        g.setColour (filled ? (atHead ? Colours::white : Colours::mid) : (atHead ? Colours::edgeHi : Colours::well));
        g.fillRoundedRectangle (cell, 2.0f);
        if (outline || atHead) { g.setColour (atHead ? Colours::white : Colours::edgeHi); g.drawRoundedRectangle (cell.reduced (0.5f), 2.0f, 1.0f); }
        if (tag.isNotEmpty())
        {
            g.setColour (filled ? Colours::bg : Colours::dim);
            g.setFont (uiFont (cw < 26 ? 8.5f : 9.5f, true));
            g.drawFittedText (tag, cell.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
    }
    g.setColour (Colours::dim);
    g.setFont (uiFont (9.5f));
    for (int b = 0; b < 4; ++b)
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (cellX (b * 4), r.getBottom() - 11.0f, cw, 11.0f), juce::Justification::left);
}

//==============================================================================
void PumpView::set (int m, float d, float r, float kk, float ph, bool on)
{
    if (m != mode || d != depth || r != release || kk != k || std::abs (ph - phase) > 0.002f || on != isOn)
    { mode = m; depth = d; release = r; k = kk; phase = ph; isOn = on; repaint(); }
}

void PumpView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    auto plot = r.reduced (6.0f, 5.0f);
    // sync modes: two note-lengths, release is a fraction of each; kick: one duck over 0..1 of release
    const bool kick = mode == 3;
    const int periods = kick ? 1 : 2;
    const float relFrac = kick ? 0.8f : 0.1f + 0.9f * release;
    juce::Path line, fill;
    const int N = 160;
    for (int i = 0; i <= N; ++i)
    {
        const float t = (float) i / N * (float) periods;
        const float inP = t - std::floor (t);
        const float x = (kick ? t / relFrac : inP / relFrac);
        const float gn = xfx::SidechainPump::curve (i == N && ! kick ? 1.0f : x, depth, k);
        const float px = plot.getX() + plot.getWidth() * (float) i / N;
        const float py = plot.getY() + plot.getHeight() * (1.0f - gn);
        if (i == 0) { line.startNewSubPath (px, py); fill.startNewSubPath (px, plot.getBottom()); fill.lineTo (px, py); }
        else { line.lineTo (px, py); fill.lineTo (px, py); }
    }
    fill.lineTo (plot.getRight(), plot.getBottom()); fill.closeSubPath();
    g.setColour (Colours::edge.brighter (0.15f));
    g.fillPath (fill);
    g.setColour (isOn ? Colours::text : Colours::dim);
    g.strokePath (line, juce::PathStrokeType (1.5f));
    if (isOn && phase < 1.0f)
    {
        const float px = plot.getX() + plot.getWidth() * (kick ? phase * relFrac : phase * relFrac / (float) periods);
        const float py = plot.getY() + plot.getHeight() * (1.0f - xfx::SidechainPump::curve (phase, depth, k));
        g.setColour (Colours::white);
        g.fillEllipse (px - 3.5f, py - 3.5f, 7.0f, 7.0f);
    }
    g.setColour (Colours::dim);
    g.setFont (uiFont (9.5f));
    g.drawText (kick ? "KICK -> RELEASE" : "2 NOTES", r.reduced (6.0f, 3.0f), juce::Justification::topRight);
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
    auto toY = [] (float lin, float h) { const float db = juce::jlimit (-60.0f, 0.0f, xfx::gainToDb (lin)); return h * (1.0f - (db + 60.0f) / 60.0f); };
    auto bars = r.withTrimmedBottom (14.0f);
    const float bw = (bars.getWidth() - 3.0f) * 0.5f;
    for (int c = 0; c < 2; ++c)
    {
        auto b = juce::Rectangle<float> (bars.getX() + c * (bw + 3.0f), bars.getY(), bw, bars.getHeight());
        g.setColour (Colours::well);
        g.fillRect (b);
        const float yp = toY (peak[c], b.getHeight()), yr = toY (rms[c], b.getHeight()), yh = toY (hold[c], b.getHeight());
        g.setColour (Colours::dim);
        g.fillRect (b.withTrimmedTop (yp));
        g.setColour (Colours::text);
        g.fillRect (b.withTrimmedTop (yr));
        g.setColour (Colours::white);
        g.fillRect (b.getX(), b.getY() + yh, b.getWidth(), 1.0f);
    }
    g.setColour (lim < -0.5f ? Colours::text : Colours::dim);
    g.setFont (uiFont (9.5f, lim < -0.5f));
    g.drawText (lim < -0.1f ? "LIM " + juce::String (lim, 1) : "SAFE", r.removeFromBottom (12.0f), juce::Justification::centred);
}

//==============================================================================
XoxicoaFXAudioProcessorEditor::XoxicoaFXAudioProcessorEditor (XoxicoaFXAudioProcessor& pr)
    : AudioProcessorEditor (&pr), proc (pr),
      ghost   (pr.apvts, "01", "Ghost Double",    "pitched shadow copy, delayed and spread", "ghost_on"),
      haze    (pr.apvts, "02", "Plugg Haze",      "key-aware tape wobble, saturation, roll-off", "haze_on"),
      stutter (pr.apvts, "03", "Rage Stutter",    "beat slicer: repeats, rolls, reverses, stops", "stutter_on"),
      phone   (pr.apvts, "04", "808 Phone-Proof", "pitch-locked harmonics so 808s hit on phones", "phone_on"),
      mosh    (pr.apvts, "05", "Mosh Meter",      "clean to full chaos, grit pumps with the kick", "mosh_on"),
      pump    (pr.apvts, "06", "Sidechain Pump",  "tempo-synced or kick-triggered ducking", "pump_on"),
      tiktok  (pr.apvts, "07", "TikTok Check",    "phone speaker + loudness preview", "preview_on", "PREVIEW OFF")
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

    stutter.addChoice ("stutter_mode", "Mode", 2);
    stutter.addChoice ("stutter_pattern", "Pattern", 3);
    stutter.addKnob ("stutter_random", "Random");
    stutter.addKnob ("stutter_mix", "Mix");
    stutter.setExtra (&patternView, 40);

    phone.addKnob ("phone_amount", "Amount");
    phone.addKnob ("phone_bright", "Brightness");
    phone.addKnob ("phone_tone", "Tone");

    mosh.addKnob ("mosh_macro", "Mosh", true);
    mosh.addKnob ("mosh_follow", "Kick-Follow");

    pump.addChoice ("pump_mode", "Mode");
    pump.addKnob ("pump_depth", "Depth");
    pump.addKnob ("pump_release", "Release");
    pump.addKnob ("pump_shape", "Shape");
    pump.setExtra (&pumpView, 52);

    tiktok.addKnob ("preview_crunch", "Crunch");
    tiktok.setFooter ("Monitoring only. Muted in offline export; turn off before recording in realtime.");

    for (auto* c : { &ghost, &haze, &stutter, &phone, &mosh, &pump, &tiktok }) { addAndMakeVisible (*c); c->setPopupParent (this); }

    for (auto* b : { &presetPrev, &presetNext, &presetName, &presetSave, &presetDelete }) addAndMakeVisible (*b);
    presetPrev.onClick = [this] { proc.presets.loadNext (-1); refreshPresetUi(); };
    presetNext.onClick = [this] { proc.presets.loadNext (+1); refreshPresetUi(); };
    presetName.onClick = [this] { showPresetMenu(); };
    presetSave.onClick = [this] { savePresetDialog(); };
    presetDelete.onClick = [this] { deletePresetDialog(); };
    presetPrev.setTooltip ("Previous preset");
    presetNext.setTooltip ("Next preset");
    presetName.setTooltip ("Browse presets");
    presetDelete.setTooltip ("Delete the current user preset");
    refreshPresetUi();
    addAndMakeVisible (meter);

    outKnob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    outKnob.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 16);
    addAndMakeVisible (outKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (pr.apvts, "output_gain", outKnob);
    setupKnobDefaults (outKnob, pr.apvts.getParameter ("output_gain"));
    outKnob.setPopupDisplayEnabled (true, true, this, 1200);
    outLabel.setText ("OUTPUT", juce::dontSendNotification);
    outLabel.setJustificationType (juce::Justification::centredLeft);
    outLabel.setFont (uiFont (10.5f, true));
    addAndMakeVisible (outLabel);

    sendLookAndFeelChange();   // cards were built before they joined the editor: re-skin them
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

    g.setColour (Colours::white);
    g.setFont (uiFont (30.0f, true));
    g.drawText ("XOXICOA", 24, 14, 160, 34, juce::Justification::left);
    g.setColour (Colours::dim);
    g.drawText ("FX", 168, 14, 60, 34, juce::Justification::left);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("GHOST  >  HAZE  >  STUTTER  >  808  >  MOSH  >  OUT", 26, 48, 420, 16, juce::Justification::left);
    g.setColour (Colours::edge);
    g.fillRect (kPad, kHeader - 1, kW - 2 * kPad, 1);

    const auto& m = proc.meters;
    const bool playing = m.playing.load();
    g.setColour (playing ? Colours::text : Colours::dim);
    g.setFont (uiFont (12.0f, true));
    g.drawText (playing ? juce::String (m.bpm.load(), 1) + " BPM" : "STOPPED", kW - 420, 22, 100, 16, juce::Justification::right);
    g.setColour (Colours::dim);
    g.setFont (uiFont (10.5f));
    g.drawText ("TRANSPORT", kW - 420, 40, 100, 14, juce::Justification::right);

    g.setColour (Colours::dim);
    g.setFont (uiFont (10.5f));
    g.drawText ("PRESET", 300, 10, 120, 12, juce::Justification::left);

    if (m.previewLive.load() || proc.apvts.getRawParameterValue ("preview_on")->load() > 0.5f)
    {
        auto banner = juce::Rectangle<float> (712.0f, 26.0f, 150.0f, 30.0f);
        g.setColour (previewBlink ? Colours::white : Colours::bg);
        g.fillRoundedRectangle (banner, 3.0f);
        g.setColour (Colours::white);
        g.drawRoundedRectangle (banner, 3.0f, 1.2f);
        g.setColour (previewBlink ? Colours::bg : Colours::white);
        g.setFont (uiFont (13.0f, true));
        g.drawText (m.offline.load() ? "PREVIEW (MUTED)" : "PREVIEW ON", banner, juce::Justification::centred);
    }
}

void XoxicoaFXAudioProcessorEditor::resized()
{
    meter.setBounds (kW - kPad - 40, 10, 40, kHeader - 18);
    outLabel.setBounds (kW - 300, 14, 80, 14);
    outKnob.setBounds (kW - 300, 26, 150, 44);

    // preset bar
    int x = 300;
    const int y = 26, h = 28;
    presetPrev.setBounds (x, y, 28, h);   x += 32;
    presetName.setBounds (x, y, 250, h);  x += 254;
    presetNext.setBounds (x, y, 28, h);   x += 38;
    presetSave.setBounds (x, y, 56, h);   x += 60;
    presetDelete.setBounds (x, y, 64, h);

    // row 1: three wide cards, row 2: four cards
    const int ch = (kH - kHeader - kPad * 3) / 2;
    const int cw3 = (kW - kPad * 4) / 3, cw4 = (kW - kPad * 5) / 4;
    ModuleCard* top[] = { &ghost, &haze, &stutter };
    ModuleCard* bottom[] = { &phone, &mosh, &pump, &tiktok };
    for (int i = 0; i < 3; ++i) top[i]->setBounds (kPad + i * (cw3 + kPad), kHeader + kPad, cw3, ch);
    for (int i = 0; i < 4; ++i) bottom[i]->setBounds (kPad + i * (cw4 + kPad), kHeader + kPad * 2 + ch, cw4, ch);
}

void XoxicoaFXAudioProcessorEditor::timerCallback()
{
    const auto& m = proc.meters;
    meter.setLevels (m.peakL.load(), m.peakR.load(), m.rmsL.load(), m.rmsR.load(), m.limiterDb.load());

    const float hz = m.hz808.load();
    phone.setStatus (hz > 0.0f ? "808 NOTE  " + noteName (xfx::midiFromHz (hz)) + "   " + juce::String (hz, 1) + " Hz"
                               : "808 NOTE  --   waiting for low end", hz > 0.0f);

    const float hm = m.hazeMidi.load();
    if (hm >= 0.0f)
    {
        const int cents = (int) std::lround (m.hazeBias.load() * 100.0f);
        haze.setStatus ("LEAD  " + noteName (hm) + (cents == 0 ? "   in key" : "   leaning " + juce::String (cents > 0 ? "+" : "") + juce::String (cents) + " c"), cents != 0);
    }
    else haze.setStatus ("LEAD  --", false);

    // Pattern view: the chosen pattern, or the swapped-in one while a randomised fill plays.
    const auto& lib = xfx::RageStutter::library();
    const int chosen = (int) std::lround (proc.apvts.getRawParameterValue ("stutter_pattern")->load());
    const bool fill = m.fill.load();
    const int shown = fill ? m.stPattern.load() : chosen;
    patternView.set (shown, fill ? m.stStep.load() : -1, fill);
    const bool stOn = proc.apvts.getRawParameterValue ("stutter_on")->load() > 0.5f;
    if (! stOn) stutter.setStatus ("off", false);
    else if (! m.playing.load()) stutter.setStatus ("press play in FL - slices follow the song position", false);
    else if (fill) stutter.setStatus ("SLICING  " + juce::String (lib[(size_t) juce::jlimit (0, (int) lib.size() - 1, shown)].name).toUpperCase(), true);
    else stutter.setStatus ("bar " + juce::String (m.barInCycle.load() + 1) + " of " + juce::String (m.cycleLen.load()) + "   waiting for the fill bar", false);

    const bool pumpOn = proc.apvts.getRawParameterValue ("pump_on")->load() > 0.5f;
    const int pumpMode = (int) std::lround (proc.apvts.getRawParameterValue ("pump_mode")->load());
    const float pumpK = 0.6f + 3.4f * proc.apvts.getRawParameterValue ("pump_shape")->load();
    pumpView.set (pumpMode, proc.apvts.getRawParameterValue ("pump_depth")->load(),
                  proc.apvts.getRawParameterValue ("pump_release")->load(), pumpK, m.pumpPhase.load(), pumpOn);
    const float gr = xfx::gainToDb (m.pumpGain.load());
    if (! pumpOn) pump.setStatus ("off", false);
    else if (pumpMode != 3 && ! m.playing.load()) pump.setStatus ("press play - synced pumps follow the song position", false);
    else pump.setStatus ("DUCK  " + juce::String (gr, 1) + " dB" + (pumpMode == 3 ? "   kick trigger" : ""), gr < -1.0f);

    const float k = m.kick.load();
    mosh.setStatus ("KICK  " + juce::String::repeatedString ("|", juce::jlimit (0, 28, (int) (k * 112.0f))), k > 0.1f);

    const bool previewOn = proc.apvts.getRawParameterValue ("preview_on")->load() > 0.5f;
    tiktok.getPower().setButtonText (previewOn ? "PREVIEW ON" : "PREVIEW OFF");
    tiktok.setStatus (previewOn ? (m.offline.load() ? "muted during export" : "LISTENING AS TIKTOK - TURN OFF BEFORE EXPORT") : "off - your mix is untouched", previewOn);

    if (++blinkCount >= 12) { blinkCount = 0; previewBlink = ! previewBlink; }
    repaint (0, 0, kW - 310, kHeader - 2);
    if (presetName.getButtonText() != proc.presets.getCurrentName()) refreshPresetUi();
}

//==============================================================================
void XoxicoaFXAudioProcessorEditor::refreshPresetUi()
{
    presetName.setButtonText (proc.presets.getCurrentName());
    presetDelete.setEnabled (proc.presets.isCurrentUserPreset());
}

void XoxicoaFXAudioProcessorEditor::showPresetMenu()
{
    proc.presets.refresh();
    juce::PopupMenu menu;
    const auto& e = proc.presets.getEntries();
    const auto cur = proc.presets.getCurrentName();
    menu.addSectionHeader ("FACTORY");
    for (int i = 0; i < (int) e.size(); ++i)
    {
        if (i == proc.presets.getNumFactory()) menu.addSectionHeader ("USER");
        menu.addItem (i + 1, e[(size_t) i].name, true, e[(size_t) i].name == cur);
    }
    if ((int) e.size() == proc.presets.getNumFactory()) { menu.addSectionHeader ("USER"); menu.addItem (-1, "(none yet - press SAVE)", false); }
    menu.addSeparator();
    menu.addItem (10000, "Open presets folder");
    juce::Component::SafePointer<XoxicoaFXAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName).withMinimumWidth (presetName.getWidth()),
                        [safe] (int r)
                        {
                            if (safe == nullptr || r == 0) return;
                            if (r == 10000)
                            {
                                auto dir = PresetManager::userDirectory();
                                dir.createDirectory();
                                dir.revealToUser();
                                return;
                            }
                            safe->proc.presets.load (r - 1);
                            safe->refreshPresetUi();
                        });
}

void XoxicoaFXAudioProcessorEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.isCurrentUserPreset() ? proc.presets.getCurrentName() : juce::String());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<XoxicoaFXAudioProcessorEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (safe == nullptr || r != 1) return;
        if (! safe->proc.presets.save (w->getTextEditorContents ("name")))
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Could not save")
                                              .withMessage ("Use a new name (factory preset names are taken).").withButton ("OK"), nullptr);
        safe->refreshPresetUi();
    }), true);
}

void XoxicoaFXAudioProcessorEditor::deletePresetDialog()
{
    if (! proc.presets.isCurrentUserPreset()) return;
    juce::Component::SafePointer<XoxicoaFXAudioProcessorEditor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withTitle ("Delete preset")
                                      .withMessage ("Delete \"" + proc.presets.getCurrentName() + "\"? This can't be undone.")
                                      .withButton ("Delete").withButton ("Cancel").withAssociatedComponent (this),
                                  [safe] (int r)
                                  {
                                      if (safe == nullptr || r != 1) return;
                                      safe->proc.presets.removeCurrent();
                                      safe->refreshPresetUi();
                                  });
}
