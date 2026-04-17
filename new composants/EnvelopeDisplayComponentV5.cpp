#include "EnvelopeDisplayComponentV5.h"

void EnvelopeDisplayComponentV5::updateFromADSR (float attack, float decay, float sustain, float release)
{
    a = juce::jlimit (0.01f, 1.0f, attack);
    d = juce::jlimit (0.01f, 1.0f, decay);
    s = juce::jlimit (0.0f, 1.0f, sustain);
    r = juce::jlimit (0.01f, 1.0f, release);
    repaint();
}

void EnvelopeDisplayComponentV5::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    UIThemeV5::fillRecess (g, area, 14.0f);

    g.setColour (juce::Colours::white.withAlpha (0.045f));
    for (int i = 1; i < 10; ++i)
        g.drawVerticalLine (juce::roundToInt (juce::jmap ((float) i / 10.0f, area.getX(), area.getRight())), area.getY(), area.getBottom());
    for (int i = 1; i < 6; ++i)
        g.drawHorizontalLine (juce::roundToInt (juce::jmap ((float) i / 6.0f, area.getY(), area.getBottom())), area.getX(), area.getRight());

    auto plot = area.reduced (21.0f, 18.0f);

    auto p0 = juce::Point<float> (plot.getX() + plot.getWidth() * 0.05f, plot.getBottom() - 8.0f);
    auto p1 = juce::Point<float> (plot.getX() + plot.getWidth() * (0.11f + a * 0.08f), plot.getY() + 8.0f);
    auto p2 = juce::Point<float> (plot.getX() + plot.getWidth() * (0.34f + d * 0.14f), plot.getY() + plot.getHeight() * (1.0f - s));
    auto p3 = juce::Point<float> (plot.getX() + plot.getWidth() * 0.83f, p2.y);
    auto p4 = juce::Point<float> (plot.getRight() - 7.0f, plot.getBottom() - 7.0f);

    juce::Path env;
    env.startNewSubPath (p0);
    env.lineTo (p1);
    env.cubicTo (p1.x + 21.0f, p1.y + 18.0f, p2.x - 26.0f, p2.y, p2.x, p2.y);
    env.lineTo (p3);
    env.cubicTo (p3.x + 17.0f, p3.y + 12.0f, p4.x - 17.0f, p4.y, p4.x, p4.y);

    juce::Path fill = env;
    fill.lineTo (p4.x, plot.getBottom());
    fill.lineTo (p0.x, plot.getBottom());
    fill.closeSubPath();

    juce::ColourGradient glow (UIThemeV5::accentGlow().withAlpha (0.74f), p1.x, p1.y,
                               juce::Colour::transparentBlack, plot.getCentreX(), plot.getBottom(), false);
    g.setGradientFill (glow);
    g.fillPath (fill);

    g.setColour (UIThemeV5::accentGlow().withAlpha (0.28f));
    g.strokePath (env, juce::PathStrokeType (8.5f));
    g.setColour (UIThemeV5::accent());
    g.strokePath (env, juce::PathStrokeType (2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (auto p : { p0, p1, p2, p3 })
    {
        g.setColour (juce::Colours::white.withAlpha (0.96f));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (p));
    }
}
