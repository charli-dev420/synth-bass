#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Shared/PresetManifest.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstring>

namespace
{
constexpr const char* kOutputGain          = "output_gain";
constexpr const char* kSelectedBass        = "selected_bass";
constexpr const char* kLfoRate             = "lfo_rate";
constexpr const char* kLfoDepth            = "lfo_depth";
constexpr const char* kLfoWave             = "lfo_wave";

constexpr int kPresetFormatVersion = 3;
constexpr int kSynthIndex = 3;

constexpr const char* kMacroFatness     = "macro_fatness";
constexpr const char* kMacroBrillance   = "macro_brillance";
constexpr const char* kMacroPunch       = "macro_punch";
constexpr const char* kMacroDepth       = "macro_depth";
constexpr const char* kModWheelTarget   = "mod_wheel_target";

constexpr const char* kCompThreshold = "comp_threshold";
constexpr const char* kCompRatio     = "comp_ratio";
constexpr const char* kCompAttack    = "comp_attack";
constexpr const char* kCompRelease   = "comp_release";
constexpr const char* kCompMakeup    = "comp_makeup";
constexpr const char* kCompMix       = "comp_mix";

constexpr const char* kSatDrive = "sat_drive";
constexpr const char* kSatMix   = "sat_mix";

constexpr const char* kTransientAttack  = "transient_attack";
constexpr const char* kTransientSustain = "transient_sustain";
constexpr const char* kTransientMix     = "transient_mix";

constexpr const char* kEqLowFreq  = "eq_low_freq";
constexpr const char* kEqLowGain  = "eq_low_gain";
constexpr const char* kEqMidFreq  = "eq_mid_freq";
constexpr const char* kEqMidGain  = "eq_mid_gain";
constexpr const char* kEqMidQ     = "eq_mid_q";
constexpr const char* kEqHighFreq = "eq_high_freq";
constexpr const char* kEqHighGain = "eq_high_gain";

constexpr const char* kChorusRate  = "chorus_rate";
constexpr const char* kChorusDepth = "chorus_depth";
constexpr const char* kChorusMix   = "chorus_mix";

constexpr const char* kDelayTime     = "delay_time";
constexpr const char* kDelayFeedback = "delay_feedback";
constexpr const char* kDelayMix      = "delay_mix";
constexpr const char* kDelaySync     = "delay_sync";
constexpr const char* kDelayNoteDiv  = "delay_division";

constexpr const char* kReverbSize    = "reverb_size";
constexpr const char* kReverbDamping = "reverb_damping";
constexpr const char* kReverbWidth   = "reverb_width";
constexpr const char* kReverbMix     = "reverb_mix";

constexpr const char* kLimiterThreshold = "limiter_threshold";
constexpr const char* kLimiterRelease   = "limiter_release";

constexpr const char* kMonoMode   = "mono_mode";
constexpr const char* kGlideTime  = "glide_time";
constexpr const char* kLfoDest    = "lfo_dest";
constexpr const char* kVelocityCurve = "velocity_curve";
constexpr const char* kPitchBendRange = "pitch_bend_range";
constexpr const char* kModLfo2Rate = "mod_lfo2_rate";
constexpr const char* kModLfo2Wave = "mod_lfo2_wave";
constexpr const char* kFxLock = "fx_lock";

constexpr const char* kBassOutputSuffix = "output";
constexpr const char* kBassDataRootEnv  = "UWDEVST_BASS_DATA_ROOT";
constexpr const char* kPresetFormatVersionAttr = "format_version";
constexpr const char* kPresetInstrumentIndexAttr = "instrument_index";
constexpr const char* kPresetFactoryIndexAttr = "preset_index";
constexpr const char* kPresetSynthIndexAttr = "synth_index";
constexpr const char* kPresetMixRoleAttr = "mix_role";
constexpr const char* kPresetFamilyAttr = "family";
constexpr const char* kPresetTagsAttr = "tags";
constexpr const char* kPresetNominalPeakDbAttr = "nominal_peak_db";

constexpr double kFxMixRampSeconds = 0.010;
constexpr double kFxTransientRampSeconds = 0.020;
constexpr double kFxCompressorRampSeconds = 0.020;
constexpr double kFxEqRampSeconds = 0.020;
constexpr double kFxChorusRampSeconds = 0.020;
constexpr double kFxDelayRampSeconds = 0.040;
constexpr double kFxReverbRampSeconds = 0.040;
constexpr double kFxLimiterRampSeconds = 0.010;

BassSynthAudioProcessor::ParamBinding bindParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    BassSynthAudioProcessor::ParamBinding binding;
    binding.raw = apvts.getRawParameterValue(paramId);
    binding.ranged = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(paramId));
    return binding;
}

void setRangedParameterValue(juce::RangedAudioParameter* parameter, float actualValue, bool notifyHost)
{
    if (parameter == nullptr)
        return;

    const auto normalised = parameter->convertTo0to1(actualValue);
    if (notifyHost)
    {
        parameter->setValueNotifyingHost(normalised);
    }
    else
    {
        parameter->setValue(normalised);
        parameter->sendValueChangedMessageToListeners(normalised);
    }
}

juce::StringArray makeOutputChoices()
{
    juce::StringArray outputs;
    outputs.add("Master");
    for (int i = 0; i < BassSynthAudioProcessor::kNumAuxOutputs; ++i)
        outputs.add("Out " + juce::String(i + 1));
    return outputs;
}

float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, v); }

juce::String slugifyName(juce::String value)
{
    value = value.toLowerCase().replaceCharacter(' ', '-');
    value = value.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_");
    while (value.contains("--"))
        value = value.replace("--", "-");
    return value.trimCharactersAtStart("-").trimCharactersAtEnd("-");
}

juce::String familyLabelForBass(const int bassIndex)
{
    switch (mbs::getFamily(bassIndex))
    {
        case mbs::Family::Acoustic: return "acoustic";
        case mbs::Family::Eight08:  return "808";
        case mbs::Family::Synth:
        default:                    return "synth";
    }
}

juce::String joinFactoryTags(const std::vector<std::string>& tags)
{
    juce::StringArray items;
    for (const auto& tag : tags)
        items.add(juce::String(juce::CharPointer_UTF8(tag.c_str())));
    return items.joinIntoString(",");
}

std::vector<std::string> splitTags(const juce::String& tags)
{
    juce::StringArray items;
    items.addTokens(tags, ",", "\"");
    items.trim();
    items.removeEmptyStrings();

    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(items.size()));
    for (const auto& item : items)
        result.emplace_back(item.toStdString());
    return result;
}

juce::String formatPresetRoleLabel(const juce::String& rawRole)
{
    juce::String role = rawRole.trim();
    if (role.isEmpty())
        return "CUSTOM";

    role = role.replace("-", " ").replace("_", " ");
    juce::StringArray tokens;
    tokens.addTokens(role, " ", "\"");
    tokens.trim();
    tokens.removeEmptyStrings();

    juce::StringArray formatted;
    for (auto token : tokens)
        formatted.add(token.substring(0, 1).toUpperCase() + token.substring(1).toLowerCase());

    return formatted.joinIntoString(" ").toUpperCase();
}

juce::String makePresetSearchText(const juce::String& name,
                                  const juce::String& modelName,
                                  const juce::String& role,
                                  const juce::String& family,
                                  const juce::String& tags)
{
    juce::StringArray parts;
    parts.add(name);
    parts.add(modelName);
    parts.add(role);
    parts.add(family);

    juce::StringArray tagItems;
    tagItems.addTokens(tags, ",", "\"");
    tagItems.trim();
    tagItems.removeEmptyStrings();
    for (const auto& tag : tagItems)
        parts.add(tag);

    parts.removeEmptyStrings();
    return parts.joinIntoString(" ").toLowerCase();
}

struct PresetMetadataSnapshot
{
    juce::String mixRole = "custom";
    juce::String family = "bass";
    juce::String tags = "bass,user,custom";
    float nominalPeakDb = -12.0f;
};

struct PresetPersistenceState
{
    juce::String name;
    int bassIndex = 0;
    std::optional<int> presetIndex;
    mbs::BassSettings settings;
    mbs::GlobalFxSettings fx;
    int outputBus = 0;
    mbs::PatchPerformanceSettings performance;
    modmatrix::MatrixState modMatrix;
    PresetMetadataSnapshot metadata;
};

float readFiniteXmlFloat(const juce::XmlElement& xml,
                         const char* attrName,
                         float fallback,
                         float minValue,
                         float maxValue,
                         int* warningCount = nullptr);
void writePerformanceAttributes(juce::XmlElement& root, const mbs::PatchPerformanceSettings& performance);
void writeGlobalFxAttributes(juce::XmlElement& root, const mbs::GlobalFxSettings& fx);

PresetMetadataSnapshot makeUserPresetMetadata(const int bassIndex)
{
    PresetMetadataSnapshot metadata;
    metadata.mixRole = "custom";
    metadata.family = familyLabelForBass(bassIndex);
    metadata.tags = juce::StringArray {
        "bass", "user", "custom", metadata.family, slugifyName(mbs::getBassName(bassIndex))
    }.joinIntoString(",");
    metadata.nominalPeakDb = -12.0f;
    return metadata;
}

PresetMetadataSnapshot makeFactoryPresetMetadata(const mbs::PresetMetadata& source)
{
    PresetMetadataSnapshot metadata;
    metadata.mixRole = juce::String(juce::CharPointer_UTF8(source.mixRole.c_str()));
    metadata.family = juce::String(juce::CharPointer_UTF8(source.familyLabel.c_str()));
    metadata.tags = joinFactoryTags(source.tags);
    metadata.nominalPeakDb = source.nominalPeakDb;
    return metadata;
}

PresetMetadataSnapshot readPresetMetadataFromXml(const juce::XmlElement& xml,
                                                 PresetMetadataSnapshot fallback)
{
    if (xml.hasAttribute(kPresetMixRoleAttr))
        fallback.mixRole = xml.getStringAttribute(kPresetMixRoleAttr);
    if (xml.hasAttribute(kPresetFamilyAttr))
        fallback.family = xml.getStringAttribute(kPresetFamilyAttr);
    if (xml.hasAttribute(kPresetTagsAttr))
        fallback.tags = xml.getStringAttribute(kPresetTagsAttr);
    if (xml.hasAttribute(kPresetNominalPeakDbAttr))
        fallback.nominalPeakDb = readFiniteXmlFloat(xml, kPresetNominalPeakDbAttr, fallback.nominalPeakDb, -24.0f, 0.0f);
    return fallback;
}

bool hasCompleteMetadata(const juce::XmlElement& xml)
{
    return xml.hasAttribute(kPresetMixRoleAttr)
        && xml.hasAttribute(kPresetFamilyAttr)
        && xml.hasAttribute(kPresetTagsAttr)
        && xml.hasAttribute(kPresetNominalPeakDbAttr);
}

void writePresetMetadataAttributes(juce::XmlElement& root, const PresetMetadataSnapshot& metadata)
{
    root.setAttribute(kPresetMixRoleAttr, metadata.mixRole);
    root.setAttribute(kPresetFamilyAttr, metadata.family);
    root.setAttribute(kPresetTagsAttr, metadata.tags);
    root.setAttribute(kPresetNominalPeakDbAttr, static_cast<double>(metadata.nominalPeakDb));
}

void writeCanonicalModMatrixXml(juce::XmlElement& parent, const modmatrix::MatrixState& state)
{
    auto* matrix = parent.createNewChildElement("ModMatrix");
    matrix->setAttribute("pbRange", state.pitchBendRange);
    matrix->setAttribute("lfo2Rate", static_cast<double>(state.lfo2Rate));
    matrix->setAttribute("lfo2Wave", state.lfo2Wave);

    for (int slotIndex = 0; slotIndex < modmatrix::ModulationMatrix::getNumSlots(); ++slotIndex)
    {
        const auto& slot = state.slots[static_cast<std::size_t>(slotIndex)];
        auto* slotXml = matrix->createNewChildElement("Slot");
        slotXml->setAttribute("idx", slotIndex);
        slotXml->setAttribute("src", static_cast<int>(slot.source));
        slotXml->setAttribute("dst", static_cast<int>(slot.destination));
        slotXml->setAttribute("amt", static_cast<double>(juce::jlimit(-1.0f, 1.0f, slot.amount)));
    }
}

bool hasCompleteCanonicalModMatrix(const juce::XmlElement& xml)
{
    const auto* matrix = xml.getChildByName("ModMatrix");
    if (matrix == nullptr
        || !matrix->hasAttribute("pbRange")
        || !matrix->hasAttribute("lfo2Rate")
        || !matrix->hasAttribute("lfo2Wave"))
    {
        return false;
    }

    std::array<bool, modmatrix::kMaxSlots> seen {};
    int slotCount = 0;
    for (auto* slotXml : matrix->getChildWithTagNameIterator("Slot"))
    {
        if (!slotXml->hasAttribute("idx")
            || !slotXml->hasAttribute("src")
            || !slotXml->hasAttribute("dst")
            || !slotXml->hasAttribute("amt"))
        {
            return false;
        }

        const int idx = slotXml->getIntAttribute("idx", -1);
        if (idx < 0 || idx >= modmatrix::kMaxSlots || seen[static_cast<std::size_t>(idx)])
            return false;

        seen[static_cast<std::size_t>(idx)] = true;
        ++slotCount;
    }

    return slotCount == modmatrix::kMaxSlots;
}

bool hasCanonicalIdentityAttributes(const juce::XmlElement& xml, const bool requiresFactoryIndex)
{
    if (!xml.hasAttribute("name")
        || !xml.hasAttribute("bass")
        || !xml.hasAttribute(kPresetInstrumentIndexAttr)
        || xml.getIntAttribute(kPresetSynthIndexAttr, -1) != kSynthIndex)
    {
        return false;
    }

    if (requiresFactoryIndex
        && (!xml.hasAttribute("index") || !xml.hasAttribute(kPresetFactoryIndexAttr)))
    {
        return false;
    }

    return true;
}

bool hasCanonicalInstrumentAttributes(const juce::XmlElement& xml)
{
    static constexpr const char* kAttrs[] = {
        "level", "tune", "brightness", "attack", "decay", "sustain", "release",
        "body", "drive", "pitch_env", "filter_env", "sub", "character", "cutoff", "pan",
        "resonance", "glide_time", "output"
    };

    return std::all_of(std::begin(kAttrs), std::end(kAttrs),
                       [&xml](const char* attr) { return xml.hasAttribute(attr); });
}

bool hasCanonicalPerformanceAttributes(const juce::XmlElement& xml)
{
    static constexpr const char* kAttrs[] = {
        "mono_mode", "lfo_rate", "lfo_depth", "lfo_wave", "lfo_dest",
        "macro_fatness", "macro_brillance", "macro_punch", "macro_depth",
        "mod_wheel_target", "pitch_bend_range"
    };

    return std::all_of(std::begin(kAttrs), std::end(kAttrs),
                       [&xml](const char* attr) { return xml.hasAttribute(attr); });
}

bool hasCanonicalFxAttributes(const juce::XmlElement& xml)
{
    static constexpr const char* kAttrs[] = {
        "sat_drive", "sat_mix",
        "transient_attack", "transient_sustain", "transient_mix",
        "comp_threshold", "comp_ratio", "comp_attack", "comp_release", "comp_makeup", "comp_mix",
        "eq_low_freq", "eq_low_gain", "eq_mid_freq", "eq_mid_gain", "eq_mid_q", "eq_high_freq", "eq_high_gain",
        "chorus_rate", "chorus_depth", "chorus_mix",
        "delay_time", "delay_feedback", "delay_mix", "delay_sync", "delay_division",
        "reverb_size", "reverb_damping", "reverb_width", "reverb_mix",
        "limiter_threshold", "limiter_release",
        "fx_tab0_en", "fx_tab1_en", "fx_tab2_en", "fx_tab3_en",
        "fx_tab4_en", "fx_tab5_en", "fx_tab6_en", "fx_tab7_en"
    };

    return std::all_of(std::begin(kAttrs), std::end(kAttrs),
                       [&xml](const char* attr) { return xml.hasAttribute(attr); });
}

bool shouldRewritePresetXml(const juce::XmlElement& xml)
{
    const auto formatVersion = xml.getIntAttribute(kPresetFormatVersionAttr, 0);
    const bool isFactoryPreset = xml.hasTagName("FactoryPreset");
    return formatVersion < kPresetFormatVersion
        || !hasCanonicalIdentityAttributes(xml, isFactoryPreset)
        || !hasCanonicalInstrumentAttributes(xml)
        || !hasCanonicalPerformanceAttributes(xml)
        || !hasCanonicalFxAttributes(xml)
        || !hasCompleteMetadata(xml)
        || !hasCompleteCanonicalModMatrix(xml);
}

modmatrix::MatrixState sanitizeModMatrixState(modmatrix::MatrixState state)
{
    state.pitchBendRange = juce::jlimit(1, 24, state.pitchBendRange);
    state.lfo2Rate = juce::jlimit(0.05f, 12.0f, std::isfinite(state.lfo2Rate) ? state.lfo2Rate : 2.0f);
    state.lfo2Wave = juce::jlimit(0, 3, state.lfo2Wave);
    for (auto& slot : state.slots)
    {
        slot.source = static_cast<modmatrix::Source>(
            juce::jlimit(0, modmatrix::kSourceCount - 1, static_cast<int>(slot.source)));
        slot.destination = static_cast<modmatrix::Destination>(
            juce::jlimit(0, modmatrix::kDestCount - 1, static_cast<int>(slot.destination)));
        slot.amount = juce::jlimit(-1.0f, 1.0f, std::isfinite(slot.amount) ? slot.amount : 0.0f);
    }
    return state;
}

std::unique_ptr<juce::XmlElement> createPresetXml(const juce::String& rootTag,
                                                  const PresetPersistenceState& state)
{
    auto root = std::make_unique<juce::XmlElement>(rootTag);
    root->setAttribute(kPresetFormatVersionAttr, kPresetFormatVersion);
    root->setAttribute("name", state.name);
    root->setAttribute("bass", state.bassIndex);
    root->setAttribute(kPresetInstrumentIndexAttr, state.bassIndex);
    root->setAttribute(kPresetSynthIndexAttr, kSynthIndex);
    if (state.presetIndex.has_value())
    {
        root->setAttribute("index", *state.presetIndex);
        root->setAttribute(kPresetFactoryIndexAttr, *state.presetIndex);
    }

    root->setAttribute("level", static_cast<double>(state.settings.level));
    root->setAttribute("tune", static_cast<double>(state.settings.tuneSemitones));
    root->setAttribute("brightness", static_cast<double>(state.settings.brightness));
    root->setAttribute("attack", static_cast<double>(state.settings.attackSeconds));
    root->setAttribute("decay", static_cast<double>(state.settings.decaySeconds));
    root->setAttribute("sustain", static_cast<double>(state.settings.sustainLevel));
    root->setAttribute("release", static_cast<double>(state.settings.releaseSeconds));
    root->setAttribute("body", static_cast<double>(state.settings.body));
    root->setAttribute("drive", static_cast<double>(state.settings.drive));
    root->setAttribute("pitch_env", static_cast<double>(state.settings.pitchEnv));
    root->setAttribute("filter_env", static_cast<double>(state.settings.filterEnv));
    root->setAttribute("sub", static_cast<double>(state.settings.subLevel));
    root->setAttribute("character", static_cast<double>(state.settings.character));
    root->setAttribute("cutoff", static_cast<double>(state.settings.cutoffHz));
    root->setAttribute("pan", static_cast<double>(state.settings.pan));
    root->setAttribute("resonance", static_cast<double>(state.settings.resonance));
    root->setAttribute("glide_time", static_cast<double>(state.settings.glideTime));
    root->setAttribute("output", state.outputBus);
    writeGlobalFxAttributes(*root, state.fx);
    writePerformanceAttributes(*root, state.performance);
    writePresetMetadataAttributes(*root, state.metadata);
    writeCanonicalModMatrixXml(*root, state.modMatrix);
    return root;
}

bool tryParseXmlDoubleAttribute(const juce::XmlElement& xml, const char* attrName, double& value)
{
    if (!xml.hasAttribute(attrName))
        return false;

    const auto raw = xml.getStringAttribute(attrName).trim();
    if (raw.isEmpty())
        return false;

    char* end = nullptr;
    const auto parsed = std::strtod(raw.toRawUTF8(), &end);
    if (end == raw.toRawUTF8() || end == nullptr || *end != '\0' || !std::isfinite(parsed))
        return false;

    value = parsed;
    return true;
}

float readValidatedXmlFloat(const juce::XmlElement& xml,
                            const char* attrName,
                            float fallback,
                            float minValue,
                            float maxValue,
                            int& warningCount)
{
    double parsed = 0.0;
    if (!tryParseXmlDoubleAttribute(xml, attrName, parsed))
    {
        if (xml.hasAttribute(attrName))
            ++warningCount;
        return fallback;
    }

    const auto clamped = juce::jlimit(minValue, maxValue, static_cast<float>(parsed));
    if (clamped != static_cast<float>(parsed))
        ++warningCount;
    return clamped;
}

float readFiniteXmlFloat(const juce::XmlElement& xml,
                         const char* attrName,
                         float fallback,
                         float minValue,
                         float maxValue,
                         int* warningCount)
{
    int localWarnings = 0;
    const auto value = readValidatedXmlFloat(xml, attrName, fallback, minValue, maxValue, localWarnings);
    if (warningCount != nullptr)
        *warningCount += localWarnings;
    return value;
}

float readFiniteStateFloat(const juce::ValueTree& state,
                           const juce::String& paramId,
                           float fallback,
                           float minValue,
                           float maxValue,
                           int* warningCount = nullptr)
{
    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
    {
        const auto child = state.getChild(childIndex);
        if (child.getProperty("id").toString() != paramId)
            continue;

        const auto rawValue = child.getProperty("value");
        if (rawValue.isDouble() || rawValue.isInt() || rawValue.isInt64() || rawValue.isBool())
        {
            const auto numeric = static_cast<float>(static_cast<double>(rawValue));
            if (std::isfinite(numeric))
            {
                const auto clamped = juce::jlimit(minValue, maxValue, numeric);
                if (warningCount != nullptr && std::abs(clamped - numeric) > 1.0e-4f)
                    ++(*warningCount);
                return clamped;
            }
        }

        const auto stringValue = rawValue.toString().trim();
        if (stringValue.isNotEmpty())
        {
            char* end = nullptr;
            const auto parsed = std::strtod(stringValue.toRawUTF8(), &end);
            if (end != stringValue.toRawUTF8() && end != nullptr && *end == '\0' && std::isfinite(parsed))
            {
                const auto numeric = static_cast<float>(parsed);
                const auto clamped = juce::jlimit(minValue, maxValue, numeric);
                if (warningCount != nullptr && std::abs(clamped - numeric) > 1.0e-4f)
                    ++(*warningCount);
                return clamped;
            }
        }

        if (warningCount != nullptr)
            ++(*warningCount);
        return fallback;
    }

    return fallback;
}

void setStateFloatProperty(juce::ValueTree& state, const juce::String& paramId, float value)
{
    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
    {
        auto child = state.getChild(childIndex);
        if (child.getProperty("id").toString() == paramId)
        {
            child.setProperty("value", value, nullptr);
            return;
        }
    }
}

void writePerformanceAttributes(juce::XmlElement& root, const mbs::PatchPerformanceSettings& performance)
{
    root.setAttribute("mono_mode",        performance.monoMode);
    root.setAttribute("lfo_rate",         static_cast<double>(performance.lfoRate));
    root.setAttribute("lfo_depth",        static_cast<double>(performance.lfoDepth));
    root.setAttribute("lfo_wave",         performance.lfoWave);
    root.setAttribute("lfo_dest",         performance.lfoDest);
    root.setAttribute("macro_fatness",    static_cast<double>(performance.macroFatness));
    root.setAttribute("macro_brillance",  static_cast<double>(performance.macroBrillance));
    root.setAttribute("macro_punch",      static_cast<double>(performance.macroPunch));
    root.setAttribute("macro_depth",      static_cast<double>(performance.macroDepth));
    root.setAttribute("mod_wheel_target", performance.modWheelTarget);
    root.setAttribute("pitch_bend_range", static_cast<double>(performance.pitchBendRange));
}

