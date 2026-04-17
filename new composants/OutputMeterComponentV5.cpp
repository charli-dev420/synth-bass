#include "OutputMeterComponentV5.h"

void OutputMeterComponentV5::setValue (float normalized)
{
    value = juce::jlimit (0.0f, 1.0f, normalized);
    repaint();
}

void OutputMeterComponentV5::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    UIThemeV5::fillPanel (g, area, 18.0f);

    auto inner = area.reduced (16.0f, 16.0f);
    UIThemeV5::fillRecess (g, inner, 13.0f);

    auto ruler = inner.reduced (22.0f, 18.0f);
    auto header = ruler.removeFromTop (17.0f);

    static constexpr int dbMarks[] = { -60, -36, -24, -12, -6, 0 };
    for (int db : dbMarks)
    {
        float t = juce::jmap ((float) db, -60.0f, 0.0f, 0.0f, 1.0f);
        float x = juce::jmap (t, ruler.getX(), ruler.getRight());
        g.setColour (UIThemeV5::textMain());
        g.setFont (UIThemeV5::smallFont());
        g.drawText (juce::String (db), (int) x - 14, (int) header.getY(), 28, 14, juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawVerticalLine ((int) x, ruler.getY() + 2.0f, ruler.getBottom());
    }

    auto bars = ruler.withTrimmedTop (18.0f).withHeight (18.0f);
    const int segs = 22;
    const float gap = 4.0f;
    const float segW = (bars.getWidth() - gap * (segs - 1)) / segs;
    const int lit = juce::roundToInt (value * segs);

    for (int i = 0; i < segs; ++i)
    {
        auto seg = juce::Rectangle<float> (bars.getX() + i * (segW + gap), bars.getY(), segW, bars.getHeight());
        bool active = i < lit;
        g.setColour (active ? UIThemeV5::accent() : juce::Colour::fromRGB (34, 41, 52));
        g.fillRoundedRectangle (seg, 2.0f);

        if (active)
        {
            g.setColour (UIThemeV5::accentGlow().withAlpha (0.54f));
            g.drawRoundedRectangle (seg, 2.0f, 0.8f);
        }
    }
}
