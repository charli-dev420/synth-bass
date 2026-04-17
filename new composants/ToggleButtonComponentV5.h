#pragma once
#include <JuceHeader.h>
#include "UIThemeV5.h"

class ToggleButtonComponentV5 : public juce::TextButton
{
public:
    explicit ToggleButtonComponentV5 (juce::String text);
    ~ToggleButtonComponentV5() override = default;

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
};