bool readPerformanceAttributes(const juce::XmlElement& xml,
                               mbs::PatchPerformanceSettings& performance,
                               int* warningCount = nullptr)
{
    int localWarnings = 0;
    bool foundAny = false;
    const auto readIntAttribute = [&](const char* attrName,
                                      int& target,
                                      const float minValue,
                                      const float maxValue)
    {
        if (!xml.hasAttribute(attrName))
            return;

        target = static_cast<int>(std::round(readFiniteXmlFloat(
            xml, attrName, static_cast<float>(target), minValue, maxValue, &localWarnings)));
        foundAny = true;
    };
    const auto readFloatAttribute = [&](const char* attrName,
                                        float& target,
                                        const float minValue,
                                        const float maxValue)
    {
        if (!xml.hasAttribute(attrName))
            return;

        target = readFiniteXmlFloat(xml, attrName, target, minValue, maxValue, &localWarnings);
        foundAny = true;
    };

    readIntAttribute("mono_mode", performance.monoMode, 0.0f, 2.0f);
    readFloatAttribute("lfo_rate", performance.lfoRate, 0.05f, 12.0f);
    readFloatAttribute("lfo_depth", performance.lfoDepth, 0.0f, 1.0f);
    readIntAttribute("lfo_wave", performance.lfoWave, 0.0f, 3.0f);
    readIntAttribute("lfo_dest", performance.lfoDest, 0.0f, 2.0f);
    readFloatAttribute("macro_fatness", performance.macroFatness, 0.0f, 1.0f);
    readFloatAttribute("macro_brillance", performance.macroBrillance, 0.0f, 1.0f);
    readFloatAttribute("macro_punch", performance.macroPunch, 0.0f, 1.0f);
    readFloatAttribute("macro_depth", performance.macroDepth, 0.0f, 1.0f);
    readIntAttribute("mod_wheel_target", performance.modWheelTarget, 0.0f, 1.0f);
    readFloatAttribute("pitch_bend_range", performance.pitchBendRange, 1.0f, 24.0f);

    if (warningCount != nullptr)
        *warningCount += localWarnings;

    return foundAny;
}

void writeGlobalFxAttributes(juce::XmlElement& root, const mbs::GlobalFxSettings& fx)
{
    root.setAttribute("sat_drive",         static_cast<double>(fx.satDrive));
    root.setAttribute("sat_mix",           static_cast<double>(fx.satMix));
    root.setAttribute("transient_attack",  static_cast<double>(fx.transientAttack));
    root.setAttribute("transient_sustain", static_cast<double>(fx.transientSustain));
    root.setAttribute("transient_mix",     static_cast<double>(fx.transientMix));
    root.setAttribute("comp_threshold",    static_cast<double>(fx.compThreshold));
    root.setAttribute("comp_ratio",        static_cast<double>(fx.compRatio));
    root.setAttribute("comp_attack",       static_cast<double>(fx.compAttack));
    root.setAttribute("comp_release",      static_cast<double>(fx.compRelease));
    root.setAttribute("comp_makeup",       static_cast<double>(fx.compMakeup));
    root.setAttribute("comp_mix",          static_cast<double>(fx.compMix));
    root.setAttribute("eq_low_freq",       static_cast<double>(fx.eqLowFreq));
    root.setAttribute("eq_low_gain",       static_cast<double>(fx.eqLowGain));
    root.setAttribute("eq_mid_freq",       static_cast<double>(fx.eqMidFreq));
    root.setAttribute("eq_mid_gain",       static_cast<double>(fx.eqMidGain));
    root.setAttribute("eq_mid_q",          static_cast<double>(fx.eqMidQ));
    root.setAttribute("eq_high_freq",      static_cast<double>(fx.eqHighFreq));
    root.setAttribute("eq_high_gain",      static_cast<double>(fx.eqHighGain));
    root.setAttribute("chorus_rate",       static_cast<double>(fx.chorusRate));
    root.setAttribute("chorus_depth",      static_cast<double>(fx.chorusDepth));
    root.setAttribute("chorus_mix",        static_cast<double>(fx.chorusMix));
    root.setAttribute("delay_time",        static_cast<double>(fx.delayTime));
    root.setAttribute("delay_feedback",    static_cast<double>(fx.delayFeedback));
    root.setAttribute("delay_mix",         static_cast<double>(fx.delayMix));
    root.setAttribute("delay_sync",        fx.delaySync ? 1 : 0);
    root.setAttribute("delay_division",    fx.delayNoteDiv);
    root.setAttribute("reverb_size",       static_cast<double>(fx.reverbSize));
    root.setAttribute("reverb_damping",    static_cast<double>(fx.reverbDamping));
    root.setAttribute("reverb_width",      static_cast<double>(fx.reverbWidth));
    root.setAttribute("reverb_mix",        static_cast<double>(fx.reverbMix));
    root.setAttribute("limiter_threshold", static_cast<double>(fx.limiterThreshold));
    root.setAttribute("limiter_release",   static_cast<double>(fx.limiterRelease));
    root.setAttribute("fx_tab0_en",        fx.saturatorOn ? 1 : 0);
    root.setAttribute("fx_tab1_en",        fx.transientOn ? 1 : 0);
    root.setAttribute("fx_tab2_en",        fx.compressorOn ? 1 : 0);
    root.setAttribute("fx_tab3_en",        fx.eqOn ? 1 : 0);
    root.setAttribute("fx_tab4_en",        fx.chorusOn ? 1 : 0);
    root.setAttribute("fx_tab5_en",        fx.delayOn ? 1 : 0);
    root.setAttribute("fx_tab6_en",        fx.reverbOn ? 1 : 0);
    root.setAttribute("fx_tab7_en",        fx.limiterOn ? 1 : 0);
}

void readGlobalFxAttributes(const juce::XmlElement& xml, mbs::GlobalFxSettings& fx)
{
    int warningCount = 0;
    fx.satDrive         = readValidatedXmlFloat(xml, "sat_drive",         fx.satDrive,         1.0f,   16.0f, warningCount);
    fx.satMix           = readValidatedXmlFloat(xml, "sat_mix",           fx.satMix,           0.0f,    1.0f, warningCount);
    fx.transientAttack  = readValidatedXmlFloat(xml, "transient_attack",  fx.transientAttack, -1.0f,    1.0f, warningCount);
    fx.transientSustain = readValidatedXmlFloat(xml, "transient_sustain", fx.transientSustain,-1.0f,    1.0f, warningCount);
    fx.transientMix     = readValidatedXmlFloat(xml, "transient_mix",     fx.transientMix,     0.0f,    1.0f, warningCount);
    fx.compThreshold    = readValidatedXmlFloat(xml, "comp_threshold",    fx.compThreshold,  -60.0f,    0.0f, warningCount);
    fx.compRatio        = readValidatedXmlFloat(xml, "comp_ratio",        fx.compRatio,        1.0f,   20.0f, warningCount);
    fx.compAttack       = readValidatedXmlFloat(xml, "comp_attack",       fx.compAttack,       0.1f,  100.0f, warningCount);
    fx.compRelease      = readValidatedXmlFloat(xml, "comp_release",      fx.compRelease,      5.0f,  500.0f, warningCount);
    fx.compMakeup       = readValidatedXmlFloat(xml, "comp_makeup",       fx.compMakeup,       0.0f,   24.0f, warningCount);
    fx.compMix          = readValidatedXmlFloat(xml, "comp_mix",          fx.compMix,          0.0f,    1.0f, warningCount);
    fx.eqLowFreq        = readValidatedXmlFloat(xml, "eq_low_freq",       fx.eqLowFreq,       40.0f,  800.0f, warningCount);
    fx.eqLowGain        = readValidatedXmlFloat(xml, "eq_low_gain",       fx.eqLowGain,      -12.0f,   12.0f, warningCount);
    fx.eqMidFreq        = readValidatedXmlFloat(xml, "eq_mid_freq",       fx.eqMidFreq,      200.0f, 8000.0f, warningCount);
    fx.eqMidGain        = readValidatedXmlFloat(xml, "eq_mid_gain",       fx.eqMidGain,      -12.0f,   12.0f, warningCount);
    fx.eqMidQ           = readValidatedXmlFloat(xml, "eq_mid_q",          fx.eqMidQ,           0.1f,   10.0f, warningCount);
    fx.eqHighFreq       = readValidatedXmlFloat(xml, "eq_high_freq",      fx.eqHighFreq,    1000.0f,16000.0f, warningCount);
    fx.eqHighGain       = readValidatedXmlFloat(xml, "eq_high_gain",      fx.eqHighGain,     -12.0f,   12.0f, warningCount);
    fx.chorusRate       = readValidatedXmlFloat(xml, "chorus_rate",       fx.chorusRate,       0.1f,    8.0f, warningCount);
    fx.chorusDepth      = readValidatedXmlFloat(xml, "chorus_depth",      fx.chorusDepth,      0.0f,    1.0f, warningCount);
    fx.chorusMix        = readValidatedXmlFloat(xml, "chorus_mix",        fx.chorusMix,        0.0f,    1.0f, warningCount);
    fx.delayTime        = readValidatedXmlFloat(xml, "delay_time",        fx.delayTime,       10.0f, 1500.0f, warningCount);
    fx.delayFeedback    = readValidatedXmlFloat(xml, "delay_feedback",    fx.delayFeedback,    0.0f,    0.95f, warningCount);
    fx.delayMix         = readValidatedXmlFloat(xml, "delay_mix",         fx.delayMix,         0.0f,    1.0f, warningCount);
    fx.delaySync        = xml.getIntAttribute("delay_sync", fx.delaySync ? 1 : 0) != 0;
    fx.delayNoteDiv     = juce::jlimit(0, 4, xml.getIntAttribute("delay_division", fx.delayNoteDiv));
    fx.reverbSize       = readValidatedXmlFloat(xml, "reverb_size",       fx.reverbSize,       0.0f,    1.0f, warningCount);
    fx.reverbDamping    = readValidatedXmlFloat(xml, "reverb_damping",    fx.reverbDamping,    0.0f,    1.0f, warningCount);
    fx.reverbWidth      = readValidatedXmlFloat(xml, "reverb_width",      fx.reverbWidth,      0.0f,    1.0f, warningCount);
    fx.reverbMix        = readValidatedXmlFloat(xml, "reverb_mix",        fx.reverbMix,        0.0f,    1.0f, warningCount);
    fx.limiterThreshold = readValidatedXmlFloat(xml, "limiter_threshold", fx.limiterThreshold,-12.0f,    0.0f, warningCount);
    fx.limiterRelease   = readValidatedXmlFloat(xml, "limiter_release",   fx.limiterRelease,   1.0f,  200.0f, warningCount);
    const auto satFallback = xml.getIntAttribute("fx_sat_en", fx.saturatorOn ? 1 : 0);
    const auto transientFallback = xml.getIntAttribute("fx_transient_en", fx.transientOn ? 1 : 0);
    const auto compFallback = xml.getIntAttribute("fx_comp_en", fx.compressorOn ? 1 : 0);
    fx.saturatorOn      = xml.getIntAttribute("fx_tab0_en", satFallback) != 0;
    fx.transientOn      = xml.getIntAttribute("fx_tab1_en", transientFallback) != 0;
    fx.compressorOn     = xml.getIntAttribute("fx_tab2_en", compFallback) != 0;
    fx.eqOn             = xml.getIntAttribute("fx_tab3_en", fx.eqOn ? 1 : 0) != 0;
    fx.chorusOn         = xml.getIntAttribute("fx_tab4_en", fx.chorusOn ? 1 : 0) != 0;
    fx.delayOn          = xml.getIntAttribute("fx_tab5_en", fx.delayOn ? 1 : 0) != 0;
    fx.reverbOn         = xml.getIntAttribute("fx_tab6_en", fx.reverbOn ? 1 : 0) != 0;
    fx.limiterOn        = xml.getIntAttribute("fx_tab7_en", fx.limiterOn ? 1 : 0) != 0;

    if (warningCount > 0)
        juce::Logger::writeToLog("[BassPreset] FX sanitization warnings=" + juce::String(warningCount));
}
} // namespace

// =============================================================================
auto BassSynthAudioProcessor::createBusLayout() -> BusesProperties
{
    BusesProperties buses;
    buses = buses.withOutput("Master", juce::AudioChannelSet::stereo(), true);
    for (int i = 0; i < kNumAuxOutputs; ++i)
        buses = buses.withOutput("Bass " + juce::String(i + 1) + " Out",
                                 juce::AudioChannelSet::stereo(), false);
    return buses;
}

// =============================================================================
BassSynthAudioProcessor::BassSynthAudioProcessor()
    : AudioProcessor(createBusLayout()),
      parameters(*this, nullptr, juce::Identifier("MBS_PARAMS"), createParameterLayout()),
      factoryPresetBanks(mbs::getFactoryPresetBanks())
{
    currentPresetIndices.fill(0);
    outputBusCache.fill(0);
    resolveParameterPointers();
    loadFactoryOverrides();

    // Apply preset 0 for each bass so defaults are populated
    for (int b = 0; b < mbs::kNumBasses; ++b)
    {
        if (!factoryPresetBanks[static_cast<std::size_t>(b)].empty())
        {
            const auto& preset = factoryPresetBanks[static_cast<std::size_t>(b)][0];
            applyBassSettingsToParams(b, preset.settings, false);
            setParamValueInternal(makeBassParamId(b, kBassOutputSuffix), static_cast<float>(preset.outputBus), false);
            outputBusCache[static_cast<std::size_t>(b)] = preset.outputBus;
        }
    }

    sanitizeAllParameters();
}

// =============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
BassSynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& banks     = mbs::getFactoryPresetBanks();
    const auto outputChoices = makeOutputChoices();

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kOutputGain, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.01f), -3.0f));

    juce::StringArray bassChoices;
    for (int i = 0; i < mbs::kNumBasses; ++i)
        bassChoices.add(mbs::getBassName(i));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kSelectedBass, "Selected Bass", bassChoices, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLfoRate, "LFO Rate",
        juce::NormalisableRange<float>(0.05f, 12.0f, 0.0001f), 1.2f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLfoDepth, "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kLfoWave, "LFO Wave",
        juce::StringArray{ "Sine", "Triangle", "Saw", "Square" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kLfoDest, "LFO Dest",
        juce::StringArray{ "Trem/Pan", "Cutoff", "Both" }, 0));

    // Mono mode & Glide
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kMonoMode, "Mono Mode",
        juce::StringArray{ "Poly", "Mono", "Legato" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kGlideTime, "Glide Time",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.0f, 0.32f), 0.0f));

    // Macros
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroFatness, "Macro Grosseur",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroBrillance, "Macro Brillance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroPunch, "Macro Punch",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroDepth, "Macro Profondeur",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.3f));

    // Mod Wheel Target
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kModWheelTarget, "Mod Wheel Target",
        juce::StringArray{ "Off", "Punch" }, 1));

    // FX: Compressor
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompThreshold, "Comp Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.01f), -19.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompRatio, "Comp Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f), 3.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompAttack, "Comp Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.01f), 10.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompRelease, "Comp Release",
        juce::NormalisableRange<float>(5.0f, 500.0f, 0.01f), 120.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompMakeup, "Comp Makeup",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompMix, "Comp Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 1.0f));

    // FX: Saturator
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kSatDrive, "Sat Drive",
        juce::NormalisableRange<float>(1.0f, 16.0f, 0.01f), 1.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kSatMix, "Sat Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.15f));

    // FX: Transient
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kTransientAttack, "Transient Attack",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0001f), 0.10f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kTransientSustain, "Transient Sustain",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kTransientMix, "Transient Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.4f));

    // FX: EQ
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqLowFreq, "EQ Low Freq",
        juce::NormalisableRange<float>(40.0f, 800.0f, 0.01f, 0.4f), 80.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqLowGain, "EQ Low Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqMidFreq, "EQ Mid Freq",
        juce::NormalisableRange<float>(200.0f, 8000.0f, 0.01f, 0.4f), 600.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqMidGain, "EQ Mid Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqMidQ, "EQ Mid Q",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqHighFreq, "EQ High Freq",
        juce::NormalisableRange<float>(1000.0f, 16000.0f, 0.01f, 0.4f), 3500.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqHighGain, "EQ High Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));

    // FX: Chorus
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kChorusRate, "Chorus Rate",
        juce::NormalisableRange<float>(0.1f, 8.0f, 0.01f), 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kChorusDepth, "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.4f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kChorusMix, "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));

    // FX: Delay
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kDelayTime, "Delay Time",
        juce::NormalisableRange<float>(10.0f, 1500.0f, 0.01f, 0.4f), 300.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kDelayFeedback, "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.0001f), 0.25f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kDelayMix, "Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        kDelaySync, "Delay Sync", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kDelayNoteDiv, "Delay Note Div",
        juce::StringArray{ "1/4", "1/8", "1/16", "1/8 Dot", "1/8 Trip" }, 0));

    // FX: Reverb
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbSize, "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.40f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbDamping, "Reverb Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.55f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbWidth, "Reverb Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.60f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbMix, "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));

    // FX: Limiter
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLimiterThreshold, "Limiter Threshold",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.01f), -0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLimiterRelease, "Limiter Release",
        juce::NormalisableRange<float>(1.0f, 200.0f, 0.01f), 50.0f));

    // FX enable toggles (true = active)
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab0_en", "Saturator Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab1_en", "Transient Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab2_en", "Compressor Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab3_en", "EQ Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab4_en", "Chorus Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab5_en", "Delay Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab6_en", "Reverb Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab7_en", "Limiter Enable", true));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        kVelocityCurve, "Velocity Curve", 0, 6, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kPitchBendRange, "Pitch Bend Range",
        juce::NormalisableRange<float>(1.0f, 24.0f, 1.0f), 2.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kModLfo2Rate, "Mod LFO2 Rate",
        juce::NormalisableRange<float>(0.05f, 12.0f, 0.0001f), 2.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kModLfo2Wave, "Mod LFO2 Wave",
        juce::StringArray{ "Sine", "Triangle", "Saw", "Square" }, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(kFxLock, "FX Lock", false));

    const auto modSourceChoices = juce::StringArray{
        modmatrix::getSourceName(modmatrix::Source::None),
        modmatrix::getSourceName(modmatrix::Source::LFO1),
        modmatrix::getSourceName(modmatrix::Source::LFO2),
        modmatrix::getSourceName(modmatrix::Source::Envelope),
        modmatrix::getSourceName(modmatrix::Source::Velocity),
        modmatrix::getSourceName(modmatrix::Source::ModWheel),
        modmatrix::getSourceName(modmatrix::Source::Aftertouch),
        modmatrix::getSourceName(modmatrix::Source::PitchBend)
    };
    const auto modDestinationChoices = juce::StringArray{
        modmatrix::getDestinationName(modmatrix::Destination::None),
        modmatrix::getDestinationName(modmatrix::Destination::Cutoff),
        modmatrix::getDestinationName(modmatrix::Destination::Resonance),
        modmatrix::getDestinationName(modmatrix::Destination::Pan),
        modmatrix::getDestinationName(modmatrix::Destination::Level),
        modmatrix::getDestinationName(modmatrix::Destination::Pitch),
        modmatrix::getDestinationName(modmatrix::Destination::AttackTime),
        modmatrix::getDestinationName(modmatrix::Destination::DecayTime),
        modmatrix::getDestinationName(modmatrix::Destination::LFO1Rate),
        modmatrix::getDestinationName(modmatrix::Destination::EqMidFreq),
        modmatrix::getDestinationName(modmatrix::Destination::EqMidGain)
    };
    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            makeModMatrixParamId(slotIndex, "source"),
            "Mod " + juce::String(slotIndex + 1) + " Source",
            modSourceChoices, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            makeModMatrixParamId(slotIndex, "dest"),
            "Mod " + juce::String(slotIndex + 1) + " Destination",
            modDestinationChoices, 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeModMatrixParamId(slotIndex, "amount"),
            "Mod " + juce::String(slotIndex + 1) + " Amount",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0001f), 0.0f));
    }

    // Per-bass parameters (9 basses x 15 + output)
    for (int b = 0; b < mbs::kNumBasses; ++b)
    {
        const auto& def = banks[static_cast<std::size_t>(b)].empty()
                          ? mbs::getDefaultSettings(b)
                          : banks[static_cast<std::size_t>(b)][0].settings;
        const auto prefix = juce::String(mbs::getBassName(b)) + " ";

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "level"), prefix + "Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.level));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "tune"), prefix + "Tune",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), def.tuneSemitones));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "brightness"), prefix + "Brightness",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.brightness));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "attack"), prefix + "Attack",
            juce::NormalisableRange<float>(0.0f, 2.0f, 0.0001f), def.attackSeconds));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "decay"), prefix + "Decay",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.001f), def.decaySeconds));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "sustain"), prefix + "Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.sustainLevel));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "release"), prefix + "Release",
            juce::NormalisableRange<float>(0.01f, 5.0f, 0.0001f), def.releaseSeconds));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "body"), prefix + "Body",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.body));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "drive"), prefix + "Drive",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.drive));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "pitch_env"), prefix + "Pitch Env",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.pitchEnv));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "filter_env"), prefix + "Filter Env",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.filterEnv));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "sub"), prefix + "Sub",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.subLevel));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "character"), prefix + "Character",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.character));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "cutoff"), prefix + "Cutoff",
            juce::NormalisableRange<float>(120.0f, 12000.0f, 0.0f, 0.28f), def.cutoffHz));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "pan"), prefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), def.pan));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeBassParamId(b, "resonance"), prefix + "Resonance",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.resonance));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            makeBassParamId(b, kBassOutputSuffix), prefix + "Output",
            outputChoices, 0));
    }

    return layout;
}

juce::String BassSynthAudioProcessor::makeBassParamId(int bassIndex, const juce::String& suffix)
{
    return "bass_" + juce::String(bassIndex) + "_" + suffix;
}

juce::String BassSynthAudioProcessor::makeModMatrixParamId(int slotIndex, const juce::String& suffix)
{
    return "mod_" + juce::String(slotIndex) + "_" + suffix;
}

float BassSynthAudioProcessor::readCachedParamValue(const ParamBinding& binding, float fallback) const noexcept
{
    if (binding.raw == nullptr)
        return fallback;

    const auto value = binding.raw->load();
    return std::isfinite(value) ? value : fallback;
}

