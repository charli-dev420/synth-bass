#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

// =============================================================================
// Layout constants — 1340 x 820
// =============================================================================
namespace lay
{
    constexpr int W = 1100, H = 780;
    constexpr int kbY = 700, kbH = 120;
    constexpr int pad = 16;
}

namespace
{
using EnvUiProfile = synthui::InstrumentUiProfile<14>;

constexpr int kBodyControlIndex = 7;
constexpr int kPitchEnvControlIndex = 9;

constexpr const char* kRightPanelSectionLabels[3] = {
    "MACRO+LFO", "MOD MATRIX", "FX"
};

constexpr std::array<const char*, mbs::kNumFamilies> kBassFamilyNames = {{
    "ACOUSTIC", "808", "SYNTH"
}};

constexpr std::array<const char*, mbs::kNumBasses> kBassDisplayNames = {{
    "Double Bass", "Finger Bass", "Slap Bass",
    "Sub 808", "Boom 808", "Distorted 808",
    "Moog Bass", "Reese Bass", "Acid Bass"
}};

double parseNumericText(const juce::String& text)
{
    const auto trimmed = text.trim();
    if (trimmed.equalsIgnoreCase("C"))
        return 0.0;

    juce::String filtered;
    for (auto ch : trimmed)
    {
        if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.')
            filtered += ch;
    }

    return filtered.isEmpty() ? 0.0 : filtered.getDoubleValue();
}

juce::String formatPercent(const double value)
{
    return juce::String(juce::roundToInt(value * 100.0)) + "%";
}

juce::String formatSignedPercent(const double value)
{
    const auto amount = juce::roundToInt(std::abs(value) * 100.0);
    if (amount == 0)
        return "0%";

    return juce::String(value > 0.0 ? "+" : "-") + juce::String(amount) + "%";
}

juce::String formatSeconds(const double seconds)
{
    if (seconds < 1.0)
        return juce::String(juce::roundToInt(seconds * 1000.0)) + " ms";

    if (seconds < 10.0)
        return juce::String(seconds, 2) + " s";

    return juce::String(seconds, 1) + " s";
}

juce::String formatMilliseconds(const double milliseconds)
{
    if (milliseconds < 1000.0)
        return juce::String(juce::roundToInt(milliseconds)) + " ms";

    return juce::String(milliseconds / 1000.0, milliseconds < 10000.0 ? 2 : 1) + " s";
}

juce::String formatFrequency(const double hz)
{
    if (hz >= 1000.0)
        return juce::String(hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";

    return juce::String(juce::roundToInt(hz)) + " Hz";
}

juce::String formatPanValue(const double pan)
{
    if (std::abs(pan) < 0.01)
        return "C";

    const auto amount = juce::roundToInt(std::abs(pan) * 100.0);
    return juce::String(pan < 0.0 ? "L" : "R") + juce::String(amount);
}

juce::String formatSemitones(const double semitones)
{
    const auto rounded = std::round(semitones);
    const bool nearInteger = std::abs(semitones - rounded) < 0.05;
    return juce::String(semitones, nearInteger ? 0 : 1) + " st";
}

juce::String formatLfoRate(const double hz)
{
    return juce::String(hz, hz < 10.0 ? 2 : 1) + " Hz";
}

juce::String formatRatio(const double ratio)
{
    return juce::String(ratio, ratio < 10.0 ? 1 : 0) + ":1";
}

juce::String formatDb(const double db)
{
    const bool nearZero = std::abs(db) < 0.05;
    const juce::String value = juce::String(nearZero ? 0.0 : db, std::abs(db) < 10.0 ? 1 : 0);
    return value + " dB";
}

juce::String formatVoices(const int count)
{
    return "VOICES " + juce::String(count);
}

struct UserPresetMetadataView
{
    juce::String mixRole;
    juce::String family;
    juce::String tags;
};

UserPresetMetadataView readUserPresetMetadataView(const juce::File& presetFile)
{
    UserPresetMetadataView view;
    if (!presetFile.existsAsFile())
        return view;

    auto xml = juce::parseXML(presetFile);
    if (xml == nullptr)
        return view;

    view.mixRole = xml->getStringAttribute("mix_role");
    view.family = xml->getStringAttribute("family");
    view.tags = xml->getStringAttribute("tags");
    return view;
}

juce::String formatBrowserRolePrefix(const juce::String& mixRole)
{
    juce::String role = mixRole.trim();
    if (role.isEmpty())
        return {};

    role = role.replace("-", " ").replace("_", " ");
    juce::StringArray tokens;
    tokens.addTokens(role, " ", "\"");
    tokens.trim();
    tokens.removeEmptyStrings();

    juce::StringArray formatted;
    for (auto token : tokens)
        formatted.add(token.substring(0, 1).toUpperCase() + token.substring(1).toLowerCase());

    const auto prefix = formatted.joinIntoString(" ").toUpperCase();
    return prefix.isNotEmpty() ? prefix + " | " : juce::String{};
}

juce::String buildUserPresetSearchText(const juce::String& displayName,
                                       const juce::String& modelName,
                                       const UserPresetMetadataView& metadata)
{
    juce::StringArray parts;
    parts.add(displayName);
    parts.add(modelName);
    parts.add(metadata.mixRole);
    parts.add(metadata.family);

    juce::StringArray tags;
    tags.addTokens(metadata.tags, ",", "\"");
    tags.trim();
    tags.removeEmptyStrings();
    for (const auto& tag : tags)
        parts.add(tag);

    parts.removeEmptyStrings();
    return parts.joinIntoString(" ").toLowerCase();
}

int countWhiteKeysInRange(int startNote, int endNote)
{
    int count = 0;
    for (int midiNote = startNote; midiNote <= endNote; ++midiNote)
    {
        switch (midiNote % 12)
        {
            case 1:
            case 3:
            case 6:
            case 8:
            case 10:
                break;
            default:
                ++count;
                break;
        }
    }

    return juce::jmax(1, count);
}

struct BassLayoutMetrics
{
    bool compact = false;
    bool roomy = false;
    int outerMargin = 24;
    int gutter = 16;
    int headerH = 96;
    int selectorH = 92;
    int kbH = 108;
    int contentW = 0;
    int contentX = 0;
    int selectorY = 0;
    int bodyY = 0;
    int bodyH = 0;
    int kbY = 0;
    int colW = 0;
    int col1X = 0;
    int col2X = 0;
    int col3X = 0;
};

float layoutDensity(const bool compact, const bool roomy)
{
    if (compact)
        return -1.0f;
    if (roomy)
        return 1.0f;
    return 0.0f;
}

int interpolateGap(const float density, const int compactValue, const int normalValue, const int roomyValue)
{
    if (density <= 0.0f)
    {
        return juce::roundToInt(juce::jmap(density,
                                           -1.0f,
                                           0.0f,
                                           static_cast<float>(compactValue),
                                           static_cast<float>(normalValue)));
    }

    return juce::roundToInt(juce::jmap(density,
                                       0.0f,
                                       1.0f,
                                       static_cast<float>(normalValue),
                                       static_cast<float>(roomyValue)));
}

BassLayoutMetrics computeLayoutMetrics(int width, int height)
{
    BassLayoutMetrics layout;
    layout.compact = width < 1160 || height < 760;
    layout.roomy = width > 1600 || height > 940;
    const float density = layoutDensity(layout.compact, layout.roomy);
    layout.outerMargin = layout.compact ? 16 : 24;
    layout.gutter = interpolateGap(density, 10, 16, 22);
    layout.headerH = layout.compact ? 88 : 96;
    layout.selectorH = layout.compact ? 88 : 94;
    layout.kbH = layout.compact
        ? juce::jlimit(72, 98, static_cast<int>(height * 0.14f))
        : juce::jlimit(84, 126, static_cast<int>(height * (layout.roomy ? 0.145f : 0.16f)));

    const int maxContentW = juce::jmin(width - layout.outerMargin * 2, 1680);
    layout.contentW = juce::jmax(920, maxContentW);
    layout.contentX = (width - layout.contentW) / 2;

    layout.selectorY = layout.outerMargin + layout.headerH + 4;
    layout.kbY = height - layout.kbH - layout.outerMargin;
    layout.bodyY = layout.selectorY + layout.selectorH + layout.gutter;
    layout.bodyH = juce::jmax(250, layout.kbY - layout.bodyY - 10);

    layout.colW = (layout.contentW - layout.gutter * 2) / 3;
    layout.col1X = layout.contentX;
    layout.col2X = layout.col1X + layout.colW + layout.gutter;
    layout.col3X = layout.col2X + layout.colW + layout.gutter;
    return layout;
}

int chooseFxDetailColumns(const int visibleCount)
{
    if (visibleCount <= 1)
        return 1;
    if (visibleCount <= 4)
        return 2;
    return 3;
}

constexpr std::array<mbs::GlobalFxSlot, 8> kBassFxSlots = {{
    mbs::GlobalFxSlot::Saturator,
    mbs::GlobalFxSlot::Transient,
    mbs::GlobalFxSlot::Compressor,
    mbs::GlobalFxSlot::Eq,
    mbs::GlobalFxSlot::Chorus,
    mbs::GlobalFxSlot::Delay,
    mbs::GlobalFxSlot::Reverb,
    mbs::GlobalFxSlot::Limiter
}};

const synthui::MacroLabelProfile<4>& macroLabelsForFamily(const mbs::Family family)
{
    static const synthui::MacroLabelProfile<4> acoustic = { "Boom", "Air", "Punch", "Depth" };
    static const synthui::MacroLabelProfile<4> eight08  = { "Weight", "Snap", "Punch", "Space" };
    static const synthui::MacroLabelProfile<4> synth    = { "Drive", "Edge", "Punch", "Depth" };

    switch (family)
    {
        case mbs::Family::Acoustic: return acoustic;
        case mbs::Family::Eight08:  return eight08;
        case mbs::Family::Synth:    return synth;
    }

    return acoustic;
}

struct MacroUiContext
{
    std::array<const char*, 4> shortTooltips;
    std::array<const char*, 4> noviceTooltips;
    const char* hint = "";
    const char* guardrail = "";
};

const MacroUiContext& macroUiContextForFamily(const mbs::Family family)
{
    static const MacroUiContext acoustic {
        { "Boom - wood and body before fake sub",
          "Air - definition and top clarity",
          "Punch - firmer attack and note start",
          "Depth - longer body without wash" },
        { "Boom - adds body resonance and a little weight while keeping the bass natural.",
          "Air - opens brightness and cutoff so the bass reads in a dense mix.",
          "Punch - tightens the attack and articulation without turning the bass harsh.",
          "Depth - extends note body and release carefully for a fuller acoustic line." },
        "Workflow: start with Boom, then Air for mix definition, then Punch for articulation.",
        "Guardrails: limited fake sub, centered low-end, natural transient behavior."
    };

    static const MacroUiContext eight08 {
        { "Weight - more sub mass, still centered",
          "Snap - more top click and cut",
          "Punch - stronger front edge and pitch bite",
          "Space - longer tail, but still groove-safe" },
        { "Weight - reinforces the low fundamental and body while keeping the 808 mono-safe.",
          "Snap - adds brightness and cutoff so the 808 reads on smaller speakers.",
          "Punch - sharpens attack and pitch envelope for a clearer front transient.",
          "Space - lengthens decay and release carefully so the kick still has room." },
        "Workflow: Weight for sub, Punch for front edge, Space only after the groove is locked.",
        "Guardrails: mono-safe sub, short-tail protection, no stereo smear in the low end."
    };

    static const MacroUiContext synth {
        { "Drive - harmonic density and weight",
          "Edge - brighter filter movement and cut",
          "Punch - faster transient and contour",
          "Depth - longer body and sustain" },
        { "Drive - adds harmonic density, character and a little extra low support.",
          "Edge - opens brightness, cutoff and filter motion for clearer note definition.",
          "Punch - tightens attack and boosts contour so the synth bass speaks earlier.",
          "Depth - extends body and release while keeping the note center readable." },
        "Workflow: Drive for density, Edge for definition, Depth only once the bass line is readable.",
        "Guardrails: clear tonal center, controlled drive, mix-safe low-end extension."
    };

    switch (family)
    {
        case mbs::Family::Acoustic: return acoustic;
        case mbs::Family::Eight08:  return eight08;
        case mbs::Family::Synth:    return synth;
    }

    return acoustic;
}

void glazeBassChrome(juce::Graphics& g,
                     juce::Rectangle<float> area,
                     juce::Colour accent,
                     float radius,
                     float intensity)
{
    const auto warmTint = juce::Colour(0xff2A241E).interpolatedWith(accent.withMultipliedSaturation(0.34f), 0.22f);
    const auto bassTop = warmTint.brighter(0.18f).withAlpha(0.028f * intensity);
    const auto bassMid = warmTint.withAlpha(0.062f * intensity);
    const auto bassBottom = juce::Colour(0xff111316).interpolatedWith(warmTint.darker(0.95f), 0.24f).withAlpha(0.17f * intensity);

    juce::ColourGradient warm(bassTop, area.getCentreX(), area.getY(),
                              bassBottom, area.getCentreX(), area.getBottom(), false);
    warm.addColour(0.42, bassMid);
    g.setGradientFill(warm);
    g.fillRoundedRectangle(area, radius);

    {
        juce::Graphics::ScopedSaveState scopedState(g);
        auto textureArea = area.reduced(2.8f);
        g.reduceClipRegion(textureArea.toNearestInt());

        const float horizontalStep = juce::jlimit(2.5f, 5.0f, textureArea.getHeight() * 0.005f);
        const float horizontalInset = juce::jlimit(4.0f, 12.0f, textureArea.getWidth() * 0.012f);
        for (float y = textureArea.getY() + horizontalStep; y < textureArea.getBottom(); y += horizontalStep)
        {
            const auto scanIndex = static_cast<int>(std::floor((y - textureArea.getY()) / horizontalStep));
            const float alpha = ((scanIndex & 1) == 0 ? 0.022f : 0.013f) * intensity;
            g.setColour(warmTint.brighter(0.10f).withAlpha(alpha));
            g.drawLine(textureArea.getX() + horizontalInset, y,
                       textureArea.getRight() - horizontalInset, y, 0.7f);
        }

        const float diagSpan = textureArea.getHeight() * 0.58f;
        const float diagonalStep = juce::jlimit(6.0f, 14.0f, textureArea.getHeight() * 0.014f);
        for (float x = textureArea.getX() - diagSpan; x < textureArea.getRight(); x += diagonalStep)
        {
            g.setColour(juce::Colours::black.withAlpha(0.016f * intensity));
            g.drawLine(x, textureArea.getBottom(),
                       x + diagSpan, textureArea.getY(),
                       0.6f);
        }
    }

    auto sheen = area.reduced(2.8f).withHeight(juce::jmax(7.0f, area.getHeight() * 0.13f));
    juce::ColourGradient highlight(juce::Colours::white.withAlpha(0.018f * intensity), sheen.getCentreX(), sheen.getY(),
                                   juce::Colours::transparentWhite, sheen.getCentreX(), sheen.getBottom(), false);
    g.setGradientFill(highlight);
    g.fillRoundedRectangle(sheen, juce::jmax(0.0f, radius - 2.0f));

    g.setColour(accent.withAlpha(0.030f * intensity));
    g.drawRoundedRectangle(area.reduced(1.0f), juce::jmax(0.0f, radius - 1.0f), 0.9f);
}
}

// =============================================================================
// Static tables
// =============================================================================
const std::array<BassSynthAudioProcessorEditor::CtrlDef,
                 BassSynthAudioProcessorEditor::kEnvN>
    BassSynthAudioProcessorEditor::kEnvCtrls = {{
        { "Level",       "level" },
        { "Tune",        "tune" },
        { "Brightness",  "brightness" },
        { "Attack",      "attack" },
        { "Decay",       "decay" },
        { "Sustain",     "sustain" },
        { "Release",     "release" },
        { "Body",        "body" },
        { "Drive",       "drive" },
        { "Pitch Env",   "pitch_env" },
        { "Sub",         "sub" },
        { "Character",   "character" },
        { "Cutoff",      "cutoff" },
        { "Pan",         "pan" }
    }};

const std::array<BassSynthAudioProcessorEditor::FxDef,
                 BassSynthAudioProcessorEditor::kMacroTotal>
    BassSynthAudioProcessorEditor::kMacroCtrls = {{
        { "Boom",        "macro_fatness" },
        { "Air",         "macro_brillance" },
        { "Punch",       "macro_punch" },
        { "Depth",       "macro_depth" }
    }};

const std::array<BassSynthAudioProcessorEditor::FxDef,
                 BassSynthAudioProcessorEditor::kFxN>
    BassSynthAudioProcessorEditor::kFxCtrls = {{
        { "Drive",      "sat_drive" },         //  0
        { "Mix",        "sat_mix" },           //  1
        { "Attack",     "transient_attack" },  //  2
        { "Sustain",    "transient_sustain" }, //  3
        { "Mix",        "transient_mix" },     //  4
        { "Threshold",  "comp_threshold" },    //  5
        { "Ratio",      "comp_ratio" },        //  6
        { "Attack",     "comp_attack" },       //  7
        { "Release",    "comp_release" },      //  8
        { "Makeup",     "comp_makeup" },       //  9
        { "Mix",        "comp_mix" },          // 10
        { "Low Freq",   "eq_low_freq" },       // 11
        { "Low Gain",   "eq_low_gain" },       // 12
        { "Mid Freq",   "eq_mid_freq" },       // 13
        { "Mid Gain",   "eq_mid_gain" },       // 14
        { "Mid Q",      "eq_mid_q" },          // 15
        { "High Freq",  "eq_high_freq" },      // 16
        { "High Gain",  "eq_high_gain" },      // 17
        { "Rate",       "chorus_rate" },       // 18
        { "Depth",      "chorus_depth" },      // 19
        { "Mix",        "chorus_mix" },        // 20
        { "Time",       "delay_time" },        // 21
        { "Feedback",   "delay_feedback" },    // 22
        { "Mix",        "delay_mix" },         // 23
        { "Size",       "reverb_size" },       // 24
        { "Damping",    "reverb_damping" },    // 25
        { "Width",      "reverb_width" },      // 26
        { "Mix",        "reverb_mix" },        // 27
        { "Threshold",  "limiter_threshold" }, // 28
        { "Release",    "limiter_release" }    // 29
    }};

const char* BassSynthAudioProcessorEditor::kFxTabNames[kFxTabs] = {
    "SAT", "TRANS", "COMP", "EQ", "CHORUS", "DELAY", "REVERB", "LIMIT"
};

const char* BassSynthAudioProcessorEditor::kFxRackSummaries[kFxTabs] = {
    "Harmonic drive",
    "Attack contour",
    "Dynamics control",
    "Tone balancing",
    "Stereo motion",
    "Echo repeats",
    "Space and width",
    "Output ceiling"
};

const char* BassSynthAudioProcessorEditor::kFxBypassParamIds[kFxTabs] = {
    "fx_tab0_en", "fx_tab1_en", "fx_tab2_en", "fx_tab3_en",
    "fx_tab4_en", "fx_tab5_en", "fx_tab6_en", "fx_tab7_en"
};

// =============================================================================
// Tooltip texts  (index order: envDials[0..13], lfoRate, lfoDepth,
//                 macroDials[0..3], fxDials[0..10], gainDial)
// =============================================================================
const char* BassSynthAudioProcessorEditor::kTooltipsShort[kTooltipCount] = {
    // env 0-13
    "Bass output volume",
    "Fine tuning in semitones",
    "Timbre brightness",
    "Envelope attack time",
    "Decay time after peak",
    "Level held while note is held",
    "Fade-out time after release",
    "Roundness / body of the sound",
    "Harmonic saturation",
    "Pitch envelope amount",
    "Sub-oscillator level",
    "Tonal colour / character",
    "Filter cutoff frequency",
    "Stereo position left/right",
    // lfo 14-15
    "LFO speed",
    "LFO modulation depth",
    // macro 16-19
    "Macro Boom - overall fullness",
    "Macro Air - overall brightness and presence",
    "Macro Punch - percussive impact",
    "Macro Depth - space and dimension",
    // fx 20-49 (30 dials)
    "Saturation intensity",
    "Saturation dry/wet balance",
    "Transient shaper attack",
    "Transient shaper sustain",
    "Transient dry/wet balance",
    "Compressor threshold",
    "Compression ratio",
    "Compressor attack time",
    "Compressor release time",
    "Compressor makeup gain",
    "Compressor dry/wet balance",
    "EQ low frequency",
    "EQ low gain",
    "EQ mid frequency",
    "EQ mid gain",
    "EQ mid Q factor",
    "EQ high frequency",
    "EQ high gain",
    "Chorus rate",
    "Chorus depth",
    "Chorus dry/wet balance",
    "Delay time",
    "Delay feedback",
    "Delay dry/wet balance",
    "Reverb size",
    "Reverb damping",
    "Reverb stereo width",
    "Reverb dry/wet balance",
    "Limiter threshold",
    "Limiter release",
    // gain 50
    "Main output volume"
};

const char* BassSynthAudioProcessorEditor::kTooltipsNovice[kTooltipCount] = {
    // env 0-13
    "Level - Bass volume. Turn up for louder, down for softer.",
    "Tune - Fine-tunes the bass in semitones. Useful when matching other instruments.",
    "Brightness - Makes the sound brighter or darker. Higher = more highs, lower = warmer.",
    "Attack - Controls how fast the sound rises at the start. Short = percussive, long = soft.",
    "Decay - How long the sound descends after the initial peak before reaching sustain.",
    "Sustain - Level held while you hold the note. 100% = no decay.",
    "Release - Time for the sound to fade after releasing the note. Short = tight, long = natural tail.",
    "Body - Adjusts roundness and thickness. Higher = bigger, warmer sound.",
    "Drive - Adds harmonic saturation. A little adds character, a lot adds aggression.",
    "Pitch Env - Varies pitch at note start. Classic 808 and synth bass effect.",
    "Sub - Adds a sub-oscillator below the main note. Ideal for reinforcing low frequencies.",
    "Character - Changes the overall tonal colour of the bass. Explore to find your sound.",
    "Cutoff - Filter frequency: right = open and bright, left = closed and dark.",
    "Pan - Moves the sound between left and right speakers. Centre = both sides equal.",
    // lfo 14-15
    "LFO Rate - Automatic oscillation speed. Slow = swell effect, fast = vibrato or tremolo.",
    "LFO Depth - Modulation amount applied by the LFO. At zero the LFO has no effect.",
    // macro 16-19
    "Boom - Macro that increases overall fullness. Great for quickly thickening the bass.",
    "Air - Macro that lifts upper harmonics and clarity. Adds presence without changing the bass role.",
    "Punch - Macro that emphasises percussive attack impact. Perfect for a punchy mix.",
    "Depth - Macro that adds space and dimension. Creates an enveloping effect.",
    // fx 20-49 (30 dials)
    "Drive - Saturation / distortion intensity. Adds grit and character to the bass.",
    "Mix - Balance between clean and saturated signal. Keep some clean for clarity.",
    "Attack - Emphasises or softens the attack transient. Higher = more snap, lower = softer.",
    "Sustain - Controls the body after the attack. Higher = more sustain, lower = shorter.",
    "Mix - Balance between original and transient effect. Dial in for a natural result.",
    "Threshold - Level above which the compressor acts. Lower = more compression.",
    "Ratio - Compression strength. 2:1 is subtle, 10:1 is pronounced. Controls dynamics.",
    "Attack - Compressor attack time in ms. Short = reacts fast to transients, long = more natural.",
    "Release - Compressor release time. Too short = pumping, too long = crushed sound.",
    "Makeup - Makeup gain after compression. Bring the level back up after reduction.",
    "Mix - Balance between original and compressed signal. 50% gives parallel compression.",
    "Low Freq - EQ low band cutoff frequency. Adjust to target the bass.",
    "Low Gain - EQ low frequency gain. Higher = more bass, lower = less.",
    "Mid Freq - EQ mid band centre frequency. Adjust to target the midrange.",
    "Mid Gain - EQ mid frequency gain. Adjust to add body or scoop the mids.",
    "Mid Q - Mid bandwidth. Low = wide and gentle, high = narrow and surgical.",
    "High Freq - EQ high band cutoff frequency. Adjust to target the highs.",
    "High Gain - EQ high frequency gain. Higher = more presence and highs.",
    "Rate - Chorus modulation speed. Slow = gentle wide effect, fast = vibrato.",
    "Depth - Chorus modulation intensity. Controls the thickness of the effect.",
    "Mix - Chorus dry/wet balance. Dial in for a subtle or pronounced effect.",
    "Time - Delay time in milliseconds. Adjust for the desired echo.",
    "Feedback - Delay repeat amount. Higher = more repeats.",
    "Mix - Delay dry/wet balance. Dial in the echo intensity.",
    "Size - Reverb size. Larger = bigger space and longer tail.",
    "Damping - High-frequency damping in the reverb. Higher = darker reverb.",
    "Width - Reverb stereo width. Higher = wider and more immersive.",
    "Mix - Reverb dry/wet balance. Dial in the sense of space.",
    "Threshold - Output limiter ceiling. Protects against clipping. Lower = more control.",
    "Release - Limiter release speed. Too short = pumping, too long = crushed.",
    // gain 50
    "Gain - Final synth output volume. Set to match the level in your mix."
};

namespace
{
const EnvUiProfile& envProfileForBass(const int bassIndex)
{
    static const EnvUiProfile acoustic = {{
        { "Level",     "Output volume",         "Level - Bass volume. Turn up for louder, down for softer." },
        { "Tune",      "Fine tuning",           "Tune - Fine-tunes the bass in semitones. Useful when matching other instruments." },
        { "Brightness","Timbre brightness",     "Brightness - Brighter or darker. Higher = more highs, lower = warmer." },
        { "Attack",    "Envelope attack time",  "Attack - How fast the sound rises. Short = percussive, long = soft." },
        { "Decay",     "Decay time",            "Decay - How long the sound descends after the peak before sustain." },
        { "Sustain",   "Sustain level",         "Sustain - Level held while you hold the note. 100% = no decay." },
        { "Release",   "Release time",          "Release - Fade-out time after releasing the note. Short = tight, long = natural tail." },
        { "Body",      "Body roundness",        "Body - Adjusts roundness and thickness. Higher = bigger, warmer sound." },
        { "Drive",     "Harmonic saturation",   "Drive - Adds harmonic saturation. A little = character, a lot = aggression." },
        { "Pitch Env", "Pitch envelope amount", "Pitch Env - Varies pitch at note start. Classic 808 and synth bass effect." },
        { "Sub",       "Sub-oscillator level",  "Sub - Adds a sub-oscillator below the main note. Reinforces low frequencies." },
        { "Character", "Tonal colour",          "Character - Changes the overall tonal colour. Explore to find your sound." },
        { "Cutoff",    "Filter cutoff",         "Cutoff - Filter frequency: right = open and bright, left = closed and dark." },
        { "Pan",       "Stereo position",       "Pan - Moves the sound between left and right. Centre = both sides equal." }
    }};

    static const EnvUiProfile eight08 = {{
        { "Level",     "Output volume",         "Level - Bass volume. Turn up for louder, down for softer." },
        { "Tune",      "Fine tuning",           "Tune - Fine-tunes the bass in semitones. Useful when matching other instruments." },
        { "Snap",      "Timbre brightness",     "Brightness - Brighter or darker. Higher = more highs, lower = warmer." },
        { "Attack",    "Envelope attack time",  "Attack - How fast the sound rises. Short = percussive, long = soft." },
        { "Decay",     "Decay time",            "Decay - How long the sound descends after the peak before sustain." },
        { "Sustain",   "Sustain level",         "Sustain - Level held while you hold the note. 100% = no decay." },
        { "Release",   "Release time",          "Release - Fade-out time after releasing the note. Short = tight, long = natural tail." },
        { "Weight",    "Body weight",           "Weight - Adjusts roundness and thickness. Higher = bigger, warmer sound." },
        { "Drive",     "Harmonic saturation",   "Drive - Adds harmonic saturation. A little = character, a lot = aggression." },
        { "Pitch Env", "Pitch envelope amount", "Pitch Env - Varies pitch at note start. Classic 808 and synth bass effect." },
        { "Sub",       "Sub-oscillator level",  "Sub - Adds a sub-oscillator below the main note. Reinforces low frequencies." },
        { "Tone",      "Tonal colour",          "Tone - Changes the overall tonal colour. Explore to find your sound." },
        { "Cutoff",    "Filter cutoff",         "Cutoff - Filter frequency: right = open and bright, left = closed and dark." },
        { "Pan",       "Stereo position",       "Pan - Moves the sound between left and right. Centre = both sides equal." }
    }};

    static const EnvUiProfile synth = {{
        { "Level",     "Output volume",         "Level - Bass volume. Turn up for louder, down for softer." },
        { "Tune",      "Fine tuning",           "Tune - Fine-tunes the bass in semitones. Useful when matching other instruments." },
        { "Edge",      "Timbre brightness",     "Edge - Brighter or darker. Higher = more highs, lower = warmer." },
        { "Attack",    "Envelope attack time",  "Attack - How fast the sound rises. Short = percussive, long = soft." },
        { "Decay",     "Decay time",            "Decay - How long the sound descends after the peak before sustain." },
        { "Sustain",   "Sustain level",         "Sustain - Level held while you hold the note. 100% = no decay." },
        { "Release",   "Release time",          "Release - Fade-out time after releasing the note. Short = tight, long = natural tail." },
        { "Filter Env","Filter sweep depth",    "Filter Env - Controls how far the filter sweeps above the resting cutoff at note start." },
        { "Drive",     "Harmonic saturation",   "Drive - Adds harmonic saturation. A little = character, a lot = aggression." },
        { "Pitch Env", "Pitch envelope amount", "Pitch Env - Varies pitch at note start. Classic 808 and synth bass effect." },
        { "Sub",       "Sub-oscillator level",  "Sub - Adds a sub-oscillator below the main note. Reinforces low frequencies." },
        { "Character", "Tonal colour",          "Character - Changes the overall tonal colour. Explore to find your sound." },
        { "Cutoff",    "Filter cutoff",         "Cutoff - Filter frequency: right = open and bright, left = closed and dark." },
        { "Pan",       "Stereo position",       "Pan - Moves the sound between left and right. Centre = both sides equal." }
    }};

    switch (mbs::getFamily(bassIndex))
    {
        case mbs::Family::Acoustic: return acoustic;
        case mbs::Family::Eight08:  return eight08;
        case mbs::Family::Synth:    return synth;
    }

    return acoustic;
}

bool shouldShowEnvControl(const int bassIndex, const int controlIndex)
{
    switch (controlIndex)
    {
        case kBodyControlIndex:
            return mbs::supportsBodyControl(bassIndex) || mbs::supportsFilterEnvControl(bassIndex);
        case kPitchEnvControlIndex:
            return mbs::supportsPitchEnvControl(bassIndex);
        default:
            return true;
    }
}

const char* envControlSuffixForBass(const int bassIndex, const int controlIndex)
{
    if (controlIndex == kBodyControlIndex && mbs::supportsFilterEnvControl(bassIndex))
        return "filter_env";

    static constexpr std::array<const char*, 14> kDefaultSuffixes = {{
        "level", "tune", "brightness", "attack", "decay", "sustain", "release",
        "body", "drive", "pitch_env", "sub", "character", "cutoff", "pan"
    }};
    return kDefaultSuffixes[static_cast<std::size_t>(controlIndex)];
}
}

// =============================================================================
// Helpers
// =============================================================================
juce::Colour BassSynthAudioProcessorEditor::familyColour(int familyIndex)
{
    switch (familyIndex)
    {
        case 0:  return juce::Colour(0xffB59B69);
        case 1:  return juce::Colour(0xffB36D46);
        case 2:  return juce::Colour(0xff6F8F86);
        default: return juce::Colour(0xffB59B69);
    }
}

juce::Colour BassSynthAudioProcessorEditor::bassCatColour(int bassIndex)
{
    switch (juce::jlimit(0, mbs::kNumBasses - 1, bassIndex))
    {
        case 0:  return juce::Colour(0xffBAA06C); // Double Bass
        case 1:  return juce::Colour(0xffA88560); // Finger Bass
        case 2:  return juce::Colour(0xffB5A96A); // Slap Bass
        case 3:  return juce::Colour(0xffC28D53); // Sub 808
        case 4:  return juce::Colour(0xffC07147); // Boom 808
        case 5:  return juce::Colour(0xffA75C4B); // Distorted 808
        case 6:  return juce::Colour(0xff78927A); // Moog Bass
        case 7:  return juce::Colour(0xff64838B); // Reese Bass
        case 8:  return juce::Colour(0xff9FA75B); // Acid Bass
        default: return familyColour(static_cast<int>(mbs::getFamily(bassIndex)));
    }
}

int BassSynthAudioProcessorEditor::selectedBassFromParam() const
{
    if (auto* raw = proc.getAPVTS().getRawParameterValue("selected_bass"))
        return juce::jlimit(0, mbs::kNumBasses - 1,
                            static_cast<int>(std::round(raw->load())));
    return 0;
}

// =============================================================================
// Virtual bridge methods
// =============================================================================
BassSynthAudioProcessorEditor::VisualLayoutSnapshot
BassSynthAudioProcessorEditor::computeVisualLayoutSnapshot(int width, int height) const
{
    const auto layout = computeLayoutMetrics(width, height);

    VisualLayoutSnapshot snapshot;
    snapshot.compact = layout.compact;
    snapshot.roomy = layout.roomy;
    snapshot.headerH = layout.headerH;
    snapshot.contentX = layout.contentX;
    snapshot.contentW = layout.contentW;
    snapshot.selectorY = layout.selectorY;
    snapshot.selectorH = layout.selectorH;
    snapshot.bodyY = layout.bodyY;
    snapshot.bodyH = layout.bodyH;
    snapshot.kbY = layout.kbY;
    snapshot.kbH = layout.kbH;
    snapshot.col1X = layout.col1X;
    snapshot.col2X = layout.col2X;
    snapshot.col3X = layout.col3X;
    snapshot.colW = layout.colW;
    snapshot.headerZones = computeHeaderZones(layout.headerH);
    snapshot.headerBounds = snapshot.headerZones.headerBounds;
    snapshot.selectorPanelBounds = { layout.contentX, layout.selectorY, layout.contentW, layout.selectorH - 8 };
    snapshot.gainSize = layout.compact ? 30 : 34;
    snapshot.statusReserve = snapshot.gainSize + (layout.compact ? 14 : 18);
    snapshot.statusPrimaryRow = snapshot.headerZones.statusPrimaryRow.withTrimmedRight(snapshot.statusReserve);
    snapshot.statusSecondaryRow = snapshot.headerZones.statusSecondaryRow.withTrimmedRight(snapshot.statusReserve);
    snapshot.gainSlotBounds = { snapshot.headerZones.statusZone.getRight() - snapshot.statusReserve,
                                snapshot.headerZones.statusZone.getY(),
                                snapshot.statusReserve,
                                snapshot.headerZones.statusZone.getHeight() };
    return snapshot;
}

#if defined(UWDEVST_BASS_TEST_BUILD)
BassSynthAudioProcessorEditor::LayoutSnapshot
BassSynthAudioProcessorEditor::captureLayoutSnapshotForTests() const
{
    const auto layout = computeVisualLayoutSnapshot(getWidth(), getHeight());

    LayoutSnapshot snapshot;
    snapshot.compact = layout.compact;
    snapshot.editorBounds = getLocalBounds();
    snapshot.headerBounds = layout.headerBounds;
    snapshot.selectorPanelBounds = layout.selectorPanelBounds;
    snapshot.statusPrimaryBounds = layout.statusPrimaryRow;
    snapshot.statusSecondaryBounds = layout.statusSecondaryRow;
    snapshot.gainSlotBounds = layout.gainSlotBounds;
    snapshot.gainBounds = gainDial.getBounds();
    snapshot.randBounds = randButton.getBounds();
    snapshot.tooltipBounds = tooltipModeBtn.getBounds();
    snapshot.voiceBounds = voiceCountLabel.getBounds();
    snapshot.ccBounds = midiCCPageLabel.getBounds();
    snapshot.fxLockBounds = fxLockButton.getBounds();
    snapshot.familyLabelBounds = familySelectorLbl.getBounds();
    snapshot.modelLabelBounds = modelSelectorLbl.getBounds();
    snapshot.modelSelectorBounds = modelSelector.getBounds();
    snapshot.fxLockVisible = fxLockButton.isVisible();

    for (const auto& tab : familyTabs)
    {
        if (!tab.isVisible())
            continue;

        snapshot.familyTabsBounds = snapshot.familyTabsBounds.isEmpty()
            ? tab.getBounds()
            : snapshot.familyTabsBounds.getUnion(tab.getBounds());
    }

    return snapshot;
}

void BassSynthAudioProcessorEditor::setRightPanelSectionForTests(int sectionIndex)
{
    switchRightPanelSection(sectionIndex);
}

void BassSynthAudioProcessorEditor::refreshBassUiForTests()
{
    rebuildBassAttachments();
    syncSelectionUiFromBass();
    syncFamilyControlVisibility();
    resized();
}

bool BassSynthAudioProcessorEditor::isEnvControlVisibleForTests(int index) const
{
    if (index < 0 || index >= kEnvN)
        return false;

    return envDials[static_cast<std::size_t>(index)].isVisible()
        && envLabels[static_cast<std::size_t>(index)].isVisible();
}

juce::String BassSynthAudioProcessorEditor::getEnvControlLabelForTests(int index) const
{
    if (index < 0 || index >= kEnvN)
        return {};

    return envLabels[static_cast<std::size_t>(index)].getText();
}

juce::String BassSynthAudioProcessorEditor::getMacroLabelForTests(int index) const
{
    if (index < 0 || index >= kMacroTotal)
        return {};

    return macroLbls[static_cast<std::size_t>(index)].getText();
}

juce::String BassSynthAudioProcessorEditor::getMacroHintForTests() const
{
    return macroHintLabel.getText();
}

juce::String BassSynthAudioProcessorEditor::getMacroGuardrailForTests() const
{
    return macroGuardrailLabel.getText();
}

juce::String BassSynthAudioProcessorEditor::getFactoryPresetBrowserLabelForTests(int presetIndex) const
{
    return proc.getFactoryPresetBrowserLabel(presetIndex);
}

juce::String BassSynthAudioProcessorEditor::getFactoryPresetBrowserSearchTextForTests(int presetIndex) const
{
    return proc.getFactoryPresetBrowserSearchText(presetIndex);
}
#endif

juce::StringArray BassSynthAudioProcessorEditor::hostGetFactoryNames()
    { return proc.getFactoryPresetNames(); }

juce::Array<juce::File> BassSynthAudioProcessorEditor::hostScanUserPresets()
    { return proc.scanUserPresets(); }

bool BassSynthAudioProcessorEditor::hostIsUserPreset()
    { return proc.isCurrentPresetUser(); }

juce::File BassSynthAudioProcessorEditor::hostCurrentUserFile()
    { return proc.getCurrentUserPresetFile(); }

int BassSynthAudioProcessorEditor::hostCurrentFactoryIdx()
    { return proc.getCurrentFactoryPresetIndex(); }

void BassSynthAudioProcessorEditor::hostApplyFactory(int idx)
    { proc.applyFactoryPreset(idx); }

void BassSynthAudioProcessorEditor::hostLoadUser(const juce::File& f)
    { proc.loadUserPreset(f); }

bool BassSynthAudioProcessorEditor::hostSaveUser(const juce::String& name)
    { return proc.saveUserPreset(name); }

void BassSynthAudioProcessorEditor::hostUpdateUser(const juce::File& f)
    { proc.updateUserPreset(f); }

void BassSynthAudioProcessorEditor::hostSaveFactory(int idx)
    { proc.saveFactoryPreset(idx); }

void BassSynthAudioProcessorEditor::hostDeleteUser(const juce::File& f)
    { proc.deleteUserPreset(f); }

juce::File BassSynthAudioProcessorEditor::hostGetUserPresetsDir()
    { return BassSynthAudioProcessor::getUserPresetsDirectory(proc.getSelectedBassIndex()); }

juce::File BassSynthAudioProcessorEditor::hostGetUserPresetsDirForIndex(int instrumentIndex)
    { return BassSynthAudioProcessor::getUserPresetsDirectory(instrumentIndex); }

juce::String BassSynthAudioProcessorEditor::hostPresetInstrumentAttr() const
    { return "bass"; }

juce::String BassSynthAudioProcessorEditor::hostFormatFactoryPresetLabel(int presetIndex,
                                                                         const juce::String& displayName) const
{
    juce::ignoreUnused(displayName);
    return proc.getFactoryPresetBrowserLabel(presetIndex);
}

juce::String BassSynthAudioProcessorEditor::hostFactoryPresetSearchText(int presetIndex,
                                                                        const juce::String& displayName) const
{
    juce::ignoreUnused(displayName);
    return proc.getFactoryPresetBrowserSearchText(presetIndex);
}

juce::String BassSynthAudioProcessorEditor::hostFormatUserPresetLabel(const juce::File& presetFile,
                                                                      const juce::String& displayName) const
{
    const auto metadata = readUserPresetMetadataView(presetFile);
    return formatBrowserRolePrefix(metadata.mixRole) + displayName;
}

juce::String BassSynthAudioProcessorEditor::hostUserPresetSearchText(const juce::File& presetFile,
                                                                     const juce::String& displayName) const
{
    return buildUserPresetSearchText(displayName,
                                     mbs::getBassName(proc.getSelectedBassIndex()),
                                     readUserPresetMetadataView(presetFile));
}

// =============================================================================
// Constructor
// =============================================================================
BassSynthAudioProcessorEditor::BassSynthAudioProcessorEditor(
    BassSynthAudioProcessor& processor)
    : CommonSynthEditor(processor,
                        processor.getAPVTS(),
                        processor.getKeyboardState(),
                        juce::Colour(0xff8CB44A),
                        24, 72, 28.0f)
    , proc(processor)
{
    familySelectorLbl.setText("FAMILY", juce::dontSendNotification);
    modelSelectorLbl.setText("MODEL", juce::dontSendNotification);

    familySelector.addItem("ACOUSTIC", 1);
    familySelector.addItem("808", 2);
    familySelector.addItem("SYNTH", 3);

    familySelector.onChange = [this]
    {
        const int fi = juce::jlimit(0, mbs::kNumFamilies - 1,
                                    familySelector.getSelectedId() - 1);
        activeFamilyIndex = fi;
        rebuildModelSelectorForFamily(activeFamilyIndex);
        const int selectedBassId = modelSelector.getSelectedId();
        if (selectedBassId > 0)
            bassSelector.setSelectedId(selectedBassId);
    };

    modelSelector.onChange = [this]
    {
        const int selectedBassId = modelSelector.getSelectedId();
        if (selectedBassId > 0)
            bassSelector.setSelectedId(selectedBassId);
    };

    addAndMakeVisible(singleBtn);

    for (int f = 0; f < mbs::kNumFamilies; ++f)
    {
        familyTabs[(size_t)f].configure(f, kBassFamilyNames[(size_t)f], familyColour(f));
        familyTabs[(size_t)f].onClicked = [this](int idx)
        {
            familySelector.setSelectedId(idx + 1, juce::sendNotificationSync);
        };
        familyTabs[(size_t)f].setVisible(false);
        addChildComponent(familyTabs[(size_t)f]);
    }

    bassSelector.setVisible(false);
    addChildComponent(bassSelector);
    for (int i = 0; i < mbs::kNumBasses; ++i)
        bassSelector.addItem(kBassDisplayNames[(size_t)i], i + 1);
    selBassAtt = std::make_unique<ComboBoxAttach>(
        proc.getAPVTS(), "selected_bass", bassSelector);
    bassSelector.onChange = [this] {
        rebuildBassAttachments();
        syncSelectionUiFromBass();
    };

    for (int i = 0; i < mbs::kNumBasses; ++i)
    {
        auto& card = presetCards[(size_t)i];
        card.configure(i, mbs::getBassName(i), bassCatColour(i));
        card.onClicked = [this](int idx) { bassSelector.setSelectedId(idx + 1); };
        addChildComponent(card);
    }

    for (int i = 0; i < kEnvN; ++i)
    {
        auto si = (size_t)i;
        switch (i)
        {
            case 0:
            case 2:
            case 5:
            case 7:
            case 9:
            case 10:
            case 11:
                setupDial(envDials[si], accent_);
                break;
            case 1:
                setupDial(envDials[si], accent_);
                break;
            case 3:
            case 4:
            case 6:
                setupDial(envDials[si], accent_);
                break;
            case 8:
                setupDial(envDials[si], accent_);
                break;
            case 12:
                setupGrandDial(envDials[si], accent_, " Hz");
                break;
            case 13:
                setupDial(envDials[si], accent_);
                break;
            default:
                setupDial(envDials[si], accent_);
                break;
        }
        addAndMakeVisible(envDials[si]);

        envLabels[si].setText(kEnvCtrls[si].label, juce::dontSendNotification);
        envLabels[si].setJustificationType(juce::Justification::centred);
        envLabels[si].setFont(juce::Font(juce::FontOptions{}.withHeight(10.4f)));
        envLabels[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.84f));
        addAndMakeVisible(envLabels[si]);
    }
    envVisual.setAccent(accent_);
    envVisual.setTitle("AMP ENV");
    envVisual.bindAdsr(&envDials[3], &envDials[4], &envDials[5], &envDials[6]);
    addAndMakeVisible(envVisual);

