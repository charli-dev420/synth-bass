#include "PluginEditorContentV5.h"

PluginEditorContentV5::PluginEditorContentV5 (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    addAndMakeVisible (gainKnob);
    addAndMakeVisible (cutoffKnob);
    addAndMakeVisible (resonanceKnob);
    addAndMakeVisible (volumeFader);
    addAndMakeVisible (panFader);
    addAndMakeVisible (vuMeter);
    addAndMakeVisible (envelope);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (enabledButton);
    addAndMakeVisible (waveformSelector);

    gainAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::gain, gainKnob.getSlider());
    cutoffAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::cutoff, cutoffKnob.getSlider());
    resonanceAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::resonance, resonanceKnob.getSlider());
    volumeAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::volume, volumeFader.getSlider());
    panAttachment = std::make_unique<SliderAttachment> (apvts, ParamIDs::pan, panFader.getSlider());
    enabledAttachment = std::make_unique<ButtonAttachment> (apvts, ParamIDs::enabled, enabledButton);
    waveformAttachment = std::make_unique<ComboBoxAttachment> (apvts, ParamIDs::waveform, waveformSelector);

    startTimerHz (30);
}

void PluginEditorContentV5::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour::fromRGB (12, 15, 21), 0.0f, 0.0f,
                             juce::Colour::fromRGB (4, 6, 9), 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();
}

void PluginEditorContentV5::resized()
{
    gainKnob.setBounds       (20, 20, 170, 190);
    cutoffKnob.setBounds     (200, 20, 170, 190);
    resonanceKnob.setBounds  (380, 20, 170, 190);

    volumeFader.setBounds    (20, 230, 112, 232);
    panFader.setBounds       (140, 230, 112, 232);

    vuMeter.setBounds        (570, 20, 230, 320);
    envelope.setBounds       (820, 20, 360, 170);
    waveformSelector.setBounds (820, 210, 220, 34);
    enabledButton.setBounds  (1084, 202, 96, 72);
    outputMeter.setBounds    (270, 270, 530, 110);
}

void PluginEditorContentV5::timerCallback()
{
    syncEnvelopeFromState();

    // démo UI seulement: meter values simulés
    auto t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
    auto l = 0.35f + 0.20f * std::sin (t * 2.1f);
    auto r = 0.32f + 0.18f * std::sin (t * 2.7f + 0.8f);
    vuMeter.setLevels (juce::jlimit (0.0f, 1.0f, l), juce::jlimit (0.0f, 1.0f, r));

    auto out = 0.45f + 0.12f * std::sin (t * 1.8f + 1.3f);
    outputMeter.setValue (juce::jlimit (0.0f, 1.0f, out));
}

void PluginEditorContentV5::syncEnvelopeFromState()
{
    auto* a = apvts.getRawParameterValue (ParamIDs::attack);
    auto* d = apvts.getRawParameterValue (ParamIDs::decay);
    auto* s = apvts.getRawParameterValue (ParamIDs::sustain);
    auto* r = apvts.getRawParameterValue (ParamIDs::release);

    if (a == nullptr || d == nullptr || s == nullptr || r == nullptr)
        return;

    auto normAttack  = juce::jmap (a->load(), 0.001f, 5.0f, 0.02f, 1.0f);
    auto normDecay   = juce::jmap (d->load(), 0.001f, 5.0f, 0.02f, 1.0f);
    auto normSustain = juce::jlimit (0.0f, 1.0f, s->load());
    auto normRelease = juce::jmap (r->load(), 0.001f, 5.0f, 0.02f, 1.0f);

    envelope.updateFromADSR (normAttack, normDecay, normSustain, normRelease);
}