void BassSynthAudioProcessor::resolveParameterPointers()
{
    globalParamRefs.selectedBass    = bindParam(parameters, kSelectedBass);
    globalParamRefs.monoMode        = bindParam(parameters, kMonoMode);
    globalParamRefs.glideTime       = bindParam(parameters, kGlideTime);
    globalParamRefs.lfoRate         = bindParam(parameters, kLfoRate);
    globalParamRefs.lfoDepth        = bindParam(parameters, kLfoDepth);
    globalParamRefs.lfoWave         = bindParam(parameters, kLfoWave);
    globalParamRefs.lfoDest         = bindParam(parameters, kLfoDest);
    globalParamRefs.outputGain      = bindParam(parameters, kOutputGain);
    globalParamRefs.macroFatness    = bindParam(parameters, kMacroFatness);
    globalParamRefs.macroBrillance  = bindParam(parameters, kMacroBrillance);
    globalParamRefs.macroPunch      = bindParam(parameters, kMacroPunch);
    globalParamRefs.macroDepth      = bindParam(parameters, kMacroDepth);
    globalParamRefs.modWheelTarget  = bindParam(parameters, kModWheelTarget);
    globalParamRefs.satDrive        = bindParam(parameters, kSatDrive);
    globalParamRefs.satMix          = bindParam(parameters, kSatMix);
    globalParamRefs.transientAttack = bindParam(parameters, kTransientAttack);
    globalParamRefs.transientSustain = bindParam(parameters, kTransientSustain);
    globalParamRefs.transientMix    = bindParam(parameters, kTransientMix);
    globalParamRefs.compThreshold   = bindParam(parameters, kCompThreshold);
    globalParamRefs.compRatio       = bindParam(parameters, kCompRatio);
    globalParamRefs.compAttack      = bindParam(parameters, kCompAttack);
    globalParamRefs.compRelease     = bindParam(parameters, kCompRelease);
    globalParamRefs.compMakeup      = bindParam(parameters, kCompMakeup);
    globalParamRefs.compMix         = bindParam(parameters, kCompMix);
    globalParamRefs.eqLowFreq       = bindParam(parameters, kEqLowFreq);
    globalParamRefs.eqLowGain       = bindParam(parameters, kEqLowGain);
    globalParamRefs.eqMidFreq       = bindParam(parameters, kEqMidFreq);
    globalParamRefs.eqMidGain       = bindParam(parameters, kEqMidGain);
    globalParamRefs.eqMidQ          = bindParam(parameters, kEqMidQ);
    globalParamRefs.eqHighFreq      = bindParam(parameters, kEqHighFreq);
    globalParamRefs.eqHighGain      = bindParam(parameters, kEqHighGain);
    globalParamRefs.chorusRate      = bindParam(parameters, kChorusRate);
    globalParamRefs.chorusDepth     = bindParam(parameters, kChorusDepth);
    globalParamRefs.chorusMix       = bindParam(parameters, kChorusMix);
    globalParamRefs.delayTime       = bindParam(parameters, kDelayTime);
    globalParamRefs.delayFeedback   = bindParam(parameters, kDelayFeedback);
    globalParamRefs.delayMix        = bindParam(parameters, kDelayMix);
    globalParamRefs.delaySync       = bindParam(parameters, kDelaySync);
    globalParamRefs.delayNoteDiv    = bindParam(parameters, kDelayNoteDiv);
    globalParamRefs.reverbSize      = bindParam(parameters, kReverbSize);
    globalParamRefs.reverbDamping   = bindParam(parameters, kReverbDamping);
    globalParamRefs.reverbWidth     = bindParam(parameters, kReverbWidth);
    globalParamRefs.reverbMix       = bindParam(parameters, kReverbMix);
    globalParamRefs.limiterThreshold = bindParam(parameters, kLimiterThreshold);
    globalParamRefs.limiterRelease  = bindParam(parameters, kLimiterRelease);
    globalParamRefs.fxSatEnable     = bindParam(parameters, "fx_tab0_en");
    globalParamRefs.fxTransientEnable = bindParam(parameters, "fx_tab1_en");
    globalParamRefs.fxCompEnable    = bindParam(parameters, "fx_tab2_en");
    globalParamRefs.fxEqEnable      = bindParam(parameters, "fx_tab3_en");
    globalParamRefs.fxChorusEnable  = bindParam(parameters, "fx_tab4_en");
    globalParamRefs.fxDelayEnable   = bindParam(parameters, "fx_tab5_en");
    globalParamRefs.fxReverbEnable  = bindParam(parameters, "fx_tab6_en");
    globalParamRefs.fxLimiterEnable    = bindParam(parameters, "fx_tab7_en");
    globalParamRefs.velocityCurveParam = bindParam(parameters, kVelocityCurve);
    globalParamRefs.pitchBendRange     = bindParam(parameters, kPitchBendRange);
    globalParamRefs.modLfo2Rate        = bindParam(parameters, kModLfo2Rate);
    globalParamRefs.modLfo2Wave        = bindParam(parameters, kModLfo2Wave);
    globalParamRefs.fxLock             = bindParam(parameters, kFxLock);

    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        auto& refs = modMatrixParamRefs[static_cast<std::size_t>(slotIndex)];
        refs.source = bindParam(parameters, makeModMatrixParamId(slotIndex, "source"));
        refs.destination = bindParam(parameters, makeModMatrixParamId(slotIndex, "dest"));
        refs.amount = bindParam(parameters, makeModMatrixParamId(slotIndex, "amount"));
    }

    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        auto& refs = bassParamRefs[static_cast<std::size_t>(bassIndex)];
        refs.level      = bindParam(parameters, makeBassParamId(bassIndex, "level"));
        refs.tune       = bindParam(parameters, makeBassParamId(bassIndex, "tune"));
        refs.brightness = bindParam(parameters, makeBassParamId(bassIndex, "brightness"));
        refs.attack     = bindParam(parameters, makeBassParamId(bassIndex, "attack"));
        refs.decay      = bindParam(parameters, makeBassParamId(bassIndex, "decay"));
        refs.sustain    = bindParam(parameters, makeBassParamId(bassIndex, "sustain"));
        refs.release    = bindParam(parameters, makeBassParamId(bassIndex, "release"));
        refs.body       = bindParam(parameters, makeBassParamId(bassIndex, "body"));
        refs.drive      = bindParam(parameters, makeBassParamId(bassIndex, "drive"));
        refs.pitchEnv   = bindParam(parameters, makeBassParamId(bassIndex, "pitch_env"));
        refs.filterEnv  = bindParam(parameters, makeBassParamId(bassIndex, "filter_env"));
        refs.sub        = bindParam(parameters, makeBassParamId(bassIndex, "sub"));
        refs.character  = bindParam(parameters, makeBassParamId(bassIndex, "character"));
        refs.cutoff     = bindParam(parameters, makeBassParamId(bassIndex, "cutoff"));
        refs.pan        = bindParam(parameters, makeBassParamId(bassIndex, "pan"));
        refs.resonance  = bindParam(parameters, makeBassParamId(bassIndex, "resonance"));
        refs.output     = bindParam(parameters, makeBassParamId(bassIndex, kBassOutputSuffix));
    }
}

// =============================================================================
void BassSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = std::max(1.0, sampleRate);

    for (auto& slot : voices)
    {
        slot.midiNote = -1;
        for (auto& auxSend : slot.auxSendGains)
        {
            auxSend.reset(preparedSampleRate, 0.080);
            auxSend.setCurrentAndTargetValue(0.0f);
        }
    }

    const juce::dsp::ProcessSpec spec {
        preparedSampleRate,
        static_cast<juce::uint32>(juce::jmax(1, samplesPerBlock)),
        static_cast<juce::uint32>(juce::jmax(1, getMainBusNumOutputChannels()))
    };

    compressor.reset();
    compressor.prepare(spec);
    compressor.setThreshold(-19.0f);
    compressor.setRatio(3.0f);
    compressor.setAttack(10.0f);
    compressor.setRelease(120.0f);

    compCache = CompressorCache{};
    fxDryBuffer.setSize(static_cast<int>(spec.numChannels),
                        static_cast<int>(spec.maximumBlockSize), false, true, true);
    mainDryBuffer.setSize(static_cast<int>(spec.numChannels),
                          static_cast<int>(spec.maximumBlockSize), false, true, true);
    voiceRenderBuffer.setSize(static_cast<int>(spec.numChannels),
                              static_cast<int>(spec.maximumBlockSize), false, true, true);
    fxChunkBuffer.setSize(static_cast<int>(spec.numChannels),
                          static_cast<int>(spec.maximumBlockSize), false, true, true);
    transientFastEnv = { 0.0f, 0.0f };
    transientSlowEnv = { 0.0f, 0.0f };
    lfoPhase = 0.0f;

    // Initialize new FX processors
    eqProcessor.prepare(preparedSampleRate);
    chorusProcessor.prepare(preparedSampleRate, samplesPerBlock);
    delayProcessor.prepare(preparedSampleRate, samplesPerBlock);
    reverbProcessor.prepare(preparedSampleRate, samplesPerBlock);
    limiterProcessor.prepare(preparedSampleRate);

    // Reset HPF state
    for (int ch = 0; ch < 2; ++ch)
    {
        hpfX1[ch] = 0.0f; hpfX2[ch] = 0.0f;
        hpfY1[ch] = 0.0f; hpfY2[ch] = 0.0f;
    }

    satOversampling.initProcessing(static_cast<size_t>(samplesPerBlock));
    initializeGlobalFxSmoothers();
}

void BassSynthAudioProcessor::releaseResources()
{
    for (auto& slot : voices)
    {
        slot.midiNote = -1;
        for (auto& auxSend : slot.auxSendGains)
            auxSend.setCurrentAndTargetValue(0.0f);
    }
    for (auto& dv : dyingVoices)
        dv.inUse = false;
    fxDryBuffer.setSize(0, 0);
    mainDryBuffer.setSize(0, 0);
    voiceRenderBuffer.setSize(0, 0);
    fxChunkBuffer.setSize(0, 0);
}

bool BassSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.outputBuses.isEmpty())
        return false;

    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() &&
        mainOutput != juce::AudioChannelSet::stereo())
        return false;

    for (int busIndex = 1; busIndex < layouts.outputBuses.size(); ++busIndex)
    {
        const auto auxSet = layouts.getChannelSet(false, busIndex);
        if (auxSet.isDisabled()) continue;
        if (auxSet != juce::AudioChannelSet::mono() &&
            auxSet != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

BassSynthAudioProcessor::GlobalBlockState BassSynthAudioProcessor::buildGlobalBlockState(int numSamples)
{
    GlobalBlockState state;
    state.selectedBass = getSelectedBassIndex();
    state.monoMode = juce::jlimit(0, 2, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.monoMode))));
    state.glideTime = readCachedParamValue(globalParamRefs.glideTime);
    state.outputGainDb = readCachedParamValue(globalParamRefs.outputGain);
    state.lfoRate = readCachedParamValue(globalParamRefs.lfoRate, 2.0f);
    state.lfoDepth = readCachedParamValue(globalParamRefs.lfoDepth);
    state.lfoWave = juce::jlimit(0, 3, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.lfoWave))));
    state.lfoDest = juce::jlimit(0, 2, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.lfoDest))));
    state.fx = sanitizeFxSettings(snapshotGlobalFxSettings());
    syncModMatrixFromParams();

    switch (state.lfoWave)
    {
        case 1: state.baseModContext.lfo1 = 1.0f - 4.0f * std::abs(lfoPhase - 0.5f); break;
        case 2: state.baseModContext.lfo1 = lfoPhase * 2.0f - 1.0f; break;
        case 3: state.baseModContext.lfo1 = lfoPhase < 0.5f ? 1.0f : -1.0f; break;
        default: state.baseModContext.lfo1 = std::sin(lfoPhase * juce::MathConstants<float>::twoPi); break;
    }

    state.baseModContext.lfo2 = modulationMatrix.lfo2.tickBlock(
        static_cast<float>(preparedSampleRate), juce::jmax(1, numSamples));
    state.baseModContext.modWheel = modulationMatrix.modWheelValue;
    state.baseModContext.aftertouch = modulationMatrix.aftertouchValue;
    state.baseModContext.pitchBend = modulationMatrix.pitchBendValue;
    state.sharedModResult = modulationMatrix.process(state.baseModContext);
    state.lfoRate = juce::jlimit(0.05f, 12.0f, state.lfoRate * state.sharedModResult.lfo1RateMul);
    state.lfoDepth = clamp01(state.lfoDepth);
    return state;
}

void BassSynthAudioProcessor::initializeGlobalFxSmoothers()
{
    const auto fx = sanitizeFxSettings(snapshotGlobalFxSettings());
    const auto resetSmoother = [this] (LinearSmoother& smoother, const double rampSeconds, const float initialValue)
    {
        smoother.reset(preparedSampleRate, rampSeconds);
        smoother.setCurrentAndTargetValue(initialValue);
    };
    const auto enabledMix = [] (const bool enabled, const float mix)
    {
        return enabled ? mix : 0.0f;
    };

    resetSmoother(globalFxSmoothers.satDrive, kFxTransientRampSeconds, fx.satDrive);
    resetSmoother(globalFxSmoothers.satMix, kFxMixRampSeconds, enabledMix(fx.saturatorOn, clamp01(fx.satMix)));
    resetSmoother(globalFxSmoothers.transientAttack, kFxTransientRampSeconds, fx.transientAttack);
    resetSmoother(globalFxSmoothers.transientSustain, kFxTransientRampSeconds, fx.transientSustain);
    resetSmoother(globalFxSmoothers.transientMix, kFxMixRampSeconds, enabledMix(fx.transientOn, clamp01(fx.transientMix)));
    resetSmoother(globalFxSmoothers.compThreshold, kFxCompressorRampSeconds, fx.compThreshold);
    resetSmoother(globalFxSmoothers.compRatio, kFxCompressorRampSeconds, fx.compRatio);
    resetSmoother(globalFxSmoothers.compAttack, kFxCompressorRampSeconds, fx.compAttack);
    resetSmoother(globalFxSmoothers.compRelease, kFxCompressorRampSeconds, fx.compRelease);
    resetSmoother(globalFxSmoothers.compMakeup, kFxMixRampSeconds, fx.compressorOn ? fx.compMakeup : 0.0f);
    resetSmoother(globalFxSmoothers.compMix, kFxMixRampSeconds, enabledMix(fx.compressorOn, clamp01(fx.compMix)));
    resetSmoother(globalFxSmoothers.eqLowFreq, kFxEqRampSeconds, fx.eqLowFreq);
    resetSmoother(globalFxSmoothers.eqLowGain, kFxEqRampSeconds, fx.eqLowGain);
    resetSmoother(globalFxSmoothers.eqMidFreq, kFxEqRampSeconds, fx.eqMidFreq);
    resetSmoother(globalFxSmoothers.eqMidGain, kFxEqRampSeconds, fx.eqMidGain);
    resetSmoother(globalFxSmoothers.eqMidQ, kFxEqRampSeconds, fx.eqMidQ);
    resetSmoother(globalFxSmoothers.eqHighFreq, kFxEqRampSeconds, fx.eqHighFreq);
    resetSmoother(globalFxSmoothers.eqHighGain, kFxEqRampSeconds, fx.eqHighGain);
    resetSmoother(globalFxSmoothers.chorusRate, kFxChorusRampSeconds, fx.chorusRate);
    resetSmoother(globalFxSmoothers.chorusDepth, kFxChorusRampSeconds, fx.chorusDepth);
    resetSmoother(globalFxSmoothers.chorusMix, kFxMixRampSeconds, enabledMix(fx.chorusOn, clamp01(fx.chorusMix)));
    resetSmoother(globalFxSmoothers.delayTime, kFxDelayRampSeconds, fx.delayTime);
    resetSmoother(globalFxSmoothers.delayFeedback, kFxDelayRampSeconds, fx.delayFeedback);
    resetSmoother(globalFxSmoothers.delayMix, kFxMixRampSeconds, enabledMix(fx.delayOn, clamp01(fx.delayMix)));
    resetSmoother(globalFxSmoothers.reverbSize, kFxReverbRampSeconds, fx.reverbSize);
    resetSmoother(globalFxSmoothers.reverbDamping, kFxReverbRampSeconds, fx.reverbDamping);
    resetSmoother(globalFxSmoothers.reverbWidth, kFxReverbRampSeconds, fx.reverbWidth);
    resetSmoother(globalFxSmoothers.reverbMix, kFxMixRampSeconds, enabledMix(fx.reverbOn, clamp01(fx.reverbMix)));
    resetSmoother(globalFxSmoothers.limiterThreshold, kFxLimiterRampSeconds, fx.limiterThreshold);
    resetSmoother(globalFxSmoothers.limiterRelease, kFxLimiterRampSeconds, fx.limiterRelease);

    outputGainCurrent = juce::Decibels::decibelsToGain(readCachedParamValue(globalParamRefs.outputGain, -3.0f));
}

void BassSynthAudioProcessor::setGlobalFxSmootherTargets(const GlobalBlockState& state)
{
    globalFxSmoothers.satDrive.setTargetValue(state.fx.satDrive);
    globalFxSmoothers.satMix.setTargetValue(state.fx.saturatorOn ? clamp01(state.fx.satMix) : 0.0f);
    globalFxSmoothers.transientAttack.setTargetValue(state.fx.transientAttack);
    globalFxSmoothers.transientSustain.setTargetValue(state.fx.transientSustain);
    globalFxSmoothers.transientMix.setTargetValue(state.fx.transientOn ? clamp01(state.fx.transientMix) : 0.0f);
    globalFxSmoothers.compThreshold.setTargetValue(state.fx.compThreshold);
    globalFxSmoothers.compRatio.setTargetValue(state.fx.compRatio);
    globalFxSmoothers.compAttack.setTargetValue(state.fx.compAttack);
    globalFxSmoothers.compRelease.setTargetValue(state.fx.compRelease);
    globalFxSmoothers.compMakeup.setTargetValue(state.fx.compressorOn ? state.fx.compMakeup : 0.0f);
    globalFxSmoothers.compMix.setTargetValue(state.fx.compressorOn ? clamp01(state.fx.compMix) : 0.0f);
    globalFxSmoothers.eqLowFreq.setTargetValue(state.fx.eqLowFreq);
    globalFxSmoothers.eqLowGain.setTargetValue(state.fx.eqLowGain);
    globalFxSmoothers.eqMidFreq.setTargetValue(state.fx.eqMidFreq);
    globalFxSmoothers.eqMidGain.setTargetValue(state.fx.eqMidGain);
    globalFxSmoothers.eqMidQ.setTargetValue(state.fx.eqMidQ);
    globalFxSmoothers.eqHighFreq.setTargetValue(state.fx.eqHighFreq);
    globalFxSmoothers.eqHighGain.setTargetValue(state.fx.eqHighGain);
    globalFxSmoothers.chorusRate.setTargetValue(state.fx.chorusRate);
    globalFxSmoothers.chorusDepth.setTargetValue(state.fx.chorusDepth);
    globalFxSmoothers.chorusMix.setTargetValue(state.fx.chorusOn ? clamp01(state.fx.chorusMix) : 0.0f);
    globalFxSmoothers.delayTime.setTargetValue(state.fx.delayTime);
    globalFxSmoothers.delayFeedback.setTargetValue(state.fx.delayFeedback);
    globalFxSmoothers.delayMix.setTargetValue(state.fx.delayOn ? clamp01(state.fx.delayMix) : 0.0f);
    globalFxSmoothers.reverbSize.setTargetValue(state.fx.reverbSize);
    globalFxSmoothers.reverbDamping.setTargetValue(state.fx.reverbDamping);
    globalFxSmoothers.reverbWidth.setTargetValue(state.fx.reverbWidth);
    globalFxSmoothers.reverbMix.setTargetValue(state.fx.reverbOn ? clamp01(state.fx.reverbMix) : 0.0f);
    globalFxSmoothers.limiterThreshold.setTargetValue(state.fx.limiterThreshold);
    globalFxSmoothers.limiterRelease.setTargetValue(state.fx.limiterRelease);
}

BassSynthAudioProcessor::GlobalBlockState BassSynthAudioProcessor::advanceSmoothedGlobalBlockState(
    const GlobalBlockState& state, const int numSamples)
{
    auto smoothedState = state;
    smoothedState.fx.satDrive = globalFxSmoothers.satDrive.skip(numSamples);
    smoothedState.fx.satMix = clamp01(globalFxSmoothers.satMix.skip(numSamples));
    smoothedState.fx.transientAttack = globalFxSmoothers.transientAttack.skip(numSamples);
    smoothedState.fx.transientSustain = globalFxSmoothers.transientSustain.skip(numSamples);
    smoothedState.fx.transientMix = clamp01(globalFxSmoothers.transientMix.skip(numSamples));
    smoothedState.fx.compThreshold = globalFxSmoothers.compThreshold.skip(numSamples);
    smoothedState.fx.compRatio = globalFxSmoothers.compRatio.skip(numSamples);
    smoothedState.fx.compAttack = globalFxSmoothers.compAttack.skip(numSamples);
    smoothedState.fx.compRelease = globalFxSmoothers.compRelease.skip(numSamples);
    smoothedState.fx.compMakeup = globalFxSmoothers.compMakeup.skip(numSamples);
    smoothedState.fx.compMix = clamp01(globalFxSmoothers.compMix.skip(numSamples));
    smoothedState.fx.eqLowFreq = globalFxSmoothers.eqLowFreq.skip(numSamples);
    smoothedState.fx.eqLowGain = globalFxSmoothers.eqLowGain.skip(numSamples);
    smoothedState.fx.eqMidFreq = globalFxSmoothers.eqMidFreq.skip(numSamples);
    smoothedState.fx.eqMidGain = globalFxSmoothers.eqMidGain.skip(numSamples);
    smoothedState.fx.eqMidQ = globalFxSmoothers.eqMidQ.skip(numSamples);
    smoothedState.fx.eqHighFreq = globalFxSmoothers.eqHighFreq.skip(numSamples);
    smoothedState.fx.eqHighGain = globalFxSmoothers.eqHighGain.skip(numSamples);
    smoothedState.fx.chorusRate = globalFxSmoothers.chorusRate.skip(numSamples);
    smoothedState.fx.chorusDepth = globalFxSmoothers.chorusDepth.skip(numSamples);
    smoothedState.fx.chorusMix = clamp01(globalFxSmoothers.chorusMix.skip(numSamples));
    smoothedState.fx.delayTime = globalFxSmoothers.delayTime.skip(numSamples);
    smoothedState.fx.delayFeedback = clamp01(globalFxSmoothers.delayFeedback.skip(numSamples));
    smoothedState.fx.delayMix = clamp01(globalFxSmoothers.delayMix.skip(numSamples));
    smoothedState.fx.reverbSize = clamp01(globalFxSmoothers.reverbSize.skip(numSamples));
    smoothedState.fx.reverbDamping = clamp01(globalFxSmoothers.reverbDamping.skip(numSamples));
    smoothedState.fx.reverbWidth = clamp01(globalFxSmoothers.reverbWidth.skip(numSamples));
    smoothedState.fx.reverbMix = clamp01(globalFxSmoothers.reverbMix.skip(numSamples));
    smoothedState.fx.limiterThreshold = globalFxSmoothers.limiterThreshold.skip(numSamples);
    smoothedState.fx.limiterRelease = juce::jmax(1.0f, globalFxSmoothers.limiterRelease.skip(numSamples));
    return smoothedState;
}

