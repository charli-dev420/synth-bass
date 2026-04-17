#pragma once
#include <JuceHeader.h>
#include "UIThemeV5.h"

class FaderComponentV5 : public juce::Component
{
public:
    FaderComponentV5 (juce::String labelText, bool bipolar = false);
    ~FaderComponentV5() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               const juce::Slider::SliderStyle, juce::Slider&) override;
    };

    LookAndFeel lnf;
    juce::Slider slider;
    juce::Label label;
    bool bipolarMode = false;
};
