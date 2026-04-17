#pragma once
#include <JuceHeader.h>
#include "UIThemeV5.h"

class VUMeterComponentV5 : public juce::Component, private juce::Timer
{
public:
    VUMeterComponentV5();
    ~VUMeterComponentV5() override = default;

    void paint (juce::Graphics& g) override;
    void setLevels (float left, float right);

private:
    void timerCallback() override;

    float targetL = 0.46f, targetR = 0.39f;
    float displayL = 0.46f, displayR = 0.39f;
};