// =============================================================================
void BassSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto outputBusCount = getBusCount(false);
    for (int busIndex = 0; busIndex < outputBusCount; ++busIndex)
        getBusBuffer(buffer, false, busIndex).clear();

    bool resetKeyboardState = false;
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isAllNotesOff()
            || msg.isAllSoundOff()
            || (msg.isController() && (msg.getControllerNumber() == 120 || msg.getControllerNumber() == 123)))
        {
            resetKeyboardState = true;
            break;
        }
    }

    if (resetKeyboardState)
        keyboardState.reset();
    else
        keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    // H1: update velocity curve from parameter
    velocityCurve = intToVelocityCurve(
        juce::jlimit(0, 6, static_cast<int>(std::round(
            readCachedParamValue(globalParamRefs.velocityCurveParam)))));

    // H2: update pitch bend range from parameter
    pitchBend.bendSemitones = readCachedParamValue(globalParamRefs.pitchBendRange, 2.0f);
    pitchBend.updateFactor();

    const int bassIdx = getSelectedBassIndex();

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        modulationMatrix.handleMidiMessage(msg);
        if (msg.isNoteOn())
            triggerNoteOn(bassIdx, msg.getNoteNumber(),
                          applyVelocityCurve(msg.getFloatVelocity(), velocityCurve));
        else if (msg.isNoteOff())
            triggerNoteOff(bassIdx, msg.getNoteNumber());
        else if (msg.isAllNotesOff())
            releaseVoices(msg.getChannel(), false);
        else if (msg.isAllSoundOff())
            panicAllVoices();
        else if (msg.isController() && msg.getControllerNumber() == 120)
            panicAllVoices();
        else if (msg.isPitchWheel())
            pitchBend.setPitchWheel(msg.getPitchWheelValue());
        else if (msg.isController())
            handleMidiCC(msg.getControllerNumber(), msg.getControllerValue(), bassIdx);
    }

    midiMessages.clear();
    auto blockState = buildGlobalBlockState(buffer.getNumSamples());
    cachedModResult = blockState.sharedModResult;

    // H4: update active voice count for UI display
    {
        int envCount = 0;
        for (const auto& slot : voices)
            if (slot.voice.isActive()) ++envCount;
        activeVoiceCountAtomic.store(envCount, std::memory_order_relaxed);
    }

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    mainDryBuffer.setSize(juce::jmax(2, mainBuffer.getNumChannels()), mainBuffer.getNumSamples(), false, false, true);
    voiceRenderBuffer.setSize(juce::jmax(2, mainBuffer.getNumChannels()), mainBuffer.getNumSamples(), false, false, true);
    mainDryBuffer.clear();

    for (int instrumentIndex = 0; instrumentIndex < mbs::kNumBasses; ++instrumentIndex)
    {
        int routedBus = captureBassOutputBus(instrumentIndex);
        if (routedBus > 0 && (routedBus >= outputBusCount || getChannelCountOfBus(false, routedBus) <= 0))
            routedBus = 0;
        outputBusCache[static_cast<std::size_t>(instrumentIndex)] = routedBus;
    }

    auto mixScratchInto = [] (const juce::AudioBuffer<float>& source, juce::AudioBuffer<float>& destination)
    {
        const int channelCount = juce::jmin(source.getNumChannels(), destination.getNumChannels());
        const int sampleCount = juce::jmin(source.getNumSamples(), destination.getNumSamples());
        for (int ch = 0; ch < channelCount; ++ch)
            destination.addFrom(ch, 0, source, ch, 0, sampleCount);
    };

    auto mixScratchIntoWithRamp = [] (const juce::AudioBuffer<float>& source,
                                      juce::AudioBuffer<float>& destination,
                                      const float startGain,
                                      const float endGain)
    {
        const int channelCount = juce::jmin(source.getNumChannels(), destination.getNumChannels());
        const int sampleCount = juce::jmin(source.getNumSamples(), destination.getNumSamples());
        if (sampleCount <= 0)
            return;

        for (int ch = 0; ch < channelCount; ++ch)
            destination.addFromWithRamp(ch, 0, source.getReadPointer(ch), sampleCount, startGain, endGain);
    };

    for (auto& slot : voices)
    {
        if (slot.voice.isActive())
        {
            modmatrix::ModContext voiceContext = blockState.baseModContext;
            voiceContext.envelope = slot.voice.getEnvelopeLevel();
            voiceContext.velocity = slot.velocity;
            const auto voiceModResult = modulationMatrix.process(voiceContext);

            mbs::VoiceModulation voiceMod;
            voiceMod.cutoffMul = voiceModResult.cutoffMul;
            voiceMod.resonanceAdd = voiceModResult.resonance;
            voiceMod.panAdd = voiceModResult.pan;
            voiceMod.attackScale = voiceModResult.attackScale;
            voiceMod.decayScale = voiceModResult.decayScale;
            voiceMod.pitchSemi = voiceModResult.pitchSemi;
            voiceMod.levelMul = voiceModResult.levelMul;

            slot.voice.setPitchBendFactor(pitchBend.pitchBendFactor);
            slot.voice.setVoiceModulation(voiceMod, preparedSampleRate);

            slot.outputBus = outputBusCache[static_cast<std::size_t>(juce::jlimit(0, mbs::kNumBasses - 1, slot.bassIndex))];
            voiceRenderBuffer.clear();
            slot.voice.render(voiceRenderBuffer, 0, voiceRenderBuffer.getNumSamples());
            mixScratchInto(voiceRenderBuffer, mainDryBuffer);

            std::array<float, kNumAuxOutputs> targetAuxGains {};
            if (slot.outputBus > 0
                && slot.outputBus < outputBusCount
                && getChannelCountOfBus(false, slot.outputBus) > 0)
            {
                targetAuxGains[static_cast<std::size_t>(slot.outputBus - 1)] = 1.0f;
            }

            for (int auxIndex = 0; auxIndex < kNumAuxOutputs; ++auxIndex)
            {
                auto& auxSend = slot.auxSendGains[static_cast<std::size_t>(auxIndex)];
                auxSend.setTargetValue(targetAuxGains[static_cast<std::size_t>(auxIndex)]);
                const float startGain = auxSend.getCurrentValue();
                const float endGain = auxSend.skip(voiceRenderBuffer.getNumSamples());

                if (std::abs(startGain) <= 1.0e-5f && std::abs(endGain) <= 1.0e-5f)
                    continue;

                auto targetBuffer = getBusBuffer(buffer, false, auxIndex + 1);
                if (targetBuffer.getNumChannels() > 0 && targetBuffer.getNumSamples() > 0)
                    mixScratchIntoWithRamp(voiceRenderBuffer, targetBuffer, startGain, endGain);
            }
        }
    }

    // Render dying voices (stolen voice fade-outs)
    for (auto& dv : dyingVoices)
    {
        if (!dv.inUse)
            continue;
        if (!dv.voice.isActive())
        {
            dv.inUse = false;
            continue;
        }
        modmatrix::ModContext voiceContext = blockState.baseModContext;
        voiceContext.envelope = dv.voice.getEnvelopeLevel();
        voiceContext.velocity = dv.velocity;
        const auto voiceModResult = modulationMatrix.process(voiceContext);

        mbs::VoiceModulation voiceMod;
        voiceMod.cutoffMul = voiceModResult.cutoffMul;
        voiceMod.resonanceAdd = voiceModResult.resonance;
        voiceMod.panAdd = voiceModResult.pan;
        voiceMod.attackScale = voiceModResult.attackScale;
        voiceMod.decayScale = voiceModResult.decayScale;
        voiceMod.pitchSemi = voiceModResult.pitchSemi;
        voiceMod.levelMul = voiceModResult.levelMul;

        dv.voice.setPitchBendFactor(pitchBend.pitchBendFactor);
        dv.voice.setVoiceModulation(voiceMod, preparedSampleRate);

        const int targetBus = (dv.outputBus > 0 && dv.outputBus < outputBusCount
                               && getChannelCountOfBus(false, dv.outputBus) > 0) ? dv.outputBus : 0;
        voiceRenderBuffer.clear();
        dv.voice.render(voiceRenderBuffer, 0, voiceRenderBuffer.getNumSamples());
        mixScratchInto(voiceRenderBuffer, mainDryBuffer);
        if (targetBus > 0)
        {
            auto targetBuffer = getBusBuffer(buffer, false, targetBus);
            if (targetBuffer.getNumChannels() > 0 && targetBuffer.getNumSamples() > 0)
                mixScratchInto(voiceRenderBuffer, targetBuffer);
        }
        if (!dv.voice.isActive())
            dv.inUse = false;
    }

    if (mainBuffer.getNumChannels() > 0 && mainBuffer.getNumSamples() > 0)
    {
        for (int ch = 0; ch < juce::jmin(mainBuffer.getNumChannels(), mainDryBuffer.getNumChannels()); ++ch)
            mainBuffer.copyFrom(ch, 0, mainDryBuffer, ch, 0, mainBuffer.getNumSamples());

        processMasterFxChain(mainBuffer, blockState);
    }
}

// =============================================================================
juce::AudioProcessorEditor* BassSynthAudioProcessor::createEditor()
{
    return new BassSynthAudioProcessorEditor(*this);
}

double BassSynthAudioProcessor::getTailLengthSeconds() const { return 15.0; }

// =============================================================================
int BassSynthAudioProcessor::getNumPrograms()
{
    const int b = getSelectedBassIndex();
    return static_cast<int>(factoryPresetBanks[static_cast<std::size_t>(b)].size());
}

int BassSynthAudioProcessor::getCurrentProgram()
{
    const int b = getSelectedBassIndex();
    return juce::jmax(0, currentPresetIndices[static_cast<std::size_t>(b)]);
}

void BassSynthAudioProcessor::setCurrentProgram(int index) { applyFactoryPreset(index); }

const juce::String BassSynthAudioProcessor::getProgramName(int index)
{
    const int b = getSelectedBassIndex();
    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(b)];
    if (index < 0 || index >= static_cast<int>(bank.size())) return {};
    return juce::String(juce::CharPointer_UTF8(bank[static_cast<std::size_t>(index)].name.c_str()));
}

void BassSynthAudioProcessor::changeProgramName(int, const juce::String&) {}

// =============================================================================
void BassSynthAudioProcessor::randomizePreset(float amount)
{
    auto& rng = juce::Random::getSystemRandom();
    const int bassIdx = getSelectedBassIndex();
    static constexpr const char* kRandSuffixes[] = {
        "level", "attack", "decay", "sustain", "release",
        "brightness", "body", "drive", "filter_env", "sub", "character",
        "cutoff", "resonance"
    };
    for (auto* suffix : kRandSuffixes)
    {
        const auto paramId = makeBassParamId(bassIdx, suffix);
        if (auto* param = parameters.getParameter(paramId))
        {
            const float cur   = param->getValue();
            const float delta = (rng.nextFloat() * 2.0f - 1.0f) * amount;
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, cur + delta));
        }
    }
}

void BassSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    // Store one preset-index per bass as "pi_N" properties
    for (int b = 0; b < mbs::kNumBasses; ++b)
        state.setProperty("pi_" + juce::String(b),
                          currentPresetIndices[static_cast<std::size_t>(b)], nullptr);

    // Store one user-preset path per bass as "upf_N" properties
    for (int b = 0; b < mbs::kNumBasses; ++b)
    {
        auto& f = currentUserPresetFiles[static_cast<std::size_t>(b)];
        if (f.existsAsFile())
            state.setProperty("upf_" + juce::String(b),
                              f.getFullPathName(), nullptr);
    }

    if (auto xml = state.createXml())
    {
        modmatrix::ModulationMatrix::saveStateToXml(*xml, captureModMatrixStateFromParams());
        copyXmlToBinary(*xml, destData);
    }
}

void BassSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xmlState = getXmlFromBinary(data, sizeInBytes);
    if (xmlState == nullptr || !xmlState->hasTagName(parameters.state.getType()))
        return;

    const auto sourceState = juce::ValueTree::fromXml(*xmlState);
    if (!sourceState.isValid())
        return;

    modmatrix::MatrixState legacyModMatrixState;
    const bool hasLegacyModMatrix = modmatrix::ModulationMatrix::loadStateFromXml(*xmlState, legacyModMatrixState);
    auto restoredState = parameters.copyState();
    const auto setSanitizedStateFloat = [&](const juce::String& paramId,
                                            const float fallback,
                                            const float minValue,
                                            const float maxValue)
    {
        setStateFloatProperty(restoredState, paramId,
            readFiniteStateFloat(sourceState, paramId, fallback, minValue, maxValue));
    };

    setSanitizedStateFloat(kSelectedBass, 0.0f, 0.0f, static_cast<float>(mbs::kNumBasses - 1));
    setSanitizedStateFloat(kMonoMode, 0.0f, 0.0f, 2.0f);
    setSanitizedStateFloat(kGlideTime, 0.0f, 0.0f, 1.5f);
    setSanitizedStateFloat(kLfoRate, 1.2f, 0.05f, 12.0f);
    setSanitizedStateFloat(kLfoDepth, 0.0f, 0.0f, 1.0f);
    setSanitizedStateFloat(kLfoWave, 0.0f, 0.0f, 3.0f);
    setSanitizedStateFloat(kLfoDest, 0.0f, 0.0f, 2.0f);
    setSanitizedStateFloat(kOutputGain, -3.0f, -24.0f, 12.0f);
    setSanitizedStateFloat(kMacroFatness, 0.5f, 0.0f, 1.0f);
    setSanitizedStateFloat(kMacroBrillance, 0.5f, 0.0f, 1.0f);
    setSanitizedStateFloat(kMacroPunch, 0.5f, 0.0f, 1.0f);
    setSanitizedStateFloat(kMacroDepth, 0.3f, 0.0f, 1.0f);
    setSanitizedStateFloat(kModWheelTarget, 1.0f, 0.0f, 1.0f);
    setSanitizedStateFloat(kVelocityCurve, 0.0f, 0.0f, 6.0f);
    setSanitizedStateFloat(kPitchBendRange, 2.0f, 1.0f, 24.0f);
    setSanitizedStateFloat(kModLfo2Rate, 2.0f, 0.05f, 12.0f);
    setSanitizedStateFloat(kModLfo2Wave, 0.0f, 0.0f, 3.0f);
    setSanitizedStateFloat(kFxLock, 0.0f, 0.0f, 1.0f);

    setSanitizedStateFloat(kCompThreshold, -19.0f, -60.0f, 0.0f);
    setSanitizedStateFloat(kCompRatio, 3.0f, 1.0f, 20.0f);
    setSanitizedStateFloat(kCompAttack, 10.0f, 0.1f, 100.0f);
    setSanitizedStateFloat(kCompRelease, 120.0f, 5.0f, 500.0f);
    setSanitizedStateFloat(kCompMakeup, 0.0f, 0.0f, 24.0f);
    setSanitizedStateFloat(kCompMix, 1.0f, 0.0f, 1.0f);
    setSanitizedStateFloat(kSatDrive, 1.8f, 1.0f, 16.0f);
    setSanitizedStateFloat(kSatMix, 0.15f, 0.0f, 1.0f);
    setSanitizedStateFloat(kTransientAttack, 0.10f, -1.0f, 1.0f);
    setSanitizedStateFloat(kTransientSustain, 0.0f, -1.0f, 1.0f);
    setSanitizedStateFloat(kTransientMix, 0.4f, 0.0f, 1.0f);
    setSanitizedStateFloat(kEqLowFreq, 80.0f, 40.0f, 800.0f);
    setSanitizedStateFloat(kEqLowGain, 0.0f, -12.0f, 12.0f);
    setSanitizedStateFloat(kEqMidFreq, 600.0f, 200.0f, 8000.0f);
    setSanitizedStateFloat(kEqMidGain, 0.0f, -12.0f, 12.0f);
    setSanitizedStateFloat(kEqMidQ, 1.0f, 0.1f, 10.0f);
    setSanitizedStateFloat(kEqHighFreq, 3500.0f, 1000.0f, 16000.0f);
    setSanitizedStateFloat(kEqHighGain, 0.0f, -12.0f, 12.0f);
    setSanitizedStateFloat(kChorusRate, 0.8f, 0.1f, 8.0f);
    setSanitizedStateFloat(kChorusDepth, 0.4f, 0.0f, 1.0f);
    setSanitizedStateFloat(kChorusMix, 0.0f, 0.0f, 1.0f);
    setSanitizedStateFloat(kDelayTime, 300.0f, 10.0f, 1500.0f);
    setSanitizedStateFloat(kDelayFeedback, 0.25f, 0.0f, 0.95f);
    setSanitizedStateFloat(kDelayMix, 0.0f, 0.0f, 1.0f);
    setSanitizedStateFloat(kDelaySync, 0.0f, 0.0f, 1.0f);
    setSanitizedStateFloat(kDelayNoteDiv, 0.0f, 0.0f, 4.0f);
    setSanitizedStateFloat(kReverbSize, 0.40f, 0.0f, 1.0f);
    setSanitizedStateFloat(kReverbDamping, 0.55f, 0.0f, 1.0f);
    setSanitizedStateFloat(kReverbWidth, 0.60f, 0.0f, 1.0f);
    setSanitizedStateFloat(kReverbMix, 0.0f, 0.0f, 1.0f);
    setSanitizedStateFloat(kLimiterThreshold, -0.3f, -12.0f, 0.0f);
    setSanitizedStateFloat(kLimiterRelease, 50.0f, 1.0f, 200.0f);
    setSanitizedStateFloat("fx_tab0_en", 1.0f, 0.0f, 1.0f);
    setSanitizedStateFloat("fx_tab1_en", 1.0f, 0.0f, 1.0f);
    setSanitizedStateFloat("fx_tab2_en", 1.0f, 0.0f, 1.0f);
    setSanitizedStateFloat("fx_tab3_en", 1.0f, 0.0f, 1.0f);
    setSanitizedStateFloat("fx_tab4_en", 1.0f, 0.0f, 1.0f);
    setSanitizedStateFloat("fx_tab5_en", 1.0f, 0.0f, 1.0f);
    setSanitizedStateFloat("fx_tab6_en", 1.0f, 0.0f, 1.0f);
    setSanitizedStateFloat("fx_tab7_en", 1.0f, 0.0f, 1.0f);

    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        const auto defaults = mbs::getDefaultSettings(bassIndex);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "level"), defaults.level, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "tune"), defaults.tuneSemitones, -24.0f, 24.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "brightness"), defaults.brightness, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "attack"), defaults.attackSeconds, 0.0f, 2.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "decay"), defaults.decaySeconds, 0.01f, 10.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "sustain"), defaults.sustainLevel, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "release"), defaults.releaseSeconds, 0.005f, 10.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "body"), defaults.body, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "drive"), defaults.drive, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "pitch_env"), defaults.pitchEnv, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "filter_env"), defaults.filterEnv, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "sub"), defaults.subLevel, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "character"), defaults.character, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "cutoff"), defaults.cutoffHz, 120.0f, 12000.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "pan"), defaults.pan, -1.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, "resonance"), defaults.resonance, 0.0f, 1.0f);
        setSanitizedStateFloat(makeBassParamId(bassIndex, kBassOutputSuffix), 0.0f, 0.0f, static_cast<float>(kNumAuxOutputs));
    }

    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        setSanitizedStateFloat(makeModMatrixParamId(slotIndex, "source"), 0.0f, 0.0f, static_cast<float>(modmatrix::kSourceCount - 1));
        setSanitizedStateFloat(makeModMatrixParamId(slotIndex, "dest"), 0.0f, 0.0f, static_cast<float>(modmatrix::kDestCount - 1));
        setSanitizedStateFloat(makeModMatrixParamId(slotIndex, "amount"), 0.0f, -1.0f, 1.0f);
    }

    parameters.replaceState(restoredState);
    resolveParameterPointers();
    if (hasLegacyModMatrix)
        applyModMatrixStateToParams(legacyModMatrixState, false);
    sanitizeAllParameters();

    for (int b = 0; b < mbs::kNumBasses; ++b)
    {
        int pi = static_cast<int>(sourceState.getProperty("pi_" + juce::String(b), 0));
        const auto& bank = factoryPresetBanks[static_cast<std::size_t>(b)];
        currentPresetIndices[static_cast<std::size_t>(b)] =
            juce::jlimit(0, juce::jmax(0, static_cast<int>(bank.size()) - 1), pi);

        auto upfKey = "upf_" + juce::String(b);
        auto path   = sourceState.getProperty(upfKey, "").toString();
        currentUserPresetFiles[static_cast<std::size_t>(b)] = juce::File{};
        if (path.isNotEmpty())
        {
            juce::File f(path);
            if (f.existsAsFile())
                currentUserPresetFiles[static_cast<std::size_t>(b)] = f;
        }

        outputBusCache[static_cast<std::size_t>(b)] = captureBassOutputBus(b);
    }
}

// =============================================================================
juce::StringArray BassSynthAudioProcessor::getFactoryPresetNames() const
{
    const int b = getSelectedBassIndex();
    juce::StringArray names;
    for (const auto& p : factoryPresetBanks[static_cast<std::size_t>(b)])
        names.add(juce::String(juce::CharPointer_UTF8(p.name.c_str())));
    return names;
}

juce::String BassSynthAudioProcessor::getFactoryPresetBrowserLabel(int presetIndex) const
{
    const int bassIndex = getSelectedBassIndex();
    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(bassIndex)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(bank.size()))
        return {};

    const auto& preset = bank[static_cast<std::size_t>(presetIndex)];
    const auto name = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
    const auto role = formatPresetRoleLabel(juce::String(juce::CharPointer_UTF8(preset.metadata.mixRole.c_str())));
    return role + " | " + name;
}

juce::String BassSynthAudioProcessor::getFactoryPresetBrowserSearchText(int presetIndex) const
{
    const int bassIndex = getSelectedBassIndex();
    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(bassIndex)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(bank.size()))
        return {};

    const auto& preset = bank[static_cast<std::size_t>(presetIndex)];
    return makePresetSearchText(
        juce::String(juce::CharPointer_UTF8(preset.name.c_str())),
        mbs::getBassName(bassIndex),
        juce::String(juce::CharPointer_UTF8(preset.metadata.mixRole.c_str())),
        juce::String(juce::CharPointer_UTF8(preset.metadata.familyLabel.c_str())),
        joinFactoryTags(preset.metadata.tags));
}

int BassSynthAudioProcessor::getCurrentFactoryPresetIndex() const noexcept
{
    return currentPresetIndices[static_cast<std::size_t>(getSelectedBassIndex())];
}

void BassSynthAudioProcessor::applyFactoryPreset(int presetIndex)
{
    const int b = getSelectedBassIndex();
    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(b)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(bank.size())) return;

    const auto& preset = bank[static_cast<std::size_t>(presetIndex)];
    applyBassSettingsToParams(b, preset.settings, false);
    setParamValueInternal(makeBassParamId(b, kBassOutputSuffix), static_cast<float>(juce::jlimit(0, kNumAuxOutputs, preset.outputBus)), false);
    outputBusCache[static_cast<std::size_t>(b)] = juce::jlimit(0, kNumAuxOutputs, preset.outputBus);
    if (preset.performance.has_value())
        applyPerformanceSettings(*preset.performance, false);
    if (preset.modMatrix.has_value())
        applyModMatrixStateToParams(*preset.modMatrix, false);

    auto fx = sanitizeFxSettings(preset.fx);
    mbs::maskUnavailableFx(b, fx);
    if (readCachedParamValue(globalParamRefs.fxLock) < 0.5f)
        applyGlobalFxSettings(fx, false);
    sanitizeAllParameters();

    currentPresetIndices[static_cast<std::size_t>(b)] = presetIndex;
    currentUserPresetFiles[static_cast<std::size_t>(b)] = juce::File{};
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
}

bool BassSynthAudioProcessor::saveFactoryPreset(int presetIndex)
{
    const int b = getSelectedBassIndex();
    auto& bank = factoryPresetBanks[static_cast<std::size_t>(b)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(bank.size()))
        return false;

    auto& preset = bank[static_cast<std::size_t>(presetIndex)];
    mbs::BassSettings rawSettings;
    rawSettings.level = getParamValue(makeBassParamId(b, "level"));
    rawSettings.tuneSemitones = getParamValue(makeBassParamId(b, "tune"));
    rawSettings.brightness = getParamValue(makeBassParamId(b, "brightness"));
    rawSettings.attackSeconds = getParamValue(makeBassParamId(b, "attack"));
    rawSettings.decaySeconds = getParamValue(makeBassParamId(b, "decay"));
    rawSettings.sustainLevel = getParamValue(makeBassParamId(b, "sustain"));
    rawSettings.releaseSeconds = getParamValue(makeBassParamId(b, "release"));
    rawSettings.body = getParamValue(makeBassParamId(b, "body"));
    rawSettings.drive = getParamValue(makeBassParamId(b, "drive"));
    rawSettings.pitchEnv = getParamValue(makeBassParamId(b, "pitch_env"));
    rawSettings.filterEnv = getParamValue(makeBassParamId(b, "filter_env"));
    rawSettings.subLevel = getParamValue(makeBassParamId(b, "sub"));
    rawSettings.character = getParamValue(makeBassParamId(b, "character"));
    rawSettings.cutoffHz = getParamValue(makeBassParamId(b, "cutoff"));
    rawSettings.pan = getParamValue(makeBassParamId(b, "pan"));
    rawSettings.resonance = getParamValue(makeBassParamId(b, "resonance"));
    rawSettings.glideTime = getParamValue(kGlideTime);
    preset.settings = sanitizeBassSettings(b, rawSettings);
    preset.outputBus = captureBassOutputBus(b);
    preset.fx = sanitizeFxSettings(snapshotGlobalFxSettings());
    preset.performance = sanitizePerformanceSettings(snapshotPerformanceSettings());
    preset.modMatrix = sanitizeModMatrixState(captureModMatrixStateFromParams());
    mbs::maskUnavailableFx(b, preset.fx);

    auto dir  = getFactoryOverridesDirectory()
                    .getChildFile("bass_" + juce::String(b));
    dir.createDirectory();
    auto file = dir.getChildFile(juce::String(presetIndex) + ".xml");

    PresetPersistenceState state;
    state.name = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
    state.bassIndex = b;
    state.presetIndex = presetIndex;
    state.settings = preset.settings;
    state.fx = preset.fx;
    state.outputBus = preset.outputBus;
    state.performance = *preset.performance;
    state.modMatrix = *preset.modMatrix;
    state.metadata = makeFactoryPresetMetadata(preset.metadata);

    auto root = createPresetXml("FactoryPreset", state);
    return root->writeTo(file);
}