    lfoVisual.setAccent(accent_);
    lfoVisual.setTitle("LFO");
    setupSmallDial(lfoRateDial, accent_);
    setupSmallDial(lfoDepthDial, accent_);
    addChildComponent(lfoRateDial);
    addChildComponent(lfoDepthDial);
    addChildComponent(lfoWaveSelector);
    lfoWaveSelector.addItem("SINE", 1);
    lfoWaveSelector.addItem("TRI", 2);
    lfoWaveSelector.addItem("SAW", 3);
    lfoWaveSelector.addItem("SQR", 4);
    lfoRateAtt  = std::make_unique<SliderAttach>(proc.getAPVTS(), "lfo_rate", lfoRateDial);
    lfoDepthAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "lfo_depth", lfoDepthDial);
    lfoWaveAtt  = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "lfo_wave", lfoWaveSelector);

    // C1: LFO destination selector
    lfoDestLabel.setText("TARGET", juce::dontSendNotification);
    lfoWaveLabel.setText("WAVE", juce::dontSendNotification);
    lfoRateLabel.setText("RATE", juce::dontSendNotification);
    lfoDepthLabel.setText("DEPTH", juce::dontSendNotification);
    for (auto* label : { &lfoDestLabel, &lfoWaveLabel, &lfoRateLabel, &lfoDepthLabel })
    {
        label->setJustificationType(juce::Justification::centredLeft);
        label->setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
        label->setColour(juce::Label::textColourId, synthcol::textDim);
    }
    lfoDestSelector.addItem("Trem / Pan", 1);
    lfoDestSelector.addItem("Cutoff",     2);
    lfoDestSelector.addItem("Both",       3);
    lfoDestAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "lfo_dest", lfoDestSelector);
    addAndMakeVisible(lfoDestLabel);
    addAndMakeVisible(lfoWaveLabel);
    addAndMakeVisible(lfoRateLabel);
    addAndMakeVisible(lfoDepthLabel);
    addAndMakeVisible(lfoDestSelector);

    // H6: Mod matrix panel
    modMatrixTitle.setText("MOD MATRIX", juce::dontSendNotification);
    modMatrixTitle.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
    modMatrixTitle.setColour(juce::Label::textColourId, accent_);
    addChildComponent(modMatrixTitle);

    modLfo2RateLabel.setText("LFO2 RATE", juce::dontSendNotification);
    modLfo2WaveLabel.setText("LFO2 WAVE", juce::dontSendNotification);
    for (auto* label : { &modLfo2RateLabel, &modLfo2WaveLabel })
    {
        label->setJustificationType(juce::Justification::centredLeft);
        label->setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
        label->setColour(juce::Label::textColourId, synthcol::textDim);
        addChildComponent(*label);
    }

    modLfo2RateDial.setSliderStyle(juce::Slider::LinearBar);
    modLfo2RateDial.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 56, 22);
    modLfo2RateDial.setTooltip("Secondary modulation oscillator rate. Used only by MOD MATRIX routes that target LFO2.");
    addChildComponent(modLfo2RateDial);
    modLfo2RateAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "mod_lfo2_rate", modLfo2RateDial);

    modLfo2WaveSelector.addItem("SINE", 1);
    modLfo2WaveSelector.addItem("TRI", 2);
    modLfo2WaveSelector.addItem("SAW", 3);
    modLfo2WaveSelector.addItem("SQR", 4);
    modLfo2WaveSelector.setTooltip("Secondary modulation oscillator waveform for MOD MATRIX routes that use LFO2.");
    addChildComponent(modLfo2WaveSelector);
    modLfo2WaveAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "mod_lfo2_wave", modLfo2WaveSelector);

    const juce::StringArray mmSrcNames { juce::String(juce::CharPointer_UTF8("\xe2\x80\x94")), "LFO1", "LFO2", "Env", "Vel", "Wheel", "AfTch", "Bend" };
    const juce::StringArray mmDstNames { juce::String(juce::CharPointer_UTF8("\xe2\x80\x94")), "Cutoff", "Reson", "Pan", "Level", "Pitch",
                                         "Atk", "Dcy", "LFO Rate", "EQ Frq", "EQ Gain" };
    for (int i = 0; i < 8; ++i)
    {
        auto& r = modRows[static_cast<std::size_t>(i)];
        for (int j = 0; j < mmSrcNames.size(); ++j) r.srcCombo.addItem(mmSrcNames[j], j + 1);
        for (int j = 0; j < mmDstNames.size(); ++j) r.dstCombo.addItem(mmDstNames[j], j + 1);
        r.amtSlider.setRange(-1.0, 1.0, 0.01);
        r.amtSlider.setSliderStyle(juce::Slider::LinearBar);
        r.amtSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addChildComponent(r.srcCombo);
        addChildComponent(r.dstCombo);
        addChildComponent(r.amtSlider);

        modSrcAtts[static_cast<std::size_t>(i)] = std::make_unique<ComboBoxAttach>(
            proc.getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(i, "source"), r.srcCombo);
        modDstAtts[static_cast<std::size_t>(i)] = std::make_unique<ComboBoxAttach>(
            proc.getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(i, "dest"), r.dstCombo);
        modAmtAtts[static_cast<std::size_t>(i)] = std::make_unique<SliderAttach>(
            proc.getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(i, "amount"), r.amtSlider);
    }

    lfoVisual.bindRateDepth(&lfoRateDial, &lfoDepthDial);
    lfoVisual.setWaveformIndex(juce::jmax(0, lfoWaveSelector.getSelectedId() - 1));
    lfoWaveSelector.onChange = [this] {
        lfoVisual.setWaveformIndex(juce::jmax(0, lfoWaveSelector.getSelectedId() - 1));
    };
    lfoVisual.onWaveformChanged = [this](int wi) {
        lfoWaveSelector.setSelectedId(wi + 1, juce::sendNotificationSync);
    };
    addAndMakeVisible(lfoVisual);

    setupSmallDial(glideTimeDial, accent_);
    glideTimeDial.setSliderStyle(juce::Slider::LinearBar);
    glideTimeDial.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 48, 22);
    setupSmallDial(resonanceDial, accent_);
    addAndMakeVisible(glideTimeDial);
    addAndMakeVisible(resonanceDial);
    addAndMakeVisible(monoModeSelector);

    glideTimeLabel.setText("Glide", juce::dontSendNotification);
    resonanceLabel.setText("Resonance", juce::dontSendNotification);
    monoModeLabel.setText("Mode", juce::dontSendNotification);
    for (auto* label : { &glideTimeLabel, &resonanceLabel, &monoModeLabel })
    {
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
        label->setColour(juce::Label::textColourId, synthcol::textDim);
        addAndMakeVisible(*label);
    }

    monoModeSelector.addItem("POLY", 1);
    monoModeSelector.addItem("MONO", 2);
    monoModeSelector.addItem("LEGATO", 3);
    glideTimeAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "glide_time", glideTimeDial);
    monoModeAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "mono_mode", monoModeSelector);

    for (int i = 0; i < kMacroTotal; ++i)
    {
        auto si = (size_t)i;
        macroAtt[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(), kMacroCtrls[si].paramId, macroDials[si]);
        setupDial(macroDials[si], accent_);
        if (i < kMacroVisible)
        {
            addAndMakeVisible(macroDials[si]);
            macroLbls[si].setText(kMacroCtrls[si].label, juce::dontSendNotification);
            macroLbls[si].setJustificationType(juce::Justification::centred);
            macroLbls[si].setFont(juce::Font(juce::FontOptions{}.withHeight(10.4f)));
            macroLbls[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.84f));
            addAndMakeVisible(macroLbls[si]);
        }
        else addChildComponent(macroDials[si]);
    }

    for (auto* label : { &macroHintLabel, &macroGuardrailLabel })
    {
        label->setJustificationType(juce::Justification::centredLeft);
        label->setFont(juce::Font(juce::FontOptions{}.withHeight(10.2f)));
        label->setColour(juce::Label::textColourId, synthcol::textDim.withAlpha(0.86f));
        addAndMakeVisible(*label);
    }

    for (int sectionIndex = 0; sectionIndex < kRightPanelSections; ++sectionIndex)
    {
        auto& tab = rightPanelTabs[(size_t)sectionIndex];
        tab.configure(sectionIndex, kRightPanelSectionLabels[sectionIndex], accent_);
        tab.setSelected(sectionIndex == activeRightPanelSection);
        tab.onClicked = [this](int idx) { switchRightPanelSection(idx); };
        addAndMakeVisible(tab);
    }

    for (int i = 0; i < kFxN; ++i)
    {
        auto si = (size_t)i;
        fxAtt[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(), kFxCtrls[si].paramId, fxDials[si]);
        setupSmallDial(fxDials[si], accent_);
        addChildComponent(fxDials[si]);
        fxLbls[si].setText(kFxCtrls[si].label, juce::dontSendNotification);
        fxLbls[si].setJustificationType(juce::Justification::centred);
        fxLbls[si].setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        fxLbls[si].setColour(juce::Label::textColourId, synthcol::textSec.withAlpha(0.84f));
        addChildComponent(fxLbls[si]);
    }

    for (int t = 0; t < kFxTabs; ++t)
    {
        auto& item = fxRackItems[(size_t)t];
        item.configure(t, kFxTabNames[t], kFxRackSummaries[t], accent_);
        item.onClicked = [this](int ti) { switchEffectTab(ti); };
        addAndMakeVisible(item);
    }

    for (int t = 0; t < kFxTabs; ++t)
    {
        auto& btn = fxBypassBtns[(size_t)t];
        btn.setButtonText("ON");
        btn.setClickingTogglesState(true);
        btn.setToggleState(true, juce::dontSendNotification);
        addAndMakeVisible(btn);
        fxBypassAtts[(size_t)t] = std::make_unique<BtnAttach>(
            proc.getAPVTS(), kFxBypassParamIds[t], btn);
        btn.onClick = [this] { syncFxRackState(); };
    }

    fxDetailTitle.setJustificationType(juce::Justification::centredLeft);
    fxDetailTitle.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
    fxDetailTitle.setColour(juce::Label::textColourId, synthcol::textSec);
    addAndMakeVisible(fxDetailTitle);

    fxUnavailableLbl.setText("Not available for this model", juce::dontSendNotification);
    fxUnavailableLbl.setJustificationType(juce::Justification::centred);
    fxUnavailableLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
    fxUnavailableLbl.setColour(juce::Label::textColourId, synthcol::textDim.withAlpha(0.55f));
    addChildComponent(fxUnavailableLbl);

    // ── Tooltip mode button ───────────────────────────────────────────
    tooltipModeBtn.setButtonText("TIP: SHORT");
    tooltipModeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2A2A32));
    tooltipModeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffBBBBCC));
    tooltipModeBtn.onClick = [this] { cycleTooltipMode(); };
    addAndMakeVisible(tooltipModeBtn);
    applyTooltips();

    // H1: Velocity curve selector
    velocityCurveSelector.addItem("Linear",  1);
    velocityCurveSelector.addItem("Soft",    2);
    velocityCurveSelector.addItem("Softer",  3);
    velocityCurveSelector.addItem("Hard",    4);
    velocityCurveSelector.addItem("Harder",  5);
    velocityCurveSelector.addItem("Fixed",   6);
    velocityCurveSelector.addItem("Touch",   7);
    velocityCurveSelector.setTooltip("Velocity curve. Linear is direct, Soft opens up low velocities, Hard needs a firmer touch, Fixed ignores velocity.");
    velocityCurveAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "velocity_curve", velocityCurveSelector);
    addAndMakeVisible(velocityCurveSelector);
    velocityCurveLabel.setText("VELOCITY", juce::dontSendNotification);
    velocityCurveLabel.setJustificationType(juce::Justification::centredLeft);
    velocityCurveLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    velocityCurveLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    addAndMakeVisible(velocityCurveLabel);

    // H2: Pitch bend range
    pitchBendRangeDial.setSliderStyle(juce::Slider::LinearBar);
    pitchBendRangeDial.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 28, 22);
    pitchBendRangeDial.setTooltip("Pitch bend range in semitones. 2 is standard, wider values suit slides and glides.");
    pitchBendRangeAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "pitch_bend_range", pitchBendRangeDial);
    addAndMakeVisible(pitchBendRangeDial);
    pitchBendRangeLabel.setText("BEND RANGE", juce::dontSendNotification);
    pitchBendRangeLabel.setJustificationType(juce::Justification::centredLeft);
    pitchBendRangeLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    pitchBendRangeLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    addAndMakeVisible(pitchBendRangeLabel);

    // H4: Voice count label
    voiceCountLabel.setText(formatVoices(0), juce::dontSendNotification);
    voiceCountLabel.setJustificationType(juce::Justification::centred);
    voiceCountLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.5f).withStyle("Bold")));
    voiceCountLabel.setColour(juce::Label::textColourId, synthcol::textSec);
    voiceCountLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a1d22));
    voiceCountLabel.setColour(juce::Label::outlineColourId, accent_.withAlpha(0.28f));
    addAndMakeVisible(voiceCountLabel);

    // H5: Preset randomization
    randButton.setButtonText("RAND");
    randButton.setTooltip("Randomize synthesis parameters by up to 15%.");
    randButton.onClick = [this] { proc.randomizePreset(); };
    addAndMakeVisible(randButton);

    // M1: Delay BPM sync
    delaySyncButton.setButtonText("SYNC HOST");
    addChildComponent(delaySyncButton);
    delaySyncAtt = std::make_unique<BtnAttach>(proc.getAPVTS(), "delay_sync", delaySyncButton);

    delayNoteDivLabel.setText("DIVISION", juce::dontSendNotification);
    delayNoteDivLabel.setJustificationType(juce::Justification::centredLeft);
    delayNoteDivLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
    delayNoteDivLabel.setColour(juce::Label::textColourId, synthcol::textDim);
    addChildComponent(delayNoteDivLabel);

    delayNoteDivSelector.addItem("1/4",         1);
    delayNoteDivSelector.addItem("1/8",         2);
    delayNoteDivSelector.addItem("1/16",        3);
    delayNoteDivSelector.addItem("DOTTED 1/8",  4);
    delayNoteDivSelector.addItem("TRIPLET 1/8", 5);
    addChildComponent(delayNoteDivSelector);
    delayNoteDivAtt = std::make_unique<ComboBoxAttach>(proc.getAPVTS(), "delay_division", delayNoteDivSelector);

    // L1: FX Lock button
    fxLockButton.setButtonText("FX LOCK");
    addAndMakeVisible(fxLockButton);
    fxLockAtt = std::make_unique<BtnAttach>(proc.getAPVTS(), "fx_lock", fxLockButton);

    // --- MIDI CC page indicator (FLkey Mini) ---
    midiCCPageLabel.setText("CC: ---", juce::dontSendNotification);
    midiCCPageLabel.setJustificationType(juce::Justification::centredLeft);
    midiCCPageLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f).withStyle("Bold")));
    midiCCPageLabel.setColour(juce::Label::textColourId, juce::Colour(0xffBBBBCC));
    midiCCPageLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    midiCCPageLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(midiCCPageLabel);

    rebuildBassAttachments();
    configureValueDisplays();
    syncSelectionUiFromBass();
    syncFxAvailability();
    backgroundImage_ = juce::ImageCache::getFromMemory(
        BinaryData::fond_basse_png, BinaryData::fond_basse_pngSize);
    applyBassTheme(selectedBassFromParam());
    initCommon();
    updateKeyboardPresentation();

    startTimerHz(30);
    setResizable(true, true);
    setResizeLimits(960, 700, 2560, 1600);
    setSize(lay::W, lay::H);
}

