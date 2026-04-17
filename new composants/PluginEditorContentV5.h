#pragma once
#include <JuceHeader.h>
#include "AttachmentHelpers.h"
#include "KnobComponentV5.h"
#include "FaderComponentV5.h"
#include "VUMeterComponentV5.h"
#include "EnvelopeDisplayComponentV5.h"
#include "OutputMeterComponentV5.h"
#include "ToggleButtonComponentV5.h"
#include "SelectorComponentV5.h"
#include "ParameterIds.h"

class PluginEditorContentV5 : public juce::Component,
                              private juce::Timer
{
public:
    explicit PluginEditorContentV5 (juce::AudioProcessorValueTreeState& state);
    ~PluginEditorContentV5() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void syncEnvelopeFromState();

    juce::AudioProcessorValueTreeState& apvts;

    KnobComponentV5 gainKnob { "GAIN" };
    KnobComponentV5 cutoffKnob { "CUTOFF", 20.0, 20000.0, 1200.0 };
    KnobComponentV5 resonanceKnob { "RESONANCE", 0.1, 1.0, 0.3 };

    FaderComponentV5 volumeFader { "VOLUME" };
    FaderComponentV5 panFader { "PAN", true };

    VUMeterComponentV5 vuMeter;
    EnvelopeDisplayComponentV5 envelope;
    OutputMeterComponentV5 outputMeter;
    ToggleButtonComponentV5 enabledButton { "ON" };
    SelectorComponentV5 waveformSelector;

    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> cutoffAttachment;
    std::unique_ptr<SliderAttachment> resonanceAttachment;
    std::unique_ptr<SliderAttachment> volumeAttachment;
    std::unique_ptr<SliderAttachment> panAttachment;
    std::unique_ptr<ButtonAttachment> enabledAttachment;
    std::unique_ptr<ComboBoxAttachment> waveformAttachment;
};