void BassSynthAudioProcessor::loadFactoryOverrides()
{
    auto rootDir = getFactoryOverridesDirectory();
    if (!rootDir.isDirectory()) return;

    for (int b = 0; b < mbs::kNumBasses; ++b)
    {
        auto bassDir = rootDir.getChildFile("bass_" + juce::String(b));
        if (!bassDir.isDirectory()) continue;

        auto& bank = factoryPresetBanks[static_cast<std::size_t>(b)];
        for (int i = 0; i < static_cast<int>(bank.size()); ++i)
        {
            auto file = bassDir.getChildFile(juce::String(i) + ".xml");
            if (!file.existsAsFile()) continue;

            auto xml = juce::XmlDocument::parse(file);
            if (xml == nullptr || !xml->hasTagName("FactoryPreset")) continue;
            const bool needsRewrite = shouldRewritePresetXml(*xml);

            int warningCount = 0;
            auto settings = bank[static_cast<std::size_t>(i)].settings;
            settings.level          = readFiniteXmlFloat(*xml, "level", settings.level, 0.0f, 1.0f, &warningCount);
            settings.tuneSemitones  = readFiniteXmlFloat(*xml, "tune", settings.tuneSemitones, -24.0f, 24.0f, &warningCount);
            settings.brightness     = readFiniteXmlFloat(*xml, "brightness", settings.brightness, 0.0f, 1.0f, &warningCount);
            settings.attackSeconds  = readFiniteXmlFloat(*xml, "attack", settings.attackSeconds, 0.0f, 2.0f, &warningCount);
            settings.decaySeconds   = readFiniteXmlFloat(*xml, "decay", settings.decaySeconds, 0.01f, 10.0f, &warningCount);
            settings.sustainLevel   = readFiniteXmlFloat(*xml, "sustain", settings.sustainLevel, 0.0f, 1.0f, &warningCount);
            settings.releaseSeconds = readFiniteXmlFloat(*xml, "release", settings.releaseSeconds, 0.005f, 10.0f, &warningCount);
            settings.body           = readFiniteXmlFloat(*xml, "body", settings.body, 0.0f, 1.0f, &warningCount);
            settings.drive          = readFiniteXmlFloat(*xml, "drive", settings.drive, 0.0f, 1.0f, &warningCount);
            settings.pitchEnv       = readFiniteXmlFloat(*xml, "pitch_env", settings.pitchEnv, 0.0f, 1.0f, &warningCount);
            settings.filterEnv      = xml->hasAttribute("filter_env")
                ? readFiniteXmlFloat(*xml, "filter_env", settings.filterEnv, 0.0f, 1.0f, &warningCount)
                : (mbs::supportsFilterEnvControl(b) ? settings.brightness : settings.filterEnv);
            settings.subLevel       = readFiniteXmlFloat(*xml, "sub", settings.subLevel, 0.0f, 1.0f, &warningCount);
            settings.character      = readFiniteXmlFloat(*xml, "character", settings.character, 0.0f, 1.0f, &warningCount);
            settings.cutoffHz       = readFiniteXmlFloat(*xml, "cutoff", settings.cutoffHz, 120.0f, 12000.0f, &warningCount);
            settings.pan            = readFiniteXmlFloat(*xml, "pan", settings.pan, -1.0f, 1.0f, &warningCount);
            settings.resonance      = readFiniteXmlFloat(*xml, "resonance", settings.resonance, 0.0f, 1.0f, &warningCount);
            settings.glideTime      = readFiniteXmlFloat(*xml, "glide_time", settings.glideTime, 0.0f, 1.5f, &warningCount);
            bank[static_cast<std::size_t>(i)].settings = sanitizeBassSettings(b, settings);
            bank[static_cast<std::size_t>(i)].outputBus =
                juce::jlimit(0, kNumAuxOutputs, xml->getIntAttribute("output", bank[static_cast<std::size_t>(i)].outputBus));
            readGlobalFxAttributes(*xml, bank[static_cast<std::size_t>(i)].fx);
            bank[static_cast<std::size_t>(i)].fx = sanitizeFxSettings(bank[static_cast<std::size_t>(i)].fx);
            mbs::maskUnavailableFx(b, bank[static_cast<std::size_t>(i)].fx);

            auto performance = bank[static_cast<std::size_t>(i)].performance.value_or(mbs::PatchPerformanceSettings{});
            readPerformanceAttributes(*xml, performance, &warningCount);
            bank[static_cast<std::size_t>(i)].performance = sanitizePerformanceSettings(performance);

            auto modMatrixState = bank[static_cast<std::size_t>(i)].modMatrix.value_or(modmatrix::MatrixState{});
            modmatrix::ModulationMatrix::loadStateFromXml(*xml, modMatrixState);
            bank[static_cast<std::size_t>(i)].modMatrix = sanitizeModMatrixState(modMatrixState);

            auto metadata = readPresetMetadataFromXml(*xml, makeFactoryPresetMetadata(bank[static_cast<std::size_t>(i)].metadata));
            bank[static_cast<std::size_t>(i)].metadata.mixRole = metadata.mixRole.toStdString();
            bank[static_cast<std::size_t>(i)].metadata.familyLabel = metadata.family.toStdString();
            bank[static_cast<std::size_t>(i)].metadata.tags = splitTags(metadata.tags);
            bank[static_cast<std::size_t>(i)].metadata.nominalPeakDb = metadata.nominalPeakDb;

            if (needsRewrite)
            {
                PresetPersistenceState rewriteState;
                rewriteState.name = juce::String(juce::CharPointer_UTF8(bank[static_cast<std::size_t>(i)].name.c_str()));
                rewriteState.bassIndex = b;
                rewriteState.presetIndex = i;
                rewriteState.settings = bank[static_cast<std::size_t>(i)].settings;
                rewriteState.fx = bank[static_cast<std::size_t>(i)].fx;
                rewriteState.outputBus = bank[static_cast<std::size_t>(i)].outputBus;
                rewriteState.performance = *bank[static_cast<std::size_t>(i)].performance;
                rewriteState.modMatrix = *bank[static_cast<std::size_t>(i)].modMatrix;
                rewriteState.metadata = makeFactoryPresetMetadata(bank[static_cast<std::size_t>(i)].metadata);
                createPresetXml("FactoryPreset", rewriteState)->writeTo(file);
            }
        }
    }
}

// =============================================================================
juce::File BassSynthAudioProcessor::getDataRootDirectory()
{
    auto overrideRoot = juce::SystemStats::getEnvironmentVariable(kBassDataRootEnv, {}).trim();
    if (overrideRoot.isNotEmpty())
    {
        juce::File root(overrideRoot);
        if (!juce::File::isAbsolutePath(overrideRoot))
            root = juce::File::getCurrentWorkingDirectory().getChildFile(overrideRoot);

        root.createDirectory();
        return root;
    }

    auto root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("MusiqueBassSynth");
    root.createDirectory();
    return root;
}

// =============================================================================
juce::File BassSynthAudioProcessor::getFactoryOverridesDirectory()
{
    auto dir = getDataRootDirectory().getChildFile("FactoryOverrides");
    dir.createDirectory();
    return dir;
}

juce::File BassSynthAudioProcessor::getUserPresetsDirectory(int bassIndex)
{
    auto dir = getDataRootDirectory()
                   .getChildFile("Presets")
                   .getChildFile("bass_" + juce::String(bassIndex));
    dir.createDirectory();
    return dir;
}

bool BassSynthAudioProcessor::writePresetManifest(const juce::File& presetFile,
                                                  const juce::String& presetName,
                                                  int bassIndex,
                                                  const juce::String& sourceModel) const
{
    const auto identity = musique::preset::getSynthIdentity(3);
    if (!identity.isValid())
        return false;

    musique::preset::PresetManifest manifest;
    manifest.synthId = identity.synthId;
    manifest.synthType = identity.synthType;
    manifest.instrumentIndex = juce::jlimit(0, mbs::kNumBasses - 1, bassIndex);
    manifest.instrumentName = mbs::getBassName(manifest.instrumentIndex);
    manifest.presetName = presetName;
    manifest.xmlRootTag = identity.xmlRootTag;
    manifest.sourceModel = sourceModel;
    manifest.createdAt = juce::Time::getCurrentTime().toISO8601(true);
    manifest.sourcePath = presetFile.getFullPathName();
    manifest.validationVersion = 1;

    return musique::preset::saveManifestToFile(
        musique::preset::manifestFileForPresetFile(presetFile), manifest);
}

juce::Array<juce::File> BassSynthAudioProcessor::scanUserPresets() const
{
    juce::Array<juce::File> results;
    auto dir = getUserPresetsDirectory(getSelectedBassIndex());
    if (dir.isDirectory())
        dir.findChildFiles(results, juce::File::findFiles, false, "*.xml");
    results.sort();
    return results;
}

bool BassSynthAudioProcessor::saveUserPreset(const juce::String& name)
{
    if (name.isEmpty()) return false;
    const int b = getSelectedBassIndex();
    auto file = getUserPresetsDirectory(b).getChildFile(
        juce::File::createLegalFileName(name) + ".xml");

    mbs::BassSettings settings;
    settings.level = getParamValue(makeBassParamId(b, "level"));
    settings.tuneSemitones = getParamValue(makeBassParamId(b, "tune"));
    settings.brightness = getParamValue(makeBassParamId(b, "brightness"));
    settings.attackSeconds = getParamValue(makeBassParamId(b, "attack"));
    settings.decaySeconds = getParamValue(makeBassParamId(b, "decay"));
    settings.sustainLevel = getParamValue(makeBassParamId(b, "sustain"));
    settings.releaseSeconds = getParamValue(makeBassParamId(b, "release"));
    settings.body = getParamValue(makeBassParamId(b, "body"));
    settings.drive = getParamValue(makeBassParamId(b, "drive"));
    settings.pitchEnv = getParamValue(makeBassParamId(b, "pitch_env"));
    settings.filterEnv = getParamValue(makeBassParamId(b, "filter_env"));
    settings.subLevel = getParamValue(makeBassParamId(b, "sub"));
    settings.character = getParamValue(makeBassParamId(b, "character"));
    settings.cutoffHz = getParamValue(makeBassParamId(b, "cutoff"));
    settings.pan = getParamValue(makeBassParamId(b, "pan"));
    settings.resonance = getParamValue(makeBassParamId(b, "resonance"));
    settings.glideTime = getParamValue(kGlideTime);
    settings = sanitizeBassSettings(b, settings);

    auto fx = sanitizeFxSettings(snapshotGlobalFxSettings());
    mbs::maskUnavailableFx(b, fx);
    PresetPersistenceState state;
    state.name = name;
    state.bassIndex = b;
    state.settings = settings;
    state.fx = fx;
    state.outputBus = juce::jlimit(
        0, kNumAuxOutputs,
        static_cast<int>(std::round(getParamValue(makeBassParamId(b, kBassOutputSuffix)))));
    state.performance = sanitizePerformanceSettings(snapshotPerformanceSettings());
    state.modMatrix = sanitizeModMatrixState(captureModMatrixStateFromParams());
    state.metadata = makeUserPresetMetadata(b);
    auto root = createPresetXml("BassPreset", state);

    if (root->writeTo(file))
    {
        writePresetManifest(file, name, b, mbs::getBassName(b));
        currentUserPresetFiles[static_cast<std::size_t>(b)] = file;
        currentPresetIndices[static_cast<std::size_t>(b)] = -1;
        return true;
    }
    return false;
}

bool BassSynthAudioProcessor::updateUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    const int b = getSelectedBassIndex();

    mbs::BassSettings settings;
    settings.level = getParamValue(makeBassParamId(b, "level"));
    settings.tuneSemitones = getParamValue(makeBassParamId(b, "tune"));
    settings.brightness = getParamValue(makeBassParamId(b, "brightness"));
    settings.attackSeconds = getParamValue(makeBassParamId(b, "attack"));
    settings.decaySeconds = getParamValue(makeBassParamId(b, "decay"));
    settings.sustainLevel = getParamValue(makeBassParamId(b, "sustain"));
    settings.releaseSeconds = getParamValue(makeBassParamId(b, "release"));
    settings.body = getParamValue(makeBassParamId(b, "body"));
    settings.drive = getParamValue(makeBassParamId(b, "drive"));
    settings.pitchEnv = getParamValue(makeBassParamId(b, "pitch_env"));
    settings.filterEnv = getParamValue(makeBassParamId(b, "filter_env"));
    settings.subLevel = getParamValue(makeBassParamId(b, "sub"));
    settings.character = getParamValue(makeBassParamId(b, "character"));
    settings.cutoffHz = getParamValue(makeBassParamId(b, "cutoff"));
    settings.pan = getParamValue(makeBassParamId(b, "pan"));
    settings.resonance = getParamValue(makeBassParamId(b, "resonance"));
    settings.glideTime = getParamValue(kGlideTime);
    settings = sanitizeBassSettings(b, settings);

    auto fx = sanitizeFxSettings(snapshotGlobalFxSettings());
    mbs::maskUnavailableFx(b, fx);
    PresetPersistenceState state;
    state.name = file.getFileNameWithoutExtension();
    state.bassIndex = b;
    state.settings = settings;
    state.fx = fx;
    state.outputBus = juce::jlimit(
        0, kNumAuxOutputs,
        static_cast<int>(std::round(getParamValue(makeBassParamId(b, kBassOutputSuffix)))));
    state.performance = sanitizePerformanceSettings(snapshotPerformanceSettings());
    state.modMatrix = sanitizeModMatrixState(captureModMatrixStateFromParams());
    state.metadata = makeUserPresetMetadata(b);
    auto root = createPresetXml("BassPreset", state);

    if (root->writeTo(file))
    {
        writePresetManifest(file, file.getFileNameWithoutExtension(), b, mbs::getBassName(b));
        currentUserPresetFiles[static_cast<std::size_t>(b)] = file;
        currentPresetIndices[static_cast<std::size_t>(b)] = -1;
        return true;
    }
    return false;
}

bool BassSynthAudioProcessor::deleteUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    const int b = getSelectedBassIndex();
    if (currentUserPresetFiles[static_cast<std::size_t>(b)] == file)
        currentUserPresetFiles[static_cast<std::size_t>(b)] = juce::File{};
    const auto manifestFile = musique::preset::manifestFileForPresetFile(file);
    if (manifestFile.existsAsFile())
        manifestFile.deleteFile();
    return file.deleteFile();
}

bool BassSynthAudioProcessor::loadUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName("BassPreset")) return false;
    const bool needsRewrite = shouldRewritePresetXml(*xml);

    const int b = getSelectedBassIndex();
    int warningCount = 0;
    auto settings = mbs::getDefaultSettings(b);
    settings.level = readFiniteXmlFloat(*xml, "level", settings.level, 0.0f, 1.0f, &warningCount);
    settings.tuneSemitones = readFiniteXmlFloat(*xml, "tune", settings.tuneSemitones, -24.0f, 24.0f, &warningCount);
    settings.brightness = readFiniteXmlFloat(*xml, "brightness", settings.brightness, 0.0f, 1.0f, &warningCount);
    settings.attackSeconds = readFiniteXmlFloat(*xml, "attack", settings.attackSeconds, 0.0f, 2.0f, &warningCount);
    settings.decaySeconds = readFiniteXmlFloat(*xml, "decay", settings.decaySeconds, 0.01f, 10.0f, &warningCount);
    settings.sustainLevel = readFiniteXmlFloat(*xml, "sustain", settings.sustainLevel, 0.0f, 1.0f, &warningCount);
    settings.releaseSeconds = readFiniteXmlFloat(*xml, "release", settings.releaseSeconds, 0.005f, 10.0f, &warningCount);
    settings.body = readFiniteXmlFloat(*xml, "body", settings.body, 0.0f, 1.0f, &warningCount);
    settings.drive = readFiniteXmlFloat(*xml, "drive", settings.drive, 0.0f, 1.0f, &warningCount);
    settings.pitchEnv = readFiniteXmlFloat(*xml, "pitch_env", settings.pitchEnv, 0.0f, 1.0f, &warningCount);
    settings.filterEnv = xml->hasAttribute("filter_env")
        ? readFiniteXmlFloat(*xml, "filter_env", settings.filterEnv, 0.0f, 1.0f, &warningCount)
        : (mbs::supportsFilterEnvControl(b) ? settings.brightness : settings.filterEnv);
    settings.subLevel = readFiniteXmlFloat(*xml, "sub", settings.subLevel, 0.0f, 1.0f, &warningCount);
    settings.character = readFiniteXmlFloat(*xml, "character", settings.character, 0.0f, 1.0f, &warningCount);
    settings.cutoffHz = readFiniteXmlFloat(*xml, "cutoff", settings.cutoffHz, 120.0f, 12000.0f, &warningCount);
    settings.pan = readFiniteXmlFloat(*xml, "pan", settings.pan, -1.0f, 1.0f, &warningCount);
    settings.resonance = readFiniteXmlFloat(*xml, "resonance", settings.resonance, 0.0f, 1.0f, &warningCount);
    settings.glideTime = readFiniteXmlFloat(*xml, "glide_time", settings.glideTime, 0.0f, 1.5f, &warningCount);
    settings = sanitizeBassSettings(b, settings);
    applyBassSettingsToParams(b, settings, false);

    const auto outputBus = juce::jlimit(0, kNumAuxOutputs,
                                        xml->getIntAttribute("output", captureBassOutputBus(b)));
    setParamValueInternal(makeBassParamId(b, kBassOutputSuffix), static_cast<float>(outputBus), false);
    outputBusCache[static_cast<std::size_t>(b)] = outputBus;

    auto fx = sanitizeFxSettings(snapshotGlobalFxSettings());
    readGlobalFxAttributes(*xml, fx);
    fx = sanitizeFxSettings(fx);
    mbs::maskUnavailableFx(b, fx);
    auto performance = snapshotPerformanceSettings();
    readPerformanceAttributes(*xml, performance, &warningCount);
    performance = sanitizePerformanceSettings(performance);
    applyPerformanceSettings(performance, false);

    auto modMatrixState = sanitizeModMatrixState(captureModMatrixStateFromParams());
    modmatrix::ModulationMatrix::loadStateFromXml(*xml, modMatrixState);
    modMatrixState = sanitizeModMatrixState(modMatrixState);
    applyModMatrixStateToParams(modMatrixState, false);

    if (readCachedParamValue(globalParamRefs.fxLock) < 0.5f)
        applyGlobalFxSettings(fx, false);
    sanitizeAllParameters();

    currentUserPresetFiles[static_cast<std::size_t>(b)] = file;
    currentPresetIndices[static_cast<std::size_t>(b)] = -1;
    writePresetManifest(file, file.getFileNameWithoutExtension(), b, mbs::getBassName(b));
    if (needsRewrite)
    {
        PresetPersistenceState rewriteState;
        rewriteState.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
        rewriteState.bassIndex = b;
        rewriteState.settings = settings;
        rewriteState.fx = fx;
        rewriteState.outputBus = outputBus;
        rewriteState.performance = performance;
        rewriteState.modMatrix = modMatrixState;
        rewriteState.metadata = readPresetMetadataFromXml(*xml, makeUserPresetMetadata(b));
        createPresetXml("BassPreset", rewriteState)->writeTo(file);
    }
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
    return true;
}

bool BassSynthAudioProcessor::isCurrentPresetUser() const noexcept
{
    return currentUserPresetFiles[static_cast<std::size_t>(getSelectedBassIndex())].existsAsFile();
}

juce::File BassSynthAudioProcessor::getCurrentUserPresetFile() const noexcept
{
    return currentUserPresetFiles[static_cast<std::size_t>(getSelectedBassIndex())];
}



// =============================================================================
int BassSynthAudioProcessor::getSelectedBassIndex() const
{
    return juce::jlimit(0, mbs::kNumBasses - 1,
                        static_cast<int>(std::round(readCachedParamValue(globalParamRefs.selectedBass))));
}

bool BassSynthAudioProcessor::isFxAvailableForCurrentBass(mbs::GlobalFxSlot slot) const
{
    return mbs::isFxAvailable(getSelectedBassIndex(), slot);
}

float BassSynthAudioProcessor::getParamValue(const juce::String& paramId) const
{
    if (const auto* raw = parameters.getRawParameterValue(paramId))
        return raw->load();
    return 0.0f;
}

float BassSynthAudioProcessor::sanitizeParameterValue(const juce::String& paramId,
                                                      float value,
                                                      float fallback,
                                                      int* warningCount) const
{
    if (!std::isfinite(value))
    {
        if (warningCount != nullptr)
            ++(*warningCount);
        value = fallback;
    }

    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(paramId)))
    {
        const auto normalised = juce::jlimit(0.0f, 1.0f, parameter->convertTo0to1(value));
        const auto sanitized = parameter->convertFrom0to1(normalised);
        if (warningCount != nullptr && std::abs(sanitized - value) > 1.0e-4f)
            ++(*warningCount);
        return sanitized;
    }

    return fallback;
}

void BassSynthAudioProcessor::setParamValueInternal(const juce::String& paramId, float value, bool notifyHost)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(paramId)))
        setRangedParameterValue(parameter, value, notifyHost);
}

void BassSynthAudioProcessor::setParamValue(const juce::String& paramId, float value)
{
    setParamValueInternal(paramId, value, true);
}

int BassSynthAudioProcessor::captureBassOutputBus(int bassIndex) const
{
    return juce::jlimit(0, kNumAuxOutputs,
                        static_cast<int>(std::round(readCachedParamValue(
                            bassParamRefs[static_cast<std::size_t>(bassIndex)].output))));
}

mbs::BassSettings BassSynthAudioProcessor::sanitizeBassSettings(int bassIndex, const mbs::BassSettings& settings) const
{
    auto sanitized = settings;
    sanitized.level = sanitizeParameterValue(makeBassParamId(bassIndex, "level"), sanitized.level, mbs::getDefaultSettings(bassIndex).level);
    sanitized.tuneSemitones = sanitizeParameterValue(makeBassParamId(bassIndex, "tune"), sanitized.tuneSemitones, mbs::getDefaultSettings(bassIndex).tuneSemitones);
    sanitized.brightness = sanitizeParameterValue(makeBassParamId(bassIndex, "brightness"), sanitized.brightness, mbs::getDefaultSettings(bassIndex).brightness);
    sanitized.attackSeconds = sanitizeParameterValue(makeBassParamId(bassIndex, "attack"), sanitized.attackSeconds, mbs::getDefaultSettings(bassIndex).attackSeconds);
    sanitized.decaySeconds = sanitizeParameterValue(makeBassParamId(bassIndex, "decay"), sanitized.decaySeconds, mbs::getDefaultSettings(bassIndex).decaySeconds);
    sanitized.sustainLevel = sanitizeParameterValue(makeBassParamId(bassIndex, "sustain"), sanitized.sustainLevel, mbs::getDefaultSettings(bassIndex).sustainLevel);
    sanitized.releaseSeconds = sanitizeParameterValue(makeBassParamId(bassIndex, "release"), sanitized.releaseSeconds, mbs::getDefaultSettings(bassIndex).releaseSeconds);
    sanitized.body = sanitizeParameterValue(makeBassParamId(bassIndex, "body"), sanitized.body, mbs::getDefaultSettings(bassIndex).body);
    sanitized.drive = sanitizeParameterValue(makeBassParamId(bassIndex, "drive"), sanitized.drive, mbs::getDefaultSettings(bassIndex).drive);
    sanitized.pitchEnv = sanitizeParameterValue(makeBassParamId(bassIndex, "pitch_env"), sanitized.pitchEnv, mbs::getDefaultSettings(bassIndex).pitchEnv);
    sanitized.filterEnv = sanitizeParameterValue(makeBassParamId(bassIndex, "filter_env"), sanitized.filterEnv, mbs::getDefaultSettings(bassIndex).filterEnv);
    sanitized.subLevel = sanitizeParameterValue(makeBassParamId(bassIndex, "sub"), sanitized.subLevel, mbs::getDefaultSettings(bassIndex).subLevel);
    sanitized.character = sanitizeParameterValue(makeBassParamId(bassIndex, "character"), sanitized.character, mbs::getDefaultSettings(bassIndex).character);
    sanitized.cutoffHz = sanitizeParameterValue(makeBassParamId(bassIndex, "cutoff"), sanitized.cutoffHz, mbs::getDefaultSettings(bassIndex).cutoffHz);
    sanitized.pan = sanitizeParameterValue(makeBassParamId(bassIndex, "pan"), sanitized.pan, mbs::getDefaultSettings(bassIndex).pan);
    sanitized.resonance = sanitizeParameterValue(makeBassParamId(bassIndex, "resonance"), sanitized.resonance, mbs::getDefaultSettings(bassIndex).resonance);
    sanitized.glideTime = sanitizeParameterValue(kGlideTime, sanitized.glideTime, mbs::getDefaultSettings(bassIndex).glideTime);
    return sanitized;
}