// =============================================================================
// Timer
// =============================================================================
void BassSynthAudioProcessorEditor::timerCallback()
{
    rebuildBassAttachments();
    syncPresetBox();
    updateKeyboardPresentation();

    const int page = proc.getMidiCCPage();
    if (page != cachedMidiCCPage)
    {
        cachedMidiCCPage = page;
        midiCCPageLabel.setText(juce::String("CC: ") + BassSynthAudioProcessor::getCCPageName(page),
                                juce::dontSendNotification);
    }

    voiceCountLabel.setText(formatVoices(proc.getActiveVoiceCount()),
                            juce::dontSendNotification);
}

// =============================================================================
// Paint
// =============================================================================
void BassSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    paintBackground(g);
    const auto visualLayout = computeVisualLayoutSnapshot(getWidth(), getHeight());
    const auto& headerZones = visualLayout.headerZones;
    const auto selectorRect = visualLayout.selectorPanelBounds.toFloat();
    const auto col1Rect = juce::Rectangle<float>(static_cast<float>(visualLayout.col1X),
                                                 static_cast<float>(visualLayout.bodyY),
                                                 static_cast<float>(visualLayout.colW),
                                                 static_cast<float>(visualLayout.bodyH));
    const auto col2Rect = juce::Rectangle<float>(static_cast<float>(visualLayout.col2X),
                                                 static_cast<float>(visualLayout.bodyY),
                                                 static_cast<float>(visualLayout.colW),
                                                 static_cast<float>(visualLayout.bodyH));
    const auto col3Rect = juce::Rectangle<float>(static_cast<float>(visualLayout.col3X),
                                                 static_cast<float>(visualLayout.bodyY),
                                                 static_cast<float>(visualLayout.colW),
                                                 static_cast<float>(visualLayout.bodyH));
    const auto keyboardRect = juce::Rectangle<float>(static_cast<float>(visualLayout.contentX),
                                                     static_cast<float>(visualLayout.kbY),
                                                     static_cast<float>(visualLayout.contentW),
                                                     static_cast<float>(visualLayout.kbH));
    const auto statusSecondaryRow = visualLayout.statusSecondaryRow;
    const auto voiceLoad = juce::jlimit(0.0f, 1.0f,
                                        static_cast<float>(proc.getActiveVoiceCount())
                                            / static_cast<float>(BassSynthAudioProcessor::kMaxVoices));
    const auto outputLoad = static_cast<float>(juce::jmap(gainDial.getValue(), -24.0, 12.0, 0.0, 1.0));

    const auto blendWarm = [accent = accent_] (juce::Colour base, float amount)
    {
        return base.interpolatedWith(accent.withMultipliedSaturation(0.32f), amount);
    };

    auto paintHeaderLane = [&](juce::Rectangle<float> area, juce::Colour tint)
    {
        constexpr float laneRadius = 7.5f;
        const auto laneTop = blendWarm(juce::Colour(0xff171914), 0.18f).interpolatedWith(tint, 0.10f);
        const auto laneMid = blendWarm(juce::Colour(0xff12150F), 0.12f).interpolatedWith(tint, 0.08f);
        const auto laneBottom = blendWarm(juce::Colour(0xff0A0D0B), 0.06f).interpolatedWith(tint, 0.04f);
        juce::ColourGradient laneGrad(laneTop, area.getCentreX(), area.getY(),
                                      laneBottom, area.getCentreX(), area.getBottom(), false);
        laneGrad.addColour(0.45, laneMid);
        g.setGradientFill(laneGrad);
        g.fillRoundedRectangle(area, laneRadius);

        auto sheen = area.reduced(1.0f).withHeight(juce::jmax(9.0f, area.getHeight() * 0.28f));
        juce::ColourGradient sheenGrad(juce::Colours::white.withAlpha(0.016f), sheen.getCentreX(), sheen.getY(),
                                       juce::Colours::transparentWhite, sheen.getCentreX(), sheen.getBottom(), false);
        g.setGradientFill(sheenGrad);
        g.fillRoundedRectangle(sheen, juce::jmax(0.0f, laneRadius - 1.0f));

        g.setColour(juce::Colours::white.withAlpha(0.024f));
        g.drawRoundedRectangle(area.reduced(0.5f), laneRadius, 0.8f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.drawRoundedRectangle(area.expanded(0.25f), laneRadius + 0.4f, 0.8f);
    };

    auto paintLaneDivider = [&](const juce::Rectangle<int>& zone, const int dividerY)
    {
        const float x = static_cast<float>(zone.getX() + 14);
        const float right = static_cast<float>(zone.getRight() - 14);
        if (right <= x)
            return;

        const float y = static_cast<float>(dividerY);
        g.setColour(juce::Colours::white.withAlpha(0.024f));
        g.drawLine(x, y, right, y, 0.8f);
        g.setColour(juce::Colours::black.withAlpha(0.18f));
        g.drawLine(x, y + 1.0f, right, y + 1.0f, 0.9f);
    };

    auto paintSelectorPanel = [&](juce::Rectangle<float> area)
    {
        constexpr float radius = 8.0f;
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.fillRoundedRectangle(area.translated(0.0f, 3.0f), radius);

        juce::ColourGradient panelGrad(blendWarm(juce::Colour(0xff171713), 0.16f), area.getCentreX(), area.getY(),
                                       blendWarm(juce::Colour(0xff0B0D0B), 0.06f), area.getCentreX(), area.getBottom(), false);
        panelGrad.addColour(0.48, blendWarm(juce::Colour(0xff11120E), 0.10f));
        g.setGradientFill(panelGrad);
        g.fillRoundedRectangle(area, radius);

        auto sheen = area.reduced(1.0f).withHeight(juce::jmax(10.0f, area.getHeight() * 0.34f));
        juce::ColourGradient sheenGrad(juce::Colours::white.withAlpha(0.016f), sheen.getCentreX(), sheen.getY(),
                                       juce::Colours::transparentWhite, sheen.getCentreX(), sheen.getBottom(), false);
        g.setGradientFill(sheenGrad);
        g.fillRoundedRectangle(sheen, juce::jmax(0.0f, radius - 1.0f));

        g.setColour(accent_.withAlpha(0.08f));
        g.drawRoundedRectangle(area.reduced(0.5f), radius, 0.8f);

        auto titleArea = juce::Rectangle<int>(static_cast<int>(area.getX()) + 14,
                                              static_cast<int>(area.getY()) + 8,
                                              static_cast<int>(area.getWidth()) - 28,
                                              16);
        g.setColour(synthcol::text.withAlpha(0.90f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f).withStyle("Bold")));
        g.drawText("Family / Model", titleArea, juce::Justification::centredLeft);

        auto titleTrace = juce::Rectangle<float>(area.getX() + 14.0f, area.getY() + 26.0f,
                                                 juce::jmin(96.0f, area.getWidth() * 0.18f), 1.6f);
        juce::ColourGradient traceGrad(accent_.withAlpha(0.24f), titleTrace.getX(), titleTrace.getCentreY(),
                                       juce::Colours::transparentBlack, titleTrace.getRight(), titleTrace.getCentreY(), false);
        g.setGradientFill(traceGrad);
        g.fillRoundedRectangle(titleTrace, 0.8f);
    };

    paintHeader(g, visualLayout.headerH);
    paintHeaderLane(headerZones.presetZone.toFloat().reduced(1.0f, 1.0f), accent_.withAlpha(0.11f));
    paintHeaderLane(headerZones.statusZone.toFloat().reduced(1.0f, 1.0f), accent_.withAlpha(0.09f));
    paintLaneDivider(headerZones.presetZone, headerZones.presetSecondaryRow.getY() - 1);
    paintLaneDivider(headerZones.statusZone, headerZones.statusSecondaryRow.getY() - 1);

    paintSelectorPanel(selectorRect);
    paintCard(g, visualLayout.col1X, visualLayout.bodyY, visualLayout.colW, visualLayout.bodyH, "Source / Envelope");
    paintCard(g, visualLayout.col2X, visualLayout.bodyY, visualLayout.colW, visualLayout.bodyH, "Tone Shaping");
    paintCard(g, visualLayout.col3X, visualLayout.bodyY, visualLayout.colW, visualLayout.bodyH, "Performance / Routing / FX");
    glazeBassChrome(g, col1Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.90f);
    glazeBassChrome(g, col2Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.90f);
    glazeBassChrome(g, col3Rect.reduced(2.0f, 2.0f), accent_, 10.0f, 0.90f);

    const int meterY = statusSecondaryRow.getY() + 8;
    const int meterLeft = statusSecondaryRow.getRight() - 108;
    if (meterLeft >= statusSecondaryRow.getX() + 164)
    {
        paintMeterBar(g, { meterLeft, meterY, 50, 8 }, voiceLoad, accent_);
        paintMeterBar(g, { meterLeft + 56, meterY, 50, 8 }, outputLoad, accent_.brighter(0.20f));
        g.setColour(synthcol::textDim);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
        g.drawText("V", juce::Rectangle<int>(meterLeft - 10, meterY - 2, 10, 12), juce::Justification::centredLeft);
        g.drawText("G", juce::Rectangle<int>(meterLeft + 46, meterY - 2, 10, 12), juce::Justification::centredLeft);
    }

    g.setColour(accent_.withAlpha(0.12f));
    g.drawLine(static_cast<float>(visualLayout.contentX + 18), static_cast<float>(visualLayout.kbY - 8),
               static_cast<float>(visualLayout.contentX + visualLayout.contentW - 18), static_cast<float>(visualLayout.kbY - 8),
               1.0f);

    paintKeyboardDock(g, visualLayout.contentX, visualLayout.kbY, visualLayout.contentW, visualLayout.kbH);
    glazeBassChrome(g, keyboardRect.reduced(2.0f, 2.0f), accent_, 11.0f, 1.05f);
}

