#include "SynthCommon.h"
#include "PresetManifest.h"
#include <cmath>
#include <map>

namespace
{
constexpr float kPanelRadius   = synthRadius::panel;
constexpr float kRecessRadius  = 9.0f;
constexpr float kCardRadius    = synthRadius::card;
constexpr float kTabRadius     = synthRadius::tab;
constexpr float kButtonRadius  = synthRadius::button;
constexpr float kShadowLight   = synthShadow::light;
constexpr float kShadowDeep    = synthShadow::deep;
constexpr float kGlowNormal    = synthGlow::normal;
constexpr float kGlowSelected  = synthGlow::selected;
constexpr float kGlowHover     = synthGlow::hover;

juce::Colour makeAccentGlow(juce::Colour accent, float alpha)
{
    return accent.brighter(0.55f).withAlpha(alpha);
}

void fillV5Panel(juce::Graphics& g,
                 juce::Rectangle<float> area,
                 float radius,
                 juce::Colour top,
                 juce::Colour mid,
                 juce::Colour bottom)
{
    juce::ColourGradient grad(top, area.getCentreX(), area.getY(),
                              bottom, area.getCentreX(), area.getBottom(), false);
    grad.addColour(0.48, mid);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(area, radius);

    auto topSheen = area.withHeight(area.getHeight() * 0.20f);
    juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.032f), topSheen.getCentreX(), topSheen.getY(),
                               juce::Colours::transparentWhite, topSheen.getCentreX(), topSheen.getBottom(), false);
    g.setGradientFill(sheen);
    g.fillRoundedRectangle(topSheen, radius);

    g.setColour(juce::Colours::black.withAlpha(0.50f));
    g.drawRoundedRectangle(area.reduced(0.5f), radius, 1.15f);
    g.setColour(juce::Colours::white.withAlpha(0.038f));
    g.drawRoundedRectangle(area.reduced(1.1f), juce::jmax(0.0f, radius - 0.6f), 0.65f);
}

void fillSharedPanel(juce::Graphics& g,
                     juce::Rectangle<float> area,
                     float radius,
                     juce::Colour base,
                     bool lifted)
{
    if (base.getFloatAlpha() >= 0.999f)
        base = base.withAlpha(lifted ? 0.72f : 0.62f);

    auto top = base.brighter(lifted ? 0.09f : 0.05f);
    auto mid = base;
    auto bottom = base.darker(lifted ? 0.14f : 0.08f);
    fillV5Panel(g, area, radius, top, mid, bottom);
}

void fillSharedRecess(juce::Graphics& g,
                      juce::Rectangle<float> area,
                      float radius)
{
    auto top = juce::Colour(0xff313742).withAlpha(0.92f);
    auto mid = juce::Colour(0xff262B34).withAlpha(0.96f);
    auto bottom = juce::Colour(0xff1C2028).withAlpha(0.98f);
    fillV5Panel(g, area, radius, top, mid, bottom);

    auto shadowArea = area.reduced(1.0f);
    juce::ColourGradient shadow(juce::Colours::black.withAlpha(0.07f), shadowArea.getCentreX(), shadowArea.getY(),
                                juce::Colours::transparentBlack, shadowArea.getCentreX(), shadowArea.getY() + shadowArea.getHeight() * 0.42f, false);
    g.setGradientFill(shadow);
    g.fillRoundedRectangle(shadowArea, juce::jmax(0.0f, radius - 1.0f));
}

void drawGlowStrip(juce::Graphics& g,
                   juce::Rectangle<float> area,
                   juce::Colour accent,
                   float radius,
                   float alpha)
{
    auto glowArea = area.expanded(1.0f, 0.7f);
    g.setColour(makeAccentGlow(accent, alpha * 0.28f));
    g.fillRoundedRectangle(glowArea, radius + 0.6f);
    g.setColour(makeAccentGlow(accent, alpha * 0.72f));
    g.fillRoundedRectangle(area, radius);
}

void drawMetalEllipse(juce::Graphics& g,
                      juce::Rectangle<float> area,
                      juce::Colour hi,
                      juce::Colour upper,
                      juce::Colour mid,
                      juce::Colour lo)
{
    juce::ColourGradient grad(hi, area.getX(), area.getY(),
                              lo, area.getRight(), area.getBottom(), true);
    grad.addColour(0.30, upper);
    grad.addColour(0.58, mid);
    g.setGradientFill(grad);
    g.fillEllipse(area);
}

struct KnobRenderMetrics
{
    int dotCount = 0;
    float dotOrbit = 1.12f;
    float dotRadius = 1.5f;
    float collarRadius = 0.84f;
    float capRadius = 0.69f;
    float hubRadius = 0.23f;
    float pointerStart = 0.22f;
    float pointerEnd = 0.78f;
    float pointerWidth = 2.0f;
    float arcRadius = 1.05f;
    float arcWidth = 2.4f;
    float pitScale = 0.30f;
};

KnobRenderMetrics getKnobRenderMetrics(float diameter)
{
    if (diameter >= 96.0f)
        return { 19, 1.11f, 1.45f, 0.89f, 0.77f, 0.21f, 0.28f, 0.81f, 2.7f, 1.01f, 2.2f, 0.24f };

    if (diameter >= 72.0f)
        return { 17, 1.10f, 1.28f, 0.87f, 0.75f, 0.20f, 0.27f, 0.80f, 2.4f, 1.00f, 1.95f, 0.22f };

    return { 15, 1.08f, 1.02f, 0.85f, 0.72f, 0.19f, 0.25f, 0.77f, 1.9f, 0.99f, 1.7f, 0.20f };
}

void addSoftBloom(juce::Graphics& g,
                  juce::Point<float> centre,
                  float radiusX,
                  float radiusY,
                  juce::Colour colour)
{
    juce::ColourGradient bloom(colour, centre.x, centre.y,
                               juce::Colours::transparentBlack, centre.x + radiusX, centre.y + radiusY, true);
    g.setGradientFill(bloom);
    g.fillEllipse(centre.x - radiusX, centre.y - radiusY, radiusX * 2.0f, radiusY * 2.0f);
}

juce::Colour blendTint(juce::Colour base, juce::Colour tint, float amount)
{
    if (tint.isTransparent())
        return base;

    return base.interpolatedWith(tint.withAlpha(1.0f), juce::jlimit(0.0f, 1.0f, amount));
}

void paintLedStrip(juce::Graphics& g,
                   juce::Rectangle<float> area,
                   juce::Colour accent,
                   float glowAlpha,
                   float coreAlpha)
{
    auto glowArea = area.expanded(4.0f, 2.0f);
    juce::ColourGradient glow(accent.withAlpha(glowAlpha), glowArea.getCentreX(), glowArea.getCentreY(),
                              juce::Colours::transparentBlack, glowArea.getCentreX(), glowArea.getBottom(), true);
    g.setGradientFill(glow);
    g.fillRoundedRectangle(glowArea, glowArea.getHeight() * 0.5f);

    juce::ColourGradient core(juce::Colours::white.withAlpha(coreAlpha), area.getCentreX(), area.getY(),
                              accent.withAlpha(juce::jlimit(0.0f, 1.0f, coreAlpha + 0.14f)), area.getCentreX(), area.getBottom(), false);
    g.setGradientFill(core);
    g.fillRoundedRectangle(area, area.getHeight() * 0.5f);

    g.setColour(juce::Colours::white.withAlpha(coreAlpha + 0.12f));
    g.drawRoundedRectangle(area.reduced(0.2f), area.getHeight() * 0.5f, 0.7f);
}

void fillPanelBackdrop(juce::Graphics& g,
                       juce::Rectangle<float> area,
                       float radius,
                       juce::Colour accent,
                       bool elevated,
                       juce::Colour tint = juce::Colours::transparentBlack)
{
    auto shellTop = blendTint(juce::Colour(0xff2F353D), tint, elevated ? 0.20f : 0.14f).withAlpha(elevated ? 0.970f : 0.948f);
    auto shellUpper = blendTint(juce::Colour(0xff242930), tint, elevated ? 0.18f : 0.12f).withAlpha(0.989f);
    auto shellMid = blendTint(juce::Colour(0xff14181D), tint, elevated ? 0.14f : 0.09f).withAlpha(0.994f);
    auto shellBottom = blendTint(juce::Colour(0xff090B0E), tint, elevated ? 0.10f : 0.06f).withAlpha(0.998f);

    juce::ColourGradient shell(shellTop, area.getX(), area.getY(),
                               shellBottom, area.getRight(), area.getBottom(), false);
    shell.addColour(0.18, shellUpper);
    shell.addColour(0.52, shellMid);
    shell.addColour(0.80, shellMid.darker(0.10f));
    g.setGradientFill(shell);
    g.fillRoundedRectangle(area, radius);

    {
        juce::Graphics::ScopedSaveState scopedState(g);
        auto face = area.reduced(2.2f, 2.0f);
        g.reduceClipRegion(face.toNearestInt());
        addSoftBloom(g,
                     { face.getX() + face.getWidth() * 0.24f, face.getY() + face.getHeight() * 0.18f },
                     face.getWidth() * 0.34f, face.getHeight() * 0.17f,
                     juce::Colours::white.withAlpha(0.022f));
        addSoftBloom(g,
                     { face.getX() + face.getWidth() * 0.77f, face.getY() + face.getHeight() * 0.30f },
                     face.getWidth() * 0.26f, face.getHeight() * 0.16f,
                     juce::Colours::white.withAlpha(0.014f));
        addSoftBloom(g,
                     { face.getCentreX(), face.getBottom() - face.getHeight() * 0.06f },
                     face.getWidth() * 0.42f, face.getHeight() * 0.16f,
                     juce::Colours::black.withAlpha(0.12f));

        if (! tint.isTransparent())
        {
            addSoftBloom(g,
                         { face.getX() + face.getWidth() * 0.68f, face.getY() + face.getHeight() * 0.24f },
                         face.getWidth() * 0.34f, face.getHeight() * 0.20f,
                         tint.withAlpha(elevated ? 0.050f : 0.035f));
        }

        auto topTexture = face.reduced(face.getWidth() * 0.10f, 0.0f).removeFromTop(juce::jlimit(10.0f, 18.0f, face.getHeight() * 0.12f));
        juce::ColourGradient topTextureGrad(juce::Colours::white.withAlpha(0.020f), topTexture.getCentreX(), topTexture.getY(),
                                            juce::Colours::transparentWhite, topTexture.getCentreX(), topTexture.getBottom(), false);
        g.setGradientFill(topTextureGrad);
        g.fillRoundedRectangle(topTexture, juce::jmax(0.0f, radius - 5.0f));
    }

    auto topBevel = area.reduced(5.0f, 4.0f).removeFromTop(juce::jlimit(7.0f, 14.0f, area.getHeight() * 0.12f));
    juce::ColourGradient topBevelGrad(juce::Colours::white.withAlpha(0.068f), topBevel.getCentreX(), topBevel.getY(),
                                      juce::Colours::transparentWhite, topBevel.getCentreX(), topBevel.getBottom(), false);
    g.setGradientFill(topBevelGrad);
    g.fillRoundedRectangle(topBevel, juce::jmax(0.0f, radius - 4.0f));

    auto lowerMass = area.reduced(area.getWidth() * 0.08f, 0.0f).removeFromBottom(juce::jlimit(16.0f, 30.0f, area.getHeight() * 0.20f));
    juce::ColourGradient lowerMassGrad(juce::Colours::transparentBlack,
                                       lowerMass.getCentreX(), lowerMass.getY(),
                                       juce::Colours::black.withAlpha(elevated ? 0.16f : 0.13f),
                                       lowerMass.getCentreX(), lowerMass.getBottom(), false);
    g.setGradientFill(lowerMassGrad);
    g.fillRoundedRectangle(lowerMass, juce::jmax(0.0f, radius - 4.0f));

    auto sideWallW = juce::jlimit(7.0f, 14.0f, area.getWidth() * 0.035f);
    auto leftWall = juce::Rectangle<float>(area.getX() + 3.0f, area.getY() + 6.0f, sideWallW, area.getHeight() - 12.0f);
    juce::ColourGradient leftWallGrad(juce::Colours::black.withAlpha(0.12f), leftWall.getX(), leftWall.getCentreY(),
                                      juce::Colours::transparentBlack, leftWall.getRight(), leftWall.getCentreY(), false);
    g.setGradientFill(leftWallGrad);
    g.fillRoundedRectangle(leftWall, juce::jmax(0.0f, radius - 5.0f));

    auto rightWall = juce::Rectangle<float>(area.getRight() - sideWallW - 3.0f, area.getY() + 7.0f, sideWallW, area.getHeight() - 14.0f);
    juce::ColourGradient rightWallGrad(juce::Colours::transparentBlack, rightWall.getX(), rightWall.getCentreY(),
                                       juce::Colours::black.withAlpha(0.09f), rightWall.getRight(), rightWall.getCentreY(), false);
    g.setGradientFill(rightWallGrad);
    g.fillRoundedRectangle(rightWall, juce::jmax(0.0f, radius - 5.0f));

    auto accentTrace = area.reduced(16.0f, 0.0f).removeFromBottom(1.8f);
    const auto traceColour = tint.isTransparent() ? accent : accent.interpolatedWith(tint, 0.45f);
    juce::ColourGradient accentTraceGrad(traceColour.withAlpha(elevated ? 0.042f : 0.025f), accentTrace.getX(), accentTrace.getCentreY(),
                                         juce::Colours::transparentBlack, accentTrace.getRight(), accentTrace.getCentreY(), false);
    g.setGradientFill(accentTraceGrad);
    g.fillRoundedRectangle(accentTrace, 1.2f);

    g.setColour(juce::Colours::white.withAlpha(elevated ? 0.058f : 0.038f));
    g.drawRoundedRectangle(area.reduced(1.25f), juce::jmax(0.0f, radius - 0.8f), 0.7f);
    g.setColour(juce::Colours::black.withAlpha(elevated ? 0.54f : 0.48f));
    g.drawRoundedRectangle(area.reduced(0.5f), radius, 1.0f);
}

void fillPanelCavity(juce::Graphics& g,
                     juce::Rectangle<float> area,
                     float radius,
                     juce::Colour accent,
                     juce::Colour tint = juce::Colours::transparentBlack)
{
    auto shell = area;
    auto top = blendTint(juce::Colour(0xff282E35), tint, 0.18f).withAlpha(0.976f);
    auto mid = blendTint(juce::Colour(0xff20252B), tint, 0.14f).withAlpha(0.987f);
    auto bottom = blendTint(juce::Colour(0xff171B20), tint, 0.08f).withAlpha(0.994f);
    juce::ColourGradient cavity(top, shell.getCentreX(), shell.getY(),
                                bottom, shell.getCentreX(), shell.getBottom(), false);
    cavity.addColour(0.24, top.brighter(0.02f));
    cavity.addColour(0.58, mid);
    cavity.addColour(0.82, bottom.darker(0.06f));
    g.setGradientFill(cavity);
    g.fillRoundedRectangle(shell, radius);

    auto lip = shell.reduced(1.2f);
    juce::ColourGradient lipShadow(juce::Colours::black.withAlpha(0.08f), lip.getCentreX(), lip.getY(),
                                   juce::Colours::transparentBlack, lip.getCentreX(), lip.getY() + lip.getHeight() * 0.42f, false);
    g.setGradientFill(lipShadow);
    g.fillRoundedRectangle(lip, juce::jmax(2.0f, radius - 1.0f));

    auto lipHighlight = shell.reduced(3.5f, 3.0f).removeFromTop(juce::jlimit(6.0f, 12.0f, shell.getHeight() * 0.11f));
    juce::ColourGradient lipHighlightGrad(juce::Colours::white.withAlpha(0.048f), lipHighlight.getCentreX(), lipHighlight.getY(),
                                          juce::Colours::transparentWhite, lipHighlight.getCentreX(), lipHighlight.getBottom(), false);
    g.setGradientFill(lipHighlightGrad);
    g.fillRoundedRectangle(lipHighlight, juce::jmax(2.0f, radius - 4.0f));

    g.setColour(juce::Colours::white.withAlpha(0.028f));
    g.drawRoundedRectangle(shell.reduced(1.15f), juce::jmax(2.0f, radius - 0.8f), 0.65f);

    auto floor = shell.reduced(3.0f);
    const auto cavityGlow = tint.isTransparent() ? accent : accent.interpolatedWith(tint, 0.38f);
    juce::ColourGradient floorGlow(cavityGlow.withAlpha(0.014f), floor.getCentreX(), floor.getY(),
                                   juce::Colours::transparentBlack, floor.getCentreX(), floor.getBottom(), false);
    g.setGradientFill(floorGlow);
    g.fillRoundedRectangle(floor, juce::jmax(2.0f, radius - 2.5f));

    auto core = shell.reduced(4.0f, 4.0f);
    juce::ColourGradient coreLift(juce::Colours::white.withAlpha(0.005f), core.getCentreX(), core.getY(),
                                  juce::Colours::transparentWhite, core.getCentreX(), core.getCentreY(), false);
    g.setGradientFill(coreLift);
    g.fillRoundedRectangle(core, juce::jmax(2.0f, radius - 3.0f));

    {
        juce::Graphics::ScopedSaveState scopedState(g);
        auto brushedBed = core.reduced(4.0f, 3.0f);
        g.reduceClipRegion(brushedBed.toNearestInt());
        addSoftBloom(g,
                 { brushedBed.getX() + brushedBed.getWidth() * 0.32f, brushedBed.getY() + brushedBed.getHeight() * 0.18f },
                 brushedBed.getWidth() * 0.25f, brushedBed.getHeight() * 0.15f,
                 juce::Colours::white.withAlpha(0.012f));
        addSoftBloom(g,
                 { brushedBed.getX() + brushedBed.getWidth() * 0.72f, brushedBed.getY() + brushedBed.getHeight() * 0.26f },
                 brushedBed.getWidth() * 0.20f, brushedBed.getHeight() * 0.17f,
                 juce::Colours::white.withAlpha(0.008f));
        addSoftBloom(g,
                     { brushedBed.getCentreX(), brushedBed.getBottom() - brushedBed.getHeight() * 0.08f },
                     brushedBed.getWidth() * 0.36f, brushedBed.getHeight() * 0.12f,
                     juce::Colours::black.withAlpha(0.07f));
    }

    auto floorCompression = core.reduced(8.0f, 0.0f).removeFromBottom(juce::jlimit(8.0f, 16.0f, core.getHeight() * 0.14f));
    juce::ColourGradient floorCompressionGrad(juce::Colours::transparentBlack,
                                              floorCompression.getCentreX(), floorCompression.getY(),
                                              juce::Colours::black.withAlpha(0.09f),
                                              floorCompression.getCentreX(), floorCompression.getBottom(), false);
    g.setGradientFill(floorCompressionGrad);
    g.fillRoundedRectangle(floorCompression, juce::jmax(2.0f, radius - 4.0f));

    g.setColour(juce::Colours::black.withAlpha(0.38f));
    g.drawRoundedRectangle(shell.reduced(0.5f), radius, 0.95f);
    g.setColour(juce::Colours::white.withAlpha(0.018f));
    g.drawRoundedRectangle(shell.reduced(1.4f), juce::jmax(2.0f, radius - 0.9f), 0.6f);
}
}