mbs::GlobalFxSettings BassSynthAudioProcessor::snapshotGlobalFxSettings() const
{
    mbs::GlobalFxSettings fx;
    fx.satDrive = readCachedParamValue(globalParamRefs.satDrive, fx.satDrive);
    fx.satMix = readCachedParamValue(globalParamRefs.satMix, fx.satMix);
    fx.transientAttack = readCachedParamValue(globalParamRefs.transientAttack, fx.transientAttack);
    fx.transientSustain = readCachedParamValue(globalParamRefs.transientSustain, fx.transientSustain);
    fx.transientMix = readCachedParamValue(globalParamRefs.transientMix, fx.transientMix);
    fx.compThreshold = readCachedParamValue(globalParamRefs.compThreshold, fx.compThreshold);
    fx.compRatio = readCachedParamValue(globalParamRefs.compRatio, fx.compRatio);
    fx.compAttack = readCachedParamValue(globalParamRefs.compAttack, fx.compAttack);
    fx.compRelease = readCachedParamValue(globalParamRefs.compRelease, fx.compRelease);
    fx.compMakeup = readCachedParamValue(globalParamRefs.compMakeup, fx.compMakeup);
    fx.compMix = readCachedParamValue(globalParamRefs.compMix, fx.compMix);
    fx.eqLowFreq = readCachedParamValue(globalParamRefs.eqLowFreq, fx.eqLowFreq);
    fx.eqLowGain = readCachedParamValue(globalParamRefs.eqLowGain, fx.eqLowGain);
    fx.eqMidFreq = readCachedParamValue(globalParamRefs.eqMidFreq, fx.eqMidFreq);
    fx.eqMidGain = readCachedParamValue(globalParamRefs.eqMidGain, fx.eqMidGain);
    fx.eqMidQ = readCachedParamValue(globalParamRefs.eqMidQ, fx.eqMidQ);
    fx.eqHighFreq = readCachedParamValue(globalParamRefs.eqHighFreq, fx.eqHighFreq);
    fx.eqHighGain = readCachedParamValue(globalParamRefs.eqHighGain, fx.eqHighGain);
    fx.chorusRate = readCachedParamValue(globalParamRefs.chorusRate, fx.chorusRate);
    fx.chorusDepth = readCachedParamValue(globalParamRefs.chorusDepth, fx.chorusDepth);
    fx.chorusMix = readCachedParamValue(globalParamRefs.chorusMix, fx.chorusMix);
    fx.delayTime = readCachedParamValue(globalParamRefs.delayTime, fx.delayTime);
    fx.delayFeedback = readCachedParamValue(globalParamRefs.delayFeedback, fx.delayFeedback);
    fx.delayMix = readCachedParamValue(globalParamRefs.delayMix, fx.delayMix);
    fx.delaySync = readCachedParamValue(globalParamRefs.delaySync) >= 0.5f;
    fx.delayNoteDiv = juce::jlimit(0, 4, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.delayNoteDiv))));
    fx.reverbSize = readCachedParamValue(globalParamRefs.reverbSize, fx.reverbSize);
    fx.reverbDamping = readCachedParamValue(globalParamRefs.reverbDamping, fx.reverbDamping);
    fx.reverbWidth = readCachedParamValue(globalParamRefs.reverbWidth, fx.reverbWidth);
    fx.reverbMix = readCachedParamValue(globalParamRefs.reverbMix, fx.reverbMix);
    fx.limiterThreshold = readCachedParamValue(globalParamRefs.limiterThreshold, fx.limiterThreshold);
    fx.limiterRelease = readCachedParamValue(globalParamRefs.limiterRelease, fx.limiterRelease);
    fx.saturatorOn = readCachedParamValue(globalParamRefs.fxSatEnable) >= 0.5f;
    fx.transientOn = readCachedParamValue(globalParamRefs.fxTransientEnable) >= 0.5f;
    fx.compressorOn = readCachedParamValue(globalParamRefs.fxCompEnable) >= 0.5f;
    fx.eqOn = readCachedParamValue(globalParamRefs.fxEqEnable) >= 0.5f;
    fx.chorusOn = readCachedParamValue(globalParamRefs.fxChorusEnable) >= 0.5f;
    fx.delayOn = readCachedParamValue(globalParamRefs.fxDelayEnable) >= 0.5f;
    fx.reverbOn = readCachedParamValue(globalParamRefs.fxReverbEnable) >= 0.5f;
    fx.limiterOn = readCachedParamValue(globalParamRefs.fxLimiterEnable) >= 0.5f;
    return fx;
}

mbs::GlobalFxSettings BassSynthAudioProcessor::sanitizeFxSettings(const mbs::GlobalFxSettings& fx) const
{
    auto sanitized = fx;
    sanitized.satDrive = juce::jlimit(1.0f, 16.0f, sanitized.satDrive);
    sanitized.satMix = clamp01(sanitized.satMix);
    sanitized.transientAttack = juce::jlimit(-1.0f, 1.0f, sanitized.transientAttack);
    sanitized.transientSustain = juce::jlimit(-1.0f, 1.0f, sanitized.transientSustain);
    sanitized.transientMix = clamp01(sanitized.transientMix);
    sanitized.compThreshold = juce::jlimit(-60.0f, 0.0f, sanitized.compThreshold);
    sanitized.compRatio = juce::jlimit(1.0f, 20.0f, sanitized.compRatio);
    sanitized.compAttack = juce::jlimit(0.1f, 100.0f, sanitized.compAttack);
    sanitized.compRelease = juce::jlimit(5.0f, 500.0f, sanitized.compRelease);
    sanitized.compMakeup = juce::jlimit(0.0f, 24.0f, sanitized.compMakeup);
    sanitized.compMix = clamp01(sanitized.compMix);
    sanitized.eqLowFreq = juce::jlimit(40.0f, 800.0f, sanitized.eqLowFreq);
    sanitized.eqLowGain = juce::jlimit(-12.0f, 12.0f, sanitized.eqLowGain);
    sanitized.eqMidFreq = juce::jlimit(200.0f, 8000.0f, sanitized.eqMidFreq);
    sanitized.eqMidGain = juce::jlimit(-12.0f, 12.0f, sanitized.eqMidGain);
    sanitized.eqMidQ = juce::jlimit(0.1f, 10.0f, sanitized.eqMidQ);
    sanitized.eqHighFreq = juce::jlimit(1000.0f, 16000.0f, sanitized.eqHighFreq);
    sanitized.eqHighGain = juce::jlimit(-12.0f, 12.0f, sanitized.eqHighGain);
    sanitized.chorusRate = juce::jlimit(0.1f, 8.0f, sanitized.chorusRate);
    sanitized.chorusDepth = clamp01(sanitized.chorusDepth);
    sanitized.chorusMix = clamp01(sanitized.chorusMix);
    sanitized.delayTime = juce::jlimit(10.0f, 1500.0f, sanitized.delayTime);
    sanitized.delayFeedback = juce::jlimit(0.0f, 0.95f, sanitized.delayFeedback);
    sanitized.delayMix = clamp01(sanitized.delayMix);
    sanitized.delaySync = fx.delaySync;
    sanitized.delayNoteDiv = juce::jlimit(0, 4, sanitized.delayNoteDiv);
    sanitized.reverbSize = clamp01(sanitized.reverbSize);
    sanitized.reverbDamping = clamp01(sanitized.reverbDamping);
    sanitized.reverbWidth = clamp01(sanitized.reverbWidth);
    sanitized.reverbMix = clamp01(sanitized.reverbMix);
    sanitized.limiterThreshold = juce::jlimit(-12.0f, 0.0f, sanitized.limiterThreshold);
    sanitized.limiterRelease = juce::jlimit(1.0f, 200.0f, sanitized.limiterRelease);
    return sanitized;
}

mbs::PatchPerformanceSettings BassSynthAudioProcessor::sanitizePerformanceSettings(
    const mbs::PatchPerformanceSettings& settings) const
{
    auto sanitized = settings;
    sanitized.monoMode = static_cast<int>(std::round(sanitizeParameterValue(
        kMonoMode, static_cast<float>(sanitized.monoMode), 0.0f)));
    sanitized.lfoRate = sanitizeParameterValue(kLfoRate, sanitized.lfoRate, 1.2f);
    sanitized.lfoDepth = sanitizeParameterValue(kLfoDepth, sanitized.lfoDepth, 0.0f);
    sanitized.lfoWave = static_cast<int>(std::round(sanitizeParameterValue(
        kLfoWave, static_cast<float>(sanitized.lfoWave), 0.0f)));
    sanitized.lfoDest = static_cast<int>(std::round(sanitizeParameterValue(
        kLfoDest, static_cast<float>(sanitized.lfoDest), 0.0f)));
    sanitized.macroFatness = sanitizeParameterValue(kMacroFatness, sanitized.macroFatness, 0.5f);
    sanitized.macroBrillance = sanitizeParameterValue(kMacroBrillance, sanitized.macroBrillance, 0.5f);
    sanitized.macroPunch = sanitizeParameterValue(kMacroPunch, sanitized.macroPunch, 0.5f);
    sanitized.macroDepth = sanitizeParameterValue(kMacroDepth, sanitized.macroDepth, 0.3f);
    sanitized.modWheelTarget = static_cast<int>(std::round(sanitizeParameterValue(
        kModWheelTarget, static_cast<float>(sanitized.modWheelTarget), 1.0f)));
    sanitized.pitchBendRange = sanitizeParameterValue(kPitchBendRange, sanitized.pitchBendRange, 2.0f);
    return sanitized;
}

mbs::PatchPerformanceSettings BassSynthAudioProcessor::snapshotPerformanceSettings() const
{
    mbs::PatchPerformanceSettings settings;
    settings.monoMode = juce::jlimit(0, 2, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.monoMode))));
    settings.lfoRate = readCachedParamValue(globalParamRefs.lfoRate, settings.lfoRate);
    settings.lfoDepth = readCachedParamValue(globalParamRefs.lfoDepth, settings.lfoDepth);
    settings.lfoWave = juce::jlimit(0, 3, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.lfoWave))));
    settings.lfoDest = juce::jlimit(0, 2, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.lfoDest))));
    settings.macroFatness = readCachedParamValue(globalParamRefs.macroFatness, settings.macroFatness);
    settings.macroBrillance = readCachedParamValue(globalParamRefs.macroBrillance, settings.macroBrillance);
    settings.macroPunch = readCachedParamValue(globalParamRefs.macroPunch, settings.macroPunch);
    settings.macroDepth = readCachedParamValue(globalParamRefs.macroDepth, settings.macroDepth);
    settings.modWheelTarget = juce::jlimit(0, 1, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.modWheelTarget, 1.0f))));
    settings.pitchBendRange = readCachedParamValue(globalParamRefs.pitchBendRange, settings.pitchBendRange);
    return sanitizePerformanceSettings(settings);
}

modmatrix::MatrixState BassSynthAudioProcessor::captureModMatrixStateFromParams() const
{
    modmatrix::MatrixState state;
    state.pitchBendRange = juce::jlimit(
        1, 24, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.pitchBendRange, 2.0f))));
    state.lfo2Rate = sanitizeParameterValue(
        kModLfo2Rate,
        readCachedParamValue(globalParamRefs.modLfo2Rate, state.lfo2Rate),
        2.0f);
    state.lfo2Wave = juce::jlimit(
        0, 3, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.modLfo2Wave, 0.0f))));
    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        const auto& refs = modMatrixParamRefs[static_cast<std::size_t>(slotIndex)];
        state.slots[static_cast<std::size_t>(slotIndex)].source = static_cast<modmatrix::Source>(
            juce::jlimit(0, modmatrix::kSourceCount - 1, static_cast<int>(std::round(readCachedParamValue(refs.source)))));
        state.slots[static_cast<std::size_t>(slotIndex)].destination = static_cast<modmatrix::Destination>(
            juce::jlimit(0, modmatrix::kDestCount - 1, static_cast<int>(std::round(readCachedParamValue(refs.destination)))));
        state.slots[static_cast<std::size_t>(slotIndex)].amount = sanitizeParameterValue(
            makeModMatrixParamId(slotIndex, "amount"),
            readCachedParamValue(refs.amount),
            0.0f);
    }
    return state;
}

void BassSynthAudioProcessor::applyPerformanceSettings(const mbs::PatchPerformanceSettings& settings, bool notifyHost)
{
    const auto sanitized = sanitizePerformanceSettings(settings);
    setParamValueInternal(kMonoMode, static_cast<float>(sanitized.monoMode), notifyHost);
    setParamValueInternal(kLfoRate, sanitized.lfoRate, notifyHost);
    setParamValueInternal(kLfoDepth, sanitized.lfoDepth, notifyHost);
    setParamValueInternal(kLfoWave, static_cast<float>(sanitized.lfoWave), notifyHost);
    setParamValueInternal(kLfoDest, static_cast<float>(sanitized.lfoDest), notifyHost);
    setParamValueInternal(kMacroFatness, sanitized.macroFatness, notifyHost);
    setParamValueInternal(kMacroBrillance, sanitized.macroBrillance, notifyHost);
    setParamValueInternal(kMacroPunch, sanitized.macroPunch, notifyHost);
    setParamValueInternal(kMacroDepth, sanitized.macroDepth, notifyHost);
    setParamValueInternal(kModWheelTarget, static_cast<float>(sanitized.modWheelTarget), notifyHost);
    setParamValueInternal(kPitchBendRange, sanitized.pitchBendRange, notifyHost);
}

void BassSynthAudioProcessor::applyModMatrixStateToParams(const modmatrix::MatrixState& state, bool notifyHost)
{
    setParamValueInternal(kPitchBendRange, static_cast<float>(juce::jlimit(1, 24, state.pitchBendRange)), notifyHost);
    setParamValueInternal(kModLfo2Rate, sanitizeParameterValue(kModLfo2Rate, state.lfo2Rate, 2.0f), notifyHost);
    setParamValueInternal(kModLfo2Wave, static_cast<float>(juce::jlimit(0, 3, state.lfo2Wave)), notifyHost);
    modulationMatrix.lfo2.setRate(sanitizeParameterValue(kModLfo2Rate, state.lfo2Rate, 2.0f));
    modulationMatrix.lfo2.setWave(juce::jlimit(0, 3, state.lfo2Wave));
    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        const auto& slot = state.slots[static_cast<std::size_t>(slotIndex)];
        setParamValueInternal(makeModMatrixParamId(slotIndex, "source"), static_cast<float>(static_cast<int>(slot.source)), notifyHost);
        setParamValueInternal(makeModMatrixParamId(slotIndex, "dest"), static_cast<float>(static_cast<int>(slot.destination)), notifyHost);
        setParamValueInternal(makeModMatrixParamId(slotIndex, "amount"), slot.amount, notifyHost);
    }
}

void BassSynthAudioProcessor::syncModMatrixFromParams()
{
    modulationMatrix.pitchBendRange.store(
        juce::jlimit(1, 24, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.pitchBendRange, 2.0f)))),
        std::memory_order_relaxed);
    modulationMatrix.lfo2.setRate(sanitizeParameterValue(
        kModLfo2Rate,
        readCachedParamValue(globalParamRefs.modLfo2Rate, 2.0f),
        2.0f));
    modulationMatrix.lfo2.setWave(juce::jlimit(
        0, 3, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.modLfo2Wave, 0.0f)))));
    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        const auto& refs = modMatrixParamRefs[static_cast<std::size_t>(slotIndex)];
        const auto source = static_cast<modmatrix::Source>(
            juce::jlimit(0, modmatrix::kSourceCount - 1, static_cast<int>(std::round(readCachedParamValue(refs.source)))));
        const auto destination = static_cast<modmatrix::Destination>(
            juce::jlimit(0, modmatrix::kDestCount - 1, static_cast<int>(std::round(readCachedParamValue(refs.destination)))));
        const auto amount = sanitizeParameterValue(
            makeModMatrixParamId(slotIndex, "amount"),
            readCachedParamValue(refs.amount),
            0.0f);
        modulationMatrix.setSlot(slotIndex, source, destination, amount);
    }
}

mbs::BassSettings BassSynthAudioProcessor::snapshotBassSettings(int bassIndex) const
{
    const auto& refs = bassParamRefs[static_cast<std::size_t>(bassIndex)];
    mbs::BassSettings settings;
    settings.level = readCachedParamValue(refs.level, settings.level);
    settings.tuneSemitones = readCachedParamValue(refs.tune, settings.tuneSemitones);
    settings.brightness = readCachedParamValue(refs.brightness, settings.brightness);
    settings.attackSeconds = readCachedParamValue(refs.attack, settings.attackSeconds);
    settings.decaySeconds = readCachedParamValue(refs.decay, settings.decaySeconds);
    settings.sustainLevel = readCachedParamValue(refs.sustain, settings.sustainLevel);
    settings.releaseSeconds = readCachedParamValue(refs.release, settings.releaseSeconds);
    settings.body = readCachedParamValue(refs.body, settings.body);
    settings.drive = readCachedParamValue(refs.drive, settings.drive);
    settings.pitchEnv = readCachedParamValue(refs.pitchEnv, settings.pitchEnv);
    settings.filterEnv = readCachedParamValue(refs.filterEnv, settings.filterEnv);
    settings.subLevel = readCachedParamValue(refs.sub, settings.subLevel);
    settings.character = readCachedParamValue(refs.character, settings.character);
    settings.cutoffHz = readCachedParamValue(refs.cutoff, settings.cutoffHz);
    settings.pan = readCachedParamValue(refs.pan, settings.pan);
    settings.resonance = readCachedParamValue(refs.resonance, settings.resonance);
    settings.glideTime = readCachedParamValue(globalParamRefs.glideTime, settings.glideTime);

    settings = sanitizeBassSettings(bassIndex, settings);
    applyPerformanceMacros(bassIndex, settings);
    return settings;
}

void BassSynthAudioProcessor::applyBassSettingsToParams(int bassIndex, const mbs::BassSettings& settings, bool notifyHost)
{
    const auto sanitized = sanitizeBassSettings(bassIndex, settings);
    setParamValueInternal(makeBassParamId(bassIndex, "level"), sanitized.level, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "tune"), sanitized.tuneSemitones, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "brightness"), sanitized.brightness, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "attack"), sanitized.attackSeconds, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "decay"), sanitized.decaySeconds, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "sustain"), sanitized.sustainLevel, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "release"), sanitized.releaseSeconds, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "body"), sanitized.body, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "drive"), sanitized.drive, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "pitch_env"), sanitized.pitchEnv, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "filter_env"), sanitized.filterEnv, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "sub"), sanitized.subLevel, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "character"), sanitized.character, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "cutoff"), sanitized.cutoffHz, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "pan"), sanitized.pan, notifyHost);
    setParamValueInternal(makeBassParamId(bassIndex, "resonance"), sanitized.resonance, notifyHost);
    setParamValueInternal(kGlideTime, sanitized.glideTime, notifyHost);
}

void BassSynthAudioProcessor::sanitizeAllParameters()
{
    setParamValueInternal(kSelectedBass, sanitizeParameterValue(kSelectedBass,
        readCachedParamValue(globalParamRefs.selectedBass), 0.0f), false);
    setParamValueInternal(kMonoMode, sanitizeParameterValue(kMonoMode,
        readCachedParamValue(globalParamRefs.monoMode), 0.0f), false);
    setParamValueInternal(kGlideTime, sanitizeParameterValue(kGlideTime,
        readCachedParamValue(globalParamRefs.glideTime), 0.0f), false);
    setParamValueInternal(kLfoRate, sanitizeParameterValue(kLfoRate,
        readCachedParamValue(globalParamRefs.lfoRate), 2.0f), false);
    setParamValueInternal(kLfoDepth, sanitizeParameterValue(kLfoDepth,
        readCachedParamValue(globalParamRefs.lfoDepth), 0.0f), false);
    setParamValueInternal(kLfoWave, sanitizeParameterValue(kLfoWave,
        readCachedParamValue(globalParamRefs.lfoWave), 0.0f), false);
    setParamValueInternal(kLfoDest, sanitizeParameterValue(kLfoDest,
        readCachedParamValue(globalParamRefs.lfoDest), 0.0f), false);
    setParamValueInternal(kOutputGain, sanitizeParameterValue(kOutputGain,
        readCachedParamValue(globalParamRefs.outputGain), 0.0f), false);
    setParamValueInternal(kMacroFatness, sanitizeParameterValue(kMacroFatness,
        readCachedParamValue(globalParamRefs.macroFatness), 0.5f), false);
    setParamValueInternal(kMacroBrillance, sanitizeParameterValue(kMacroBrillance,
        readCachedParamValue(globalParamRefs.macroBrillance), 0.5f), false);
    setParamValueInternal(kMacroPunch, sanitizeParameterValue(kMacroPunch,
        readCachedParamValue(globalParamRefs.macroPunch), 0.5f), false);
    setParamValueInternal(kMacroDepth, sanitizeParameterValue(kMacroDepth,
        readCachedParamValue(globalParamRefs.macroDepth), 0.5f), false);
    setParamValueInternal(kModWheelTarget, sanitizeParameterValue(kModWheelTarget,
        readCachedParamValue(globalParamRefs.modWheelTarget), 1.0f), false);
    setParamValueInternal(kVelocityCurve, sanitizeParameterValue(kVelocityCurve,
        readCachedParamValue(globalParamRefs.velocityCurveParam), 0.0f), false);
    setParamValueInternal(kPitchBendRange, sanitizeParameterValue(kPitchBendRange,
        readCachedParamValue(globalParamRefs.pitchBendRange), 2.0f), false);
    setParamValueInternal(kModLfo2Rate, sanitizeParameterValue(kModLfo2Rate,
        readCachedParamValue(globalParamRefs.modLfo2Rate), 2.0f), false);
    setParamValueInternal(kModLfo2Wave, sanitizeParameterValue(kModLfo2Wave,
        readCachedParamValue(globalParamRefs.modLfo2Wave), 0.0f), false);
    setParamValueInternal(kFxLock, sanitizeParameterValue(kFxLock,
        readCachedParamValue(globalParamRefs.fxLock), 0.0f), false);

    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        const auto& refs = bassParamRefs[static_cast<std::size_t>(bassIndex)];
        setParamValueInternal(makeBassParamId(bassIndex, "level"), sanitizeParameterValue(makeBassParamId(bassIndex, "level"), readCachedParamValue(refs.level), mbs::getDefaultSettings(bassIndex).level), false);
        setParamValueInternal(makeBassParamId(bassIndex, "tune"), sanitizeParameterValue(makeBassParamId(bassIndex, "tune"), readCachedParamValue(refs.tune), mbs::getDefaultSettings(bassIndex).tuneSemitones), false);
        setParamValueInternal(makeBassParamId(bassIndex, "brightness"), sanitizeParameterValue(makeBassParamId(bassIndex, "brightness"), readCachedParamValue(refs.brightness), mbs::getDefaultSettings(bassIndex).brightness), false);
        setParamValueInternal(makeBassParamId(bassIndex, "attack"), sanitizeParameterValue(makeBassParamId(bassIndex, "attack"), readCachedParamValue(refs.attack), mbs::getDefaultSettings(bassIndex).attackSeconds), false);
        setParamValueInternal(makeBassParamId(bassIndex, "decay"), sanitizeParameterValue(makeBassParamId(bassIndex, "decay"), readCachedParamValue(refs.decay), mbs::getDefaultSettings(bassIndex).decaySeconds), false);
        setParamValueInternal(makeBassParamId(bassIndex, "sustain"), sanitizeParameterValue(makeBassParamId(bassIndex, "sustain"), readCachedParamValue(refs.sustain), mbs::getDefaultSettings(bassIndex).sustainLevel), false);
        setParamValueInternal(makeBassParamId(bassIndex, "release"), sanitizeParameterValue(makeBassParamId(bassIndex, "release"), readCachedParamValue(refs.release), mbs::getDefaultSettings(bassIndex).releaseSeconds), false);
        setParamValueInternal(makeBassParamId(bassIndex, "body"), sanitizeParameterValue(makeBassParamId(bassIndex, "body"), readCachedParamValue(refs.body), mbs::getDefaultSettings(bassIndex).body), false);
        setParamValueInternal(makeBassParamId(bassIndex, "drive"), sanitizeParameterValue(makeBassParamId(bassIndex, "drive"), readCachedParamValue(refs.drive), mbs::getDefaultSettings(bassIndex).drive), false);
        setParamValueInternal(makeBassParamId(bassIndex, "pitch_env"), sanitizeParameterValue(makeBassParamId(bassIndex, "pitch_env"), readCachedParamValue(refs.pitchEnv), mbs::getDefaultSettings(bassIndex).pitchEnv), false);
        setParamValueInternal(makeBassParamId(bassIndex, "filter_env"), sanitizeParameterValue(makeBassParamId(bassIndex, "filter_env"), readCachedParamValue(refs.filterEnv), mbs::getDefaultSettings(bassIndex).filterEnv), false);
        setParamValueInternal(makeBassParamId(bassIndex, "sub"), sanitizeParameterValue(makeBassParamId(bassIndex, "sub"), readCachedParamValue(refs.sub), mbs::getDefaultSettings(bassIndex).subLevel), false);
        setParamValueInternal(makeBassParamId(bassIndex, "character"), sanitizeParameterValue(makeBassParamId(bassIndex, "character"), readCachedParamValue(refs.character), mbs::getDefaultSettings(bassIndex).character), false);
        setParamValueInternal(makeBassParamId(bassIndex, "cutoff"), sanitizeParameterValue(makeBassParamId(bassIndex, "cutoff"), readCachedParamValue(refs.cutoff), mbs::getDefaultSettings(bassIndex).cutoffHz), false);
        setParamValueInternal(makeBassParamId(bassIndex, "pan"), sanitizeParameterValue(makeBassParamId(bassIndex, "pan"), readCachedParamValue(refs.pan), mbs::getDefaultSettings(bassIndex).pan), false);
        setParamValueInternal(makeBassParamId(bassIndex, "resonance"), sanitizeParameterValue(makeBassParamId(bassIndex, "resonance"), readCachedParamValue(refs.resonance), mbs::getDefaultSettings(bassIndex).resonance), false);
        setParamValueInternal(makeBassParamId(bassIndex, kBassOutputSuffix), sanitizeParameterValue(makeBassParamId(bassIndex, kBassOutputSuffix), readCachedParamValue(refs.output), 0.0f), false);
        outputBusCache[static_cast<std::size_t>(bassIndex)] = captureBassOutputBus(bassIndex);
    }

    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        const auto& refs = modMatrixParamRefs[static_cast<std::size_t>(slotIndex)];
        setParamValueInternal(makeModMatrixParamId(slotIndex, "source"), sanitizeParameterValue(
            makeModMatrixParamId(slotIndex, "source"), readCachedParamValue(refs.source), 0.0f), false);
        setParamValueInternal(makeModMatrixParamId(slotIndex, "dest"), sanitizeParameterValue(
            makeModMatrixParamId(slotIndex, "dest"), readCachedParamValue(refs.destination), 0.0f), false);
        setParamValueInternal(makeModMatrixParamId(slotIndex, "amount"), sanitizeParameterValue(
            makeModMatrixParamId(slotIndex, "amount"), readCachedParamValue(refs.amount), 0.0f), false);
    }

    applyGlobalFxSettings(sanitizeFxSettings(snapshotGlobalFxSettings()), false);
    syncModMatrixFromParams();
}