// =============================================================================
// Resized
// =============================================================================
void BassSynthAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const auto layout = computeVisualLayoutSnapshot(w, getHeight());
    const auto& headerZones = layout.headerZones;
    const float gapDensity = layoutDensity(layout.compact, layout.roomy);

    const int ctrlH = layout.compact ? 34 : 36;
    const auto presetPrimaryRow = headerZones.presetPrimaryRow.reduced(0, 1);
    const auto presetSecondaryRow = headerZones.presetSecondaryRow.reduced(0, 1);
    const auto statusPrimaryRow = layout.statusPrimaryRow;
    const auto statusSecondaryRow = layout.statusSecondaryRow;
    const int topRowY = presetPrimaryRow.getY() + (presetPrimaryRow.getHeight() - ctrlH) / 2;

    const int gainX = layout.gainSlotBounds.getX()
                    + juce::jmax(0, (layout.gainSlotBounds.getWidth() - layout.gainSize) / 2);
    const int gainY = layout.gainSlotBounds.getY()
                    + juce::jmax(0, (layout.gainSlotBounds.getHeight() - layout.gainSize) / 2)
                    + (layout.compact ? 1 : 2);
    gainDial.setBounds(gainX, gainY, layout.gainSize, layout.gainSize);

    const int navW = 26;
    int searchW = juce::jlimit(layout.compact ? 96 : 104,
                               layout.compact ? 132 : 148,
                               presetPrimaryRow.getWidth() / 5);
    int x = presetPrimaryRow.getX();
    const int presetW = juce::jmax(layout.compact ? 220 : 320,
                                   presetPrimaryRow.getRight() - x - searchW - navW * 2 - 16);
    presetSearch.setBounds(x, topRowY, searchW, ctrlH); x += searchW + 8;
    prevPresetBtn.setBounds(x, topRowY, navW, ctrlH); x += navW + 4;
    presetBox.setBounds(x, topRowY, presetW, ctrlH); x += presetW + 4;
    nextPresetBtn.setBounds(x, topRowY, navW, ctrlH);

    const int actionY = presetSecondaryRow.getY() + juce::jmax(0, (presetSecondaryRow.getHeight() - 22) / 2);
    const int saveW = layout.compact ? 56 : 64;
    const int saveAsW = layout.compact ? 66 : 78;
    const int deleteW = layout.compact ? 62 : 70;
    const int importW = layout.compact ? 62 : 70;
    const int btnGap = 8;
    const int actionBtnH = layout.compact ? 22 : 24;
    int actionX = presetSecondaryRow.getX();
    savePresetBtn.setBounds(actionX, actionY, saveW, actionBtnH); actionX += saveW + btnGap;
    saveAsPresetBtn.setBounds(actionX, actionY, saveAsW, actionBtnH); actionX += saveAsW + btnGap;
    deletePresetBtn.setBounds(actionX, actionY, deleteW, actionBtnH); actionX += deleteW + btnGap;
    importPresetsBtn.setBounds(actionX, actionY, importW, actionBtnH); actionX += importW + btnGap;

    const int statusBtnH = layout.compact ? 22 : 24;
    const int statusPrimaryY = statusPrimaryRow.getY() + juce::jmax(0, (statusPrimaryRow.getHeight() - statusBtnH) / 2);
    const int randW = layout.compact ? 62 : 70;
    randButton.setBounds(statusPrimaryRow.getX(), statusPrimaryY, randW, statusBtnH);

    const int statusY = statusSecondaryRow.getY() + juce::jmax(0, (statusSecondaryRow.getHeight() - statusBtnH) / 2);
    int statusX = statusSecondaryRow.getX();
    const int statusGap = interpolateGap(gapDensity, 4, 5, 6);
    const int tooltipW = layout.compact ? 82 : 90;
    const int voiceW = layout.compact ? 72 : 82;
    tooltipModeBtn.setBounds(statusX, statusY, tooltipW, statusBtnH); statusX += tooltipW + statusGap;
    voiceCountLabel.setBounds(statusX, statusY, voiceW, statusBtnH); statusX += voiceW + statusGap;
    midiCCPageLabel.setBounds(statusX, statusY,
                              juce::jmax(0, statusSecondaryRow.getRight() - statusX), statusBtnH);
    singleBtn.setVisible(false);
    singleBtn.setBounds(0, 0, 0, 0);

    const int selPad = layout.compact ? 12 : 14;
    const int selectorInnerX = layout.contentX + selPad;
    const int selectorInnerW = layout.contentW - selPad * 2;
    const int selectorLabelY = layout.selectorY + (layout.compact ? 28 : 30);
    const int selectorTopY = selectorLabelY + 14;
    const int selectorRowH = layout.compact ? 23 : 26;
    const int selectorGap = layout.compact ? 10 : 12;
    const int tabsZoneW = static_cast<int>(selectorInnerW * (layout.compact ? 0.58f : 0.62f));
    const int comboZoneW = selectorInnerW - tabsZoneW - selectorGap;
    const int tabGap = interpolateGap(gapDensity, 6, 8, 10);
    const int tabW = (tabsZoneW - tabGap * (mbs::kNumFamilies - 1)) / mbs::kNumFamilies;
    familySelectorLbl.setBounds(selectorInnerX, selectorLabelY, tabsZoneW, 12);
    for (int familyIndex = 0; familyIndex < mbs::kNumFamilies; ++familyIndex)
    {
        auto& tab = familyTabs[(size_t)familyIndex];
        tab.setBounds(selectorInnerX + familyIndex * (tabW + tabGap), selectorTopY, tabW, selectorRowH);
        tab.setVisible(true);
        tab.setSelected(familyIndex == activeFamilyIndex);
    }
    familySelector.setVisible(false);
    familySelector.setBounds(0, 0, 0, 0);
    bassSelector.setVisible(false);
    bassSelector.setBounds(0, 0, 0, 0);
    modelSelectorLbl.setBounds(selectorInnerX + tabsZoneW + selectorGap, selectorLabelY, comboZoneW, 12);
    modelSelector.setBounds(selectorInnerX + tabsZoneW + selectorGap, selectorTopY, comboZoneW, selectorRowH);
    for (auto& card : presetCards)
    {
        card.setVisible(false);
        card.setBounds(0, 0, 0, 0);
    }

    const int cPad = layout.compact ? 13 : 16;
    const int knobGapX = interpolateGap(gapDensity, 7, 10, 12);
    const int knobGapY = interpolateGap(gapDensity, 8, 10, 12);
    const int knobW = (layout.colW - cPad * 2 - knobGapX * 2) / 3;
    const int labelH = layout.compact ? 12 : 14;
    const int graphTargetH = layout.compact ? 86 : (layout.roomy ? 178 : 126);
    const int knobH = juce::jlimit(layout.compact ? 50 : 58,
                                   layout.roomy ? 98 : 84,
                                   (layout.bodyH - graphTargetH - cPad * 2 - labelH * 3 - knobGapY * 3) / 3);
    const int protectedKeyboardTop = layout.kbY - (layout.compact ? 14 : 18);

    const int sourceInnerX = layout.col1X + cPad;
    const int sourceInnerW = layout.colW - cPad * 2;
    const int sourceTopY = layout.bodyY + cPad + 28;
    const int sourceBottomY = protectedKeyboardTop - 8;
    const int envH = juce::jlimit(layout.compact ? 132 : 154,
                                  layout.roomy ? 250 : 212,
                                  static_cast<int>((sourceBottomY - sourceTopY) * 0.47f));
    envVisual.setVisible(true);
    envVisual.setBounds(sourceInnerX, sourceTopY, sourceInnerW, envH);

    const int sourceControlsY = envVisual.getBottom() + (layout.compact ? 8 : 10);
    const int adsrGapX = interpolateGap(gapDensity, 6, 8, 10);
    const int adsrGapY = interpolateGap(gapDensity, 8, 10, 12);
    const int remainingH = sourceBottomY - sourceControlsY;
    const int adsrW = (sourceInnerW - adsrGapX * 3) / 4;
    const int adsrKnobH = juce::jlimit(layout.compact ? 44 : 50,
                                       layout.roomy ? 78 : 64,
                                       juce::jmax(layout.compact ? 44 : 50,
                                                  (remainingH - labelH * 4 - adsrGapY * 2) / 2));
    const int secondaryGapX = interpolateGap(gapDensity, 8, 10, 12);
    const int secondaryW = (sourceInnerW - secondaryGapX * 2) / 3;
    const int smallSourceKnobH = juce::jlimit(layout.compact ? 42 : 48,
                                              layout.roomy ? 76 : 62,
                                              juce::jmax(layout.compact ? 42 : 48,
                                                         remainingH - adsrKnobH - labelH * 2 - adsrGapY - 4));
    auto layoutSourceDial = [this, labelH](int paramIndex, int xk, int yk, int width, int height)
    {
        auto si = static_cast<std::size_t>(paramIndex);
        envLabels[si].setBounds(xk, yk, width, labelH);
        envDials[si].setBounds(xk, yk + labelH, width, height);
    };
    layoutSourceDial(3, sourceInnerX, sourceControlsY, adsrW, adsrKnobH);
    layoutSourceDial(4, sourceInnerX + (adsrW + adsrGapX), sourceControlsY, adsrW, adsrKnobH);
    layoutSourceDial(5, sourceInnerX + 2 * (adsrW + adsrGapX), sourceControlsY, adsrW, adsrKnobH);
    layoutSourceDial(6, sourceInnerX + 3 * (adsrW + adsrGapX), sourceControlsY, adsrW, adsrKnobH);
    const int sourceTailY = sourceControlsY + labelH + adsrKnobH + adsrGapY;
    layoutSourceDial(9, sourceInnerX, sourceTailY, secondaryW, smallSourceKnobH);
    layoutSourceDial(0, sourceInnerX + secondaryW + secondaryGapX, sourceTailY, secondaryW, smallSourceKnobH);
    layoutSourceDial(1, sourceInnerX + 2 * (secondaryW + secondaryGapX), sourceTailY, secondaryW, smallSourceKnobH);

    int toneIdx[] = { 7, 10, 11, 2, 13, 8 };
    const int col2StartY = layout.bodyY + cPad + 28;
    for (int i = 0; i < 6; ++i)
    {
        int row = i / 3, col = i % 3;
        int xk = layout.col2X + cPad + col * (knobW + knobGapX);
        int yk = col2StartY + row * (knobH + labelH + knobGapY);
        auto si = (size_t)toneIdx[i];
        envLabels[si].setBounds(xk, yk, knobW, labelH);
        envDials[si].setBounds(xk, yk + labelH, knobW, knobH);
    }

    const int cutoffSize = juce::jlimit(layout.compact ? 66 : 72,
                                        layout.roomy ? 116 : 104,
                                        knobH + (layout.compact ? 14 : 18));
    const int cutoffX = layout.col2X + (layout.colW - cutoffSize) / 2;
    const int cutoffY = col2StartY + 2 * (knobH + labelH + knobGapY) + (layout.compact ? 6 : 10);
    envLabels[12].setBounds(cutoffX, cutoffY, cutoffSize, labelH);
    envDials[12].setBounds(cutoffX, cutoffY + labelH, cutoffSize, cutoffSize);

    const int perfY = cutoffY + labelH + cutoffSize + (layout.compact ? 10 : 14);
    const int perfCtrlH = juce::jlimit(layout.compact ? 40 : 46,
                                       layout.roomy ? 68 : 56,
                                       knobH + (layout.compact ? 2 : 6));
    const int toneFooterGap = interpolateGap(gapDensity, 10, 12, 14);
    const int toneFooterW = (layout.colW - cPad * 2 - toneFooterGap) / 2;
    resonanceLabel.setBounds(layout.col2X + cPad, perfY, toneFooterW, labelH);
    resonanceDial.setBounds(layout.col2X + cPad + (toneFooterW - knobW) / 2, perfY + labelH, knobW, perfCtrlH);
    monoModeLabel.setBounds(layout.col2X + cPad + toneFooterW + toneFooterGap, perfY, toneFooterW, labelH);
    monoModeSelector.setBounds(layout.col2X + cPad + toneFooterW + toneFooterGap, perfY + labelH + 4, toneFooterW, layout.compact ? 24 : 26);

    lfoRateDial.setVisible(false);
    lfoRateDial.setBounds(0, 0, 0, 0);
    lfoDepthDial.setVisible(false);
    lfoDepthDial.setBounds(0, 0, 0, 0);
    lfoWaveSelector.setVisible(false);
    lfoWaveSelector.setBounds(0, 0, 0, 0);
    lfoRateDial.setVisible(false);
    lfoDepthDial.setVisible(false);
    lfoWaveSelector.setVisible(false);
    lfoWaveLabel.setVisible(false);
    lfoRateLabel.setVisible(false);
    lfoDepthLabel.setVisible(false);
    lfoDestLabel.setVisible(false);
    lfoDestSelector.setVisible(false);
    lfoWaveLabel.setVisible(false);
    lfoWaveLabel.setBounds(0, 0, 0, 0);
    lfoRateLabel.setVisible(false);
    lfoRateLabel.setBounds(0, 0, 0, 0);
    lfoDepthLabel.setVisible(false);
    lfoDepthLabel.setBounds(0, 0, 0, 0);
    lfoDestLabel.setVisible(false);
    lfoDestLabel.setBounds(0, 0, 0, 0);
    lfoDestSelector.setVisible(false);
    lfoDestSelector.setBounds(0, 0, 0, 0);
    lfoVisual.setVisible(false);
    lfoVisual.setBounds(0, 0, 0, 0);
    modLfo2RateLabel.setVisible(false);
    modLfo2RateLabel.setBounds(0, 0, 0, 0);
    modLfo2WaveLabel.setVisible(false);
    modLfo2WaveLabel.setBounds(0, 0, 0, 0);
    modLfo2RateDial.setVisible(false);
    modLfo2RateDial.setBounds(0, 0, 0, 0);
    modLfo2WaveSelector.setVisible(false);
    modLfo2WaveSelector.setBounds(0, 0, 0, 0);
    modMatrixTitle.setVisible(false);
    modMatrixTitle.setBounds(0, 0, 0, 0);
    for (auto& row : modRows)
    {
        row.srcCombo.setVisible(false);
        row.dstCombo.setVisible(false);
        row.amtSlider.setVisible(false);
        row.srcCombo.setVisible(false);
        row.srcCombo.setBounds(0, 0, 0, 0);
        row.dstCombo.setVisible(false);
        row.dstCombo.setBounds(0, 0, 0, 0);
        row.amtSlider.setVisible(false);
        row.amtSlider.setBounds(0, 0, 0, 0);
    }

    const int col3StartY = layout.bodyY + cPad + 28;
    const int rightTabGap = interpolateGap(gapDensity, 6, 8, 10);
    const int rightTabH = layout.compact ? 23 : 26;
    const int rightTabW = (layout.colW - cPad * 2 - rightTabGap * (kRightPanelSections - 1)) / kRightPanelSections;
    for (int sectionIndex = 0; sectionIndex < kRightPanelSections; ++sectionIndex)
    {
        auto& tab = rightPanelTabs[(size_t)sectionIndex];
        tab.setBounds(layout.col3X + cPad + sectionIndex * (rightTabW + rightTabGap), col3StartY, rightTabW, rightTabH);
        tab.setSelected(sectionIndex == activeRightPanelSection);
    }
    const int sectionContentY = col3StartY + rightTabH + (layout.compact ? 10 : 12);

    const int macroGap = interpolateGap(gapDensity, 7, 9, 11);
    const int macroW = (layout.colW - cPad * 2 - macroGap * 3) / 4;
    const int macroH = juce::jlimit(layout.compact ? 44 : 50,
                                    layout.roomy ? 78 : 68,
                                    juce::jmin(macroW, knobH - (layout.compact ? 6 : 10)));
    for (int i = 0; i < kMacroVisible; ++i)
    {
        auto si = (size_t)i;
        macroLbls[si].setVisible(activeRightPanelSection == 0);
        macroDials[si].setVisible(activeRightPanelSection == 0);
        if (activeRightPanelSection == 0)
        {
            const int xk = layout.col3X + cPad + i * (macroW + macroGap);
            macroLbls[si].setBounds(xk, sectionContentY, macroW, labelH);
            macroDials[si].setBounds(xk, sectionContentY + labelH, macroW, macroH);
        }
        else
        {
            macroLbls[si].setVisible(false);
            macroLbls[si].setBounds(0, 0, 0, 0);
            macroDials[si].setVisible(false);
            macroDials[si].setBounds(0, 0, 0, 0);
        }
    }

    macroHintLabel.setVisible(activeRightPanelSection == 0);
    macroGuardrailLabel.setVisible(activeRightPanelSection == 0);
    const int macroInfoY = sectionContentY + labelH + macroH + (layout.compact ? 8 : 10);
    if (activeRightPanelSection == 0)
    {
        const int macroInfoW = layout.colW - cPad * 2;
        macroHintLabel.setBounds(layout.col3X + cPad, macroInfoY, macroInfoW, 14);
        macroGuardrailLabel.setBounds(layout.col3X + cPad, macroInfoY + 14, macroInfoW, 14);
    }
    else
    {
        macroHintLabel.setBounds(0, 0, 0, 0);
        macroGuardrailLabel.setBounds(0, 0, 0, 0);
    }

    // Performance controls: pitch bend + glide always visible, velocity curve only in MACRO section
    velocityCurveLabel.setVisible(activeRightPanelSection == 0);
    velocityCurveSelector.setVisible(activeRightPanelSection == 0);
    pitchBendRangeLabel.setVisible(true);
    pitchBendRangeDial.setVisible(true);
    glideTimeLabel.setVisible(true);
    glideTimeDial.setVisible(true);

    const int perfGap = interpolateGap(gapDensity, 8, 10, 12);
    const int perfControlH = layout.compact ? 24 : 26;
    int perfStripBottom = sectionContentY;

    if (activeRightPanelSection == 0)
    {
        const int perfLabelY = macroGuardrailLabel.getBottom() + (layout.compact ? 10 : 12);
        const int perfControlY = perfLabelY + 16;
        const int perfW = (layout.colW - cPad * 2 - perfGap * 2) / 3;
        velocityCurveLabel.setBounds(layout.col3X + cPad, perfLabelY, perfW, 14);
        velocityCurveSelector.setBounds(layout.col3X + cPad, perfControlY, perfW, perfControlH);
        pitchBendRangeLabel.setBounds(velocityCurveSelector.getRight() + perfGap, perfLabelY, perfW, 14);
        pitchBendRangeDial.setBounds(velocityCurveSelector.getRight() + perfGap, perfControlY, perfW, perfControlH);
        glideTimeLabel.setBounds(pitchBendRangeDial.getRight() + perfGap, perfLabelY, perfW, 14);
        glideTimeDial.setBounds(pitchBendRangeDial.getRight() + perfGap, perfControlY, perfW, perfControlH);

        const int modGapY = interpolateGap(gapDensity, 10, 12, 14);
        const int modY = juce::jmax(glideTimeDial.getBottom(), velocityCurveSelector.getBottom()) + modGapY;
        const int topControlGap = layout.compact ? 8 : 10;
        const int halfW = (layout.colW - cPad * 2 - topControlGap) / 2;
        lfoDestLabel.setVisible(true);
        lfoWaveLabel.setVisible(true);
        lfoRateLabel.setVisible(true);
        lfoDepthLabel.setVisible(true);
        lfoDestSelector.setVisible(true);
        lfoWaveSelector.setVisible(true);
        lfoRateDial.setVisible(true);
        lfoDepthDial.setVisible(true);
        lfoDestLabel.setBounds(layout.col3X + cPad, modY, halfW, 14);
        lfoWaveLabel.setBounds(layout.col3X + cPad + halfW + topControlGap, modY, halfW, 14);
        lfoDestSelector.setBounds(layout.col3X + cPad, modY + 16, halfW, layout.compact ? 24 : 26);
        lfoWaveSelector.setBounds(layout.col3X + cPad + halfW + topControlGap, modY + 16, halfW, layout.compact ? 24 : 26);
        const int lfoDialY = lfoDestSelector.getBottom() + (layout.compact ? 8 : 10);
        const int lfoDialH = juce::jlimit(layout.compact ? 42 : 48, layout.roomy ? 72 : 60, knobH - 4);
        lfoRateLabel.setBounds(layout.col3X + cPad, lfoDialY, halfW, 14);
        lfoDepthLabel.setBounds(layout.col3X + cPad + halfW + topControlGap, lfoDialY, halfW, 14);
        lfoRateDial.setBounds(layout.col3X + cPad, lfoDialY + 14, halfW, lfoDialH);
        lfoDepthDial.setBounds(layout.col3X + cPad + halfW + topControlGap, lfoDialY + 14, halfW, lfoDialH);

        const int lfoVisualY = lfoRateDial.getBottom() + (layout.compact ? 8 : 10);
        const int lfoVisualH = juce::jmax(84, protectedKeyboardTop - lfoVisualY - 8);
        lfoVisual.setVisible(true);
        lfoVisual.setBounds(layout.col3X + cPad, lfoVisualY, layout.colW - cPad * 2, lfoVisualH);
    }
    else
    {
        // Pitch bend + glide visible at top of right panel in MOD MATRIX and FX sections
        const int perfLabelY = sectionContentY;
        const int perfControlY = perfLabelY + 16;
        const int perfW = (layout.colW - cPad * 2 - perfGap) / 2;
        velocityCurveLabel.setVisible(false);
        velocityCurveLabel.setBounds(0, 0, 0, 0);
        velocityCurveSelector.setVisible(false);
        velocityCurveSelector.setBounds(0, 0, 0, 0);
        pitchBendRangeLabel.setBounds(layout.col3X + cPad, perfLabelY, perfW, 14);
        pitchBendRangeDial.setBounds(layout.col3X + cPad, perfControlY, perfW, perfControlH);
        glideTimeLabel.setBounds(pitchBendRangeDial.getRight() + perfGap, perfLabelY, perfW, 14);
        glideTimeDial.setBounds(pitchBendRangeDial.getRight() + perfGap, perfControlY, perfW, perfControlH);
        perfStripBottom = perfControlY + perfControlH + (layout.compact ? 6 : 8);
    }

    if (activeRightPanelSection == 1)
    {
        const int matrixY = perfStripBottom;
        modMatrixTitle.setVisible(true);
        modMatrixTitle.setText("MOD MATRIX", juce::dontSendNotification);
        modMatrixTitle.setBounds(layout.col3X + cPad, matrixY, layout.colW - cPad * 2, 14);
        const int controlGap = layout.compact ? 8 : 10;
        const int controlW = (layout.colW - cPad * 2 - controlGap) / 2;
        const int controlLabelY = modMatrixTitle.getBottom() + 4;
        const int controlY = controlLabelY + 16;
        const int controlH = layout.compact ? 24 : 26;
        modLfo2RateLabel.setVisible(true);
        modLfo2WaveLabel.setVisible(true);
        modLfo2RateDial.setVisible(true);
        modLfo2WaveSelector.setVisible(true);
        modLfo2RateLabel.setBounds(layout.col3X + cPad, controlLabelY, controlW, 14);
        modLfo2WaveLabel.setBounds(layout.col3X + cPad + controlW + controlGap, controlLabelY, controlW, 14);
        modLfo2RateDial.setBounds(layout.col3X + cPad, controlY, controlW, controlH);
        modLfo2WaveSelector.setBounds(layout.col3X + cPad + controlW + controlGap, controlY, controlW, controlH);
        int rowY = juce::jmax(modLfo2RateDial.getBottom(), modLfo2WaveSelector.getBottom()) + 8;
        const int rowGap = 4;
        const int rowH = layout.compact ? 20 : 22;
        const int srcW = juce::jlimit(64, 80, (layout.colW - cPad * 2) / 4);
        const int dstW = juce::jlimit(74, 92, (layout.colW - cPad * 2) / 3);
        const int amtW = juce::jmax(70, layout.colW - cPad * 2 - srcW - dstW - rowGap * 2);
        int visibleRowCount = 0;
        for (auto& row : modRows)
        {
            if (rowY + rowH > protectedKeyboardTop - 8)
                break;
            row.srcCombo.setVisible(true);
            row.dstCombo.setVisible(true);
            row.amtSlider.setVisible(true);
            row.srcCombo.setBounds(layout.col3X + cPad, rowY, srcW, rowH);
            row.dstCombo.setBounds(layout.col3X + cPad + srcW + rowGap, rowY, dstW, rowH);
            row.amtSlider.setBounds(layout.col3X + cPad + srcW + rowGap + dstW + rowGap, rowY, amtW, rowH);
            rowY += rowH + rowGap;
            ++visibleRowCount;
        }
        const int hiddenRowCount = static_cast<int>(modRows.size()) - visibleRowCount;
        modMatrixTitle.setText(hiddenRowCount > 0
            ? "MOD MATRIX (+" + juce::String(hiddenRowCount) + " off-screen)"
            : "MOD MATRIX",
            juce::dontSendNotification);
    }

    const bool selectedFxAvailable = activeFxTab >= 0 && activeFxTab < kFxTabs && isFxTabAvailable(activeFxTab);
    fxLockButton.setVisible(activeRightPanelSection == 2);
    if (activeRightPanelSection == 2)
    {
        fxDetailTitle.setVisible(selectedFxAvailable);
        fxUnavailableLbl.setVisible(!selectedFxAvailable);
    }
    else
    {
        fxDetailTitle.setVisible(false);
        fxUnavailableLbl.setVisible(false);
    }
    delaySyncButton.setVisible(false);
    delayNoteDivLabel.setVisible(false);
    delayNoteDivSelector.setVisible(false);
    if (activeRightPanelSection == 2)
    {
        fxLockButton.setBounds(layout.col3X + layout.colW - cPad - 82, perfStripBottom, 82, 20);
    }
    else
    {
        fxLockButton.setVisible(false);
        fxLockButton.setBounds(0, 0, 0, 0);
    }

    const int fxAreaY = (activeRightPanelSection == 2 ? perfStripBottom : sectionContentY) + 18;
    const int fxAreaH = juce::jmax(180, protectedKeyboardTop - fxAreaY - 10);
    constexpr int kBypassW = 34;
    constexpr int kRackGap = 14;
    constexpr int kRackRowGap = 4;
    const int rackTotalW = juce::jlimit(layout.compact ? 104 : 112,
                                        layout.roomy ? 152 : 136,
                                        layout.colW / 2 - 6);
    const int rackItemW = rackTotalW - kBypassW - 6;
    const int rackRowH = juce::jlimit(layout.compact ? 18 : 20,
                                      layout.roomy ? 30 : 26,
                                      (fxAreaH - kRackRowGap * (kFxTabs - 1)) / kFxTabs);
    int availableFxTabs = 0;
    for (int t = 0; t < kFxTabs; ++t)
        if (isFxTabAvailable(t))
            ++availableFxTabs;
    const int rackBlockH = availableFxTabs > 0
        ? availableFxTabs * rackRowH + kRackRowGap * (availableFxTabs - 1)
        : 0;
    const int rackStartY = fxAreaY + juce::jmax(0, (fxAreaH - rackBlockH) / 2);
    int visibleRackRow = 0;
    for (int t = 0; t < kFxTabs; ++t)
    {
        const bool available = activeRightPanelSection == 2 && isFxTabAvailable(t);
        fxRackItems[(size_t)t].setVisible(available);
        fxBypassBtns[(size_t)t].setVisible(available);
        if (!available)
        {
            fxRackItems[(size_t)t].setVisible(false);
            fxRackItems[(size_t)t].setBounds(0, 0, 0, 0);
            fxBypassBtns[(size_t)t].setVisible(false);
            fxBypassBtns[(size_t)t].setBounds(0, 0, 0, 0);
            continue;
        }
        const int rowY = rackStartY + visibleRackRow * (rackRowH + kRackRowGap);
        fxRackItems[(size_t)t].setBounds(layout.col3X + cPad, rowY, rackItemW, rackRowH);
        fxBypassBtns[(size_t)t].setBounds(layout.col3X + cPad + rackItemW + 6,
                                          rowY + juce::jmax(0, (rackRowH - 20) / 2),
                                          kBypassW, 20);
        ++visibleRackRow;
    }

    const int detailX = layout.col3X + cPad + rackTotalW + kRackGap;
    const int detailW = layout.colW - cPad * 2 - rackTotalW - kRackGap;
    fxDetailTitle.setBounds(detailX, fxAreaY, detailW, 16);
    fxUnavailableLbl.setBounds(detailX, fxAreaY + 22, detailW, 36);
    for (int i = 0; i < kFxN; ++i)
    {
        fxDials[(size_t)i].setVisible(false);
        fxLbls[(size_t)i].setVisible(false);
        fxDials[(size_t)i].setVisible(false);
        fxDials[(size_t)i].setBounds(0, 0, 0, 0);
        fxLbls[(size_t)i].setVisible(false);
        fxLbls[(size_t)i].setBounds(0, 0, 0, 0);
    }
    const bool showDelayOptions = activeFxTab == kDelayFxTab;
    const int currentFxTab = (activeFxTab >= 0 && activeFxTab < kFxTabs) ? activeFxTab : firstAvailableFxTab();
    int visibleCount = 0;
    for (int slot = 0; slot < kFxPerTab; ++slot)
        if (currentFxTab >= 0 && kFxTabMap[currentFxTab][slot] >= 0)
            ++visibleCount;
    const int detailTop = fxAreaY + 26;
    const int detailBottomReserve = showDelayOptions ? 34 : 0;
    const int detailH = juce::jmax(80, fxAreaY + fxAreaH - detailTop - detailBottomReserve);
    const int detailCols = chooseFxDetailColumns(visibleCount);
    const int detailRows = juce::jmax(1, (visibleCount + detailCols - 1) / detailCols);
    const int fxGapX = layout.compact ? 8 : 12;
    const int fxGapY = layout.compact ? 8 : 12;
    const int detailDialW = detailCols > 0 ? (detailW - fxGapX * (detailCols - 1)) / detailCols : detailW;
    const int detailDialH = juce::jlimit(layout.compact ? 40 : 48,
        layout.roomy ? 112 : 86,
        (detailH - labelH * detailRows - fxGapY * juce::jmax(0, detailRows - 1)) / detailRows);
    int visibleIndex = 0;
    for (int slot = 0; slot < kFxPerTab; ++slot)
    {
        const int fxIndex = currentFxTab >= 0 ? kFxTabMap[currentFxTab][slot] : -1;
        if (fxIndex < 0) continue;
        const int row = visibleIndex / detailCols;
        const int col = visibleIndex % detailCols;
        const int xk = detailX + col * (detailDialW + fxGapX);
        const int yk = detailTop + row * (detailDialH + labelH + fxGapY);
        const auto si = (size_t)fxIndex;
        if (activeRightPanelSection == 2)
        {
            fxLbls[si].setBounds(xk, yk, detailDialW, labelH);
            fxDials[si].setBounds(xk, yk + labelH, detailDialW, detailDialH);
            fxLbls[si].setVisible(true);
            fxDials[si].setVisible(true);
        }
        ++visibleIndex;
    }
    if (showDelayOptions && activeRightPanelSection == 2)
    {
        const int optionsY = fxAreaY + fxAreaH - 30;
        delaySyncButton.setVisible(true);
        delayNoteDivLabel.setVisible(true);
        delayNoteDivSelector.setVisible(true);
        delaySyncButton.setBounds(detailX, optionsY, 94, 24);
        delayNoteDivLabel.setBounds(delaySyncButton.getRight() + 10, optionsY + 4, 58, 16);
        delayNoteDivSelector.setBounds(delayNoteDivLabel.getRight() + 6, optionsY,
                                       juce::jmax(110, detailW - 180), 24);
    }
    else
    {
        delaySyncButton.setVisible(false);
        delaySyncButton.setBounds(0, 0, 0, 0);
        delayNoteDivLabel.setVisible(false);
        delayNoteDivLabel.setBounds(0, 0, 0, 0);
        delayNoteDivSelector.setVisible(false);
        delayNoteDivSelector.setBounds(0, 0, 0, 0);
    }

    const int keyboardInsetLeft = 78;
    const int keyboardInsetTop = layout.compact ? 6 : 8;
    const int keyboardH = juce::jmax(36, layout.kbH - keyboardInsetTop * 2);
    const int keyboardCenterY = layout.kbY + keyboardInsetTop + keyboardH / 2;
    keyboard->setBounds(layout.contentX + keyboardInsetLeft, layout.kbY + keyboardInsetTop,
                        layout.contentW - keyboardInsetLeft - 6, keyboardH);
    octaveDownBtn.setBounds(layout.contentX + 14, keyboardCenterY - 14, 24, 26);
    octaveUpBtn.setBounds(layout.contentX + 42, keyboardCenterY - 14, 24, 26);

    auto applyLbl = [layout](juce::Label& l) {
        l.setFont(juce::Font(juce::FontOptions{}.withHeight(layout.compact ? 11.0f : 12.0f).withStyle("Bold")));
        l.setColour(juce::Label::textColourId, synthcol::textSec);
    };
    for (auto& l : envLabels) applyLbl(l);
    for (auto& l : macroLbls) applyLbl(l);
    for (auto& l : fxLbls)    applyLbl(l);
    applyLbl(glideTimeLabel);
    applyLbl(resonanceLabel);
    applyLbl(monoModeLabel);
    applyLbl(lfoDestLabel);
    applyLbl(lfoWaveLabel);
    applyLbl(lfoRateLabel);
    applyLbl(lfoDepthLabel);
    applyLbl(velocityCurveLabel);
    applyLbl(pitchBendRangeLabel);
    applyLbl(modLfo2RateLabel);
    applyLbl(modLfo2WaveLabel);

    updateKeyboardPresentation();
}

