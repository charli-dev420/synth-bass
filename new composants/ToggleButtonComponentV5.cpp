#include "ToggleButtonComponentV5.h"

ToggleButtonComponentV5::ToggleButtonComponentV5 (juce::String text) : juce::TextButton (text)
{
    setClickingTogglesState (true);
}

void ToggleButtonComponentV5::paintButton (juce::Graphics& g, bool, bool isButtonDown)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    bool on = getToggleState();

    juce::ColourGradient grad (UIThemeV5::bgTop().brighter (on ? 0.07f : 0.0f), area.getCentreX(), area.getY(),
                               UIThemeV5::bgBottom(), area.getCentreX(), area.getBottom(), false);
    grad.addColour (0.45, UIThemeV5::bgMid());
    g.setGradientFill (grad);
    g.fillRoundedRectangle (area, 10.0f);

    g.setColour (juce::Colours::black.withAlpha (0.56f));
    g.drawRoundedRectangle (area, 10.0f, 1.4f);

    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (area.reduced (1.0f), 9.0f, 0.7f);

    if (on)
    {
        auto glow = area.reduced (14.0f, 0.0f).removeFromBottom (5.0f);
        UIThemeV5::drawGlowStrip (g, glow, 2.5f, 0.96f);
    }

    g.setColour (UIThemeV5::textMain().withAlpha (isButtonDown ? 0.82f : 1.0f));
    g.setFont (UIThemeV5::labelFont());
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
}