// =========================================================================
// MIDI CC page-based mapping  (Novation FLkey Mini – full control)
//
// 8 knobs (CC 21-28) are paged across 5 pages to control ALL parameters.
// CC 1 (mod wheel / touch-strip) controls macro_punch when mod_wheel_target != Off.
// CC 102 / 103  = previous / next page  (assignable to < > buttons).
// CC 44-48      = direct page select    (assignable to pads in CC mode).
// Each CC value (0-127) is normalised to the parameter's full range.
// Per-bass parameters follow the currently selected bass index.
// =========================================================================

namespace
{
    struct CCSlot {
        const char* paramId;        // global param, or nullptr for per-bass
        const char* bassSuffix;     // per-bass suffix (when paramId == nullptr)
    };

    static constexpr int kKnobsPerPage = 8;

    static const char* kCCPageNames[] = {
        "MACROS",       // 0
        "ENVELOPE",     // 1
        "TONE",         // 2
        "COMP/SAT",     // 3
        "EQ",           // 4
        "CHORUS/TRANS",  // 5
        "DELAY/REVERB"  // 6
    };

    // 7 pages x 8 knobs – covers every tweakable parameter
    static const CCSlot kCCPages[][kKnobsPerPage] = {
        // Page 0 — Macros & Master
        { { "macro_grosseur",  nullptr }, { "macro_brillance", nullptr },
          { "macro_punch",     nullptr }, { "macro_profondeur",nullptr },
          { "lfo_rate",        nullptr }, { "lfo_depth",       nullptr },
          { "reverb_mix",      nullptr }, { "output_gain",     nullptr } },

        // Page 1 — Envelope (per-bass)
        { { nullptr, "attack" },    { nullptr, "decay"  },
          { nullptr, "sustain" },   { nullptr, "release" },
          { nullptr, "level" },     { nullptr, "tune" },
          { nullptr, "pitch_env" }, { nullptr, "sub" } },

        // Page 2 — Tone (per-bass)
        { { nullptr, "brightness" }, { nullptr, "body" },
          { nullptr, "drive" },      { nullptr, "character" },
          { nullptr, "cutoff" },     { nullptr, "resonance" },
          { nullptr, "pan" },        { "output_gain", nullptr } },

        // Page 3 — Compressor & Saturator
        { { "comp_threshold", nullptr }, { "comp_ratio",   nullptr },
          { "comp_attack",    nullptr }, { "comp_release", nullptr },
          { "comp_makeup",    nullptr }, { "comp_mix",     nullptr },
          { "sat_drive",      nullptr }, { "sat_mix",      nullptr } },

        // Page 4 — EQ
        { { "eq_low_freq",   nullptr }, { "eq_low_gain",  nullptr },
          { "eq_mid_freq",   nullptr }, { "eq_mid_gain",  nullptr },
          { "eq_mid_q",      nullptr }, { "eq_high_freq", nullptr },
          { "eq_high_gain",  nullptr }, { "output_gain",  nullptr } },

        // Page 5 — Chorus & Transient
        { { "chorus_rate",       nullptr }, { "chorus_depth",       nullptr },
          { "chorus_mix",        nullptr }, { "transient_attack",   nullptr },
          { "transient_sustain", nullptr }, { "transient_mix",      nullptr },
          { "limiter_threshold", nullptr }, { "limiter_release",    nullptr } },

        // Page 6 — Delay & Reverb
        { { "delay_time",     nullptr }, { "delay_feedback", nullptr },
          { "delay_mix",      nullptr }, { "reverb_size",    nullptr },
          { "reverb_damping", nullptr }, { "reverb_width",   nullptr },
          { "reverb_mix",     nullptr }, { "output_gain",    nullptr } }
    };
}

const char* BassSynthAudioProcessor::getCCPageName(int page) noexcept
{
    if (page >= 0 && page < kNumCCPages)
        return kCCPageNames[page];
    return "???";
}

void BassSynthAudioProcessor::handleMidiCC(int ccNumber, int ccValue, int bassIndex)
{
    auto resolveGlobalParameter = [this](const char* paramId) -> juce::RangedAudioParameter*
    {
        if (std::strcmp(paramId, kMacroFatness) == 0 || std::strcmp(paramId, "macro_grosseur") == 0)
            return globalParamRefs.macroFatness.ranged;
        if (std::strcmp(paramId, kMacroBrillance) == 0)
            return globalParamRefs.macroBrillance.ranged;
        if (std::strcmp(paramId, kMacroPunch) == 0)
            return globalParamRefs.macroPunch.ranged;
        if (std::strcmp(paramId, kMacroDepth) == 0 || std::strcmp(paramId, "macro_profondeur") == 0)
            return globalParamRefs.macroDepth.ranged;
        if (std::strcmp(paramId, kLfoRate) == 0)
            return globalParamRefs.lfoRate.ranged;
        if (std::strcmp(paramId, kLfoDepth) == 0)
            return globalParamRefs.lfoDepth.ranged;
        if (std::strcmp(paramId, kReverbMix) == 0)
            return globalParamRefs.reverbMix.ranged;
        if (std::strcmp(paramId, kOutputGain) == 0)
            return globalParamRefs.outputGain.ranged;
        if (std::strcmp(paramId, kCompThreshold) == 0)
            return globalParamRefs.compThreshold.ranged;
        if (std::strcmp(paramId, kCompRatio) == 0)
            return globalParamRefs.compRatio.ranged;
        if (std::strcmp(paramId, kCompAttack) == 0)
            return globalParamRefs.compAttack.ranged;
        if (std::strcmp(paramId, kCompRelease) == 0)
            return globalParamRefs.compRelease.ranged;
        if (std::strcmp(paramId, kCompMakeup) == 0)
            return globalParamRefs.compMakeup.ranged;
        if (std::strcmp(paramId, kCompMix) == 0)
            return globalParamRefs.compMix.ranged;
        if (std::strcmp(paramId, kSatDrive) == 0)
            return globalParamRefs.satDrive.ranged;
        if (std::strcmp(paramId, kSatMix) == 0)
            return globalParamRefs.satMix.ranged;
        if (std::strcmp(paramId, kEqLowFreq) == 0)
            return globalParamRefs.eqLowFreq.ranged;
        if (std::strcmp(paramId, kEqLowGain) == 0)
            return globalParamRefs.eqLowGain.ranged;
        if (std::strcmp(paramId, kEqMidFreq) == 0)
            return globalParamRefs.eqMidFreq.ranged;
        if (std::strcmp(paramId, kEqMidGain) == 0)
            return globalParamRefs.eqMidGain.ranged;
        if (std::strcmp(paramId, kEqMidQ) == 0)
            return globalParamRefs.eqMidQ.ranged;
        if (std::strcmp(paramId, kEqHighFreq) == 0)
            return globalParamRefs.eqHighFreq.ranged;
        if (std::strcmp(paramId, kEqHighGain) == 0)
            return globalParamRefs.eqHighGain.ranged;
        if (std::strcmp(paramId, kChorusRate) == 0)
            return globalParamRefs.chorusRate.ranged;
        if (std::strcmp(paramId, kChorusDepth) == 0)
            return globalParamRefs.chorusDepth.ranged;
        if (std::strcmp(paramId, kChorusMix) == 0)
            return globalParamRefs.chorusMix.ranged;
        if (std::strcmp(paramId, kTransientAttack) == 0)
            return globalParamRefs.transientAttack.ranged;
        if (std::strcmp(paramId, kTransientSustain) == 0)
            return globalParamRefs.transientSustain.ranged;
        if (std::strcmp(paramId, kTransientMix) == 0)
            return globalParamRefs.transientMix.ranged;
        if (std::strcmp(paramId, kLimiterThreshold) == 0)
            return globalParamRefs.limiterThreshold.ranged;
        if (std::strcmp(paramId, kLimiterRelease) == 0)
            return globalParamRefs.limiterRelease.ranged;
        if (std::strcmp(paramId, kDelayTime) == 0)
            return globalParamRefs.delayTime.ranged;
        if (std::strcmp(paramId, kDelayFeedback) == 0)
            return globalParamRefs.delayFeedback.ranged;
        if (std::strcmp(paramId, kDelayMix) == 0)
            return globalParamRefs.delayMix.ranged;
        if (std::strcmp(paramId, kReverbSize) == 0)
            return globalParamRefs.reverbSize.ranged;
        if (std::strcmp(paramId, kReverbDamping) == 0)
            return globalParamRefs.reverbDamping.ranged;
        if (std::strcmp(paramId, kReverbWidth) == 0)
            return globalParamRefs.reverbWidth.ranged;
        return nullptr;
    };

    auto resolveBassParameter = [this, bassIndex](const char* bassSuffix) -> juce::RangedAudioParameter*
    {
        const auto& refs = bassParamRefs[static_cast<std::size_t>(juce::jlimit(0, mbs::kNumBasses - 1, bassIndex))];
        if (std::strcmp(bassSuffix, "attack") == 0) return refs.attack.ranged;
        if (std::strcmp(bassSuffix, "decay") == 0) return refs.decay.ranged;
        if (std::strcmp(bassSuffix, "sustain") == 0) return refs.sustain.ranged;
        if (std::strcmp(bassSuffix, "release") == 0) return refs.release.ranged;
        if (std::strcmp(bassSuffix, "level") == 0) return refs.level.ranged;
        if (std::strcmp(bassSuffix, "tune") == 0) return refs.tune.ranged;
        if (std::strcmp(bassSuffix, "pitch_env") == 0) return refs.pitchEnv.ranged;
        if (std::strcmp(bassSuffix, "sub") == 0) return refs.sub.ranged;
        if (std::strcmp(bassSuffix, "brightness") == 0) return refs.brightness.ranged;
        if (std::strcmp(bassSuffix, "body") == 0) return refs.body.ranged;
        if (std::strcmp(bassSuffix, "drive") == 0) return refs.drive.ranged;
        if (std::strcmp(bassSuffix, "character") == 0) return refs.character.ranged;
        if (std::strcmp(bassSuffix, "cutoff") == 0) return refs.cutoff.ranged;
        if (std::strcmp(bassSuffix, "resonance") == 0) return refs.resonance.ranged;
        if (std::strcmp(bassSuffix, "pan") == 0) return refs.pan.ranged;
        return nullptr;
    };

    // --- Mod wheel: controls punch macro (if target enabled) ---
    if (ccNumber == 1)
    {
        // Only map to macro if mod_wheel_target != Off (index 0)
        const float targetVal = globalParamRefs.modWheelTarget.raw
            ? globalParamRefs.modWheelTarget.raw->load(std::memory_order_relaxed)
            : 1.0f;
        if (targetVal >= 0.5f)
        {
            if (auto* param = globalParamRefs.macroPunch.ranged)
                queueParamUpdate(param, static_cast<float>(ccValue) / 127.0f);
        }
        return;
    }

    // --- Page navigation: CC 102 = prev, CC 103 = next ---
    if (ccNumber == 102 && ccValue > 0)
    {
        int p = midiCCPage.load(std::memory_order_relaxed);
        midiCCPage.store((p + kNumCCPages - 1) % kNumCCPages, std::memory_order_relaxed);
        return;
    }
    if (ccNumber == 103 && ccValue > 0)
    {
        int p = midiCCPage.load(std::memory_order_relaxed);
        midiCCPage.store((p + 1) % kNumCCPages, std::memory_order_relaxed);
        return;
    }

    // --- Direct page select via pads: CC 44-50 ---
    if (ccNumber >= 44 && ccNumber <= 50 && ccValue > 0)
    {
        midiCCPage.store(ccNumber - 44, std::memory_order_relaxed);
        return;
    }

    // --- Knob mapping: CC 21-28 → paged parameters ---
    if (ccNumber < 21 || ccNumber > 28)
        return;

    const int knobIndex = ccNumber - 21;
    const int page = midiCCPage.load(std::memory_order_relaxed);
    const auto& slot = kCCPages[page][knobIndex];

    juce::RangedAudioParameter* param = nullptr;
    if (slot.paramId != nullptr)
        param = resolveGlobalParameter(slot.paramId);
    else if (slot.bassSuffix != nullptr)
        param = resolveBassParameter(slot.bassSuffix);

    if (param != nullptr)
        queueParamUpdate(param, static_cast<float>(ccValue) / 127.0f);
}

// =============================================================================
// RT-safe deferred parameter update — called from audio thread (handleMidiCC)
// =============================================================================
void BassSynthAudioProcessor::queueParamUpdate(juce::RangedAudioParameter* param, float normalisedValue)
{
    int start1, size1, start2, size2;
    pendingParamFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0)
    {
        pendingParamQueue[static_cast<std::size_t>(start1)] = { param, normalisedValue };
        pendingParamFifo.finishedWrite(1);
    }
    triggerAsyncUpdate();
}

// =============================================================================
// Deferred updates (message thread) — MIDI CC parameter updates
// =============================================================================
void BassSynthAudioProcessor::handleAsyncUpdate()
{
    const int numReady = pendingParamFifo.getNumReady();
    int start1, size1, start2, size2;
    pendingParamFifo.prepareToRead(numReady, start1, size1, start2, size2);
    for (int i = 0; i < size1; ++i)
    {
        auto& entry = pendingParamQueue[static_cast<std::size_t>(start1 + i)];
        if (entry.param != nullptr)
            entry.param->setValueNotifyingHost(entry.normalisedValue);
    }
    for (int i = 0; i < size2; ++i)
    {
        auto& entry = pendingParamQueue[static_cast<std::size_t>(start2 + i)];
        if (entry.param != nullptr)
            entry.param->setValueNotifyingHost(entry.normalisedValue);
    }
    pendingParamFifo.finishedRead(size1 + size2);
}

void BassSynthAudioProcessor::applyPerformanceMacros(int bassIndex, mbs::BassSettings& s) const
{
    const auto fatness   = (readCachedParamValue(globalParamRefs.macroFatness, 0.5f)   - 0.5f) * 2.0f;
    const auto brillance = (readCachedParamValue(globalParamRefs.macroBrillance, 0.5f) - 0.5f) * 2.0f;
    const auto punch     = (readCachedParamValue(globalParamRefs.macroPunch, 0.5f)     - 0.5f) * 2.0f;
    const auto depth     = (readCachedParamValue(globalParamRefs.macroDepth, 0.5f)     - 0.5f) * 2.0f;

    const auto family = mbs::getFamily(bassIndex);
    const bool is808Family = family == mbs::Family::Eight08;
    const bool isDistorted808 = bassIndex == 5;

    if (is808Family)
    {
        // 808 macros must stay mono-safe and rhythm readable.
        s.subLevel = clamp01(s.subLevel + fatness * 0.12f + depth * 0.05f);
        s.body = clamp01(s.body + fatness * 0.05f);
        s.cutoffHz = juce::jlimit(110.0f, 4500.0f, s.cutoffHz * std::pow(2.0f, -fatness * 0.10f));

        s.brightness = clamp01(s.brightness + brillance * 0.10f);
        s.cutoffHz = juce::jlimit(110.0f, 4500.0f, s.cutoffHz * std::pow(2.0f, brillance * 0.18f + depth * 0.04f));

        s.attackSeconds = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - punch * 0.28f));
        s.pitchEnv = clamp01(s.pitchEnv + punch * 0.10f);
        s.decaySeconds = juce::jlimit(0.08f, 4.5f, s.decaySeconds * (1.0f + depth * 0.18f - punch * 0.10f));
        s.releaseSeconds = juce::jlimit(0.03f, 1.2f, s.releaseSeconds * (1.0f + depth * 0.18f - punch * 0.06f));

        if (isDistorted808)
            s.drive = clamp01(s.drive + brillance * 0.07f + punch * 0.07f);
        else
            s.drive = clamp01(s.drive + brillance * 0.03f + punch * 0.03f);

        return;
    }

    if (family == mbs::Family::Acoustic)
    {
        // Acoustic macros should enhance wood/body before fake sub.
        s.body = clamp01(s.body + fatness * 0.18f + depth * 0.06f);
        s.subLevel = clamp01(s.subLevel + fatness * 0.05f + depth * 0.02f);
        s.cutoffHz = juce::jlimit(120.0f, 9000.0f, s.cutoffHz * std::pow(2.0f, -fatness * 0.14f + brillance * 0.26f));
        s.brightness = clamp01(s.brightness + brillance * 0.16f + punch * 0.04f);
        s.attackSeconds = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - punch * 0.24f));
        s.decaySeconds = juce::jlimit(0.1f, 8.0f, s.decaySeconds * (1.0f - punch * 0.10f + depth * 0.14f));
        s.releaseSeconds = juce::jlimit(0.03f, 2.5f, s.releaseSeconds * (1.0f + depth * 0.14f));
        s.drive = clamp01(s.drive + brillance * 0.02f + punch * 0.03f);
        s.subLevel = juce::jmin(s.subLevel, 0.68f);
        return;
    }

    // Synth macros should prioritize harmonic identity and note definition.
    s.drive = clamp01(s.drive + fatness * 0.14f + punch * 0.06f);
    s.character = clamp01(s.character + fatness * 0.08f + depth * 0.05f + brillance * 0.04f);
    s.subLevel = clamp01(s.subLevel + fatness * 0.06f + depth * 0.04f);
    s.brightness = clamp01(s.brightness + brillance * 0.16f);
    s.filterEnv = clamp01(s.filterEnv + brillance * 0.10f + punch * 0.08f);
    s.cutoffHz = juce::jlimit(120.0f, 12000.0f, s.cutoffHz * std::pow(2.0f, -fatness * 0.08f + brillance * 0.32f));
    s.attackSeconds = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - punch * 0.26f));
    s.decaySeconds = juce::jlimit(0.1f, 8.0f, s.decaySeconds * (1.0f - punch * 0.08f + depth * 0.16f));
    s.releaseSeconds = juce::jlimit(0.03f, 3.0f, s.releaseSeconds * (1.0f + depth * 0.18f));
    s.body = clamp01(s.body + depth * 0.04f);
}

#if defined(UWDEVST_BASS_TEST_BUILD)
mbs::BassSettings BassSynthAudioProcessor::snapshotMacroAppliedSettingsForTests(int bassIndex) const
{
    return snapshotBassSettings(bassIndex);
}
#endif

int BassSynthAudioProcessor::findFreeVoice() const
{
    for (int i = 0; i < kMaxVoices; ++i)
        if (!voices[static_cast<std::size_t>(i)].voice.isActive())
            return i;

    int oldest = 0;
    uint64_t oldestAge = UINT64_MAX;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[static_cast<std::size_t>(i)].voice.isReleasing()
            && voices[static_cast<std::size_t>(i)].activationAge < oldestAge)
        {
            oldest = i;
            oldestAge = voices[static_cast<std::size_t>(i)].activationAge;
        }
    }
    if (oldestAge < UINT64_MAX) return oldest;

    oldestAge = UINT64_MAX;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[static_cast<std::size_t>(i)].activationAge < oldestAge)
        {
            oldest = i;
            oldestAge = voices[static_cast<std::size_t>(i)].activationAge;
        }
    }
    return oldest;
}

void BassSynthAudioProcessor::triggerNoteOn(int bassIndex, int midiNote, float velocity)
{
    if (bassIndex < 0 || bassIndex >= mbs::kNumBasses) return;
    if (preparedSampleRate <= 0.0) return;

    const int monoMode = juce::jlimit(0, 2,
        static_cast<int>(std::round(readCachedParamValue(globalParamRefs.monoMode))));

    auto settings = snapshotBassSettings(bassIndex);
    const auto& chars = mbs::getCharacteristics(bassIndex);
    const int outputBus = outputBusCache[static_cast<std::size_t>(bassIndex)];

    if (monoMode == 0)
    {
        // ---- Polyphonic mode ----
        const int slot = findFreeVoice();
        auto& v = voices[static_cast<std::size_t>(slot)];

        // If stealing an active voice, move it to the dying pool
        if (v.voice.isActive())
        {
            DyingVoiceSlot* target = nullptr;
            for (auto& dv : dyingVoices)
            {
                if (!dv.inUse) { target = &dv; break; }
            }
            if (target == nullptr)
            {
                // All dying slots full — evict the oldest
                target = &dyingVoices[0];
                for (auto& dv : dyingVoices)
                    if (dv.activationAge < target->activationAge)
                        target = &dv;
            }
            std::swap(target->voice, v.voice);
            target->voice.forceQuickRelease();
            target->outputBus = v.outputBus;
            target->velocity = v.velocity;
            target->inUse = true;
            target->activationAge = ++voiceAgeCounter;
        }

        v.midiNote = midiNote;
        v.bassIndex = bassIndex;
        v.outputBus = outputBus;
        for (int auxIndex = 0; auxIndex < kNumAuxOutputs; ++auxIndex)
        {
            const float gain = (outputBus == auxIndex + 1) ? 1.0f : 0.0f;
            v.auxSendGains[static_cast<std::size_t>(auxIndex)].setCurrentAndTargetValue(gain);
        }
        v.velocity = velocity;
        v.activationAge = ++voiceAgeCounter;
        v.voice.noteOn(settings, chars, midiNote, velocity, preparedSampleRate);
    }
    else
    {
        // ---- Mono / Legato mode ----
        auto& ms = monoStates[static_cast<std::size_t>(bassIndex)];
        ms.pushNote(midiNote);

        const float glideTime = readCachedParamValue(globalParamRefs.glideTime);

        if (ms.voiceSlot >= 0 && ms.voiceSlot < kMaxVoices
            && voices[static_cast<std::size_t>(ms.voiceSlot)].voice.isActive())
        {
            // Voice already playing — glide or retrigger
            auto& v = voices[static_cast<std::size_t>(ms.voiceSlot)];
            v.midiNote = midiNote;
            v.outputBus = outputBus;
            v.velocity = velocity;
            v.activationAge = ++voiceAgeCounter;

            if (monoMode == 2)
            {
                // Legato: glide pitch, don't retrigger envelope
                v.voice.glideToNote(midiNote, glideTime);
            }
            else
            {
                // Mono: retrigger envelope with glide
                settings.glideTime = glideTime;
                v.voice.retriggerWithGlide(settings, chars, midiNote, velocity, preparedSampleRate);
            }
        }
        else
        {
            // No active voice — fresh start
            const int slot = findFreeVoice();
            auto& v = voices[static_cast<std::size_t>(slot)];
            v.midiNote = midiNote;
            v.bassIndex = bassIndex;
            v.outputBus = outputBus;
            for (int auxIndex = 0; auxIndex < kNumAuxOutputs; ++auxIndex)
            {
                const float gain = (outputBus == auxIndex + 1) ? 1.0f : 0.0f;
                v.auxSendGains[static_cast<std::size_t>(auxIndex)].setCurrentAndTargetValue(gain);
            }
            v.velocity = velocity;
            v.activationAge = ++voiceAgeCounter;
            v.voice.noteOn(settings, chars, midiNote, velocity, preparedSampleRate);
            ms.voiceSlot = slot;
        }
    }
}