// =============================================================================
// Switch effect tab
// =============================================================================
bool BassSynthAudioProcessorEditor::isFxTabAvailable(int tabIndex) const
{
    return tabIndex >= 0 && tabIndex < kFxTabs
        && proc.isFxAvailableForCurrentBass(kBassFxSlots[(size_t)tabIndex]);
}

int BassSynthAudioProcessorEditor::firstAvailableFxTab() const
{
    for (int t = 0; t < kFxTabs; ++t)
        if (isFxTabAvailable(t))
            return t;
    return 0;
}

void BassSynthAudioProcessorEditor::syncFxAvailability()
{
    const int fallbackTab = firstAvailableFxTab();
    if (activeFxTab < 0 || activeFxTab >= kFxTabs || !isFxTabAvailable(activeFxTab))
    {
        activeFxTab = fallbackTab;
        fxDetailTitle.setText(juce::String("DETAIL: ") + kFxTabNames[activeFxTab], juce::dontSendNotification);
        syncFxRackState();
        resized();
        repaint();
        return;
    }

    fxDetailTitle.setText(juce::String("DETAIL: ") + kFxTabNames[activeFxTab], juce::dontSendNotification);
    syncFxRackState();
    resized();
    repaint();
}

void BassSynthAudioProcessorEditor::switchEffectTab(int tabIndex)
{
    if (!isFxTabAvailable(tabIndex))
        return;
    if (activeFxTab == tabIndex)
        return;

    activeFxTab = tabIndex;
    fxDetailTitle.setText(juce::String("DETAIL: ") + kFxTabNames[tabIndex], juce::dontSendNotification);

    const bool showDelayOpts = activeFxTab == kDelayFxTab;
    delaySyncButton.setVisible(showDelayOpts);
    delayNoteDivLabel.setVisible(showDelayOpts);
    delayNoteDivSelector.setVisible(showDelayOpts);

    fxUnavailableLbl.setVisible(activeRightPanelSection == 2 && !isFxTabAvailable(activeFxTab));

    syncFxRackState();
    resized();
    repaint();
}

