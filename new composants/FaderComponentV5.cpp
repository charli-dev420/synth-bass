#include "FaderComponentV5.h"

FaderComponentV5::FaderComponentV5 (juce::String labelText, bool bipolar) : bipolarMode (bipolar)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (bipolar ? -1.0 : 0.0, 1.0);
    slider.setValue (bipolar ? 0.0 : 0.62);
    slider.setLookAndFeel (&lnf);

    label.setText (labelText, juce::dontSendNotification);
    label.setFont (UIThemeV5::labelFont());
    label.setColour (juce::Label::textColourId, UIThemeV5::textMain());
    label.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void FaderComponentV5::paint (juce::Graphics& g)
{
    if (! bipolarMode)
        return;

    auto r = slider.getBounds();
    int y = r.getCentreY();
    g.setColour (UIThemeV5::textDim());
    g.setFont (UIThemeV5::smallFont());
    g.drawText ("L", r.getX() - 12, y - 8, 12, 16, juce::Justification::centred);
    g.drawText ("R", r.getRight(), y - 8, 12, 16, juce::Justification::centred);
}

void FaderComponentV5::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromBottom (24));
    slider.setBounds (area.reduced (16, 2));
}

void FaderComponentV5::LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                      float sliderPos, float, float,
                                                      const juce::Slider::SliderStyle, juce::Slider&)
{
    auto a = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    auto slot = juce::Rectangle<float> (a.getCentreX() - width * 0.115f, a.getY() + 4.0f, width * 0.23f, a.getHeight() - 8.0f);
    UIThemeV5::fillRecess (g, slot, 7.0f);

    for (int i = 0; i <= 20; ++i)
    {
        float yy = juce::jmap ((float) i / 20.0f, slot.getY() + 7.0f, slot.getBottom() - 7.0f);
        float alpha = (i % 5 == 0) ? 0.15f : ((i % 2 == 0) ? 0.08f : 0.05f);
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.drawLine (a.getX() + 8.0f, yy, a.getRight() - 8.0f, yy, 1.0f);
    }

    auto thumb = juce::Rectangle<float> (a.getX() + width * 0.18f, sliderPos - 21.5f, width * 0.64f, 43.0f);
    g.setColour (juce::Colours::black.withAlpha (0.36f));
    g.fillRoundedRectangle (thumb.translated (0.0f, 3.0f), 6.5f);

    juce::ColourGradient tgrad (UIThemeV5::metalHi(), thumb.getCentreX(), thumb.getY(),
                                UIThemeV5::metalMid(), thumb.getCentreX(), thumb.getBottom(), false);
    tgrad.addColour (0.33, juce::Colour::fromRGB (205, 210, 217));
    tgrad.addColour (0.58, juce::Colour::fromRGB (151, 157, 166));
    g.setGradientFill (tgrad);
    g.fillRoundedRectangle (thumb, 6.5f);

    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.drawRoundedRectangle (thumb, 6.5f, 1.0f);

    auto topGloss = thumb.withTrimmedBottom (thumb.getHeight() * 0.58f).reduced (4.0f, 2.0f);
    juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.22f), topGloss.getCentreX(), topGloss.getY(),
                                juce::Colours::transparentWhite, topGloss.getCentreX(), topGloss.getBottom(), false);
    g.setGradientFill (gloss);
    g.fillRoundedRectangle (topGloss, 4.0f);

    auto strip = juce::Rectangle<float> (thumb.getX() + 4.5f, thumb.getY() + thumb.getHeight() * 0.49f,
                                         thumb.getWidth() - 9.0f, 5.0f);
    UIThemeV5::drawGlowStrip (g, strip, 2.5f, 0.94f);

    g.setColour (juce::Colours::white.withAlpha (0.42f));
    g.drawLine (thumb.getX() + thumb.getWidth() * 0.73f, thumb.getY() + 7.5f,
                thumb.getX() + thumb.getWidth() * 0.73f, thumb.getBottom() - 7.5f, 1.6f);
}