void BassSynthAudioProcessor::triggerNoteOff(int bassIndex, int midiNote)
{
    const int monoMode = juce::jlimit(0, 2,
        static_cast<int>(std::round(readCachedParamValue(globalParamRefs.monoMode))));

    if (monoMode == 0)
    {
        // ---- Polyphonic ----
        for (auto& slot : voices)
        {
            if (slot.voice.isActive() && !slot.voice.isReleasing() &&
                slot.midiNote == midiNote && slot.bassIndex == bassIndex)
            {
                slot.voice.noteOff();
            }
        }
    }
    else
    {
        // ---- Mono / Legato ----
        auto& ms = monoStates[static_cast<std::size_t>(bassIndex)];
        ms.removeNote(midiNote);

        if (ms.voiceSlot >= 0 && ms.voiceSlot < kMaxVoices
            && voices[static_cast<std::size_t>(ms.voiceSlot)].voice.isActive())
        {
            const int prevNote = ms.topNote();
            if (prevNote >= 0)
            {
                // Still have held notes — glide back to previous
                auto& v = voices[static_cast<std::size_t>(ms.voiceSlot)];
                v.midiNote = prevNote;
                const float glideTime = readCachedParamValue(globalParamRefs.glideTime);
                v.voice.glideToNote(prevNote, glideTime);
            }
            else
            {
                // No more held notes — release
                voices[static_cast<std::size_t>(ms.voiceSlot)].voice.noteOff();
                ms.voiceSlot = -1;
            }
        }
    }
}

// =============================================================================
void BassSynthAudioProcessor::panicAllVoices()
{
    pitchBend.reset();
    modulationMatrix.resetMidiSources();
    for (auto& ms : monoStates)
    {
        ms.heldCount = 0;
        ms.voiceSlot = -1;
    }
    for (auto& slot : voices)
    {
        slot.voice.forceStop();
        slot.midiNote = -1;
        slot.bassIndex = 0;
        slot.velocity = 0.0f;
        slot.activationAge = 0;
    }
    for (auto& slot : dyingVoices)
    {
        slot.inUse = false;
        slot.outputBus = 0;
        slot.velocity = 0.0f;
        slot.activationAge = 0;
    }
    resetGlobalTailState();
    activeVoiceCountAtomic.store(0, std::memory_order_relaxed);
}

void BassSynthAudioProcessor::releaseVoices(int midiChannel, bool immediate)
{
    juce::ignoreUnused(midiChannel);

    for (auto& ms : monoStates)
    {
        ms.heldCount = 0;
        ms.voiceSlot = -1;
    }

    for (auto& slot : voices)
    {
        if (!slot.voice.isActive())
            continue;

        if (immediate)
        {
            slot.voice.forceQuickRelease();
            slot.midiNote = -1;
            slot.bassIndex = 0;
            slot.velocity = 0.0f;
            slot.activationAge = 0;
        }
        else
        {
            slot.voice.forceStop();
            slot.midiNote = -1;
            slot.bassIndex = 0;
            slot.velocity = 0.0f;
            slot.activationAge = 0;
        }
    }

    if (!immediate)
        resetGlobalTailState();
}

void BassSynthAudioProcessor::resetGlobalTailState() noexcept
{
    compressor.reset();
    eqProcessor.reset();
    chorusProcessor.reset();
    delayProcessor.reset();
    reverbProcessor.reset();
    limiterProcessor.reset();
    satOversampling.reset();

    if (fxDryBuffer.getNumChannels() > 0) fxDryBuffer.clear();
    if (mainDryBuffer.getNumChannels() > 0) mainDryBuffer.clear();
    if (voiceRenderBuffer.getNumChannels() > 0) voiceRenderBuffer.clear();
    if (fxChunkBuffer.getNumChannels() > 0) fxChunkBuffer.clear();

    transientFastEnv = { 0.0f, 0.0f };
    transientSlowEnv = { 0.0f, 0.0f };
    lfoPhase = 0.0f;
    outputGainCurrent = juce::Decibels::decibelsToGain(readCachedParamValue(globalParamRefs.outputGain, -3.0f));

    for (int ch = 0; ch < 2; ++ch)
    {
        hpfX1[ch] = 0.0f;
        hpfX2[ch] = 0.0f;
        hpfY1[ch] = 0.0f;
        hpfY2[ch] = 0.0f;
    }
}

// =============================================================================
void BassSynthAudioProcessor::updateGlobalEffectParameters(const GlobalBlockState& blockState)
{
    const auto threshold = blockState.fx.compThreshold;
    const auto ratio     = blockState.fx.compRatio;
    const auto attack    = blockState.fx.compAttack;
    const auto release   = blockState.fx.compRelease;

    if (threshold != compCache.threshold) { compressor.setThreshold(threshold); compCache.threshold = threshold; }
    if (ratio     != compCache.ratio)     { compressor.setRatio(ratio);          compCache.ratio     = ratio; }
    if (attack    != compCache.attack)    { compressor.setAttack(attack);         compCache.attack    = attack; }
    if (release   != compCache.release)   { compressor.setRelease(release);       compCache.release   = release; }
}

void BassSynthAudioProcessor::processMasterFxChain(juce::AudioBuffer<float>& mainBuffer,
                                                   const GlobalBlockState& blockState)
{
    if (mainBuffer.getNumChannels() <= 0 || mainBuffer.getNumSamples() <= 0)
        return;

    setGlobalFxSmootherTargets(blockState);

    const int totalSamples = mainBuffer.getNumSamples();
    const float targetGain = juce::Decibels::decibelsToGain(blockState.outputGainDb);
    const float gainStep = totalSamples > 0
        ? (targetGain - outputGainCurrent) / static_cast<float>(totalSamples)
        : 0.0f;
    float chunkGainStart = outputGainCurrent;

    for (int chunkStart = 0; chunkStart < totalSamples; chunkStart += kFxControlChunkSize)
    {
        const int chunkSamples = juce::jmin(kFxControlChunkSize, totalSamples - chunkStart);
        fxChunkBuffer.setSize(mainBuffer.getNumChannels(), chunkSamples, false, false, true);

        for (int channel = 0; channel < mainBuffer.getNumChannels(); ++channel)
            fxChunkBuffer.copyFrom(channel, 0, mainBuffer, channel, chunkStart, chunkSamples);

        auto chunkState = advanceSmoothedGlobalBlockState(blockState, chunkSamples);

        processGlobalTransient(fxChunkBuffer, chunkState);
        processGlobalSaturator(fxChunkBuffer, chunkState);
        processGlobalCompressor(fxChunkBuffer, chunkState);
        processGlobalEQ(fxChunkBuffer, chunkState);
        processGlobalChorus(fxChunkBuffer, chunkState);
        applyGlobalLfo(fxChunkBuffer, chunkState);
        processGlobalDelay(fxChunkBuffer, chunkState);
        processGlobalReverb(fxChunkBuffer, chunkState);

        const float chunkGainEnd = chunkGainStart + gainStep * static_cast<float>(chunkSamples);
        for (int channel = 0; channel < fxChunkBuffer.getNumChannels(); ++channel)
            fxChunkBuffer.applyGainRamp(channel, 0, chunkSamples, chunkGainStart, chunkGainEnd);
        chunkGainStart = chunkGainEnd;

        processGlobalLimiter(fxChunkBuffer, chunkState);
        applyGlobalHpf(fxChunkBuffer);

        for (int channel = 0; channel < mainBuffer.getNumChannels(); ++channel)
            mainBuffer.copyFrom(channel, chunkStart, fxChunkBuffer, channel, 0, chunkSamples);
    }

    outputGainCurrent = targetGain;
}

void BassSynthAudioProcessor::processGlobalTransient(juce::AudioBuffer<float>& mainBuffer,
                                                     const GlobalBlockState& blockState)
{
    if (!isFxAvailableForCurrentBass(mbs::GlobalFxSlot::Transient)) return;
    const auto mix = clamp01(blockState.fx.transientMix);
    const auto attack = juce::jlimit(-1.0f, 1.0f, blockState.fx.transientAttack);
    const auto sustain = juce::jlimit(-1.0f, 1.0f, blockState.fx.transientSustain);

    if (mix <= 0.0001f || (std::abs(attack) <= 0.0001f && std::abs(sustain) <= 0.0001f))
        return;

    const auto sampleRate = static_cast<float>(std::max(1.0, preparedSampleRate));
    const auto fastCoeff = std::exp(-1.0f / (0.0018f * sampleRate));
    const auto slowCoeff = std::exp(-1.0f / (0.055f  * sampleRate));

    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
    {
        auto* data = mainBuffer.getWritePointer(ch);
        auto& fast = transientFastEnv[static_cast<std::size_t>(juce::jlimit(0, 1, ch))];
        auto& slow = transientSlowEnv[static_cast<std::size_t>(juce::jlimit(0, 1, ch))];

        for (int i = 0; i < mainBuffer.getNumSamples(); ++i)
        {
            const auto dry = data[i];
            const auto absSample = std::abs(dry);
            fast = fastCoeff * fast + (1.0f - fastCoeff) * absSample;
            slow = slowCoeff * slow + (1.0f - slowCoeff) * absSample;

            const auto transient = fast - slow;
            const auto gain = juce::jlimit(0.2f, 4.0f,
                1.0f + attack * std::max(0.0f, transient) * 7.0f
                     + sustain * std::max(0.0f, -transient) * 5.0f);
            data[i] = dry + (dry * gain - dry) * mix;
        }
    }
}

void BassSynthAudioProcessor::processGlobalSaturator(juce::AudioBuffer<float>& mainBuffer,
                                                     const GlobalBlockState& blockState)
{
    if (!isFxAvailableForCurrentBass(mbs::GlobalFxSlot::Saturator)) return;
    const auto mix = clamp01(blockState.fx.satMix);
    if (mix <= 0.0001f) return;

    const auto drive = juce::jlimit(1.0f, 16.0f, blockState.fx.satDrive);
    const auto norm  = 1.0f / std::max(0.0001f, std::tanh(drive));

    juce::dsp::AudioBlock<float> block(mainBuffer);
    const bool needsMix = mix < 0.9999f;
    if (needsMix)
        fxDryBuffer.makeCopyOf(mainBuffer, true);

    auto osBlock = satOversampling.processSamplesUp(block);
    for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
    {
        auto* data = osBlock.getChannelPointer(ch);
        for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
            data[i] = std::tanh(data[i] * drive) * norm;
    }

    satOversampling.processSamplesDown(block);

    if (needsMix)
    {
        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        {
            auto* wet = mainBuffer.getWritePointer(ch);
            const auto* dry = fxDryBuffer.getReadPointer(ch);
            for (int i = 0; i < mainBuffer.getNumSamples(); ++i)
                wet[i] = dry[i] + (wet[i] - dry[i]) * mix;
        }
    }
}

void BassSynthAudioProcessor::processGlobalCompressor(juce::AudioBuffer<float>& mainBuffer,
                                                      const GlobalBlockState& blockState)
{
    if (!isFxAvailableForCurrentBass(mbs::GlobalFxSlot::Compressor)) return;
    const auto mix = clamp01(blockState.fx.compMix);
    const auto makeupGain = juce::Decibels::decibelsToGain(blockState.fx.compMakeup);

    if (mix <= 0.0001f && std::abs(makeupGain - 1.0f) <= 0.0001f)
        return;

    updateGlobalEffectParameters(blockState);
    fxDryBuffer.makeCopyOf(mainBuffer, true);

    juce::dsp::AudioBlock<float> block(mainBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);
    mainBuffer.applyGain(makeupGain);

    if (mix < 0.9999f)
    {
        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        {
            auto* wet = mainBuffer.getWritePointer(ch);
            const auto* dry = fxDryBuffer.getReadPointer(ch);
            for (int i = 0; i < mainBuffer.getNumSamples(); ++i)
                wet[i] = dry[i] + (wet[i] - dry[i]) * mix;
        }
    }
}

void BassSynthAudioProcessor::processGlobalEQ(juce::AudioBuffer<float>& mainBuffer,
                                              const GlobalBlockState& blockState)
{
    if (!isFxAvailableForCurrentBass(mbs::GlobalFxSlot::Eq)) return;
    if (!blockState.fx.eqOn) return;

    mbs::fx::ParametricEQ3Band::Params p;
    p.lowFreq   = blockState.fx.eqLowFreq;
    p.lowGainDb = blockState.fx.eqLowGain;
    p.midFreq   = blockState.fx.eqMidFreq;
    p.midGainDb = blockState.fx.eqMidGain;
    p.midQ      = blockState.fx.eqMidQ;
    p.highFreq  = blockState.fx.eqHighFreq;
    p.highGainDb = blockState.fx.eqHighGain;

    auto* left  = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    eqProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

void BassSynthAudioProcessor::processGlobalChorus(juce::AudioBuffer<float>& mainBuffer,
                                                  const GlobalBlockState& blockState)
{
    if (!isFxAvailableForCurrentBass(mbs::GlobalFxSlot::Chorus)) return;

    mbs::fx::StereoChorus::Params p;
    p.rateHz = blockState.fx.chorusRate;
    p.depth  = blockState.fx.chorusDepth;
    p.mix    = blockState.fx.chorusMix;

    auto* left  = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    chorusProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

void BassSynthAudioProcessor::processGlobalDelay(juce::AudioBuffer<float>& mainBuffer,
                                                 const GlobalBlockState& blockState)
{
    if (!isFxAvailableForCurrentBass(mbs::GlobalFxSlot::Delay)) return;

    mbs::fx::StereoDelay::Params p;
    p.timeMs   = blockState.fx.delayTime;
    p.feedback = blockState.fx.delayFeedback;
    p.mix      = blockState.fx.delayMix;
    if (p.mix <= 0.0001f) return;
    p.syncToBpm = blockState.fx.delaySync;
    p.noteDiv   = blockState.fx.delayNoteDiv;

    if (auto* ph = getPlayHead())
    {
        if (auto posInfo = ph->getPosition())
        {
            if (auto bpm = posInfo->getBpm())
                p.bpm = static_cast<float>(*bpm);
        }
    }

    auto* left  = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    delayProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

void BassSynthAudioProcessor::processGlobalReverb(juce::AudioBuffer<float>& mainBuffer,
                                                  const GlobalBlockState& blockState)
{
    if (!isFxAvailableForCurrentBass(mbs::GlobalFxSlot::Reverb)) return;

    mbs::fx::DattorroPlateReverb::Params p;
    p.decay   = blockState.fx.reverbSize;
    p.damping = blockState.fx.reverbDamping;
    p.width   = blockState.fx.reverbWidth;
    p.mix     = blockState.fx.reverbMix;
    if (p.mix <= 0.0001f) return;

    auto* left  = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    reverbProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

void BassSynthAudioProcessor::processGlobalLimiter(juce::AudioBuffer<float>& mainBuffer,
                                                   const GlobalBlockState& blockState)
{
    if (!isFxAvailableForCurrentBass(mbs::GlobalFxSlot::Limiter)) return;
    if (!blockState.fx.limiterOn) return;

    mbs::fx::OutputLimiter::Params p;
    p.thresholdDb = blockState.fx.limiterThreshold;
    p.releaseMs   = blockState.fx.limiterRelease;

    auto* left  = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    limiterProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

void BassSynthAudioProcessor::applyGlobalFxSettings(const mbs::GlobalFxSettings& fx, bool notifyHost)
{
    const auto sanitized = sanitizeFxSettings(fx);
    setParamValueInternal(kSatDrive, sanitized.satDrive, notifyHost);
    setParamValueInternal(kSatMix, sanitized.satMix, notifyHost);
    setParamValueInternal(kTransientAttack, sanitized.transientAttack, notifyHost);
    setParamValueInternal(kTransientSustain, sanitized.transientSustain, notifyHost);
    setParamValueInternal(kTransientMix, sanitized.transientMix, notifyHost);
    setParamValueInternal(kCompThreshold, sanitized.compThreshold, notifyHost);
    setParamValueInternal(kCompRatio, sanitized.compRatio, notifyHost);
    setParamValueInternal(kCompAttack, sanitized.compAttack, notifyHost);
    setParamValueInternal(kCompRelease, sanitized.compRelease, notifyHost);
    setParamValueInternal(kCompMakeup, sanitized.compMakeup, notifyHost);
    setParamValueInternal(kCompMix, sanitized.compMix, notifyHost);
    setParamValueInternal(kEqLowFreq, sanitized.eqLowFreq, notifyHost);
    setParamValueInternal(kEqLowGain, sanitized.eqLowGain, notifyHost);
    setParamValueInternal(kEqMidFreq, sanitized.eqMidFreq, notifyHost);
    setParamValueInternal(kEqMidGain, sanitized.eqMidGain, notifyHost);
    setParamValueInternal(kEqMidQ, sanitized.eqMidQ, notifyHost);
    setParamValueInternal(kEqHighFreq, sanitized.eqHighFreq, notifyHost);
    setParamValueInternal(kEqHighGain, sanitized.eqHighGain, notifyHost);
    setParamValueInternal(kChorusRate, sanitized.chorusRate, notifyHost);
    setParamValueInternal(kChorusDepth, sanitized.chorusDepth, notifyHost);
    setParamValueInternal(kChorusMix, sanitized.chorusMix, notifyHost);
    setParamValueInternal(kDelayTime, sanitized.delayTime, notifyHost);
    setParamValueInternal(kDelayFeedback, sanitized.delayFeedback, notifyHost);
    setParamValueInternal(kDelayMix, sanitized.delayMix, notifyHost);
    setParamValueInternal(kDelaySync, sanitized.delaySync ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal(kDelayNoteDiv, static_cast<float>(sanitized.delayNoteDiv), notifyHost);
    setParamValueInternal(kReverbSize, sanitized.reverbSize, notifyHost);
    setParamValueInternal(kReverbDamping, sanitized.reverbDamping, notifyHost);
    setParamValueInternal(kReverbWidth, sanitized.reverbWidth, notifyHost);
    setParamValueInternal(kReverbMix, sanitized.reverbMix, notifyHost);
    setParamValueInternal(kLimiterThreshold, sanitized.limiterThreshold, notifyHost);
    setParamValueInternal(kLimiterRelease, sanitized.limiterRelease, notifyHost);
    setParamValueInternal("fx_tab0_en", sanitized.saturatorOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab1_en", sanitized.transientOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab2_en", sanitized.compressorOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab3_en", sanitized.eqOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab4_en", sanitized.chorusOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab5_en", sanitized.delayOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab6_en", sanitized.reverbOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab7_en", sanitized.limiterOn ? 1.0f : 0.0f, notifyHost);
}

void BassSynthAudioProcessor::applyGlobalLfo(juce::AudioBuffer<float>& mainBuffer,
                                             const GlobalBlockState& blockState)
{
    const auto numCh = mainBuffer.getNumChannels();
    const auto numSamples = mainBuffer.getNumSamples();
    if (numCh <= 0 || numSamples <= 0) return;

    const float rateHz = blockState.lfoRate;
    const float depth  = blockState.lfoDepth;
    if (depth <= 0.0001f)
    {
        for (auto& slot : voices)
        {
            if (slot.voice.isActive())
                slot.voice.setLfoCutoffMod(0.0f);
        }
        return;
    }

    const bool is808Family = mbs::getFamily(blockState.selectedBass) == mbs::Family::Eight08;
    const int wave = blockState.lfoWave;
    const int dest = blockState.lfoDest;
    const float phaseInc = rateHz / static_cast<float>(juce::jmax(1.0, preparedSampleRate));
    constexpr float kTremDepth = 0.65f;
    constexpr float kPanDepth  = 0.50f;
    const float cutoffDepthScale = is808Family ? 0.45f : 1.0f;

    const bool doTremPan = (dest == 0 || dest == 2);
    const bool doCutoff  = (dest == 1 || dest == 2);

    auto* left  = mainBuffer.getWritePointer(0);
    auto* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float lfo = 0.0f;
        switch (wave)
        {
            case 1: lfo = 1.0f - 4.0f * std::abs(lfoPhase - 0.5f); break;
            case 2: lfo = lfoPhase * 2.0f - 1.0f; break;
            case 3: lfo = lfoPhase < 0.5f ? 1.0f : -1.0f; break;
            default: lfo = std::sin(lfoPhase * juce::MathConstants<float>::twoPi); break;
        }

        if (doTremPan)
        {
            const float tremAmt = depth * kTremDepth;
            const float trem = 1.0f - tremAmt * 0.5f + lfo * tremAmt * 0.5f;

            if (right != nullptr)
            {
                const float pan = lfo * depth * kPanDepth;
                const float gL = std::sqrt(0.5f * (1.0f - pan)) * trem;
                const float gR = std::sqrt(0.5f * (1.0f + pan)) * trem;
                left[i]  *= gL;
                right[i] *= gR;
            }
            else
            {
                left[i] *= trem;
            }
        }

        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }

    if (doCutoff)
    {
        float midLfo = 0.0f;
        const float midPhase = lfoPhase - phaseInc * static_cast<float>(numSamples) * 0.5f;
        const float mp = midPhase - std::floor(midPhase);
        switch (wave)
        {
            case 1: midLfo = 1.0f - 4.0f * std::abs(mp - 0.5f); break;
            case 2: midLfo = mp * 2.0f - 1.0f; break;
            case 3: midLfo = mp < 0.5f ? 1.0f : -1.0f; break;
            default: midLfo = std::sin(mp * juce::MathConstants<float>::twoPi); break;
        }

        const float cutoffMod = midLfo * depth * cutoffDepthScale;
        for (auto& slot : voices)
        {
            if (slot.voice.isActive())
                slot.voice.setLfoCutoffMod(cutoffMod);
        }
    }
    else
    {
        for (auto& slot : voices)
        {
            if (slot.voice.isActive())
                slot.voice.setLfoCutoffMod(0.0f);
        }
    }
}

// =============================================================================
// Global HPF — 2nd order Butterworth at ~25Hz to remove sub-sonic energy
// =============================================================================
void BassSynthAudioProcessor::applyGlobalHpf(juce::AudioBuffer<float>& mainBuffer)
{
    const auto sr = static_cast<float>(std::max(1.0, preparedSampleRate));
    constexpr float hpfFreq = 25.0f;

    // Butterworth 2nd order HPF coefficients
    const float omega = 2.0f * juce::MathConstants<float>::pi * hpfFreq / sr;
    const float cosw  = std::cos(omega);
    const float sinw  = std::sin(omega);
    const float alpha = sinw / (2.0f * 0.7071f);  // Q = 0.7071 (Butterworth)

    const float a0 = 1.0f + alpha;
    const float b0 = ((1.0f + cosw) * 0.5f) / a0;
    const float b1 = (-(1.0f + cosw)) / a0;
    const float b2 = ((1.0f + cosw) * 0.5f) / a0;
    const float a1 = (-2.0f * cosw) / a0;
    const float a2 = (1.0f - alpha) / a0;

    for (int ch = 0; ch < juce::jmin(2, mainBuffer.getNumChannels()); ++ch)
    {
        auto* data = mainBuffer.getWritePointer(ch);
        float& x1 = hpfX1[ch];
        float& x2 = hpfX2[ch];
        float& y1 = hpfY1[ch];
        float& y2 = hpfY2[ch];

        for (int i = 0; i < mainBuffer.getNumSamples(); ++i)
        {
            const float x0 = data[i];
            const float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x0;
            y2 = y1; y1 = y0;
            data[i] = y0;
        }
    }
}

// =============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassSynthAudioProcessor();
}
