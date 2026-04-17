#include "VUMeterComponentV5.h"

VUMeterComponentV5::VUMeterComponentV5() { startTimerHz (60); }

void VUMeterComponentV5::setLevels (float left, float right)
{
    targetL = juce::jlimit (0.0f, 1.0f, left);
    targetR = juce::jlimit (0.0f, 1.0f, right);
}

void VUMeterComponentV5::timerCallback()
{
    displayL = juce::jmax (targetL, displayL * 0.92f);
    displayR = juce::jmax (targetR, displayR * 0.92f);
    repaint();
}

void VUMeterComponentV5::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    UIThemeV5::fillPanel (g, area, 18.0f);

    auto inner = area.reduced (24.0f, 18.0f);
    UIThemeV5::fillRecess (g, inner, 15.0f);

    auto left = inner.removeFromLeft (inner.getWidth() * 0.43f).reduced (18.0f, 16.0f);
    auto right = inner.reduced (18.0f, 16.0f);

    auto drawCol = [&] (juce::Rectangle<float> r, float value, const juce::String& name)
    {
        g.setColour (UIThemeV5::textMain());
        g.setFont (UIThemeV5::labelFont());
        g.drawText (name, r.removeFromTop (18.0f).toNearestInt(), juce::Justification::centred);

        const int segs = 13;
        const float gap = 4.0f;
        const float segH = (r.getHeight() - gap * (segs - 1)) / segs;
        const int lit = juce::roundToInt (value * segs);

        for (int i = 0; i < segs; ++i)
        {
            float y = r.getBottom() - (i + 1) * segH - i * gap;
            auto seg = juce::Rectangle<float> (r.getX(), y, r.getWidth() * 0.50f, segH);
            bool active = i < lit;

            g.setColour (active ? UIThemeV5::accent() : juce::Colour::fromRGB (51, 59, 71));
            g.fillRoundedRectangle (seg, 2.0f);

            if (active)
            {
                g.setColour (UIThemeV5::accentGlow().withAlpha (0.55f));
                g.drawRoundedRectangle (seg, 2.0f, 0.8f);
            }
        }

        static constexpr int dbs[] = { 0, -6, -12, -24, -36, -60 };
        for (int db : dbs)
        {
            float t = juce::jmap ((float) db, -60.0f, 0.0f, 1.0f, 0.0f);
            int yy = juce::roundToInt (juce::jmap (t, r.getY(), r.getBottom()));
            g.setColour (UIThemeV5::textDim());
            g.setFont (UIThemeV5::smallFont());
            g.drawText (juce::String (db), (int) (r.getRight() - 3), yy - 7, 26, 14, juce::Justification::left);
        }
    };

    drawCol (left, displayL, "L");
    drawCol (right, displayR, "R");
}
