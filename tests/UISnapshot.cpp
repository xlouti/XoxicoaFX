// Renders the real Xoxicoa FX editor to a PNG without opening a window.
// usage: XoxicoaFXUISnapshot out.png [scale]
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("XoxicoaFX_ui.png"));
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 2.0f;

    XoxicoaFXAudioProcessor proc;
    proc.presets.load (1);   // "Glokk Mosh" so the header shows a preset
    for (auto id : { "ghost_on", "phone_on", "mosh_on", "stutter_on", "pump_on" })
        proc.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
    proc.apvts.getParameter ("mosh_macro")->setValueNotifyingHost (0.55f);

    // Feed a little 808 so the meter and note readout show something real.
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer m;
    double ph = 0;
    for (int blk = 0; blk < 40; ++blk)
    {
        for (int i = 0; i < 512; ++i)
        {
            ph += 43.65 / 48000.0;
            const float v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * ph)
                          + 0.08f * (float) std::sin (juce::MathConstants<double>::twoPi * ph * 8.0);
            b.setSample (0, i, v); b.setSample (1, i, v * 0.95f);
        }
        proc.processBlock (b, m);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (400); // let the meter timer tick
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    const bool ok = os.openedOk() && png.writeImageToStream (img, os);
    std::printf ("%s %s (%dx%d)\n", ok ? "wrote" : "FAILED", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return ok ? 0 : 1;
}