// =============================================================================
// SynthLookAndFeel
// =============================================================================
SynthLookAndFeel::SynthLookAndFeel(juce::Colour accent)
{
    setAccent(accent);
}

void SynthLookAndFeel::setAccent(juce::Colour accent)
{
    accent_ = accent;
    setColour(juce::Slider::thumbColourId,               synthcol::text);
    setColour(juce::Slider::rotarySliderFillColourId,    accent_);
    setColour(juce::Slider::rotarySliderOutlineColourId, synthcol::border);
    setColour(juce::Slider::textBoxTextColourId,         synthcol::textSec.withAlpha(0.96f));
    setColour(juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId,        synthcol::surface);
    setColour(juce::ComboBox::outlineColourId,           synthcol::ink);
    setColour(juce::ComboBox::textColourId,              synthcol::text);
    setColour(juce::ComboBox::arrowColourId,             accent_.brighter(0.20f));
    setColour(juce::PopupMenu::backgroundColourId,       synthcol::surface);
    setColour(juce::PopupMenu::textColourId,             synthcol::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent_.withAlpha(0.18f).interpolatedWith(synthcol::surfHi, 0.72f));
    setColour(juce::PopupMenu::highlightedTextColourId,  synthcol::text);
    setColour(juce::Label::textColourId,                 synthcol::textSec.withAlpha(0.88f));
    setColour(juce::ToggleButton::textColourId,          synthcol::text);
    setColour(juce::ToggleButton::tickColourId,          accent_);
    setColour(juce::TextEditor::backgroundColourId,      synthcol::surfHi);
    setColour(juce::TextEditor::outlineColourId,         synthcol::ink);
    setColour(juce::TextEditor::textColourId,            synthcol::text);
    setColour(juce::TextButton::buttonColourId,          synthcol::surface);
    setColour(juce::TextButton::buttonOnColourId,        accent_.withAlpha(0.22f).interpolatedWith(synthcol::surfHi, 0.62f));
    setColour(juce::TextButton::textColourOffId,         synthcol::text);
    setColour(juce::TextButton::textColourOnId,          synthcol::text);
    setColour(knobGlowColourId,                          juce::Colours::transparentBlack);
    setColour(knobBezelColourId,                         juce::Colours::transparentBlack);
    setColour(knobCollarColourId,                        juce::Colours::transparentBlack);
    setColour(knobCapAccentColourId,                     juce::Colours::transparentBlack);
}

void SynthLookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPos, float startAngle, float endAngle,
    juce::Slider& slider)
{
    auto fullBounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));
    const float boundsSize = static_cast<float>(juce::jmin(width, height));
    const auto spec = getKnobRenderMetrics(boundsSize);
    bool isGrand = boundsSize >= 96.0f;
    auto area = fullBounds.reduced(isGrand ? 4.4f : 3.4f, isGrand ? 3.8f : 2.8f);
    float diameter = juce::jmin(area.getWidth(), area.getHeight());
    auto dial = juce::Rectangle<float>(area.getCentreX() - diameter * 0.5f,
                                       area.getCentreY() - diameter * 0.5f,
                                       diameter, diameter);
    auto c = dial.getCentre();
    auto fill = slider.findColour(juce::Slider::rotarySliderFillColourId);
    auto glowTint = slider.findColour(knobGlowColourId, true);
    auto bezelTint = slider.findColour(knobBezelColourId, true);
    auto collarTint = slider.findColour(knobCollarColourId, true);
    auto capTint = slider.findColour(knobCapAccentColourId, true);
    const bool hasGlowTint = !glowTint.isTransparent();
    const bool hasBezelTint = !bezelTint.isTransparent();
    const bool hasCollarTint = !collarTint.isTransparent();
    const bool hasCapTint = !capTint.isTransparent();
    auto glowSource = hasGlowTint ? fill.interpolatedWith(glowTint.withAlpha(1.0f), 0.56f) : fill;
    auto glow = makeAccentGlow(glowSource, hasGlowTint ? 0.30f : 0.18f);

    float outerRadius = diameter * 0.5f;
    float capRadius = outerRadius * spec.capRadius;
    float hubRadius = outerRadius * spec.hubRadius;
    float toAngle = startAngle + sliderPos * (endAngle - startAngle);

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillEllipse(dial.translated(0.0f, isGrand ? 4.0f : 3.0f));

    auto recess = dial.expanded(outerRadius * (spec.pitScale - 0.08f), outerRadius * (spec.pitScale - 0.08f))
                      .withSizeKeepingCentre(dial.getWidth() + outerRadius * (spec.pitScale - 0.02f),
                                            dial.getHeight() + outerRadius * (spec.pitScale - 0.02f));
    fillSharedRecess(g, recess, outerRadius * 0.46f);

    if (sliderPos > 0.0f)
    {
        const float glowAlpha = hasGlowTint ? 0.012f : 0.032f;
        juce::Path haloArc;
        const float haloRadius = outerRadius * spec.arcRadius;
        haloArc.addCentredArc(c.x, c.y, haloRadius, haloRadius, 0.0f, startAngle, toAngle, true);
        g.setColour(glow.withAlpha(glowAlpha));
        g.strokePath(haloArc, juce::PathStrokeType(spec.arcWidth + 2.2f,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour(fill.withAlpha(hasGlowTint ? synthAlpha::solid : synthAlpha::strong));
        g.strokePath(haloArc, juce::PathStrokeType(spec.arcWidth,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    const float dotOrbit = outerRadius * spec.dotOrbit;
    for (int i = 0; i < spec.dotCount; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(spec.dotCount - 1);
        float angle = juce::jmap(t, startAngle, endAngle);
        float cs = std::cos(angle);
        float sn = std::sin(angle);
        const bool active = t <= sliderPos;
        const float tickInner = dotOrbit - outerRadius * 0.07f;
        const float tickOuter = dotOrbit + outerRadius * 0.02f;
        const float tickWidth = active ? spec.dotRadius * 0.95f : spec.dotRadius * 0.78f;
        g.setColour(active ? fill.withAlpha(synthAlpha::solid)
                           : synthcol::border.withAlpha(synthAlpha::strong));
        g.drawLine(c.x + cs * tickInner,
                   c.y + sn * tickInner,
                   c.x + cs * tickOuter,
                   c.y + sn * tickOuter,
                   tickWidth);
    }

    auto bezel = dial.reduced(outerRadius * 0.04f);
    juce::ColourGradient bezelGrad(blendTint(juce::Colour(0xff2D333C), bezelTint, hasBezelTint ? 0.44f : 0.30f), bezel.getX(), bezel.getY(),
                                   blendTint(juce::Colour(0xff101318), bezelTint, hasBezelTint ? 0.22f : 0.14f), bezel.getRight(), bezel.getBottom(), true);
    bezelGrad.addColour(0.26, blendTint(juce::Colour(0xff353C47), bezelTint, hasBezelTint ? 0.38f : 0.26f));
    bezelGrad.addColour(0.55, blendTint(juce::Colour(0xff1D232B), bezelTint, hasBezelTint ? 0.28f : 0.18f));
    g.setGradientFill(bezelGrad);
    g.fillEllipse(bezel);
    g.setColour(juce::Colours::black.withAlpha(0.36f));
    g.drawEllipse(bezel, 1.0f);

    auto collar = juce::Rectangle<float>(c.x - outerRadius * spec.collarRadius,
                                         c.y - outerRadius * spec.collarRadius,
                                         outerRadius * spec.collarRadius * 2.0f,
                                         outerRadius * spec.collarRadius * 2.0f);
    juce::ColourGradient collarGrad(blendTint(juce::Colour(0xff353C47), collarTint, hasCollarTint ? 0.42f : 0.34f), collar.getX(), collar.getY(),
                                    blendTint(juce::Colour(0xff1A1F26), collarTint, hasCollarTint ? 0.24f : 0.18f), collar.getRight(), collar.getBottom(), true);
    collarGrad.addColour(0.22, blendTint(juce::Colour(0xff2F3742), collarTint, hasCollarTint ? 0.36f : 0.30f));
    collarGrad.addColour(0.52, blendTint(juce::Colour(0xff242A33), collarTint, hasCollarTint ? 0.28f : 0.22f));
    g.setGradientFill(collarGrad);
    g.fillEllipse(collar);
    g.setColour(juce::Colours::black.withAlpha(0.34f));
    g.drawEllipse(collar, 1.1f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawEllipse(collar.reduced(1.0f), 0.8f);

    auto innerSeat = collar.reduced(outerRadius * 0.12f);
    juce::ColourGradient innerSeatGrad(blendTint(juce::Colour(0xff2B313B), collarTint, 0.18f), innerSeat.getCentreX(), innerSeat.getY(),
                                       blendTint(juce::Colour(0xff181C22), collarTint, 0.10f), innerSeat.getCentreX(), innerSeat.getBottom(), false);
    g.setGradientFill(innerSeatGrad);
    g.fillEllipse(innerSeat);

    auto cap = juce::Rectangle<float>(c.x - capRadius, c.y - capRadius,
                                      capRadius * 2.0f, capRadius * 2.0f);
    drawMetalEllipse(g, cap,
                     juce::Colour(0xffCDD4DE),
                     juce::Colour(0xffA8B2BE),
                     juce::Colour(0xff6B7580),
                     juce::Colour(0xff2A3038));
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawEllipse(cap, 0.95f);

    auto capFace = cap.reduced(cap.getWidth() * 0.08f, cap.getHeight() * 0.08f);
    juce::ColourGradient capFaceGrad(juce::Colours::white.withAlpha(0.040f),
                                     capFace.getX(), capFace.getY(),
                                     juce::Colours::transparentBlack,
                                     capFace.getRight(), capFace.getBottom(),
                                     true);
    capFaceGrad.addColour(0.36, blendTint(juce::Colour(0xffC8D0DB), capTint, hasCapTint ? 0.34f : 0.26f).withAlpha(0.050f));
    capFaceGrad.addColour(0.72, juce::Colours::black.withAlpha(0.12f));
    g.setGradientFill(capFaceGrad);
    g.fillEllipse(capFace);

    auto groove1 = cap.reduced(cap.getWidth() * 0.17f, cap.getHeight() * 0.17f);
    auto groove2 = cap.reduced(cap.getWidth() * 0.29f, cap.getHeight() * 0.29f);
    auto groove3 = cap.reduced(cap.getWidth() * 0.41f, cap.getHeight() * 0.41f);
    g.setColour(juce::Colours::white.withAlpha(0.038f));
    g.drawEllipse(groove1, 0.6f);
    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.drawEllipse(groove1.translated(0.0f, 0.22f), 0.55f);
    g.setColour(juce::Colours::white.withAlpha(0.028f));
    g.drawEllipse(groove2, 0.5f);
    g.setColour(juce::Colours::white.withAlpha(0.018f));
    g.drawEllipse(groove3, 0.45f);

    auto capGloss = cap.reduced(cap.getWidth() * 0.16f, cap.getHeight() * 0.16f)
                       .translated(-cap.getWidth() * 0.07f, -cap.getHeight() * 0.06f);
    juce::ColourGradient capGlossGrad(blendTint(juce::Colours::white, capTint, hasCapTint ? 0.34f : 0.24f).withAlpha(0.088f), capGloss.getX(), capGloss.getY(),
                                      juce::Colours::transparentWhite, capGloss.getRight(), capGloss.getBottom(), true);
    g.setGradientFill(capGlossGrad);
    g.fillEllipse(capGloss);

    auto capSpec = cap.reduced(cap.getWidth() * 0.28f, cap.getHeight() * 0.42f)
                     .translated(-cap.getWidth() * 0.11f, -cap.getHeight() * 0.13f);
    juce::ColourGradient capSpecGrad(blendTint(juce::Colours::white, capTint, hasCapTint ? 0.26f : 0.18f).withAlpha(0.11f), capSpec.getCentreX(), capSpec.getY(),
                                     juce::Colours::transparentWhite, capSpec.getCentreX(), capSpec.getBottom(), false);
    g.setGradientFill(capSpecGrad);
    g.fillEllipse(capSpec);

    auto capShadow = cap.reduced(cap.getWidth() * 0.12f);
    juce::ColourGradient capShadowGrad(juce::Colours::transparentBlack, c.x, capShadow.getY(),
                                       juce::Colours::black.withAlpha(0.12f), c.x, capShadow.getBottom(), false);
    g.setGradientFill(capShadowGrad);
    g.fillEllipse(capShadow);

    auto hub = juce::Rectangle<float>(c.x - hubRadius, c.y - hubRadius,
                                      hubRadius * 2.0f, hubRadius * 2.0f);
    juce::ColourGradient hubGrad(blendTint(juce::Colour(0xff8A95A4), capTint, 0.22f), hub.getCentreX(), hub.getY(),
                                 blendTint(juce::Colour(0xff343A44), capTint, 0.10f), hub.getCentreX(), hub.getBottom(), false);
    hubGrad.addColour(0.45, blendTint(juce::Colour(0xff566270), capTint, 0.16f));
    g.setGradientFill(hubGrad);
    g.fillEllipse(hub);
    g.setColour(juce::Colours::black.withAlpha(0.32f));
    g.drawEllipse(hub, 0.7f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawEllipse(hub.reduced(0.7f), 0.45f);

    float pointerAngle = toAngle - juce::MathConstants<float>::halfPi;
    float lineStart = capRadius * spec.pointerStart;
    float lineEnd = capRadius * spec.pointerEnd;
    float lineWidth = spec.pointerWidth;
    float cs = std::cos(pointerAngle);
    float sn = std::sin(pointerAngle);

    g.setColour(glow.withAlpha(0.09f));
    g.drawLine(c.x + cs * lineStart, c.y + sn * lineStart,
               c.x + cs * lineEnd, c.y + sn * lineEnd,
               lineWidth + 1.4f);
    const auto pointerBaseColour = capTint.isTransparent()
        ? fill
        : fill.interpolatedWith(capTint.withAlpha(1.0f), 0.34f);
    const auto pointerColour = pointerBaseColour.getPerceivedBrightness() > 0.70f
        ? pointerBaseColour.darker(0.30f)
        : pointerBaseColour.interpolatedWith(juce::Colour(0xffF4F0E6), capTint.isTransparent() ? 0.55f : 0.48f);
    g.setColour(pointerColour.withAlpha(0.84f));
    g.drawLine(c.x + cs * lineStart, c.y + sn * lineStart,
               c.x + cs * lineEnd, c.y + sn * lineEnd,
               lineWidth);

    float tipRadius = isGrand ? 2.8f : 2.1f;
    g.setColour(fill);
    g.fillEllipse(c.x + cs * lineEnd - tipRadius,
                  c.y + sn * lineEnd - tipRadius,
                  tipRadius * 2.0f,
                  tipRadius * 2.0f);
}

juce::Font SynthLookAndFeel::getLabelFont(juce::Label& label)
{
    const float height = static_cast<float>(label.getHeight());
    return juce::Font(juce::FontOptions{}.withHeight(juce::jlimit(11.4f, 13.8f, height * 0.72f)));
}

juce::Font SynthLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions{}
                          .withHeight(juce::jlimit(11.3f, 13.8f, static_cast<float>(buttonHeight) * 0.48f))
                          .withStyle("Bold"));
}

juce::Font SynthLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return juce::Font(juce::FontOptions{}
                          .withHeight(juce::jlimit(12.0f, 14.8f, static_cast<float>(box.getHeight()) * 0.46f)));
}

juce::Font SynthLookAndFeel::getPopupMenuFont()
{
    return juce::Font(juce::FontOptions{}.withHeight(12.5f));
}

void SynthLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
    const juce::Colour& bgColour, bool hi, bool dn)
{
    auto area = btn.getLocalBounds().toFloat().reduced(0.8f);
    bool isOn = btn.getToggleState();
    const bool isAmberBtn = (dynamic_cast<AmberShakeButton*>(&btn) != nullptr);
    auto accent = isAmberBtn ? juce::Colour(0xFFFFB300) : accent_;
    auto base = bgColour;

    if (isOn)
        base = base.interpolatedWith(accent, 0.14f);
    if (hi)
        base = base.brighter(0.035f);
    if (dn)
        base = base.darker(0.12f);

    auto drawArea = dn ? area.translated(0.0f, 1.0f) : area;
    const float radius = kButtonRadius;

    if (!dn)
    {
        g.setColour(juce::Colours::black.withAlpha(0.26f));
        g.fillRoundedRectangle(drawArea.translated(0.0f, kShadowLight), radius);
    }

    juce::ColourGradient shellGrad(base.brighter(isOn ? 0.06f : 0.03f), drawArea.getCentreX(), drawArea.getY(),
                                   base.darker(0.22f), drawArea.getCentreX(), drawArea.getBottom(), false);
    shellGrad.addColour(0.22, base);
    shellGrad.addColour(0.62, base.darker(0.08f));
    g.setGradientFill(shellGrad);
    g.fillRoundedRectangle(drawArea, radius);

    auto topFace = drawArea.reduced(1.5f, 1.4f);
    juce::ColourGradient topFaceGrad(juce::Colour(0xff252A31).interpolatedWith(accent, isOn ? 0.05f : 0.008f), topFace.getCentreX(), topFace.getY(),
                                     juce::Colour(0xff101318), topFace.getCentreX(), topFace.getBottom(), false);
    topFaceGrad.addColour(0.34, juce::Colour(0xff1A1E25).interpolatedWith(accent, isOn ? 0.035f : 0.0f));
    g.setGradientFill(topFaceGrad);
    g.fillRoundedRectangle(topFace, juce::jmax(0.0f, radius - 1.5f));

    auto faceHighlight = topFace.reduced(9.0f, 4.0f).removeFromTop(juce::jlimit(4.0f, 6.0f, topFace.getHeight() * 0.16f));
    juce::ColourGradient faceHighlightGrad(juce::Colours::white.withAlpha(0.04f), faceHighlight.getCentreX(), faceHighlight.getY(),
                                           juce::Colours::transparentWhite, faceHighlight.getCentreX(), faceHighlight.getBottom(), false);
    g.setGradientFill(faceHighlightGrad);
    g.fillRoundedRectangle(faceHighlight, juce::jmax(0.0f, radius - 4.0f));

    auto lowerWeight = topFace.reduced(7.0f, 0.0f).removeFromBottom(juce::jlimit(7.0f, 10.0f, topFace.getHeight() * 0.18f));
    juce::ColourGradient lowerWeightGrad(juce::Colours::transparentBlack,
                                         lowerWeight.getCentreX(), lowerWeight.getY(),
                                         juce::Colours::black.withAlpha(0.14f),
                                         lowerWeight.getCentreX(), lowerWeight.getBottom(), false);
    g.setGradientFill(lowerWeightGrad);
    g.fillRoundedRectangle(lowerWeight, juce::jmax(0.0f, radius - 4.0f));

    if (isOn)
    {
        auto ledStrip = topFace.reduced(topFace.getWidth() * 0.30f, 0.0f).removeFromBottom(juce::jlimit(2.6f, 3.6f, topFace.getHeight() * 0.09f));
        paintLedStrip(g, ledStrip, accent, 0.12f, 0.80f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.03f));
    g.drawRoundedRectangle(topFace.reduced(0.5f), juce::jmax(0.0f, radius - 1.8f), 0.8f);
    g.setColour((hi || isOn) ? accent.withAlpha(isAmberBtn ? 0.58f : 0.28f)
                             : synthcol::border.withAlpha(0.34f));
    g.drawRoundedRectangle(drawArea.reduced(0.5f), radius, hi ? 1.15f : 0.95f);
}

void SynthLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
    bool /*isDown*/, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    auto cbBg = box.findColour(juce::ComboBox::backgroundColourId);
    bool hover = box.isMouseOver(true);

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(bounds.translated(0.0f, kShadowLight), kPanelRadius);
    fillSharedPanel(g, bounds, kPanelRadius, hover ? cbBg.brighter(0.06f) : cbBg, hover);

    if (hover)
    {
        g.setColour(accent_.withAlpha(0.18f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), kPanelRadius, synthStroke::normal);
    }

    if (box.hasKeyboardFocus(true))
    {
        g.setColour(accent_.withAlpha(0.18f));
        g.drawRoundedRectangle(bounds.expanded(0.4f), kPanelRadius + 0.4f, 1.4f);
    }

    auto arrowArea = bounds.removeFromRight(28.0f);
    float cx = arrowArea.getCentreX();
    float cy = arrowArea.getCentreY();
    juce::Path arrow;
    arrow.startNewSubPath(cx - 5.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 3.2f);
    arrow.lineTo(cx + 5.0f, cy - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha(hover ? 1.0f : 0.82f));
    g.strokePath(arrow, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void SynthLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
    bool shouldDrawHighlighted, bool shouldDrawDown)
{
    auto area = btn.getLocalBounds().toFloat().reduced(1.0f);
    bool on = btn.getToggleState();
    auto drawArea = shouldDrawDown ? area.translated(0.0f, 1.0f) : area;
    float radius = drawArea.getHeight() * 0.5f;

    if (!shouldDrawDown)
    {
        g.setColour(juce::Colours::black.withAlpha(0.24f));
        g.fillRoundedRectangle(drawArea.translated(0.0f, kShadowLight), radius);
    }

    fillSharedRecess(g, drawArea, radius);

    auto innerTrack = drawArea.reduced(1.8f);
    auto trackBase = on ? synthcol::surface.interpolatedWith(accent_, 0.14f)
                        : synthcol::surface;
    fillSharedPanel(g, innerTrack, juce::jmax(0.0f, radius - 1.8f), trackBase, on || shouldDrawHighlighted);

    auto innerHighlight = innerTrack.reduced(6.0f, 3.0f).removeFromTop(juce::jlimit(4.0f, 7.0f, innerTrack.getHeight() * 0.18f));
    juce::ColourGradient innerHighlightGrad(juce::Colours::white.withAlpha(0.05f), innerHighlight.getCentreX(), innerHighlight.getY(),
                                            juce::Colours::transparentWhite, innerHighlight.getCentreX(), innerHighlight.getBottom(), false);
    g.setGradientFill(innerHighlightGrad);
    g.fillRoundedRectangle(innerHighlight, juce::jmax(0.0f, radius - 6.0f));

    if (on)
    {
        auto glowStrip = innerTrack.reduced(innerTrack.getWidth() * 0.18f, 0.0f);
        glowStrip = glowStrip.removeFromBottom(4.2f);
        paintLedStrip(g, glowStrip, accent_, 0.16f, 0.80f);
    }

    g.setColour(on ? accent_.withAlpha(0.34f) : synthcol::border.withAlpha(0.36f));
    g.drawRoundedRectangle(innerTrack.reduced(0.5f), juce::jmax(0.0f, radius - 2.3f), on ? 1.2f : 0.9f);

    float knobD = innerTrack.getHeight() - 4.0f;
    float knobX = on ? innerTrack.getRight() - knobD - 2.0f : innerTrack.getX() + 2.0f;
    float knobY = innerTrack.getY() + 2.0f;
    auto knob = juce::Rectangle<float>(knobX, knobY, knobD, knobD);

    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillEllipse(knob.translated(0.0f, 1.4f));
    drawMetalEllipse(g, knob,
                     juce::Colour(0xffF6FAFE),
                     juce::Colour(0xffD1D9E3),
                     juce::Colour(0xff929CAA),
                     juce::Colour(0xff4D5561));
    g.setColour(juce::Colours::white.withAlpha(0.20f));
    g.drawEllipse(knob, 0.9f);

    auto knobGloss = knob.reduced(knob.getWidth() * 0.26f, knob.getHeight() * 0.30f).translated(-knob.getWidth() * 0.04f, -knob.getHeight() * 0.03f);
    juce::ColourGradient knobGlossGrad(juce::Colours::white.withAlpha(0.22f), knobGloss.getCentreX(), knobGloss.getY(),
                                       juce::Colours::transparentWhite, knobGloss.getCentreX(), knobGloss.getBottom(), false);
    g.setGradientFill(knobGlossGrad);
    g.fillEllipse(knobGloss);
}

// =============================================================================
// SynthFamilyTab
// =============================================================================
void SynthFamilyTab::configure(int familyIndex, const juce::String& n, juce::Colour c)
{
    idx = familyIndex; name = n; col = c;
    setName(n);
}
void SynthFamilyTab::setSelected(bool s) { if (sel != s) { sel = s; repaint(); } }
void SynthFamilyTab::setIcon(juce::Image img) { icon = img; repaint(); }

void SynthFamilyTab::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f, 0.75f);
    constexpr float radius = kTabRadius;

    g.setColour(juce::Colours::black.withAlpha(sel ? 0.20f : 0.12f));
    g.fillRoundedRectangle(b.translated(0.0f, kShadowLight), radius);

    fillSharedRecess(g, b, radius);

    if (sel || hover)
    {
        auto activeBody = b.reduced(sel ? 1.0f : 1.4f, sel ? 1.0f : 1.8f);
        auto base = sel
            ? synthcol::surface.withAlpha(0.84f).interpolatedWith(col.withAlpha(0.42f), 0.10f)
            : synthcol::surfHi.withAlpha(0.50f);
        fillSharedPanel(g, activeBody, juce::jmax(0.0f, radius - 1.0f), base, true);

        if (sel)
        {
            auto glowStrip = activeBody.reduced(18.0f, 0.0f);
            glowStrip = glowStrip.removeFromBottom(2.2f);
            drawGlowStrip(g, glowStrip, col, 1.2f, kGlowSelected);
            g.setColour(col.withAlpha(0.26f));
            g.drawRoundedRectangle(activeBody.reduced(0.5f), juce::jmax(0.0f, radius - 1.0f), 1.0f);
        }
    }
    else
    {
        g.setColour(synthcol::border.withAlpha(0.20f));
        g.drawRoundedRectangle(b.reduced(0.5f), radius, 1.0f);
    }

    auto content = b.reduced(12.0f, 0.0f);
    if (icon.isValid())
    {
        const float iconSize = b.getHeight() * 0.46f;
        auto iconArea = content.removeFromLeft(iconSize + 8.0f);
        iconArea.setWidth(iconSize);
        g.setOpacity(sel ? 1.0f : 0.60f);
        g.drawImage(icon,
                    juce::Rectangle<float>(iconArea.getX(), b.getCentreY() - iconSize * 0.5f,
                                           iconSize, iconSize),
                    juce::RectanglePlacement::centred);
        g.setOpacity(1.0f);
    }

    g.setColour(sel ? synthcol::text : (hover ? synthcol::textSec : synthcol::textDim));
    g.setFont(juce::Font(juce::FontOptions{}
                 .withHeight(juce::jlimit(9.8f, 11.4f, b.getHeight() * 0.40f))
                             .withStyle("Bold")));
    g.drawText(name,
               content.toNearestInt(),
               icon.isValid() ? juce::Justification::centredLeft : juce::Justification::centred);
}

void SynthFamilyTab::mouseDown(const juce::MouseEvent&)  { if (onClicked) onClicked(idx); }
void SynthFamilyTab::mouseEnter(const juce::MouseEvent&) { hover = true;  repaint(); }
void SynthFamilyTab::mouseExit(const juce::MouseEvent&)  { hover = false; repaint(); }

// =============================================================================
// SynthPresetCard
// =============================================================================
void SynthPresetCard::configure(int instrIndex, const juce::String& n, juce::Colour c)
{
    idx = instrIndex; name = n; cat = c;
    setName(n);
}
void SynthPresetCard::setSelected(bool s) { if (sel != s) { sel = s; repaint(); } }
void SynthPresetCard::setIcon(juce::Image img) { icon = img; repaint(); }

void SynthPresetCard::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float cr = kCardRadius;

    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillRoundedRectangle(b.translated(0.0f, kShadowDeep), cr);

    auto base = synthcol::surface.withAlpha(0.64f);
    if (sel)
        base = synthcol::surface.withAlpha(0.70f).interpolatedWith(cat.withAlpha(0.56f), 0.16f);
    else if (hover)
        base = synthcol::surfHi.withAlpha(0.66f);
    fillSharedPanel(g, b, cr, base, sel || hover);

    if (sel)
    {
        auto glowStrip = b.reduced(12.0f, 0.0f);
        glowStrip = glowStrip.removeFromTop(3.5f);
        drawGlowStrip(g, glowStrip, cat, 1.4f, kGlowSelected);
        g.setColour(cat.withAlpha(0.38f));
        g.drawRoundedRectangle(b.reduced(0.5f), cr, 1.2f);
    }

    auto iconArea = b.withTrimmedBottom(24.0f);
    auto iconRect = juce::Rectangle<float>(
        iconArea.getCentreX() - 24.0f, iconArea.getCentreY() - 24.0f, 48.0f, 48.0f);

    if (icon.isValid())
    {
        if (sel)
        {
            g.setColour(cat.withAlpha(0.12f));
            g.fillRoundedRectangle(iconRect.expanded(6.0f), 8.0f);
        }
        g.setOpacity(sel ? 1.0f : 0.7f);
        g.drawImage(icon, iconRect, juce::RectanglePlacement::centred);
        g.setOpacity(1.0f);
    }
    else
    {
        fillSharedRecess(g, iconRect, 6.0f);
        g.setColour(sel ? cat.withAlpha(0.18f) : synthcol::border.withAlpha(0.30f));
        g.drawRoundedRectangle(iconRect, 6.0f, 1.0f);
    }

    g.setColour(sel ? cat.brighter(0.08f) : synthcol::text);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    g.drawText(name, b.removeFromBottom(22.0f), juce::Justification::centred);
}

void SynthPresetCard::mouseDown(const juce::MouseEvent&)  { if (onClicked) onClicked(idx); }
void SynthPresetCard::mouseEnter(const juce::MouseEvent&) { hover = true;  repaint(); }
void SynthPresetCard::mouseExit(const juce::MouseEvent&)  { hover = false; repaint(); }

// =============================================================================
// SynthEffectTab
// =============================================================================
void SynthEffectTab::configure(int tabIndex, const juce::String& n, juce::Colour a)
{
    idx = tabIndex; name = n; accent = a;
    setName(n);
}
void SynthEffectTab::setAccent(juce::Colour a) { if (accent != a) { accent = a; repaint(); } }
void SynthEffectTab::setSelected(bool s) { if (sel != s) { sel = s; repaint(); } }

void SynthEffectTab::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    constexpr float cr = kTabRadius;
    auto activeAccent = accent;
    auto base = synthcol::surface.withAlpha(0.66f);
    if (sel)
        base = synthcol::surface.withAlpha(0.74f).interpolatedWith(accent.withAlpha(0.34f), 0.10f);
    else if (hover)
        base = synthcol::surface.withAlpha(0.69f).interpolatedWith(accent.withAlpha(0.20f), 0.06f);

    g.setColour(juce::Colours::black.withAlpha(sel ? 0.24f : 0.16f));
    g.fillRoundedRectangle(b.translated(0.0f, kShadowLight), cr);
    fillSharedPanel(g, b, cr, base, sel || hover);

    if (sel || hover)
    {
        auto glowStrip = b.reduced(14.0f, 0.0f);
        glowStrip = glowStrip.removeFromBottom(2.2f);
        drawGlowStrip(g, glowStrip, activeAccent, 1.2f, sel ? kGlowSelected : kGlowHover);
    }

    g.setColour(sel ? synthcol::text : (hover ? synthcol::textSec : synthcol::textDim));

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.6f).withStyle("Bold")));
    g.drawText(name, b, juce::Justification::centred);
}

