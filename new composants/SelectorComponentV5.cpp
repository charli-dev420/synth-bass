#include "SelectorComponentV5.h"

SelectorComponentV5::SelectorComponentV5()
{
    addItem ("SINE", 1);
    addItem ("TRIANGLE", 2);
    addItem ("SAW", 3);
    setSelectedId (1);

    setColour (juce::ComboBox::textColourId, UIThemeV5::textMain());
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
}

void SelectorComponentV5::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    UIThemeV5::fillPanel (g, area, 10.0f);

    g.setColour (UIThemeV5::textMain());
    g.setFont (UIThemeV5::labelFont());
    g.drawText (getText(), getLocalBounds().reduced (14, 0), juce::Justification::centred);

    juce::Path arrow;
    float cx = (float) getWidth() - 18.0f;
    float cy = (float) getHeight() * 0.5f;
    arrow.startNewSubPath (cx - 5.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 3.0f);
    arrow.lineTo (cx + 5.0f, cy - 2.0f);

    g.setColour (UIThemeV5::textDim());
    g.strokePath (arrow, juce::PathStrokeType (1.8f));
}
