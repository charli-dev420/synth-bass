#pragma once
#include <JuceHeader.h>
#include "UIThemeV5.h"

class SelectorComponentV5 : public juce::ComboBox
{
public:
    SelectorComponentV5();
    ~SelectorComponentV5() override = default;

    void paint (juce::Graphics& g) override;
};