void SynthEffectTab::mouseDown(const juce::MouseEvent&)  { if (onClicked) onClicked(idx); }
void SynthEffectTab::mouseEnter(const juce::MouseEvent&)
{
    hover = true;
    repaint();
}
void SynthEffectTab::mouseExit(const juce::MouseEvent&)
{
    hover = false;
    repaint();
}
void SynthEffectTab::timerCallback()
{
    stopTimer();
    setTransform(juce::AffineTransform{});
}

// =============================================================================
// SynthFxRackItem
// =============================================================================
void SynthFxRackItem::configure(int itemIndex, const juce::String& n,
                                const juce::String& s, juce::Colour a)
{
    idx = itemIndex;
    name = n;
    summary = s;
    accent = a;
    setName(s.isNotEmpty() ? (n + " " + s) : n);
    setTooltip(s.isNotEmpty() ? (n + ": " + s + ". Click to edit.") : (n + ": Click to edit."));
}

void SynthFxRackItem::setAccent(juce::Colour accentCol)
{
    if (accent != accentCol)
    {
        accent = accentCol;
        repaint();
    }
}

void SynthFxRackItem::setSelected(bool s)
{
    if (sel != s)
    {
        sel = s;
        repaint();
    }
}

void SynthFxRackItem::setEnabledState(bool s)
{
    if (enabledState != s)
    {
        enabledState = s;
        repaint();
    }
}

void SynthFxRackItem::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    constexpr float cr = 7.0f;

    g.setColour(juce::Colours::black.withAlpha(sel ? 0.30f : 0.18f));
    g.fillRoundedRectangle(b.translated(0.0f, 2.2f), cr);

    auto fill = synthcol::surface.withAlpha(0.68f);
    if (sel)
        fill = synthcol::surface.withAlpha(0.72f).interpolatedWith(accent.withAlpha(0.82f), 0.28f);
    else if (hover)
        fill = synthcol::surfHi.withAlpha(0.72f);

    if (!enabledState)
        fill = fill.interpolatedWith(synthcol::graphite, 0.42f);

    fillSharedPanel(g, b, cr, fill, sel || hover);

    // State-dependent border: strong on selected, accent-tinted on hover, invisible otherwise
    if (sel)
    {
        g.setColour(accent.withAlpha(0.70f));
        g.drawRoundedRectangle(b.reduced(0.5f), cr, 1.4f);

        auto glowStrip = b.reduced(14.0f, 0.0f);
        glowStrip = glowStrip.removeFromBottom(3.5f);
        drawGlowStrip(g, glowStrip, accent, 2.0f, 0.88f);
    }
    else if (hover)
    {
        g.setColour(accent.withAlpha(0.28f));
        g.drawRoundedRectangle(b.reduced(0.5f), cr, 1.0f);
    }

    // Status indicator bar (left edge): active=accent, bypass=dim border
    auto statusArea = b.removeFromLeft(8.0f).reduced(0.0f, 3.0f);
    if (enabledState)
        g.setColour(accent.withAlpha(sel ? 1.0f : 0.80f));
    else
        g.setColour(synthcol::border.withAlpha(0.30f));
    g.fillRoundedRectangle(statusArea, 2.0f);

    auto textArea = getLocalBounds().toFloat().reduced(15.0f, 4.0f);
    auto titleArea = textArea.removeFromTop(textArea.getHeight() > 28.0f ? 13.0f : textArea.getHeight());
    g.setColour(enabledState ? (sel ? synthcol::text : synthcol::textSec)
                             : synthcol::textDim.withAlpha(0.45f));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.8f).withStyle("Bold")));
    g.drawText(name, titleArea, juce::Justification::centredLeft, true);

    if (textArea.getHeight() >= 10.0f && summary.isNotEmpty())
    {
        g.setColour(synthcol::textDim.withAlpha(enabledState ? 0.80f : 0.35f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
        g.drawText(summary, textArea, juce::Justification::centredLeft, true);
    }

    // Bypass badge: explicit "OFF" pill at right edge
    if (!enabledState)
    {
        auto badge = getLocalBounds().toFloat().removeFromRight(26.0f).reduced(3.0f, 7.0f);
        g.setColour(synthcol::border.withAlpha(0.40f));
        g.fillRoundedRectangle(badge, 3.0f);
        g.setColour(synthcol::textDim.withAlpha(0.60f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f).withStyle("Bold")));
        g.drawText("OFF", badge.toNearestInt(), juce::Justification::centred);
    }
}

void SynthFxRackItem::mouseDown(const juce::MouseEvent&)
{
    if (onClicked)
        onClicked(idx);
}

void SynthFxRackItem::mouseEnter(const juce::MouseEvent&)
{
    hover = true;
    repaint();
}

void SynthFxRackItem::mouseExit(const juce::MouseEvent&)
{
    hover = false;
    repaint();
}

// =============================================================================
// AmberShakeButton
// =============================================================================
void AmberShakeButton::mouseEnter(const juce::MouseEvent& e)
{
    juce::TextButton::mouseEnter(e);
    stopTimer();
    setTransform(juce::AffineTransform{});
}
void AmberShakeButton::mouseExit(const juce::MouseEvent& e)
{
    juce::TextButton::mouseExit(e);
    stopTimer();
    setTransform(juce::AffineTransform{});
}
void AmberShakeButton::timerCallback()
{
    stopTimer();
    setTransform(juce::AffineTransform{});
}

// =============================================================================
// EnvelopeDisplay
// =============================================================================
EnvelopeDisplay::~EnvelopeDisplay()
{
    removeSliderListener(attack_);
    removeSliderListener(decay_);
    removeSliderListener(sustain_);
    removeSliderListener(release_);
}

void EnvelopeDisplay::addSliderListener(juce::Slider* s)
{
    if (s) s->addListener(this);
}

void EnvelopeDisplay::removeSliderListener(juce::Slider* s)
{
    if (s) s->removeListener(this);
}

void EnvelopeDisplay::bindAdsr(juce::Slider* attack,
                               juce::Slider* decay,
                               juce::Slider* sustain,
                               juce::Slider* release)
{
    removeSliderListener(attack_);
    removeSliderListener(decay_);
    removeSliderListener(sustain_);
    removeSliderListener(release_);

    attack_  = attack;
    decay_   = decay;
    sustain_ = sustain;
    release_ = release;

    addSliderListener(attack_);
    addSliderListener(decay_);
    addSliderListener(sustain_);
    addSliderListener(release_);
    drag_ = DragTarget::None;
    hover_ = DragTarget::None;
    updateCursor(DragTarget::None);
    repaint();
}

EnvelopeDisplay::DragTarget EnvelopeDisplay::hitTestTarget(juce::Point<float> position,
                                                           const bool allowRegionFallback) const
{
    const auto p = getPlotBounds();
    if (!p.expanded(8.0f).contains(position))
        return DragTarget::None;

    const float a = getNorm(attack_);
    const float d = getNorm(decay_);
    const float s = getNorm(sustain_);
    const float r = getNorm(release_);
    const float sustainSpan = 0.22f;
    const float tSum = juce::jmax(0.001f, a + d + sustainSpan + r);

    const float x0 = p.getX();
    const float x1 = x0 + p.getWidth() * (a / tSum);
    const float x2 = x1 + p.getWidth() * (d / tSum);
    const float x3 = p.getRight() - p.getWidth() * (r / tSum);
    const float x4 = p.getRight();
    const auto p1 = juce::Point<float>(x1, p.getY());
    const auto p2 = juce::Point<float>(x2, p.getBottom() - s * p.getHeight());
    const auto p3 = juce::Point<float>(x3, p2.y);

    constexpr float pickRadius = 16.0f;
    const float d1 = position.getDistanceFrom(p1);
    const float d2 = position.getDistanceFrom(p2);
    const float d3 = position.getDistanceFrom(p3);

    if (d1 <= pickRadius && d1 <= d2 && d1 <= d3)
        return DragTarget::Attack;
    if (d2 <= pickRadius && d2 <= d3)
        return DragTarget::DecaySustain;
    if (d3 <= pickRadius)
        return DragTarget::Release;
    if (!allowRegionFallback)
        return DragTarget::None;

    if (position.x <= (x0 + x2) * 0.5f)
        return DragTarget::Attack;
    if (position.x >= (x3 + x4) * 0.5f)
        return DragTarget::Release;
    return DragTarget::DecaySustain;
}

void EnvelopeDisplay::updateCursor(const DragTarget target)
{
    switch (target)
    {
        case DragTarget::Attack:
        case DragTarget::Release:
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            break;
        case DragTarget::DecaySustain:
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            break;
        case DragTarget::None:
            setMouseCursor(juce::MouseCursor::NormalCursor);
            break;
    }
}

float EnvelopeDisplay::getNorm(juce::Slider* s) const
{
    if (s == nullptr) return 0.0f;
    return juce::jlimit(0.0f, 1.0f, static_cast<float>(s->valueToProportionOfLength(s->getValue())));
}

void EnvelopeDisplay::setNorm(juce::Slider* s, float n) const
{
    if (s == nullptr) return;
    n = juce::jlimit(0.0f, 1.0f, n);
    s->setValue(s->proportionOfLengthToValue(n), juce::sendNotificationSync);
}

juce::Rectangle<float> EnvelopeDisplay::getPlotBounds() const
{
    auto bounds = getLocalBounds().toFloat().reduced(10.0f, 8.0f);
    const float topTrim = juce::jlimit(16.0f, 24.0f, bounds.getHeight() * 0.15f);
    const float bottomTrim = juce::jlimit(10.0f, 18.0f, bounds.getHeight() * 0.11f);
    return bounds.withTrimmedTop(topTrim).withTrimmedBottom(bottomTrim);
}

void EnvelopeDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillRoundedRectangle(b.translated(0.0f, kShadowDeep), kRecessRadius);
    fillPanelCavity(g, b, kRecessRadius, accent_);

    auto header = b.reduced(9.0f, 7.0f).removeFromTop(15.0f);
    if (title_.isNotEmpty())
    {
        auto headerGlow = header.withWidth(juce::jlimit(56.0f, 92.0f, 10.0f + title_.length() * 7.0f));
        headerGlow = headerGlow.removeFromBottom(3.0f);
        drawGlowStrip(g, headerGlow, accent_, 2.0f, 0.42f);
        g.setColour(synthcol::textSec);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.8f).withStyle("Bold")));
        g.drawText(title_, header.toNearestInt(), juce::Justification::left);
    }

    auto p = getPlotBounds();
    auto plotFrame = p.expanded(3.0f, 2.0f);
    fillPanelCavity(g, plotFrame, 5.0f, accent_);
    g.setColour(accent_.withAlpha(0.022f));
    g.fillRoundedRectangle(p.expanded(0.5f), 4.0f);
    g.setColour(synthcol::border.withAlpha(0.22f));
    g.drawRoundedRectangle(plotFrame.reduced(0.5f), 5.0f, 1.0f);

    g.setColour(synthcol::border.withAlpha(0.16f));
    for (int i = 1; i < 4; ++i)
    {
        const float x = p.getX() + p.getWidth() * (static_cast<float>(i) / 4.0f);
        const float y = p.getY() + p.getHeight() * (static_cast<float>(i) / 4.0f);
        g.drawLine(x, p.getY(), x, p.getBottom(), 1.0f);
        if (i < 3)
            g.drawLine(p.getX(), y, p.getRight(), y, 1.0f);
    }

    const float a = getNorm(attack_);
    const float d = getNorm(decay_);
    const float s = getNorm(sustain_);
    const float r = getNorm(release_);
    const float sustainSpan = 0.22f;
    const float tSum = juce::jmax(0.001f, a + d + sustainSpan + r);

    const float x0 = p.getX();
    const float y0 = p.getBottom();
    const float x1 = x0 + p.getWidth() * (a / tSum);
    const float y1 = p.getY();
    const float x2 = x1 + p.getWidth() * (d / tSum);
    const float y2 = p.getBottom() - s * p.getHeight();
    const float x3 = p.getRight() - p.getWidth() * (r / tSum);
    const float y3 = y2;
    const float x4 = p.getRight();
    const float y4 = p.getBottom();
    const auto activeTarget = drag_ != DragTarget::None ? drag_ : hover_;

    juce::Path env;
    env.startNewSubPath(x0, y0);
    env.lineTo(x1, y1);
    env.lineTo(x2, y2);
    env.lineTo(x3, y3);
    env.lineTo(x4, y4);

    juce::Path fillPath = env;
    fillPath.lineTo(x4, p.getBottom());
    fillPath.lineTo(x0, p.getBottom());
    fillPath.closeSubPath();

    juce::ColourGradient glowGrad(makeAccentGlow(accent_, 0.24f), x1, y1,
                                  juce::Colours::transparentBlack, p.getCentreX(), p.getBottom(), false);
    g.setGradientFill(glowGrad);
    g.fillPath(fillPath);

    g.setColour(accent_.withAlpha(0.07f));
    g.drawLine(x1, p.getY(), x1, p.getBottom(), 1.0f);
    g.drawLine(x2, p.getY(), x2, p.getBottom(), 1.0f);
    g.drawLine(x3, p.getY(), x3, p.getBottom(), 1.0f);

    g.setColour(makeAccentGlow(accent_, 0.22f));
    g.strokePath(env, juce::PathStrokeType(5.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white.withAlpha(0.86f).interpolatedWith(accent_, 0.24f));
    g.strokePath(env, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto drawHandle = [&](const juce::Point<float> point, const DragTarget target, const juce::String& label)
    {
        const bool active = activeTarget == target;
        auto outer = juce::Rectangle<float>(10.0f, 10.0f).withCentre(point);
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillEllipse(outer.translated(0.0f, 1.2f));
        g.setColour(active ? accent_.withAlpha(0.18f) : synthcol::surface.withAlpha(0.90f));
        g.fillEllipse(outer);
        g.setColour(active ? accent_.brighter(0.18f) : synthcol::text.withAlpha(0.88f));
        g.drawEllipse(outer.reduced(0.8f), active ? 1.5f : 1.0f);
        g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre(point));

        g.setColour(active ? accent_.brighter(0.12f) : synthcol::textDim.withAlpha(0.84f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.6f).withStyle("Bold")));
        g.drawText(label, juce::Rectangle<int>(static_cast<int>(point.x - 10.0f),
                                               static_cast<int>(point.y - 18.0f),
                                               20, 10),
                   juce::Justification::centred);
    };

    drawHandle({ x1, y1 }, DragTarget::Attack, "A");
    drawHandle({ x2, y2 }, DragTarget::DecaySustain, "D/S");
    drawHandle({ x3, y3 }, DragTarget::Release, "R");

    const int footerY = static_cast<int>(p.getBottom()) + 2;
    g.setColour(synthcol::textDim.withAlpha(0.82f));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(getHeight() < 120 ? 8.0f : 8.6f).withStyle("Bold")));
    g.drawText("A", juce::Rectangle<int>(static_cast<int>(x0), footerY, 18, 10), juce::Justification::centredLeft);
    g.drawText("D", juce::Rectangle<int>(static_cast<int>(x1), footerY, 18, 10), juce::Justification::centred);
    g.drawText("S", juce::Rectangle<int>(static_cast<int>(x2), footerY, 18, 10), juce::Justification::centred);
    g.drawText("R", juce::Rectangle<int>(static_cast<int>(x3), footerY, 18, 10), juce::Justification::centred);
}

void EnvelopeDisplay::mouseDown(const juce::MouseEvent& e)
{
    drag_ = hitTestTarget(e.position, true);
    updateCursor(drag_);
    repaint();
}

void EnvelopeDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (drag_ == DragTarget::None) return;
    auto p = getPlotBounds();
    if (p.getWidth() <= 1.0f || p.getHeight() <= 1.0f) return;

    float a = getNorm(attack_);
    float d = getNorm(decay_);
    float r = getNorm(release_);

    const float sustainSpan = 0.22f;
    const float tSum = juce::jmax(0.001f, a + d + sustainSpan + r);
    const float x1 = p.getX() + p.getWidth() * (a / tSum);
    const float x2 = x1 + p.getWidth() * (d / tSum);
    const float x3 = p.getRight() - p.getWidth() * (r / tSum);
    const float minDx = p.getWidth() * 0.04f;

    if (drag_ == DragTarget::Attack)
    {
        const float nx = juce::jlimit(p.getX(), juce::jmax(p.getX(), x3 - minDx), e.position.x);
        const float ratio = juce::jlimit(0.0f, 0.98f, (nx - p.getX()) / p.getWidth());
        const float fixedSpan = d + sustainSpan + r;
        const float newA = fixedSpan * ratio / juce::jmax(0.02f, 1.0f - ratio);
        setNorm(attack_, juce::jlimit(0.0f, 1.0f, newA));
    }
    else if (drag_ == DragTarget::DecaySustain)
    {
        const float nx = juce::jlimit(x1, juce::jmax(x1, x3 - minDx), e.position.x);
        const float ratio = juce::jlimit(0.0f, 0.98f, (nx - p.getX()) / p.getWidth());
        const float fixedSpan = sustainSpan + r;
        const float attackAndDecay = fixedSpan * ratio / juce::jmax(0.02f, 1.0f - ratio);
        const float newD = juce::jmax(0.0f, attackAndDecay - a);
        setNorm(decay_, juce::jlimit(0.0f, 1.0f, newD));

        const float ns = juce::jlimit(0.0f, 1.0f, 1.0f - (e.position.y - p.getY()) / p.getHeight());
        setNorm(sustain_, ns);
    }
    else if (drag_ == DragTarget::Release)
    {
        const float nx = juce::jlimit(juce::jmin(p.getRight(), x2 + minDx), p.getRight(), e.position.x);
        const float ratio = juce::jlimit(0.0f, 0.98f, (p.getRight() - nx) / p.getWidth());
        const float fixedSpan = a + d + sustainSpan;
        const float newR = fixedSpan * ratio / juce::jmax(0.02f, 1.0f - ratio);
        setNorm(release_, juce::jlimit(0.0f, 1.0f, newR));
    }

    repaint();
}