void BassSynthAudioProcessorEditor::switchRightPanelSection(int sectionIndex)
{
    sectionIndex = juce::jlimit(0, kRightPanelSections - 1, sectionIndex);
    if (activeRightPanelSection == sectionIndex)
        return;

    activeRightPanelSection = sectionIndex;
    if (activeRightPanelSection == 2)
        syncFxAvailability();

    resized();
    repaint();
}

// =============================================================================
// Sync FX rack visual state
// =============================================================================
void BassSynthAudioProcessorEditor::syncFxRackState()
{
    for (int t = 0; t < kFxTabs; ++t)
    {
        if (!isFxTabAvailable(t)) continue; // item est masqué dans resized()
        auto& rackItem = fxRackItems[(size_t)t];
        auto& bypass = fxBypassBtns[(size_t)t];
        rackItem.setSelected(t == activeFxTab);
        rackItem.setEnabledState(bypass.getToggleState());
        bypass.setEnabled(true);
    }
}

// =============================================================================
// Bass attachment management
// =============================================================================
void BassSynthAudioProcessorEditor::rebuildBassAttachments()
{
    auto bassIdx = selectedBassFromParam();
    if (bassIdx == cachedBassIdx) return;
    cachedBassIdx = bassIdx;
    const auto family = mbs::getFamily(bassIdx);
    const auto& profile = envProfileForBass(bassIdx);
    const auto& macroLabels = macroLabelsForFamily(family);

    for (auto& a : envAttach) a.reset();
    resonanceAtt.reset();

    for (int i = 0; i < kEnvN; ++i)
    {
        auto si = (size_t)i;
        auto id = BassSynthAudioProcessor::makeBassParamId(
            cachedBassIdx, envControlSuffixForBass(cachedBassIdx, i));
        envAttach[si] = std::make_unique<SliderAttach>(
            proc.getAPVTS(), id, envDials[si]);
    }

    for (int i = 0; i < kEnvN; ++i)
    {
        const auto si = static_cast<std::size_t>(i);
        switch (i)
        {
            case 0:
            case 2:
            case 5:
            case 7:
            case 9:
            case 10:
            case 11:
                setupDial(envDials[si], accent_);
                break;
            case 1:
                setupDial(envDials[si], accent_);
                break;
            case 3:
            case 4:
            case 6:
                setupDial(envDials[si], accent_);
                break;
            case 8:
                setupDial(envDials[si], accent_);
                break;
            case 12:
                setupGrandDial(envDials[si], accent_, " Hz");
                break;
            case 13:
                setupDial(envDials[si], accent_);
                break;
            default:
                setupDial(envDials[si], accent_);
                break;
        }
    }
    synthui::applyLabelProfile(profile, envLabels);
    synthui::applyMacroLabelProfile(macroLabels, macroLbls);
    updateMacroContext(bassIdx);
    resonanceAtt = std::make_unique<SliderAttach>(
        proc.getAPVTS(),
        BassSynthAudioProcessor::makeBassParamId(cachedBassIdx, "resonance"),
        resonanceDial);
    configureValueDisplays();
    applyBassTheme(bassIdx);
    syncFamilyControlVisibility();

    activeFamilyIndex = static_cast<int>(family);
    syncSelectionUiFromBass();
    syncFxAvailability();
    syncFxRackState();

    // Refresh preset browser to show presets for the newly selected bass
    refreshPresetList();

    repaint();
}

