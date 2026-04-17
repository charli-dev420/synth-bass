#include "KnobComponentV5.h"

static void drawKnobTicksV5 (juce::Graphics& g, juce::Point<float> c, float radius, float value)
{
    const float start = juce::MathConstants<float>::pi * 1.19f;
    const float end   = juce::MathConstants<float>::pi * 2.81f;
    const int count = 29;

    for (int i = 0; i < count; ++i)
    {
        float t = (float) i / (float) (count - 1);
        float a = juce::jmap (t, start, end);
        auto p1 = c.getPointOnCircumference (radius * 1.05f, a);
        auto p2 = c.getPointOnCircumference (radius * 1.145f, a);

        bool active = i <= juce::roundToInt (value * (float) (count - 1));
        g.setColour (active ? UIThemeV5::accent().withAlpha (0.95f)
                            : juce::Colours::white.withAlpha (0.15f));
        g.drawLine ({ p1, p2 }, active ? 1.75f : 1.1f);
    }
}

KnobComponentV5::KnobComponentV5 (juce::String labelText, double min, double max, double value)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (min, max);
    slider.setValue (value);
    slider.setLookAndFeel (&lnf);

    label.setText (labelText, juce::dontSendNotification);
    label.setFont (UIThemeV5::labelFont());
    label.setColour (juce::Label::textColourId, UIThemeV5::textMain());
    label.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void KnobComponentV5::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromBottom (24));
    slider.setBounds (area.reduced (2));
}

void KnobComponentV5::LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                     float sliderPosProportional, float, float, juce::Slider&)
{
    auto full = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    auto area = full.reduced (9.0f, 8.0f);
    auto c = area.getCentre();
    auto radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;

    g.setColour (juce::Colours::black.withAlpha (0.34f));
    g.fillEllipse (area.translated (0.0f, 5.0f));

    auto ring = area;
    juce::ColourGradient ringGrad (UIThemeV5::metalHi(), ring.getX(), ring.getY(),
                                   UIThemeV5::metalLo(), ring.getRight(), ring.getBottom(), true);
    ringGrad.addColour (0.28, UIThemeV5::metalUpper());
    ringGrad.addColour (0.58, UIThemeV5::metalMid());
    g.setGradientFill (ringGrad);
    g.fillEllipse (ring);

    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawEllipse (ring, 1.0f);

    auto middle = ring.reduced (ring.getWidth() * 0.125f);
    juce::ColourGradient midGrad (juce::Colour::fromRGB (196, 202, 210), c.x, middle.getY(),
                                  juce::Colour::fromRGB (86, 92, 100), c.x, middle.getBottom(), false);
    g.setGradientFill (midGrad);
    g.fillEllipse (middle);

    auto face = middle.reduced (middle.getWidth() * 0.17f);
    juce::ColourGradient faceGrad (juce::Colour::fromRGB (237, 240, 245), c.x, face.getY(),
                                   juce::Colour::fromRGB (115, 120, 128), c.x, face.getBottom(), false);
    g.setGradientFill (faceGrad);
    g.fillEllipse (face);

    auto gloss = face.withTrimmedBottom (face.getHeight() * 0.55f).translated (-face.getWidth() * 0.08f, 0.0f);
    juce::ColourGradient glossGrad (juce::Colours::white.withAlpha (0.34f), gloss.getCentreX(), gloss.getY(),
                                    juce::Colours::transparentWhite, gloss.getCentreX(), gloss.getBottom(), false);
    g.setGradientFill (glossGrad);
    g.fillEllipse (gloss);

    drawKnobTicksV5 (g, c, radius, sliderPosProportional);

    const float start = juce::MathConstants<float>::pi * 1.19f;
    const float end   = juce::MathConstants<float>::pi * 2.81f;
    const float angle = juce::jmap (sliderPosProportional, start, end);

    juce::Path arc;
    arc.addCentredArc (c.x, c.y, radius * 1.01f, radius * 1.01f, 0.0f, start, angle, true);
    g.setColour (UIThemeV5::accentGlow().withAlpha (0.82f));
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto p1 = c.getPointOnCircumference (radius * 0.18f, angle);
    auto p2 = c.getPointOnCircumference (radius * 0.69f, angle);
    g.setColour (juce::Colour::fromRGB (246, 250, 255));
    g.drawLine ({ p1, p2 }, 2.9f);
}