void EnvelopeDisplay::mouseMove(const juce::MouseEvent& e)
{
    const auto nextHover = hitTestTarget(e.position, true);
    if (nextHover != hover_)
    {
        hover_ = nextHover;
        if (drag_ == DragTarget::None)
            updateCursor(hover_);
        repaint();
    }
}

void EnvelopeDisplay::mouseExit(const juce::MouseEvent&)
{
    if (hover_ != DragTarget::None)
    {
        hover_ = DragTarget::None;
        if (drag_ == DragTarget::None)
            updateCursor(DragTarget::None);
        repaint();
    }
}

void EnvelopeDisplay::mouseUp(const juce::MouseEvent&)
{
    drag_ = DragTarget::None;
    updateCursor(hover_);
    repaint();
}

// =============================================================================
// LfoModulationDisplay
// =============================================================================
LfoModulationDisplay::LfoModulationDisplay()
{
    startTimerHz(30);
}

void LfoModulationDisplay::bindRateDepth(juce::Slider* rate, juce::Slider* depth)
{
    rate_ = rate;
    depth_ = depth;
    rateMem_ = getRateNorm();
    depthMem_ = getDepthNorm();
    hoverWaveIndex_ = -1;
    hoverPlot_ = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void LfoModulationDisplay::setWaveformIndex(int idx)
{
    idx = juce::jlimit(0, 3, idx);
    waveform_ = static_cast<Waveform>(idx);
    repaint();
}

float LfoModulationDisplay::getRateNorm() const
{
    if (rate_ != nullptr)
        return juce::jlimit(0.0f, 1.0f, static_cast<float>(rate_->valueToProportionOfLength(rate_->getValue())));
    return rateMem_;
}

float LfoModulationDisplay::getDepthNorm() const
{
    if (depth_ != nullptr)
        return juce::jlimit(0.0f, 1.0f, static_cast<float>(depth_->valueToProportionOfLength(depth_->getValue())));
    return depthMem_;
}

void LfoModulationDisplay::setRateNorm(float n)
{
    rateMem_ = juce::jlimit(0.0f, 1.0f, n);
    if (rate_ != nullptr)
        rate_->setValue(rate_->proportionOfLengthToValue(rateMem_), juce::sendNotificationSync);
}

void LfoModulationDisplay::setDepthNorm(float n)
{
    depthMem_ = juce::jlimit(0.0f, 1.0f, n);
    if (depth_ != nullptr)
        depth_->setValue(depth_->proportionOfLengthToValue(depthMem_), juce::sendNotificationSync);
}

float LfoModulationDisplay::sampleWave(float phase) const
{
    phase -= std::floor(phase);
    switch (waveform_)
    {
        case Waveform::Sine:     return std::sin(phase * juce::MathConstants<float>::twoPi);
        case Waveform::Triangle: return 1.0f - 4.0f * std::abs(phase - 0.5f);
        case Waveform::Saw:      return phase * 2.0f - 1.0f;
        case Waveform::Square:   return phase < 0.5f ? 1.0f : -1.0f;
    }
    return 0.0f;
}

juce::Rectangle<float> LfoModulationDisplay::getHeaderBounds() const
{
    return getLocalBounds().toFloat().reduced(8.0f, 6.0f).removeFromTop(20.0f);
}

juce::Rectangle<float> LfoModulationDisplay::getWaveChipBounds(const int index) const
{
    if (index < 0 || index > 3)
        return {};

    auto header = getHeaderBounds();
    const float maxChipArea = juce::jmax(84.0f, header.getWidth() - 36.0f);
    const float chipAreaWidth = juce::jlimit(84.0f,
                                             juce::jmin(176.0f, maxChipArea),
                                             header.getWidth() * 0.60f);
    auto chipArea = juce::Rectangle<float>(header.getRight() - chipAreaWidth,
                                           header.getY(),
                                           chipAreaWidth,
                                           header.getHeight());
    constexpr float gap = 4.0f;
    const float chipWidth = (chipArea.getWidth() - gap * 3.0f) / 4.0f;
    return juce::Rectangle<float>(chipArea.getX() + index * (chipWidth + gap),
                                  chipArea.getY(),
                                  chipWidth,
                                  chipArea.getHeight() - 1.0f);
}

int LfoModulationDisplay::hitTestWaveformChip(const juce::Point<float> position) const
{
    for (int i = 0; i < 4; ++i)
        if (getWaveChipBounds(i).contains(position))
            return i;
    return -1;
}

juce::Rectangle<float> LfoModulationDisplay::getPlotBounds() const
{
    auto bounds = getLocalBounds().toFloat().reduced(10.0f, 9.0f);
    const float topTrim = juce::jlimit(22.0f, 34.0f, bounds.getHeight() * 0.24f);
    const float bottomTrim = juce::jlimit(16.0f, 28.0f, bounds.getHeight() * 0.20f);
    return bounds.withTrimmedTop(topTrim).withTrimmedBottom(bottomTrim);
}

void LfoModulationDisplay::timerCallback()
{
    const float rateN = getRateNorm();
    const float speed = 0.004f + rateN * 0.032f;
    phase_ += speed;
    if (phase_ >= 1.0f) phase_ -= 1.0f;
    repaint();
}

void LfoModulationDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillRoundedRectangle(b.translated(0.0f, kShadowDeep), kRecessRadius);
    fillSharedRecess(g, b, kRecessRadius);

    auto header = getHeaderBounds();
    auto headerGlow = juce::Rectangle<float>(header.getX(), header.getY() + header.getHeight() - 3.0f, 44.0f, 2.2f);
    drawGlowStrip(g, headerGlow, accent_, 1.8f, 0.78f);
    g.setColour(synthcol::textSec);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
    g.drawText(title_, juce::Rectangle<int>(static_cast<int>(header.getX()), static_cast<int>(header.getY()),
                                            static_cast<int>(header.getWidth() * 0.36f), static_cast<int>(header.getHeight())),
               juce::Justification::centredLeft);

    static constexpr const char* kWaveLabels[] = { "SIN", "TRI", "SAW", "SQR" };
    for (int i = 0; i < 4; ++i)
    {
        auto chip = getWaveChipBounds(i);
        const bool active = i == static_cast<int>(waveform_);
        const bool hover = i == hoverWaveIndex_;
        auto chipBase = active
            ? synthcol::surface.interpolatedWith(accent_, 0.22f)
            : synthcol::surface.withAlpha(0.74f);
        fillSharedPanel(g, chip, 4.5f, chipBase, active || hover);
        if (active || hover)
        {
            auto chipGlow = chip.reduced(6.0f, 0.0f);
            chipGlow = chipGlow.removeFromBottom(2.0f);
            drawGlowStrip(g, chipGlow, accent_, 1.6f, active ? 0.84f : 0.48f);
        }
        g.setColour(active ? synthcol::text : synthcol::textSec);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
        g.drawText(kWaveLabels[i], chip.toNearestInt(), juce::Justification::centred);
    }

    auto p = getPlotBounds();
    auto plotFrame = p.expanded(2.0f);
    fillSharedPanel(g, plotFrame, 5.0f,
                    synthcol::surfHi.withAlpha(0.18f).interpolatedWith(accent_.withAlpha(0.08f), 0.18f),
                    false);
    g.setColour(accent_.withAlpha(0.05f));
    g.fillRoundedRectangle(p.expanded(0.5f), 4.0f);
    g.setColour(synthcol::border.withAlpha(0.32f));
    g.drawRoundedRectangle(plotFrame.reduced(0.5f), 5.0f, 1.0f);

    g.setColour(synthcol::border.withAlpha(0.30f));
    for (int i = 1; i < 4; ++i)
    {
        const float x = p.getX() + p.getWidth() * (static_cast<float>(i) / 4.0f);
        g.drawLine(x, p.getY(), x, p.getBottom(), 1.0f);
    }

    const float midY = p.getCentreY();
    g.setColour(synthcol::border.withAlpha(0.60f));
    g.drawLine(p.getX(), midY, p.getRight(), midY, 1.0f);

    const float rateNorm = getRateNorm();
    const float depth = getDepthNorm();
    juce::Path wave;
    juce::Path waveFill;
    bool started = false;
    const int n = juce::jmax(32, static_cast<int>(p.getWidth()));
    for (int i = 0; i < n; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(n - 1);
        const float ph = phase_ + t;
        const float w = sampleWave(ph) * depth;
        const float x = p.getX() + t * p.getWidth();
        const float y = midY - w * (p.getHeight() * 0.46f);
        if (!started)
        {
            wave.startNewSubPath(x, y);
            waveFill.startNewSubPath(x, midY);
            waveFill.lineTo(x, y);
            started = true;
        }
        else
        {
            wave.lineTo(x, y);
            waveFill.lineTo(x, y);
        }
    }
    waveFill.lineTo(p.getRight(), midY);
    waveFill.closeSubPath();

    juce::ColourGradient waveGlow(makeAccentGlow(accent_, 0.26f), p.getCentreX(), p.getY(),
                                  juce::Colours::transparentBlack, p.getCentreX(), p.getBottom(), false);
    g.setGradientFill(waveGlow);
    g.fillPath(waveFill);

    g.setColour(makeAccentGlow(accent_, 0.28f));
    g.strokePath(wave, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent_.brighter(0.10f));
    g.strokePath(wave, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float controlX = p.getX() + rateNorm * p.getWidth();
    const float controlY = p.getBottom() - depth * p.getHeight();
    g.setColour(accent_.withAlpha(hoverPlot_ ? 0.18f : 0.10f));
    g.drawLine(controlX, p.getY(), controlX, p.getBottom(), 1.0f);
    g.drawLine(p.getX(), controlY, p.getRight(), controlY, 1.0f);
    g.setColour(accent_.withAlpha(0.26f));
    g.fillEllipse(controlX - 5.0f, controlY - 5.0f, 10.0f, 10.0f);
    g.setColour(synthcol::text);
    g.fillEllipse(controlX - 2.2f, controlY - 2.2f, 4.4f, 4.4f);

    auto footer = getLocalBounds().reduced(10, 6);
    g.setColour(synthcol::textDim);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(getHeight() < 120 ? 8.0f : 8.5f).withStyle("Bold")));
    const int footerW = footer.getWidth();
    const int depthW = juce::jlimit(56, 100, static_cast<int>(footerW * 0.40f));
    const int rateW = juce::jmax(50, footerW - depthW - 8);
    g.drawText("RATE " + juce::String(juce::roundToInt(rateNorm * 100.0f)) + "%",
               footer.removeFromLeft(rateW), juce::Justification::bottomLeft);
    g.drawText("DEPTH " + juce::String(juce::roundToInt(depth * 100.0f)) + "%",
               footer.removeFromRight(depthW), juce::Justification::bottomRight);
}

void LfoModulationDisplay::mouseDown(const juce::MouseEvent& e)
{
    if (const int chipIndex = hitTestWaveformChip(e.position); chipIndex >= 0)
    {
        setWaveformIndex(chipIndex);
        if (onWaveformChanged)
            onWaveformChanged(chipIndex);
        repaint();
        return;
    }

    if (getHeaderBounds().contains(e.position))
    {
        setWaveformIndex((static_cast<int>(waveform_) + 1) % 4);
        if (onWaveformChanged)
            onWaveformChanged(static_cast<int>(waveform_));
        repaint();
        return;
    }
    mouseDrag(e);
}

void LfoModulationDisplay::mouseDrag(const juce::MouseEvent& e)
{
    auto p = getPlotBounds();
    if (p.getWidth() <= 1.0f || p.getHeight() <= 1.0f) return;

    const float rateN  = juce::jlimit(0.0f, 1.0f, (e.position.x - p.getX()) / p.getWidth());
    const float depthN = juce::jlimit(0.0f, 1.0f, 1.0f - (e.position.y - p.getY()) / p.getHeight());
    setRateNorm(rateN);
    setDepthNorm(depthN);
    hoverPlot_ = true;
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    hoverWaveIndex_ = -1;
    repaint();
}