void BassSynthAudioProcessorEditor::syncFamilyControlVisibility()
{
    const int bassIdx = cachedBassIdx >= 0 ? cachedBassIdx : selectedBassFromParam();
    for (int controlIndex = 0; controlIndex < kEnvN; ++controlIndex)
    {
        const bool visible = shouldShowEnvControl(bassIdx, controlIndex);
        envLabels[static_cast<std::size_t>(controlIndex)].setVisible(visible);
        envDials[static_cast<std::size_t>(controlIndex)].setVisible(visible);
    }
}

void BassSynthAudioProcessorEditor::updateMacroContext(int bassIndex)
{
    const auto family = mbs::getFamily(bassIndex);
    const auto& context = macroUiContextForFamily(family);

    macroHintLabel.setText(context.hint, juce::dontSendNotification);
    macroGuardrailLabel.setText(context.guardrail, juce::dontSendNotification);

    const synthui::TooltipMode mode = tooltipMode == TooltipMode::Short ? synthui::TooltipMode::Short
                                   : tooltipMode == TooltipMode::Novice ? synthui::TooltipMode::Novice
                                                                        : synthui::TooltipMode::Off;

    for (int macroIndex = 0; macroIndex < kMacroTotal; ++macroIndex)
    {
        juce::String tooltip;
        if (mode == synthui::TooltipMode::Short)
            tooltip = context.shortTooltips[static_cast<std::size_t>(macroIndex)];
        else if (mode == synthui::TooltipMode::Novice)
            tooltip = context.noviceTooltips[static_cast<std::size_t>(macroIndex)];
        macroDials[static_cast<std::size_t>(macroIndex)].setTooltip(tooltip);
    }
}

