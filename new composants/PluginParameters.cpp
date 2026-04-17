#include "PluginParameters.h"
#include "ParameterIds.h"

juce::AudioProcessorValueTreeState::ParameterLayout PluginParameters::createParameterLayout()
{
    using APVTS = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParamIDs::gain, "Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParamIDs::volume, "Volume",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.62f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParamIDs::pan, "Pan",
        juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParamIDs::cutoff, "Cutoff",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 1200.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParamIDs::resonance, "Resonance",
        juce::NormalisableRange<float> (0.1f, 1.0f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParamIDs::attack, "Attack",
        juce::NormalisableRange<float> (0.001f, 5.0f, 0.0f, 0.35f), 0.05f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParamIDs::decay, "Decay",
        juce::NormalisableRange<float> (0.001f, 5.0f, 0.0f, 0.35f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParamIDs::sustain, "Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.48f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (ParamIDs::release, "Release",
        juce::NormalisableRange<float> (0.001f, 5.0f, 0.0f, 0.35f), 0.28f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (ParamIDs::enabled, "Enabled", true));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParamIDs::waveform, "Waveform",
        juce::StringArray { "Sine", "Triangle", "Saw" }, 0));

    return { params.begin(), params.end() };
}