void LfoModulationDisplay::mouseMove(const juce::MouseEvent& e)
{
    const int nextWave = hitTestWaveformChip(e.position);
    const bool nextPlot = getPlotBounds().contains(e.position);
    if (nextWave != hoverWaveIndex_ || nextPlot != hoverPlot_)
    {
        hoverWaveIndex_ = nextWave;
        hoverPlot_ = nextPlot;
        if (hoverWaveIndex_ >= 0)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else if (hoverPlot_)
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void LfoModulationDisplay::mouseExit(const juce::MouseEvent&)
{
    if (hoverWaveIndex_ >= 0 || hoverPlot_)
    {
        hoverWaveIndex_ = -1;
        hoverPlot_ = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

// =============================================================================
// PlayableRangeKeyboard
// =============================================================================
PlayableRangeKeyboard::PlayableRangeKeyboard(juce::MidiKeyboardState& state,
                                             juce::MidiKeyboardComponent::Orientation orientation)
    : juce::MidiKeyboardComponent(state, orientation)
{
}

void PlayableRangeKeyboard::setPlayableRange(int lowNote, int highNote)
{
    lowNote  = juce::jlimit(0, 127, lowNote);
    highNote = juce::jlimit(lowNote, 127, highNote);

    if (playableLow_ != lowNote || playableHigh_ != highNote)
    {
        playableLow_ = lowNote;
        playableHigh_ = highNote;
        repaint();
    }
}

bool PlayableRangeKeyboard::isPlayable(int midiNoteNumber) const
{
    return midiNoteNumber >= playableLow_ && midiNoteNumber <= playableHigh_;
}

void PlayableRangeKeyboard::drawWhiteNote(int midiNoteNumber, juce::Graphics& g,
                                          juce::Rectangle<float> area, bool isDown,
                                          bool isOver, juce::Colour lineColour,
                                          juce::Colour textColour)
{
    juce::MidiKeyboardComponent::drawWhiteNote(midiNoteNumber, g, area, isDown, isOver,
                                               lineColour, juce::Colours::transparentWhite);

    if (midiNoteNumber % 12 == 0 && area.getWidth() >= 14.0f)
    {
        g.setColour(textColour.withAlpha(0.88f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(juce::jlimit(9.0f, 12.0f, area.getHeight() * 0.13f))
                                                     .withStyle("Bold")));
        g.drawText(juce::MidiMessage::getMidiNoteName(midiNoteNumber, true, true, 3),
                   juce::Rectangle<int>(static_cast<int>(area.getX()),
                                        static_cast<int>(area.getBottom()) - 16,
                                        static_cast<int>(area.getWidth()), 12),
                   juce::Justification::centredBottom,
                   false);
    }
}

void PlayableRangeKeyboard::drawBlackNote(int midiNoteNumber, juce::Graphics& g,
                                          juce::Rectangle<float> area, bool isDown,
                                          bool isOver, juce::Colour noteFillColour)
{
    juce::MidiKeyboardComponent::drawBlackNote(midiNoteNumber, g, area, isDown, isOver,
                                               noteFillColour);

    auto face = area.reduced(0.6f, 0.5f);
    auto topEdge = face.reduced(face.getWidth() * 0.12f, 0.0f)
                        .removeFromTop(juce::jlimit(2.0f, 4.0f, face.getHeight() * 0.16f));
    juce::ColourGradient topEdgeGrad(juce::Colours::white.withAlpha(isDown ? 0.08f : isOver ? 0.12f : 0.06f),
                                     topEdge.getCentreX(), topEdge.getY(),
                                     juce::Colours::transparentWhite, topEdge.getCentreX(), topEdge.getBottom(), false);
    g.setGradientFill(topEdgeGrad);
    g.fillRoundedRectangle(topEdge, 1.6f);

    g.setColour((isDown ? findColour(juce::MidiKeyboardComponent::keyDownOverlayColourId)
                        : isOver ? findColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId).brighter(0.10f)
                                 : juce::Colours::white.withAlpha(0.04f)).withAlpha(isDown ? 0.34f : isOver ? 0.18f : 0.10f));
    g.drawRoundedRectangle(face, 2.0f, isDown ? 1.1f : 0.8f);

}

// =============================================================================
// CommonSynthEditor — constructor
// =============================================================================
namespace
{
constexpr int kFactoryPresetItemIdBase = 1000;
constexpr int kUserPresetItemIdBase = 100000;

int makeFactoryPresetItemId(int factoryIndex)
{
    return kFactoryPresetItemIdBase + factoryIndex;
}

int makeUserPresetItemId(int userPresetIndex)
{
    return kUserPresetItemIdBase + userPresetIndex;
}

bool isFactoryPresetItemId(int itemId)
{
    return itemId >= kFactoryPresetItemIdBase && itemId < kUserPresetItemIdBase;
}

bool isUserPresetItemId(int itemId)
{
    return itemId >= kUserPresetItemIdBase;
}
}

CommonSynthEditor::CommonSynthEditor(juce::AudioProcessor&              proc,
                                     juce::AudioProcessorValueTreeState& apvts,
                                     juce::MidiKeyboardState&            kbState,
                                     juce::Colour                        accent,
                                     int   kbLow,
                                     int   kbHigh,
                                     float kbKeyWidth)
    : AudioProcessorEditor(&proc),
      accent_(accent),
      lnf_(accent)
{
    setOpaque(true);
    setLookAndFeel(&lnf_);

    // Neutral procedural grain texture (anthracite studio finish)
    bgTexture_ = juce::Image(juce::Image::ARGB, 256, 256, true);
    {
        juce::Image::BitmapData bits(bgTexture_, juce::Image::BitmapData::writeOnly);
        juce::Random rng(42);
        for (int ny = 0; ny < 256; ++ny)
            for (int nx = 0; nx < 256; ++nx)
            {
                // Neutral anthracite grain — no color bias
                float noise = rng.nextFloat();
                auto lum = static_cast<juce::uint8>(noise * 24.0f + 8.0f);
                auto a   = static_cast<juce::uint8>(rng.nextFloat() * 8.0f);
                bits.setPixelColour(nx, ny, juce::Colour(lum, lum, lum, a));
            }
    }

    // ---- Preset browser ----
    addAndMakeVisible(presetSearch);
    presetSearch.setTextToShowWhenEmpty("Search...", synthcol::textDim);
    presetSearch.setJustification(juce::Justification::centredLeft);
    presetSearch.setBorder(juce::BorderSize<int>(0));
    presetSearch.setIndents(10, 0);
    presetSearch.setTooltip("Filter presets");
    presetSearch.setColour(juce::TextEditor::backgroundColourId, synthcol::surfHi);
    presetSearch.setColour(juce::TextEditor::outlineColourId, synthcol::ink);
    presetSearch.setColour(juce::TextEditor::focusedOutlineColourId, accent_.withAlpha(0.42f));
    presetSearch.setColour(juce::TextEditor::textColourId, synthcol::text);
    presetSearch.setColour(juce::TextEditor::highlightColourId, accent_.withAlpha(0.22f));
    presetSearch.setColour(juce::TextEditor::highlightedTextColourId, synthcol::text);

    addAndMakeVisible(presetBox);

    addAndMakeVisible(prevPresetBtn);
    prevPresetBtn.setButtonText(juce::String::charToString(0x25C0));
    prevPresetBtn.setColour(juce::TextButton::buttonColourId, synthcol::surfHi);

    addAndMakeVisible(nextPresetBtn);
    nextPresetBtn.setButtonText(juce::String::charToString(0x25B6));
    nextPresetBtn.setColour(juce::TextButton::buttonColourId, synthcol::surfHi);

    addAndMakeVisible(savePresetBtn);
    savePresetBtn.setButtonText("Save");

    addAndMakeVisible(saveAsPresetBtn);
    saveAsPresetBtn.setButtonText("Save As");

    addAndMakeVisible(deletePresetBtn);
    deletePresetBtn.setButtonText("Delete");

    addAndMakeVisible(importPresetsBtn);
    importPresetsBtn.setButtonText("Import");

    // ---- Gain dial ----
    addAndMakeVisible(gainDial);
    setupSmallDial(gainDial, accent_);
    gainDial.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainAtt_ = std::make_unique<SliderAttach>(apvts, "output_gain", gainDial);

    // ---- Single note toggle (not added here — derived class chooses to addAndMakeVisible if needed) ----
    singleBtn.setButtonText("SINGLE");
    singleBtn.setClickingTogglesState(true);

    // ---- Instrument selector labels ----
    familySelectorLbl.setJustificationType(juce::Justification::centredLeft);
    familySelectorLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f).withStyle("Bold")));
    familySelectorLbl.setColour(juce::Label::textColourId, synthcol::textSec);
    addAndMakeVisible(familySelectorLbl);

    modelSelectorLbl.setJustificationType(juce::Justification::centredLeft);
    modelSelectorLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f).withStyle("Bold")));
    modelSelectorLbl.setColour(juce::Label::textColourId, synthcol::textSec);
    addAndMakeVisible(modelSelectorLbl);

    addAndMakeVisible(familySelector);
    addAndMakeVisible(modelSelector);

    // ---- Keyboard ----
    keyboard = std::make_unique<PlayableRangeKeyboard>(
        kbState, juce::MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setAvailableRange(kbLow, kbHigh);
    keyboard->setPlayableRange(kbLow, kbHigh);
    keyboard->setKeyWidth(kbKeyWidth);
    keyboard->setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                        juce::Colour(0xffE8E8EC));  // neutral white keys
    keyboard->setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                        juce::Colour(0xff1A1A22));  // neutral dark black keys
    keyboard->setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                        synthcol::ink);
    keyboard->setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                        accent_.withAlpha(0.55f));
    keyboard->setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                        accent_.withAlpha(0.20f));
    addAndMakeVisible(*keyboard);

    addAndMakeVisible(octaveDownBtn);
    octaveDownBtn.setButtonText(juce::CharPointer_UTF8("\xe2\x97\x84")); // ◄
    octaveDownBtn.onClick = [this] {
        int lo = keyboard->getRangeStart() - 12;
        int hi = keyboard->getRangeEnd()   - 12;
        if (lo >= 12) keyboard->setAvailableRange(lo, hi);
    };

    addAndMakeVisible(octaveUpBtn);
    octaveUpBtn.setButtonText(juce::CharPointer_UTF8("\xe2\x96\xba")); // ►
    octaveUpBtn.onClick = [this] {
        int lo = keyboard->getRangeStart() + 12;
        int hi = keyboard->getRangeEnd()   + 12;
        if (hi <= 108) keyboard->setAvailableRange(lo, hi);
    };
}

CommonSynthEditor::~CommonSynthEditor()
{
    setLookAndFeel(nullptr);
}

void CommonSynthEditor::setAccentTheme(juce::Colour accent)
{
    if (accent_ == accent)
        return;

    accent_ = accent;
    lnf_.setAccent(accent_);
    gainDial.setColour(juce::Slider::rotarySliderFillColourId, accent_);

    if (keyboard != nullptr)
    {
        keyboard->setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                            accent_.withAlpha(0.55f));
        keyboard->setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                            accent_.withAlpha(0.20f));
    }

    repaint();
}

void CommonSynthEditor::setChromePalette(juce::Colour headerTint,
                                         juce::Colour panelBaseTint,
                                         juce::Colour panelCavityTint,
                                         juce::Colour panelHeaderTint,
                                         juce::Colour keyboardTint)
{
    chromePalette_.headerTint = headerTint;
    chromePalette_.panelBaseTint = panelBaseTint;
    chromePalette_.panelCavityTint = panelCavityTint;
    chromePalette_.panelHeaderTint = panelHeaderTint;
    chromePalette_.keyboardTint = keyboardTint;
    repaint();
}

void CommonSynthEditor::setHeaderLogo(juce::Image logo)
{
    headerLogoImage_ = std::move(logo);
    repaint();
}

void CommonSynthEditor::setKeyboardPlayableRange(int lowNote, int highNote)
{
    if (keyboard != nullptr)
        keyboard->setPlayableRange(lowNote, highNote);
}

// =============================================================================
// initCommon — call at end of derived constructor
// =============================================================================
void CommonSynthEditor::initCommon()
{
    // Wire preset box callbacks (needs virtual table to be derived class's)
    presetBox.onChange = [this] {
        presetBox.setTooltip(presetBox.getText());

        const auto itemId = presetBox.getSelectedId();
        if (isFactoryPresetItemId(itemId))
        {
            const auto factoryIdx = itemId - kFactoryPresetItemIdBase;
            if (factoryIdx >= 0 && factoryIdx < factoryPresetNames_.size())
                hostApplyFactory(factoryIdx);
            return;
        }

        if (isUserPresetItemId(itemId))
        {
            const auto userIdx = itemId - kUserPresetItemIdBase;
            if (userIdx >= 0 && userIdx < userPresetFiles_.size())
                hostLoadUser(userPresetFiles_[userIdx]);
        }
    };

    presetSearch.onTextChange = [this] {
        applyPresetFilter(presetSearch.getText());
    };

    prevPresetBtn.onClick = [this] { navigatePreset(-1); };
    nextPresetBtn.onClick = [this] { navigatePreset(1); };

    savePresetBtn.onClick   = [this] { saveCurrentPreset(); };
    saveAsPresetBtn.onClick = [this] {
        auto defaultName = hostIsUserPreset()
            ? hostCurrentUserFile().getFileNameWithoutExtension()
            : juce::String("Mon Preset");
        showSaveAsDialog(defaultName);
    };
    deletePresetBtn.onClick = [this] { deleteCurrentUserPreset(); };

    importPresetsBtn.onClick = [this] { importPresetsFromZip(); };

    refreshPresetList();
}

// =============================================================================
// syncPresetBox — call from derived timerCallback
// =============================================================================
void CommonSynthEditor::syncPresetBox()
{
    if (hostIsUserPreset())
    {
        auto currentFile = hostCurrentUserFile();
        auto userIdx = userPresetFiles_.indexOf(currentFile);
        auto targetId = makeUserPresetItemId(userIdx);
        if (userIdx >= 0 && presetBox.getSelectedId() != targetId)
            presetBox.setSelectedId(targetId, juce::dontSendNotification);
    }
    else
    {
        auto pi = hostCurrentFactoryIdx();
        auto targetId = makeFactoryPresetItemId(pi);
        if (pi >= 0 && presetBox.getSelectedId() != targetId)
            presetBox.setSelectedId(targetId, juce::dontSendNotification);
    }

    presetBox.setTooltip(presetBox.getText());

    deletePresetBtn.setEnabled(hostIsUserPreset());
}

// =============================================================================
// Paint helpers
// =============================================================================
void CommonSynthEditor::paintBackground(juce::Graphics& g) const
{
    g.fillAll(synthcol::bg);

    if (backgroundImage_.isValid())
    {
        g.setOpacity(0.34f);
        g.drawImage(backgroundImage_, getLocalBounds().toFloat(),
                    juce::RectanglePlacement::stretchToFit);
        g.setOpacity(1.0f);

        juce::ColourGradient readability(
            juce::Colours::black.withAlpha(0.28f), 0.0f, 0.0f,
            juce::Colours::black.withAlpha(0.42f), 0.0f, static_cast<float>(getHeight()), false);
        g.setGradientFill(readability);
        g.fillRect(getLocalBounds());

        juce::ColourGradient centreDim(
            juce::Colours::transparentBlack, static_cast<float>(getWidth()) * 0.5f, static_cast<float>(getHeight()) * 0.42f,
            juce::Colours::black.withAlpha(0.22f), static_cast<float>(getWidth()) * 1.05f, static_cast<float>(getHeight()) * 0.42f, true);
        g.setGradientFill(centreDim);
        g.fillRect(getLocalBounds());
    }
    else
    {
        auto bounds = getLocalBounds().toFloat();
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY() * 0.80f;
        float r = juce::jmax(bounds.getWidth(), bounds.getHeight()) * 0.65f;
        juce::ColourGradient lit(
            juce::Colour(0xff1A1D24), cx, cy,
            synthcol::bg, cx + r, cy, true);
        g.setGradientFill(lit);
        g.fillRect(getLocalBounds());
    }

    // Fine procedural grain overlay
    if (bgTexture_.isValid())
    {
        g.setOpacity(backgroundImage_.isValid() ? 0.004f : 0.010f);
        g.setTiledImageFill(bgTexture_, 0, 0, 1.0f);
        g.fillRect(getLocalBounds());
        g.setOpacity(1.0f);
    }

    {
        auto bounds = getLocalBounds().toFloat();
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float r = juce::jmax(bounds.getWidth(), bounds.getHeight()) * 0.70f;
        juce::ColourGradient edge(
            juce::Colours::transparentBlack, cx, cy,
            juce::Colours::black.withAlpha(backgroundImage_.isValid() ? 0.34f : 0.40f), cx + r, cy, true);
        edge.addColour(0.50, juce::Colours::transparentBlack);
        g.setGradientFill(edge);
        g.fillRect(getLocalBounds());
    }
}

CommonSynthEditor::HeaderZones CommonSynthEditor::computeHeaderZones(int headerH) const
{
    HeaderZones zones;

    const int safeHeaderH = juce::jmax(64, headerH);
    zones.headerBounds = { 12, 10, juce::jmax(240, getWidth() - 24), juce::jmax(44, safeHeaderH - 14) };
    zones.contentBounds = zones.headerBounds.reduced(14, 8);

    const int zoneGap = 12;
    const int contentW = zones.contentBounds.getWidth();
    const int identityW = juce::jlimit(196, 236, static_cast<int>(std::round(contentW * 0.18f)));
    const int statusW = juce::jlimit(284, 376, static_cast<int>(std::round(contentW * 0.28f)));
    const int presetW = juce::jmax(320, contentW - identityW - statusW - zoneGap * 2);

    auto strip = zones.contentBounds;
    zones.identityZone = strip.removeFromLeft(identityW);
    strip.removeFromLeft(zoneGap);
    zones.presetZone = strip.removeFromLeft(juce::jmin(presetW, strip.getWidth()));
    strip.removeFromLeft(juce::jmin(zoneGap, strip.getWidth()));
    zones.statusZone = strip;

    auto presetZoneRows = zones.presetZone;
    zones.presetPrimaryRow = presetZoneRows.removeFromTop(32);
    zones.presetSecondaryRow = zones.presetZone.withTrimmedTop(36);

    auto statusZoneRows = zones.statusZone;
    zones.statusPrimaryRow = statusZoneRows.removeFromTop(24);
    zones.statusSecondaryRow = zones.statusZone.withTrimmedTop(28);

    return zones;
}