void BassSynthAudioProcessorEditor::applyBassTheme(int bassIndex)
{
    const auto accent = bassCatColour(bassIndex);
    const auto controlText = accent.brighter(0.10f).withMultipliedSaturation(0.78f);
    const auto panelBg = juce::Colour(0xff181A1D).interpolatedWith(accent.withMultipliedSaturation(0.22f), 0.08f);
    const auto knobAccent = accent.withMultipliedSaturation(0.82f).interpolatedWith(juce::Colour(0xffB9B6AC), 0.14f);
    const auto knobGlow = accent.darker(0.32f).withMultipliedSaturation(0.82f);
    const auto knobBezel = juce::Colour(0xff3C3A36).interpolatedWith(accent.darker(0.85f), 0.24f);
    const auto knobCollar = juce::Colour(0xff5C5850).interpolatedWith(accent.darker(0.62f), 0.30f);
    const auto knobCapTint = juce::Colour(0xffCDC8BC).interpolatedWith(accent.withMultipliedSaturation(0.50f), 0.22f);

    auto applyKnobPalette = [&](juce::Slider& slider)
    {
        slider.setColour(juce::Slider::rotarySliderFillColourId, knobAccent);
        slider.setColour(SynthLookAndFeel::knobGlowColourId, knobGlow);
        slider.setColour(SynthLookAndFeel::knobBezelColourId, knobBezel);
        slider.setColour(SynthLookAndFeel::knobCollarColourId, knobCollar);
        slider.setColour(SynthLookAndFeel::knobCapAccentColourId, knobCapTint);
    };

    setAccentTheme(accent);
    envVisual.setAccent(accent);
    lfoVisual.setAccent(accent);

    for (auto& dial : envDials)
        applyKnobPalette(dial);
    for (auto& dial : macroDials)
        applyKnobPalette(dial);
    for (auto& dial : fxDials)
        applyKnobPalette(dial);

    applyKnobPalette(glideTimeDial);
    glideTimeDial.setColour(juce::Slider::trackColourId, accent.withAlpha(0.66f));
    glideTimeDial.setColour(juce::Slider::thumbColourId, knobAccent.brighter(0.02f));
    applyKnobPalette(resonanceDial);
    pitchBendRangeDial.setColour(juce::Slider::trackColourId, accent.withAlpha(0.66f));
    pitchBendRangeDial.setColour(juce::Slider::thumbColourId, knobAccent.brighter(0.02f));
    modLfo2RateDial.setColour(juce::Slider::trackColourId, accent.withAlpha(0.66f));
    modLfo2RateDial.setColour(juce::Slider::thumbColourId, knobAccent.brighter(0.02f));

    lfoDestSelector.setColour(juce::ComboBox::backgroundColourId, panelBg);
    lfoDestSelector.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.30f));
    lfoWaveSelector.setColour(juce::ComboBox::backgroundColourId, panelBg);
    lfoWaveSelector.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.30f));
    modLfo2WaveSelector.setColour(juce::ComboBox::backgroundColourId, panelBg);
    modLfo2WaveSelector.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.30f));
    monoModeSelector.setColour(juce::ComboBox::backgroundColourId, panelBg);
    monoModeSelector.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.30f));
    velocityCurveSelector.setColour(juce::ComboBox::backgroundColourId, panelBg);
    velocityCurveSelector.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.30f));
    delayNoteDivSelector.setColour(juce::ComboBox::backgroundColourId, panelBg);
    delayNoteDivSelector.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.30f));
    for (auto& row : modRows)
    {
        row.srcCombo.setColour(juce::ComboBox::backgroundColourId, panelBg);
        row.srcCombo.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.30f));
        row.dstCombo.setColour(juce::ComboBox::backgroundColourId, panelBg);
        row.dstCombo.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.30f));
        row.amtSlider.setColour(juce::Slider::trackColourId, accent.withAlpha(0.78f));
        row.amtSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1E2226));
        row.amtSlider.setColour(juce::Slider::thumbColourId, knobAccent.brighter(0.05f));
    }

    tooltipModeBtn.setColour(juce::TextButton::buttonColourId, panelBg);
    tooltipModeBtn.setColour(juce::TextButton::textColourOffId, controlText);
    randButton.setColour(juce::TextButton::buttonColourId, panelBg);
    randButton.setColour(juce::TextButton::textColourOffId, controlText);
    delaySyncButton.setColour(juce::ToggleButton::tickColourId, accent);
    fxLockButton.setColour(juce::ToggleButton::tickColourId, accent);

    voiceCountLabel.setColour(juce::Label::textColourId, controlText);
    voiceCountLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff171B16).withAlpha(0.88f));
    voiceCountLabel.setColour(juce::Label::outlineColourId, accent.withAlpha(0.45f));
    macroHintLabel.setColour(juce::Label::textColourId, controlText.withAlpha(0.88f));
    macroGuardrailLabel.setColour(juce::Label::textColourId, synthcol::textDim.interpolatedWith(accent, 0.18f));
    midiCCPageLabel.setColour(juce::Label::textColourId, controlText);
    midiCCPageLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff171B16).withAlpha(0.82f));
    midiCCPageLabel.setColour(juce::Label::outlineColourId, accent.withAlpha(0.45f));
    fxDetailTitle.setColour(juce::Label::textColourId, controlText);

    for (auto& tab : rightPanelTabs)
        tab.setAccent(accent);
    for (auto& rackItem : fxRackItems)
        rackItem.setAccent(accent);

    syncFxRackState();
}

void BassSynthAudioProcessorEditor::configureValueDisplays()
{
    const auto percentFromText = [](const juce::String& text) { return parseNumericText(text) / 100.0; };
    const auto signedPercentFromText = [](const juce::String& text) { return parseNumericText(text) / 100.0; };
    const auto secondsFromText = [](const juce::String& text)
    {
        const auto raw = parseNumericText(text);
        return text.containsIgnoreCase("ms") ? raw / 1000.0 : raw;
    };
    const auto millisecondsFromText = [](const juce::String& text)
    {
        const auto raw = parseNumericText(text);
        return text.containsIgnoreCase("ms") || !text.containsIgnoreCase("s") ? raw : raw * 1000.0;
    };
    const auto frequencyFromText = [](const juce::String& text)
    {
        const auto raw = parseNumericText(text);
        return text.containsIgnoreCase("khz") ? raw * 1000.0 : raw;
    };
    const auto panFromText = [](const juce::String& text)
    {
        const auto trimmed = text.trim();
        if (trimmed.equalsIgnoreCase("C"))
            return 0.0;
        if (trimmed.startsWithIgnoreCase("L"))
            return -juce::jlimit(0.0, 100.0, parseNumericText(trimmed)) / 100.0;
        if (trimmed.startsWithIgnoreCase("R"))
            return juce::jlimit(0.0, 100.0, parseNumericText(trimmed)) / 100.0;
        return juce::jlimit(-1.0, 1.0, parseNumericText(trimmed));
    };

    auto setPercentDisplay = [&](juce::Slider& slider)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        slider.textFromValueFunction = [](double value) { return formatPercent(value); };
        slider.valueFromTextFunction = percentFromText;
    };

    auto setSignedPercentDisplay = [&](juce::Slider& slider)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        slider.textFromValueFunction = [](double value) { return formatSignedPercent(value); };
        slider.valueFromTextFunction = signedPercentFromText;
    };

    auto setSecondsDisplay = [&](juce::Slider& slider)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 18);
        slider.textFromValueFunction = [](double value) { return formatSeconds(value); };
        slider.valueFromTextFunction = secondsFromText;
    };

    auto setMillisecondsDisplay = [&](juce::Slider& slider)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
        slider.textFromValueFunction = [](double value) { return formatMilliseconds(value); };
        slider.valueFromTextFunction = millisecondsFromText;
    };

    auto setFrequencyDisplay = [&](juce::Slider& slider, int width = 96)
    {
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, width, 18);
        slider.textFromValueFunction = [](double value) { return formatFrequency(value); };
        slider.valueFromTextFunction = frequencyFromText;
    };

    envDials[0].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    envDials[0].textFromValueFunction = [](double value) { return formatPercent(value); };
    envDials[0].valueFromTextFunction = percentFromText;

    envDials[1].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    envDials[1].textFromValueFunction = [](double value) { return formatSemitones(value); };
    envDials[1].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    setPercentDisplay(envDials[2]);
    setSecondsDisplay(envDials[3]);
    setSecondsDisplay(envDials[4]);
    setPercentDisplay(envDials[5]);
    setSecondsDisplay(envDials[6]);
    setPercentDisplay(envDials[7]);
    setPercentDisplay(envDials[8]);
    setPercentDisplay(envDials[9]);
    setPercentDisplay(envDials[10]);
    setPercentDisplay(envDials[11]);
    setFrequencyDisplay(envDials[12]);

    envDials[13].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    envDials[13].textFromValueFunction = [](double value) { return formatPanValue(value); };
    envDials[13].valueFromTextFunction = panFromText;

    for (auto& slider : macroDials)
        setPercentDisplay(slider);

    setMillisecondsDisplay(lfoRateDial);
    lfoRateDial.textFromValueFunction = [](double value) { return formatLfoRate(value); };
    lfoRateDial.valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };
    setPercentDisplay(lfoDepthDial);
    modLfo2RateDial.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 56, 22);
    modLfo2RateDial.textFromValueFunction = [](double value) { return formatLfoRate(value); };
    modLfo2RateDial.valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    resonanceDial.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    resonanceDial.textFromValueFunction = [](double value) { return formatPercent(value); };
    resonanceDial.valueFromTextFunction = percentFromText;

    glideTimeDial.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 56, 22);
    glideTimeDial.textFromValueFunction = [](double value) { return formatSeconds(value); };
    glideTimeDial.valueFromTextFunction = secondsFromText;

    pitchBendRangeDial.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 62, 22);
    pitchBendRangeDial.textFromValueFunction = [](double value)
    {
        return juce::String(juce::roundToInt(value)) + " st";
    };
    pitchBendRangeDial.valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    fxDials[0].textFromValueFunction = [](double value) { return juce::String(value, value < 10.0 ? 1 : 0) + "x"; };
    fxDials[0].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };
    fxDials[0].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);

    setPercentDisplay(fxDials[1]);
    setSignedPercentDisplay(fxDials[2]);
    setSignedPercentDisplay(fxDials[3]);
    setPercentDisplay(fxDials[4]);

    fxDials[5].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    fxDials[5].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[5].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    fxDials[6].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    fxDials[6].textFromValueFunction = [](double value) { return formatRatio(value); };
    fxDials[6].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    setMillisecondsDisplay(fxDials[7]);
    setMillisecondsDisplay(fxDials[8]);

    fxDials[9].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    fxDials[9].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[9].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    setPercentDisplay(fxDials[10]);
    setFrequencyDisplay(fxDials[11], 86);

    fxDials[12].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    fxDials[12].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[12].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    setFrequencyDisplay(fxDials[13], 86);

    fxDials[14].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    fxDials[14].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[14].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    fxDials[15].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    fxDials[15].textFromValueFunction = [](double value) { return juce::String(value, value < 10.0 ? 2 : 1); };
    fxDials[15].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    setFrequencyDisplay(fxDials[16], 86);

    fxDials[17].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    fxDials[17].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[17].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    fxDials[18].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    fxDials[18].textFromValueFunction = [](double value) { return formatLfoRate(value); };
    fxDials[18].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    setPercentDisplay(fxDials[19]);
    setPercentDisplay(fxDials[20]);
    setMillisecondsDisplay(fxDials[21]);
    setPercentDisplay(fxDials[22]);
    setPercentDisplay(fxDials[23]);
    setPercentDisplay(fxDials[24]);
    setPercentDisplay(fxDials[25]);
    setPercentDisplay(fxDials[26]);
    setPercentDisplay(fxDials[27]);

    fxDials[28].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    fxDials[28].textFromValueFunction = [](double value) { return formatDb(value); };
    fxDials[28].valueFromTextFunction = [](const juce::String& text) { return parseNumericText(text); };

    setMillisecondsDisplay(fxDials[29]);

    auto refreshDisplay = [](juce::Slider& slider)
    {
        slider.setValue(slider.getValue(), juce::dontSendNotification);
    };

    for (auto& slider : envDials) refreshDisplay(slider);
    for (auto& slider : macroDials) refreshDisplay(slider);
    for (auto& slider : fxDials) refreshDisplay(slider);
    refreshDisplay(lfoRateDial);
    refreshDisplay(lfoDepthDial);
    refreshDisplay(modLfo2RateDial);
    refreshDisplay(glideTimeDial);
    refreshDisplay(resonanceDial);
    refreshDisplay(pitchBendRangeDial);
}

void BassSynthAudioProcessorEditor::updateKeyboardPresentation()
{
    if (keyboard == nullptr || keyboard->getWidth() <= 0)
        return;

    const int rangeStart = keyboard->getRangeStart();
    const int rangeEnd = keyboard->getRangeEnd();
    const int whiteKeys = countWhiteKeysInRange(rangeStart, rangeEnd);
    const float targetKeyWidth = juce::jmax(14.0f, (keyboard->getWidth() - 2.0f) / static_cast<float>(whiteKeys));
    keyboard->setKeyWidth(targetKeyWidth);
}

// =============================================================================
// Selection UI
// =============================================================================
void BassSynthAudioProcessorEditor::rebuildModelSelectorForFamily(
    int familyIndex, int preferredBass)
{
    familyIndex = juce::jlimit(0, mbs::kNumFamilies - 1, familyIndex);
    modelSelector.clear(juce::dontSendNotification);

    const int first = mbs::kFamilyStart[familyIndex];
    const int count = mbs::kFamilySize[familyIndex];

    for (int i = 0; i < count; ++i)
        modelSelector.addItem(kBassDisplayNames[(size_t)(first + i)], first + i + 1);

    int targetBass = preferredBass;
    if (targetBass < first || targetBass >= first + count) targetBass = first;
    modelSelector.setSelectedId(targetBass + 1, juce::dontSendNotification);
}

void BassSynthAudioProcessorEditor::syncSelectionUiFromBass()
{
    const int bassIndex   = selectedBassFromParam();
    const int familyIndex = static_cast<int>(mbs::getFamily(bassIndex));

    activeFamilyIndex = familyIndex;

    const bool familyChanged = familySelector.getSelectedId() != familyIndex + 1;
    const bool modelChanged = modelSelector.getSelectedId() != bassIndex + 1;

    if (familyChanged)
        familySelector.setSelectedId(familyIndex + 1, juce::dontSendNotification);

    if (familyChanged || modelChanged)
        rebuildModelSelectorForFamily(familyIndex, bassIndex);

    for (int tabIndex = 0; tabIndex < mbs::kNumFamilies; ++tabIndex)
        familyTabs[(size_t)tabIndex].setSelected(tabIndex == familyIndex);
}

// =============================================================================
// Tooltip mode cycling
// =============================================================================
void BassSynthAudioProcessorEditor::cycleTooltipMode()
{
    switch (tooltipMode)
    {
        case TooltipMode::Off:    tooltipMode = TooltipMode::Short;  break;
        case TooltipMode::Short:  tooltipMode = TooltipMode::Novice; break;
        case TooltipMode::Novice: tooltipMode = TooltipMode::Off;    break;
    }

    switch (tooltipMode)
    {
        case TooltipMode::Off:
        case TooltipMode::Short:
        case TooltipMode::Novice:
            tooltipModeBtn.setButtonText(tooltipMode == TooltipMode::Off ? "TIP: OFF"
                                                                         : tooltipMode == TooltipMode::Short ? "TIP: SHORT"
                                                                                                              : "TIP: NOVICE");
            break;
    }

    tooltipWindow.setVisible(tooltipMode != TooltipMode::Off);
    applyTooltips();
}

// =============================================================================
// Apply tooltips according to current mode
// =============================================================================
void BassSynthAudioProcessorEditor::applyTooltips()
{
    const char** src = nullptr;
    if (tooltipMode == TooltipMode::Short)  src = kTooltipsShort;
    if (tooltipMode == TooltipMode::Novice) src = kTooltipsNovice;
    const auto& profile = envProfileForBass(cachedBassIdx >= 0 ? cachedBassIdx : selectedBassFromParam());
    const auto sharedTooltipMode = tooltipMode == TooltipMode::Short ? synthui::TooltipMode::Short
                                 : tooltipMode == TooltipMode::Novice ? synthui::TooltipMode::Novice
                                                                      : synthui::TooltipMode::Off;

    int idx = 0;

    synthui::applyTooltipProfile(profile, envDials, sharedTooltipMode);
    idx += kEnvN;

    lfoRateDial .setTooltip(src ? juce::String(src[idx])     : juce::String()); ++idx;
    lfoDepthDial.setTooltip(src ? juce::String(src[idx])     : juce::String()); ++idx;

    idx += kMacroTotal;
    updateMacroContext(cachedBassIdx >= 0 ? cachedBassIdx : selectedBassFromParam());

    for (int i = 0; i < kFxN; ++i, ++idx)
        fxDials[(size_t)i].setTooltip(src ? juce::String(src[idx]) : juce::String());

    gainDial.setTooltip(src ? juce::String(src[idx]) : juce::String());
}

