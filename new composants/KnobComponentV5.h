#pragma once
#include <JuceHeader.h>
#include "UIThemeV5.h"

class KnobComponentV5 : public juce::Component
{
public:
    KnobComponentV5 (juce::String labelText, double min = 0.0, double max = 1.0, double value = 0.5);
    ~KnobComponentV5() override = default;

    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;
    };

    LookAndFeel lnf;
    juce::Slider slider;
    juce::Label label;
};