void CommonSynthEditor::paintHeader(juce::Graphics& g, int headerH) const
{
    const auto zones = computeHeaderZones(headerH);
    auto headerBounds = zones.headerBounds.toFloat();
    const auto headerTint = chromePalette_.headerTint.isTransparent() ? accent_ : chromePalette_.headerTint;

    g.setColour(juce::Colours::black.withAlpha(0.14f));
    g.fillRoundedRectangle(headerBounds.translated(0.0f, 3.0f), 10.0f);
    fillSharedPanel(g, headerBounds, 10.0f,
                    synthcol::surface.withAlpha(0.50f).interpolatedWith(headerTint.withAlpha(0.16f), 0.26f),
                    true);

    auto accentLane = juce::Rectangle<float>(headerBounds.getX() + 5.0f, headerBounds.getBottom() - 3.0f,
                                             76.0f, 1.6f);
    drawGlowStrip(g, accentLane, accent_, 0.7f, 0.18f);

    auto titleWell = zones.identityZone.toFloat();
    titleWell.removeFromBottom(16.0f);
    titleWell.setHeight(28.0f);
    fillSharedPanel(g, titleWell, 6.0f,
                    synthcol::surfHi.withAlpha(0.16f).interpolatedWith(headerTint.withAlpha(0.12f), 0.38f),
                    false);

    auto titleSheen = titleWell;
    titleSheen.setHeight(titleSheen.getHeight() * 0.48f);
    juce::ColourGradient titleSheenGrad(juce::Colours::white.withAlpha(0.035f), titleSheen.getCentreX(), titleSheen.getY(),
                                        juce::Colours::transparentWhite, titleSheen.getCentreX(), titleSheen.getBottom(), false);
    g.setGradientFill(titleSheenGrad);
    g.fillRoundedRectangle(titleSheen, 6.0f);

    auto presetPrimary = zones.presetPrimaryRow.toFloat();
    auto presetSecondary = zones.presetSecondaryRow.toFloat();
    auto statusPrimary = zones.statusPrimaryRow.toFloat();
    auto statusSecondary = zones.statusSecondaryRow.toFloat();

    fillSharedRecess(g, presetPrimary, 7.0f);
    fillSharedPanel(g, presetSecondary, 7.0f,
                    synthcol::surfHi.withAlpha(0.14f).interpolatedWith(headerTint.withAlpha(0.10f), 0.32f),
                    false);
    fillSharedPanel(g, statusPrimary, 7.0f,
                    synthcol::surfHi.withAlpha(0.14f).interpolatedWith(headerTint.withAlpha(0.12f), 0.34f),
                    false);
    fillSharedRecess(g, statusSecondary, 7.0f);

    g.setColour(headerTint.withAlpha(0.035f));
    g.fillRoundedRectangle(presetPrimary.withWidth(juce::jmin(260.0f, presetPrimary.getWidth() * 0.34f)), 7.0f);
    g.fillRoundedRectangle(statusPrimary.withWidth(juce::jmin(150.0f, statusPrimary.getWidth() * 0.46f)), 7.0f);
    g.setColour(synthcol::border.withAlpha(0.20f));
    g.drawRoundedRectangle(presetPrimary.reduced(0.5f), 7.0f, 0.75f);
    g.drawRoundedRectangle(presetSecondary.reduced(0.5f), 7.0f, 0.75f);
    g.drawRoundedRectangle(statusPrimary.reduced(0.5f), 7.0f, 0.75f);
    g.drawRoundedRectangle(statusSecondary.reduced(0.5f), 7.0f, 0.75f);

    auto accentGlow = headerBounds.reduced(22.0f, 0.0f);
    accentGlow = accentGlow.removeFromBottom(2.2f);
    drawGlowStrip(g, accentGlow, accent_, 0.8f, 0.14f);

    if (headerLogoImage_.isValid())
    {
        const int logoX = static_cast<int>(titleWell.getX()) - 2;
        const int logoY = static_cast<int>(titleWell.getY()) - 5;
        const int logoW = static_cast<int>(juce::jmin(208.0f, titleWell.getWidth() + 56.0f));
        const int logoH = static_cast<int>(titleWell.getHeight() + 22.0f);
        g.setOpacity(1.0f);
        g.drawImageWithin(headerLogoImage_, logoX, logoY, logoW, logoH,
                             juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize,
                          false);
    }
    else
    {
        const auto titleArea = titleWell.reduced(10.0f, 0.0f).toNearestInt();
        g.setColour(synthcol::text.withAlpha(0.96f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(16.2f).withStyle("Bold")));
        g.drawText(pluginTitle(), titleArea, juce::Justification::centredLeft);

        auto titleUnderline = juce::Rectangle<float>(titleWell.getX() + 12.0f, titleWell.getBottom() - 3.0f,
                                                     juce::jmin(84.0f, titleWell.getWidth() * 0.36f), 1.4f);
        drawGlowStrip(g, titleUnderline, accent_, 0.8f, kGlowNormal);

        if (pluginNamespace().isNotEmpty())
        {
            g.setColour(synthcol::textSec.withAlpha(0.58f));
            g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
            g.drawText(pluginNamespace(),
                       juce::Rectangle<int>(titleArea.getX(), zones.identityZone.getBottom() - 18,
                                            juce::jmin(140, titleArea.getWidth()), 14),
                       juce::Justification::centredLeft);
        }
    }
}

void CommonSynthEditor::paintCard(juce::Graphics& g,
                                  int x, int y, int cw, int ch,
                                  const juce::String& title) const
{
    constexpr float cr = 8.0f;
    auto card = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                       static_cast<float>(cw), static_cast<float>(ch));

    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(card.translated(0.0f, 4.0f), cr);
    fillPanelBackdrop(g, card, cr, accent_, true, chromePalette_.panelBaseTint);

    auto facePlate = card.reduced(8.0f, 8.0f);
    facePlate.removeFromTop(22.0f);
    fillPanelCavity(g, facePlate, 7.0f, accent_, chromePalette_.panelCavityTint);
    g.setColour(juce::Colours::white.withAlpha(0.022f));
    g.drawRoundedRectangle(facePlate.reduced(0.8f), 7.0f, 0.8f);
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.drawRoundedRectangle(facePlate.expanded(0.2f), 7.4f, 0.9f);

    auto controlDeck = facePlate.reduced(6.0f, 7.0f);
    controlDeck.removeFromTop(1.0f);
    const auto deckTint = chromePalette_.panelCavityTint.isTransparent() ? accent_ : chromePalette_.panelCavityTint;
    juce::ColourGradient deckGrad(blendTint(juce::Colour(0xff2A3038), deckTint, 0.16f), controlDeck.getCentreX(), controlDeck.getY(),
                                  blendTint(juce::Colour(0xff171B21), deckTint, 0.08f), controlDeck.getCentreX(), controlDeck.getBottom(), false);
    deckGrad.addColour(0.20, blendTint(juce::Colour(0xff242A32), deckTint, 0.14f));
    deckGrad.addColour(0.52, blendTint(juce::Colour(0xff1F242B), deckTint, 0.10f));
    deckGrad.addColour(0.82, blendTint(juce::Colour(0xff161A1F), deckTint, 0.06f));
    g.setGradientFill(deckGrad);
    g.fillRoundedRectangle(controlDeck, 5.8f);

    {
        juce::Graphics::ScopedSaveState scopedState(g);
        auto brushedDeck = controlDeck.reduced(4.0f);
        g.reduceClipRegion(brushedDeck.toNearestInt());
        auto addCloud = [&](juce::Point<float> centre, float radiusX, float radiusY, juce::Colour colour)
        {
            juce::ColourGradient cloud(colour, centre.x, centre.y,
                                       juce::Colours::transparentBlack, centre.x + radiusX, centre.y + radiusY, true);
            g.setGradientFill(cloud);
            g.fillEllipse(centre.x - radiusX, centre.y - radiusY, radiusX * 2.0f, radiusY * 2.0f);
        };
        addCloud({ brushedDeck.getX() + brushedDeck.getWidth() * 0.30f, brushedDeck.getY() + brushedDeck.getHeight() * 0.20f },
             brushedDeck.getWidth() * 0.34f, brushedDeck.getHeight() * 0.18f,
               juce::Colours::white.withAlpha(0.014f));
        addCloud({ brushedDeck.getX() + brushedDeck.getWidth() * 0.75f, brushedDeck.getY() + brushedDeck.getHeight() * 0.32f },
             brushedDeck.getWidth() * 0.28f, brushedDeck.getHeight() * 0.18f,
               juce::Colours::white.withAlpha(0.009f));
        addCloud({ brushedDeck.getCentreX(), brushedDeck.getBottom() - brushedDeck.getHeight() * 0.10f },
             brushedDeck.getWidth() * 0.42f, brushedDeck.getHeight() * 0.14f,
               juce::Colours::black.withAlpha(0.10f));
    }

        auto deckBezel = controlDeck.reduced(1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.026f));
        g.drawRoundedRectangle(deckBezel, 5.0f, 0.7f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.drawRoundedRectangle(deckBezel.reduced(1.1f), 4.3f, 0.7f);

        auto deckSheen = controlDeck.reduced(14.0f, 8.0f).removeFromTop(juce::jlimit(8.0f, 12.0f, controlDeck.getHeight() * 0.07f));
        juce::ColourGradient deckSheenGrad(juce::Colours::white.withAlpha(0.038f), deckSheen.getCentreX(), deckSheen.getY(),
                                       juce::Colours::transparentWhite, deckSheen.getCentreX(), deckSheen.getBottom(), false);
    g.setGradientFill(deckSheenGrad);
    g.fillRoundedRectangle(deckSheen, 3.2f);

        auto deckLowerShade = controlDeck.reduced(10.0f, 0.0f).removeFromBottom(12.0f);
    juce::ColourGradient deckLowerShadeGrad(juce::Colours::transparentBlack,
                                            deckLowerShade.getCentreX(), deckLowerShade.getY(),
                            juce::Colours::black.withAlpha(0.05f),
                                            deckLowerShade.getCentreX(), deckLowerShade.getBottom(), false);
    g.setGradientFill(deckLowerShadeGrad);
    g.fillRoundedRectangle(deckLowerShade, 3.0f);

    auto lowerShadowBand = facePlate.reduced(8.0f, 0.0f).removeFromBottom(14.0f);
    juce::ColourGradient lowerShadow(juce::Colours::transparentBlack,
                                     lowerShadowBand.getCentreX(), lowerShadowBand.getY(),
                                     juce::Colours::black.withAlpha(0.07f),
                                     lowerShadowBand.getCentreX(), lowerShadowBand.getBottom(), false);
    g.setGradientFill(lowerShadow);
    g.fillRoundedRectangle(lowerShadowBand, 5.0f);

    auto titleBand = card.reduced(10.0f, 6.0f).removeFromTop(18.0f);
    auto titlePlate = titleBand;
    const auto titleTint = chromePalette_.panelHeaderTint.isTransparent() ? accent_ : chromePalette_.panelHeaderTint;
    juce::ColourGradient titlePlateGrad(blendTint(juce::Colour(0xff272C35), titleTint, 0.24f), titlePlate.getCentreX(), titlePlate.getY(),
                                        blendTint(juce::Colour(0xff101317), titleTint, 0.12f), titlePlate.getCentreX(), titlePlate.getBottom(), false);
    titlePlateGrad.addColour(0.45, blendTint(juce::Colour(0xff1A1F26), titleTint, 0.18f));
    g.setGradientFill(titlePlateGrad);
    g.fillRoundedRectangle(titlePlate, 5.5f);
    g.setColour(juce::Colours::white.withAlpha(0.018f));
    g.drawRoundedRectangle(titlePlate.reduced(0.8f), 5.5f, 0.7f);
    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.drawRoundedRectangle(titlePlate.expanded(0.2f), 5.8f, 0.9f);

    auto titleTrace = titlePlate.reduced(14.0f, 0.0f).removeFromBottom(1.8f);
    drawGlowStrip(g, titleTrace.withWidth(juce::jmin(96.0f, titleTrace.getWidth() * 0.22f)), accent_, 0.7f, 0.12f);

    g.setColour(synthcol::text.withAlpha(0.92f));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.4f).withStyle("Bold")));
    g.drawText(title, titlePlate.reduced(14.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);

    auto divider = juce::Rectangle<float>(card.getX() + 12.0f, titlePlate.getBottom() + 6.0f,
                                          card.getWidth() - 24.0f, 2.0f);
    juce::ColourGradient dividerGrad(juce::Colours::white.withAlpha(0.010f), divider.getCentreX(), divider.getY(),
                                     juce::Colours::black.withAlpha(0.07f), divider.getCentreX(), divider.getBottom(), false);
    g.setGradientFill(dividerGrad);
    g.fillRoundedRectangle(divider, 1.0f);
}

void CommonSynthEditor::paintSection(juce::Graphics& g,
                                     int x, int y, int w, int h,
                                     const juce::String& label,
                                     bool fillPanel) const
{
    constexpr float cr = 6.0f;

    if (fillPanel)
    {
        auto panel = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                            static_cast<float>(w), static_cast<float>(h));
        fillPanelBackdrop(g, panel, cr, accent_, false, chromePalette_.panelBaseTint);
    }

    constexpr int labH = 16;
    int labY = y - labH - 4;
    auto badge = juce::Rectangle<float>(static_cast<float>(x + 2), static_cast<float>(labY),
                                        static_cast<float>(juce::jmin(w, 140)), static_cast<float>(labH));
    juce::ColourGradient badgeGrad(juce::Colour(0xff2C313B), badge.getCentreX(), badge.getY(),
                                   juce::Colour(0xff101318), badge.getCentreX(), badge.getBottom(), false);
    g.setGradientFill(badgeGrad);
    g.fillRoundedRectangle(badge, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.034f));
    g.drawRoundedRectangle(badge.reduced(0.6f), 4.0f, 0.6f);
    g.setColour(accent_.withAlpha(0.36f));
    g.fillRoundedRectangle(badge.reduced(8.0f, 0.0f).removeFromBottom(2.0f), 1.0f);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.8f).withStyle("Bold")));
    g.setColour(synthcol::textSec.withAlpha(0.92f));
    g.drawText(label, badge.reduced(10.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
}

void CommonSynthEditor::paintStatusChip(juce::Graphics& g,
                                        juce::Rectangle<int> area,
                                        const juce::String& text,
                                        juce::Colour fill,
                                        juce::Colour outline) const
{
    auto r = area.toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 6.0f);

    auto chipBase = fill.interpolatedWith(synthcol::surface, 0.28f);
    juce::ColourGradient chipGrad(chipBase.brighter(0.03f), r.getCentreX(), r.getY(),
                                  chipBase.darker(0.10f), r.getCentreX(), r.getBottom(), false);
    chipGrad.addColour(0.46, chipBase);
    g.setGradientFill(chipGrad);
    g.fillRoundedRectangle(r, 6.0f);

    auto sheen = r.withHeight(r.getHeight() * 0.48f);
    juce::ColourGradient sheenGrad(juce::Colours::white.withAlpha(0.04f), sheen.getCentreX(), sheen.getY(),
                                   juce::Colours::transparentWhite, sheen.getCentreX(), sheen.getBottom(), false);
    g.setGradientFill(sheenGrad);
    g.fillRoundedRectangle(sheen, 6.0f);

    g.setColour(outline.withAlpha(0.52f));
    g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 0.85f);
    g.setColour(synthcol::textSec);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.4f).withStyle("Bold")));
    g.drawText(text, area, juce::Justification::centred);
}

void CommonSynthEditor::paintModePill(juce::Graphics& g,
                                      juce::Rectangle<int> area,
                                      const juce::String& text,
                                      juce::Colour fill,
                                      juce::Colour outline) const
{
    auto r = area.toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.14f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 8.0f);

    auto pillBase = fill.interpolatedWith(accent_, 0.12f);
    juce::ColourGradient grad(pillBase.withAlpha(0.92f), r.getX(), r.getCentreY(),
                              pillBase.darker(0.10f).withAlpha(0.94f), r.getRight(), r.getCentreY(), false);
    grad.addColour(0.55, pillBase.brighter(0.03f).withAlpha(0.94f));
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, 8.0f);

    g.setColour(outline.withAlpha(0.82f));
    g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 0.9f);
    g.setColour(synthcol::text);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.8f).withStyle("Bold")));
    g.drawText(text, area, juce::Justification::centred);
}

void CommonSynthEditor::paintMeterBar(juce::Graphics& g,
                                      juce::Rectangle<int> area,
                                      float level,
                                      juce::Colour colour) const
{
    auto r = area.toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 4.0f);

    juce::ColourGradient bgGrad(synthcol::bg.brighter(0.05f), r.getCentreX(), r.getY(),
                                synthcol::surface.darker(0.18f), r.getCentreX(), r.getBottom(), false);
    bgGrad.addColour(0.46, synthcol::surface.withAlpha(0.92f));
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(synthcol::border.withAlpha(0.42f));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);

    const auto clamped = juce::jlimit(0.0f, 1.0f, level);
    constexpr int segments = 12;
    constexpr float gap = 2.0f;
    const float segW = (r.getWidth() - gap * (segments - 1)) / static_cast<float>(segments);
    const int active = static_cast<int>(std::round(clamped * static_cast<float>(segments)));

    for (int i = 0; i < segments; ++i)
    {
        auto seg = juce::Rectangle<float>(r.getX() + i * (segW + gap), r.getY() + 1.5f,
                                          segW, r.getHeight() - 3.0f);
        const bool lit = i < active;
        g.setColour(lit ? colour.withAlpha(0.90f) : synthcol::border.withAlpha(0.34f));
        g.fillRoundedRectangle(seg, 1.2f);
        if (lit)
        {
            g.setColour(colour.brighter(0.35f).withAlpha(0.26f));
            g.fillRoundedRectangle(seg.expanded(0.5f, 0.2f), 1.4f);
        }
    }
}

