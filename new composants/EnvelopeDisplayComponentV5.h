#pragma once
#include <JuceHeader.h>
#include "UIThemeV5.h"

class EnvelopeDisplayComponentV5 : public juce::Component
{
public:
    EnvelopeDisplayComponentV5() = default;
    ~EnvelopeDisplayComponentV5() override = default;

    void paint (juce::Graphics& g) override;
    void updateFromADSR (float attack, float decay, float sustain, float release);

private:
    float a = 0.18f, d = 0.28f, s = 0.48f, r = 0.22f;
};
