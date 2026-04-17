#pragma once
#include <JuceHeader.h>
#include "UIThemeV5.h"

class OutputMeterComponentV5 : public juce::Component
{
public:
    OutputMeterComponentV5() = default;
    ~OutputMeterComponentV5() override = default;

    void paint (juce::Graphics& g) override;
    void setValue (float normalized);

private:
    float value = 0.5f;
};