void CommonSynthEditor::paintKeyboardDock(juce::Graphics& g,
                                          int x, int y, int w, int h) const
{
    auto dock = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                       static_cast<float>(w), static_cast<float>(h));
    constexpr float cr = 10.0f;
    const auto keyboardTint = chromePalette_.keyboardTint.isTransparent()
        ? (chromePalette_.panelCavityTint.isTransparent() ? accent_ : chromePalette_.panelCavityTint)
        : chromePalette_.keyboardTint;

    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(dock.translated(0.0f, 4.0f), cr);
    fillPanelBackdrop(g, dock, cr, accent_, true, keyboardTint);

    auto topRail = dock.reduced(10.0f, 8.0f);
    topRail = topRail.removeFromTop(16.0f);
    fillPanelCavity(g, topRail, 6.0f, accent_, keyboardTint);
    auto accentStrip = topRail.reduced(14.0f, 0.0f);
    accentStrip.setWidth(juce::jmin(92.0f, accentStrip.getWidth() * 0.30f));
    accentStrip = accentStrip.removeFromBottom(2.0f);
    drawGlowStrip(g, accentStrip, accent_, 1.4f, 0.14f);

    auto cavity = dock.reduced(6.0f);
    auto serviceBay = cavity.removeFromLeft(60.0f);
    fillPanelCavity(g, serviceBay, cr - 3.0f, accent_, keyboardTint);
    fillPanelCavity(g, cavity, cr - 2.0f, accent_, keyboardTint);
    g.setColour(accent_.withAlpha(0.014f));
    g.fillRoundedRectangle(cavity.withTrimmedBottom(cavity.getHeight() * 0.48f), cr - 2.0f);
    g.setColour(synthcol::border.withAlpha(0.14f));
    g.drawRoundedRectangle(cavity.reduced(0.5f), cr - 2.5f, 0.8f);

    auto serviceText = serviceBay.reduced(6.0f, 6.0f);
    g.setColour(synthcol::textSec.withAlpha(0.64f));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.4f).withStyle("Bold")));
    g.drawFittedText("OCT", serviceText.removeFromTop(13.0f).toNearestInt(), juce::Justification::centred, 1);
    auto serviceUnderline = juce::Rectangle<float>(serviceBay.getX() + 14.0f, serviceBay.getY() + 18.0f,
                                                   serviceBay.getWidth() - 28.0f, 1.6f);
    drawGlowStrip(g, serviceUnderline, accent_, 0.9f, 0.11f);

    auto buttonWell = serviceBay.reduced(9.0f, 22.0f);
    fillPanelCavity(g, buttonWell, 5.0f, accent_, keyboardTint);
    g.setColour(accent_.withAlpha(0.06f));
    g.drawRoundedRectangle(buttonWell.reduced(0.5f), 5.0f, 0.8f);

    auto bayAccent = serviceBay.reduced(14.0f, 0.0f).removeFromBottom(3.0f);
    bayAccent.setWidth(juce::jmax(16.0f, bayAccent.getWidth() * 0.52f));
    drawGlowStrip(g, bayAccent, accent_, 0.9f, 0.10f);
}

// =============================================================================
// Dial helpers
// =============================================================================
void CommonSynthEditor::setupDial(juce::Slider& s, juce::Colour fill)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 16);
    s.setColour(juce::Slider::rotarySliderFillColourId, fill);
    s.setNumDecimalPlacesToDisplay(2);
    s.setDoubleClickReturnValue(true, 0.5);
}

void CommonSynthEditor::setupSmallDial(juce::Slider& s, juce::Colour fill)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    s.setColour(juce::Slider::rotarySliderFillColourId, fill);
    s.setNumDecimalPlacesToDisplay(2);
    s.setDoubleClickReturnValue(true, 0.5);
}

void CommonSynthEditor::setupGrandDial(juce::Slider& s, juce::Colour fill,
                                       const juce::String& suffix)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 112, 20);
    s.setColour(juce::Slider::rotarySliderFillColourId, fill);
    s.setNumDecimalPlacesToDisplay(1);
    s.setDoubleClickReturnValue(true, 0.5);
    if (suffix.isNotEmpty()) s.setTextValueSuffix(suffix);
}

// =============================================================================
// Chrome layout
// =============================================================================
CommonSynthEditor::ChromeLayout
CommonSynthEditor::resizeChrome(int presetsStartX, int selectorH, int kbH)
{
    juce::ignoreUnused(presetsStartX);

    constexpr int headerH = 80;
    constexpr int margin  = 24;
    constexpr int gutter  = 16;

    const int w    = getWidth();
    const int h    = getHeight();
    const int kbY  = h - kbH;
    const auto headerZones = computeHeaderZones(headerH);

    // ---- Header row ----
    const int ctrlH   = 30;
    const auto presetPrimary = headerZones.presetPrimaryRow.reduced(0, 1);
    const auto presetSecondary = headerZones.presetSecondaryRow.reduced(0, 1);
    const auto statusPrimary = headerZones.statusPrimaryRow;
    const int topRowY = presetPrimary.getY() + (presetPrimary.getHeight() - ctrlH) / 2;

    // Gain dial (far right within status zone)
    int rightX = headerZones.statusZone.getRight() - 54;
    gainDial.setBounds(rightX, headerZones.contentBounds.getY() - 1, 54, 54);

    // Single button (if visible)
    rightX -= 68;
    singleBtn.setBounds(rightX, statusPrimary.getY() + 1, 64, 24);

    // Preset search
    const int searchW = juce::jlimit(120, 180, presetPrimary.getWidth() / 4);
    int       x       = presetPrimary.getX();
    presetSearch.setBounds(x, topRowY, searchW, ctrlH);
    x += searchW + 8;

    // Nav + preset box
    const int navW    = 28;
    const int presetBoxW = juce::jmax(180, presetPrimary.getRight() - x - navW * 2 - 8);
    prevPresetBtn.setBounds(x, topRowY, navW, ctrlH);
    x += navW + 4;
    presetBox.setBounds(x, topRowY, presetBoxW, ctrlH);
    x += presetBoxW + 4;
    nextPresetBtn.setBounds(x, topRowY, navW, ctrlH);

    // Save / SaveAs / Delete
    const int btnY = presetSecondary.getY() + juce::jmax(0, (presetSecondary.getHeight() - 22) / 2);
    const int btnX = presetSecondary.getX();
    savePresetBtn.setBounds(  btnX,        btnY, 64, 22);
    saveAsPresetBtn.setBounds(btnX + 72,   btnY, 78, 22);
    deletePresetBtn.setBounds(btnX + 158,  btnY, 70, 22);
    importPresetsBtn.setBounds(btnX + 236, btnY, 70, 22);

    // ---- Selector row ----
    const int selectorY = margin + headerH + 8;
    {
        const int selPad = 12;
        const int fieldW = (w - margin * 2 - selPad * 3) / 2;
        familySelectorLbl.setBounds(margin + selPad, selectorY + 6,  fieldW, 14);
        familySelector.setBounds(   margin + selPad, selectorY + 22, fieldW, 28);
        modelSelectorLbl.setBounds( margin + selPad * 2 + fieldW, selectorY + 6,  fieldW, 14);
        modelSelector.setBounds(    margin + selPad * 2 + fieldW, selectorY + 22, fieldW, 28);
    }

    // ---- Keyboard ----
    const int kbCtrlW = 56;
    keyboard->setBounds(margin + kbCtrlW, kbY, w - margin * 2 - kbCtrlW, kbH);
    octaveDownBtn.setBounds(margin + 6,  kbY + kbH / 2 - 14, 24, 26);
    octaveUpBtn.setBounds(  margin + 32, kbY + kbH / 2 - 14, 24, 26);

    return {
        headerH,
        selectorY, selectorH,
        selectorY + selectorH + gutter,
        kbY, kbH,
        margin, gutter
    };
}

// =============================================================================
// Preset management
// =============================================================================
void CommonSynthEditor::refreshPresetList()
{
    presetBox.clear(juce::dontSendNotification);

    factoryPresetNames_ = hostGetFactoryNames();
    factoryPresetCount_ = factoryPresetNames_.size();

    for (int i = 0; i < factoryPresetCount_; ++i)
    {
        if (!hostShouldIncludeFactoryPreset(i))
            continue;
        presetBox.addItem(hostFormatFactoryPresetLabel(i, factoryPresetNames_[i]),
                          makeFactoryPresetItemId(i));
    }

    userPresetFiles_ = hostScanUserPresets();

    if (!userPresetFiles_.isEmpty())
    {
        bool addedSeparator = false;
        for (int i = 0; i < userPresetFiles_.size(); ++i)
        {
            const auto& file = userPresetFiles_.getReference(i);
            if (!hostShouldIncludeUserPreset(file))
                continue;
            if (!addedSeparator)
            {
                presetBox.addSeparator();
                addedSeparator = true;
            }
            presetBox.addItem(hostFormatUserPresetLabel(file, file.getFileNameWithoutExtension()),
                              makeUserPresetItemId(i));
        }
    }

    if (hostIsUserPreset())
    {
        auto idx = userPresetFiles_.indexOf(hostCurrentUserFile());
        if (idx >= 0)
            presetBox.setSelectedId(makeUserPresetItemId(idx),
                                    juce::dontSendNotification);
    }
    else
    {
        auto fi = hostCurrentFactoryIdx();
        if (fi >= 0 && fi < factoryPresetCount_)
            presetBox.setSelectedId(makeFactoryPresetItemId(fi),
                                    juce::dontSendNotification);
    }

    if (presetSearch.getText().isNotEmpty())
        applyPresetFilter(presetSearch.getText());

    presetBox.setTooltip(presetBox.getText());
}

void CommonSynthEditor::applyPresetFilter(const juce::String& query)
{
    auto q = query.trim().toLowerCase();
    presetBox.clear(juce::dontSendNotification);

    for (int i = 0; i < factoryPresetNames_.size(); ++i)
    {
        if (!hostShouldIncludeFactoryPreset(i))
            continue;

        const auto& n = factoryPresetNames_[i];
        const auto searchable = hostFactoryPresetSearchText(i, n).toLowerCase();
        if (q.isEmpty() || searchable.contains(q))
            presetBox.addItem(hostFormatFactoryPresetLabel(i, n), makeFactoryPresetItemId(i));
    }

    bool hasVisibleUserPreset = false;
    for (int i = 0; i < userPresetFiles_.size(); ++i)
    {
        const auto& file = userPresetFiles_.getReference(i);
        if (!hostShouldIncludeUserPreset(file))
            continue;
        const auto n = file.getFileNameWithoutExtension();
        const auto searchable = hostUserPresetSearchText(file, n).toLowerCase();
        if (q.isEmpty() || searchable.contains(q))
            hasVisibleUserPreset = true;
    }

    if (hasVisibleUserPreset)
    {
        presetBox.addSeparator();
        for (int i = 0; i < userPresetFiles_.size(); ++i)
        {
            const auto& file = userPresetFiles_.getReference(i);
            if (!hostShouldIncludeUserPreset(file))
                continue;
            const auto n = file.getFileNameWithoutExtension();
            const auto searchable = hostUserPresetSearchText(file, n).toLowerCase();
            if (q.isEmpty() || searchable.contains(q))
                presetBox.addItem(hostFormatUserPresetLabel(file, n), makeUserPresetItemId(i));
        }
    }

    if (hostIsUserPreset())
    {
        const auto idx = userPresetFiles_.indexOf(hostCurrentUserFile());
        if (idx >= 0 && presetBox.indexOfItemId(makeUserPresetItemId(idx)) >= 0)
        {
            presetBox.setSelectedId(makeUserPresetItemId(idx), juce::dontSendNotification);
            return;
        }
    }
    else
    {
        const auto fi = hostCurrentFactoryIdx();
        if (fi >= 0 && presetBox.indexOfItemId(makeFactoryPresetItemId(fi)) >= 0)
        {
            presetBox.setSelectedId(makeFactoryPresetItemId(fi), juce::dontSendNotification);
            return;
        }
    }

    if (presetBox.getNumItems() > 0)
        presetBox.setSelectedItemIndex(0, juce::dontSendNotification);

    presetBox.setTooltip(presetBox.getText());
}

void CommonSynthEditor::showSaveAsDialog(const juce::String& defaultName)
{
    auto* aw = new juce::AlertWindow("Sauvegarder le Preset",
                                     "Nom du nouveau preset :",
                                     juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", defaultName, "Nom :");
    aw->addButton("Sauvegarder", 1);
    aw->addButton("Annuler", 0);

    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, aw](int result) {
            if (result == 1)
            {
                auto name = aw->getTextEditorContents("name").trim();
                if (name.isNotEmpty() && hostSaveUser(name))
                    refreshPresetList();
            }
            delete aw;
        }), false);
}

void CommonSynthEditor::saveCurrentPreset()
{
    if (hostIsUserPreset())
        hostUpdateUser(hostCurrentUserFile());
    else
        hostSaveFactory(hostCurrentFactoryIdx());
}

void CommonSynthEditor::deleteCurrentUserPreset()
{
    if (!hostIsUserPreset()) return;

    auto file = hostCurrentUserFile();
    auto name = file.getFileNameWithoutExtension();

    auto* aw = new juce::AlertWindow("Supprimer le Preset",
                                     "Supprimer \"" + name + "\" ?",
                                     juce::MessageBoxIconType::WarningIcon);
    aw->addButton("Supprimer", 1);
    aw->addButton("Annuler", 0);

    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, aw, file](int result) {
            if (result == 1)
            {
                hostDeleteUser(file);
                hostApplyFactory(0);
                refreshPresetList();
            }
            delete aw;
        }), false);
}

void CommonSynthEditor::navigatePreset(int direction)
{
    auto total = presetBox.getNumItems();
    if (total <= 0) return;
    auto current = presetBox.getSelectedItemIndex();
    auto next    = current + direction;
    if (next < 0)      next = total - 1;
    if (next >= total) next = 0;
    presetBox.setSelectedItemIndex(next);
}

void CommonSynthEditor::importPresetsFromZip()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Importer des presets (ZIP)",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.zip");

    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto results = fc.getResults();
            if (results.isEmpty()) return;

            auto zipFile = results.getFirst();
            if (!zipFile.existsAsFile()) return;

            juce::FileInputStream zipStream(zipFile);
            if (!zipStream.openedOk()) return;

            juce::ZipFile zip(zipStream);
            auto instrAttr = hostPresetInstrumentAttr();
            auto fallbackDir = hostGetUserPresetsDir();
            int imported = 0;
            int skipped = 0;
            std::map<int, int> perInstrCount;

            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                auto* entry = zip.getEntry(i);
                if (entry == nullptr) continue;

                auto name = juce::File::createLegalFileName(
                    juce::File(entry->filename).getFileName());
                if (!name.endsWithIgnoreCase(".xml")) continue;

                auto* entryStream = zip.createStreamForEntry(i);
                if (entryStream == nullptr) continue;

            auto xmlContent = entryStream->readEntireStreamAsString();
            delete entryStream;

            // Parse XML to extract instrument index
            auto destDir = fallbackDir;
            int resolvedIdx = -1;
            bool xmlAccepted = false;

            if (auto xml = juce::parseXML(xmlContent))
            {
                int synthId = -1;
                if (instrAttr == "piano_index") synthId = 2;
                    else if (instrAttr == "bass") synthId = 3;
                    else if (instrAttr == "inst") synthId = 1;
                    else if (instrAttr == "instr") synthId = 4;
                    else if (instrAttr == "instrIndex") synthId = 5;

                    if (synthId >= 0)
                    {
                        const auto identity = musique::preset::getSynthIdentity(synthId);
                        if (!xml->hasTagName(identity.xmlRootTag))
                        {
                            ++skipped;
                            continue;
                        }
                        resolvedIdx = musique::preset::readInstrumentIndexFromXml(*xml, identity);
                        xmlAccepted = true;
                    }
                    else if (xml->hasAttribute(instrAttr))
                    {
                        resolvedIdx = xml->getIntAttribute(instrAttr, -1);
                        xmlAccepted = true;
                    }
                }
                else
                {
                    ++skipped;
                    continue;
                }

                // Fallback: parse ZIP directory path for instrument index
                // e.g. "piano_3/MyPreset.xml" or "bass_1/Jazz.xml"
                if (resolvedIdx < 0)
                {
                    auto zipPath = entry->filename;
                    auto parentDir = juce::File(zipPath).getParentDirectory().getFileName();
                    auto underscoreIdx = parentDir.lastIndexOfChar('_');
                    if (underscoreIdx >= 0)
                    {
                        auto indexPart = parentDir.substring(underscoreIdx + 1);
                        if (indexPart.containsOnly("0123456789"))
                            resolvedIdx = indexPart.getIntValue();
                    }
                }

                if (!xmlAccepted && resolvedIdx < 0)
                {
                    ++skipped;
                    continue;
                }

                if (resolvedIdx >= 0)
                    destDir = hostGetUserPresetsDirForIndex(resolvedIdx);

                // Skip duplicates (same name in same directory)
                auto destFile = destDir.getChildFile(name);
                if (destFile.existsAsFile())
                {
                    ++skipped;
                    continue;
                }

                destFile.replaceWithText(xmlContent);
                ++imported;
                perInstrCount[resolvedIdx >= 0 ? resolvedIdx : -1]++;
            }

            refreshPresetList();

            // Build detailed summary
            juce::String msg;
            if (imported > 0)
            {
                msg = juce::String(imported) + " preset(s) importes";
                if (skipped > 0)
                    msg += " (" + juce::String(skipped) + " doublons ignores)";
                msg += ".\n\n";
                for (auto& [idx, count] : perInstrCount)
                {
                    if (idx >= 0)
                        msg += juce::String(count) + " vers instrument " + juce::String(idx) + "\n";
                    else
                        msg += juce::String(count) + " vers dossier par defaut\n";
                }
            }
            else
            {
                msg = "Aucun preset XML trouve dans le ZIP.";
                if (skipped > 0)
                    msg += " (" + juce::String(skipped) + " doublons ignores)";
            }

            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Import Presets", msg);
        });
}

