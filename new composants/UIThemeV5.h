#pragma once
#include <JuceHeader.h>

namespace UIThemeV5
{
    static constexpr float panelRadius   = 18.0f;
    static constexpr float recessRadius  = 12.0f;

    inline juce::Colour bgTop()        { return juce::Colour::fromRGB (24, 28, 36); }
    inline juce::Colour bgMid()        { return juce::Colour::fromRGB (16, 20, 28); }
    inline juce::Colour bgBottom()     { return juce::Colour::fromRGB (10, 13, 19); }
    inline juce::Colour outlineHi()    { return juce::Colour::fromRGBA (255, 255, 255, 20); }
    inline juce::Colour outlineLo()    { return juce::Colour::fromRGBA (0, 0, 0, 110); }

    inline juce::Colour recessTop()    { return juce::Colour::fromRGB (13, 17, 25); }
    inline juce::Colour recessMid()    { return juce::Colour::fromRGB (8, 11, 17); }
    inline juce::Colour recessBottom() { return juce::Colour::fromRGB (5, 7, 12); }

    inline juce::Colour textMain()     { return juce::Colour::fromRGB (231, 236, 242); }
    inline juce::Colour textDim()      { return juce::Colour::fromRGB (118, 127, 141); }

    inline juce::Colour accent()       { return juce::Colour::fromRGB (219, 244, 255); }
    inline juce::Colour accentGlow()   { return juce::Colour::fromRGBA (196, 233, 255, 100); }

    inline juce::Colour metalHi()      { return juce::Colour::fromRGB (232, 236, 241); }
    inline juce::Colour metalUpper()   { return juce::Colour::fromRGB (176, 183, 191); }
    inline juce::Colour metalMid()     { return juce::Colour::fromRGB (126, 132, 141); }
    inline juce::Colour metalLo()      { return juce::Colour::fromRGB (66, 72, 81); }

    inline juce::Font labelFont()      { return juce::FontOptions ("Inter", 14.0f, juce::Font::plain); }
    inline juce::Font smallFont()      { return juce::FontOptions ("Inter", 11.0f, juce::Font::plain); }

    inline void fillPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = panelRadius)
    {
        juce::ColourGradient grad (bgTop(), r.getCentreX(), r.getY(),
                                   bgBottom(), r.getCentreX(), r.getBottom(), false);
        grad.addColour (0.48, bgMid());
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, radius);

        auto topSheen = r.withHeight (r.getHeight() * 0.22f);
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.025f), topSheen.getCentreX(), topSheen.getY(),
                                    juce::Colours::transparentWhite, topSheen.getCentreX(), topSheen.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (topSheen, radius);

        g.setColour (outlineLo());
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (outlineHi());
        g.drawRoundedRectangle (r.reduced (1.0f), radius - 0.4f, 0.6f);
    }

    inline void fillRecess (juce::Graphics& g, juce::Rectangle<float> r, float radius = recessRadius)
    {
        juce::ColourGradient grad (recessTop(), r.getCentreX(), r.getY(),
                                   recessBottom(), r.getCentreX(), r.getBottom(), false);
        grad.addColour (0.5, recessMid());
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, radius);

        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.drawRoundedRectangle (r, radius, 1.2f);

        g.setColour (juce::Colours::white.withAlpha (0.03f));
        g.drawRoundedRectangle (r.reduced (1.0f), radius - 1.0f, 0.6f);
    }

    inline void drawGlowStrip (juce::Graphics& g, juce::Rectangle<float> r, float radius = 2.5f, float alpha = 0.65f)
    {
        g.setColour (accentGlow().withAlpha (alpha));
        g.fillRoundedRectangle (r, radius);
    }
}
