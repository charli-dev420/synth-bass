#pragma once

#include <JuceHeader.h>

namespace musique::preset
{

struct SynthIdentity
{
    int synthId = -1;
    const char* synthType = "";
    const char* synthDisplayName = "";
    const char* xmlRootTag = "";
    const char* instrumentAttrName = "";
    const char* presetDirPrefix = "";
    const char* appDataFolder = "";

    bool isValid() const noexcept
    {
        return synthId >= 0 && xmlRootTag[0] != '\0';
    }
};

inline SynthIdentity getSynthIdentity(int hubSynthIndex)
{
    static constexpr SynthIdentity identities[] = {
        { 0, "drum",   "Musique Drum Synth",       "",             "",           "",      "" },
        { 1, "instr",  "Musique Instrument Synth", "InstrPreset",  "inst",       "inst",  "MusiqueInstrSynth" },
        { 2, "piano",  "Musique Piano Synth",      "PianoPreset",  "piano_index","piano", "MusiquePianoSynth" },
        { 3, "bass",   "Musique Bass Synth",       "BassPreset",   "bass",       "bass",  "MusiqueBassSynth" },
        { 4, "orch",   "UWdeVST_Orch",             "OrchPreset",   "instr",      "instr", "MusiqueOrchSynth" },
        { 5, "perc",   "Musique Percussion Synth", "PercPreset",   "instrIndex", "instr", "MusiquePercSynth" },
        { 6, "guitar", "Musique Guitar Synth",     "GuitarPreset", "instrIndex", "instr", "MusiqueGuitarSynth" },
    };

    if (hubSynthIndex < 0 || hubSynthIndex >= (int) std::size(identities))
        return {};

    return identities[(size_t) hubSynthIndex];
}

inline int inferSynthIdFromRootTag(const juce::String& rootTag)
{
    for (int synthIndex = 0; synthIndex <= 6; ++synthIndex)
    {
        const auto identity = getSynthIdentity(synthIndex);
        if (rootTag == identity.xmlRootTag)
            return synthIndex;
    }

    return -1;
}

enum class ValidationStatus
{
    valid,
    missingManifest,
    invalidManifest,
    invalidXml,
    unsupportedSynth,
    rootTagMismatch,
    synthMismatch,
    instrumentMismatch,
};

inline juce::String toString(ValidationStatus status)
{
    switch (status)
    {
        case ValidationStatus::valid:              return "valid";
        case ValidationStatus::missingManifest:    return "missing_manifest";
        case ValidationStatus::invalidManifest:    return "invalid_manifest";
        case ValidationStatus::invalidXml:         return "invalid_xml";
        case ValidationStatus::unsupportedSynth:   return "unsupported_synth";
        case ValidationStatus::rootTagMismatch:    return "root_tag_mismatch";
        case ValidationStatus::synthMismatch:      return "synth_mismatch";
        case ValidationStatus::instrumentMismatch: return "instrument_mismatch";
    }

    return "unknown";
}

struct PresetManifest
{
    int synthId = -1;
    juce::String synthType;
    int instrumentIndex = 0;
    juce::String instrumentName;
    juce::String presetName;
    juce::String xmlRootTag;
    juce::String sourceModel;
    juce::String createdAt;
    juce::String sourcePath;
    int validationVersion = 1;

    bool isValid() const noexcept
    {
        return synthId >= 0 && presetName.isNotEmpty() && xmlRootTag.isNotEmpty();
    }

    juce::var toVar() const
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("synthId", synthId);
        object->setProperty("synthType", synthType);
        object->setProperty("instrumentIndex", instrumentIndex);
        object->setProperty("instrumentName", instrumentName);
        object->setProperty("presetName", presetName);
        object->setProperty("xmlRootTag", xmlRootTag);
        object->setProperty("sourceModel", sourceModel);
        object->setProperty("createdAt", createdAt);
        object->setProperty("sourcePath", sourcePath);
        object->setProperty("validationVersion", validationVersion);
        return juce::var(object);
    }

    juce::String toJson() const
    {
        return juce::JSON::toString(toVar(), true);
    }

    static bool fromVar(const juce::var& value, PresetManifest& out)
    {
        auto* object = value.getDynamicObject();
        if (object == nullptr)
            return false;

        out.synthId = (int) object->getProperty("synthId");
        out.synthType = object->getProperty("synthType").toString();
        out.instrumentIndex = (int) object->getProperty("instrumentIndex");
        out.instrumentName = object->getProperty("instrumentName").toString();
        out.presetName = object->getProperty("presetName").toString();
        out.xmlRootTag = object->getProperty("xmlRootTag").toString();
        out.sourceModel = object->getProperty("sourceModel").toString();
        out.createdAt = object->getProperty("createdAt").toString();
        out.sourcePath = object->getProperty("sourcePath").toString();
        out.validationVersion = object->hasProperty("validationVersion")
            ? (int) object->getProperty("validationVersion")
            : 1;
        return out.isValid();
    }

    static bool fromJson(const juce::String& json, PresetManifest& out)
    {
        auto parsed = juce::JSON::parse(json);
        if (parsed.isVoid())
            return false;
        return fromVar(parsed, out);
    }
};

struct PresetValidationResult
{
    ValidationStatus status = ValidationStatus::invalidManifest;
    PresetManifest manifest;
    juce::String message;

    bool ok() const noexcept
    {
        return status == ValidationStatus::valid || status == ValidationStatus::missingManifest;
    }
};

inline juce::File manifestFileForPresetFile(const juce::File& presetFile)
{
    return presetFile.getSiblingFile(presetFile.getFileNameWithoutExtension() + ".preset.json");
}

inline juce::File nativeUserPresetsDirectoryForSynth(int hubSynthIndex, int instrumentIndex)
{
    const auto identity = getSynthIdentity(hubSynthIndex);
    if (identity.appDataFolder[0] == '\0' || identity.presetDirPrefix[0] == '\0')
        return {};

    const auto presetsFolder = hubSynthIndex == 2 ? juce::String("Presets_v4") : juce::String("Presets");
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(identity.appDataFolder)
        .getChildFile(presetsFolder)
        .getChildFile(juce::String(identity.presetDirPrefix) + "_" + juce::String(juce::jmax(0, instrumentIndex)));
}

inline int readInstrumentIndexFromXml(const juce::XmlElement& xml, const SynthIdentity& identity)
{
    if (identity.instrumentAttrName[0] != '\0' && xml.hasAttribute(identity.instrumentAttrName))
        return xml.getIntAttribute(identity.instrumentAttrName, 0);

    if (xml.hasAttribute("piano_index"))
        return xml.getIntAttribute("piano_index", 0);

    if (xml.hasAttribute("instrument_index"))
        return xml.getIntAttribute("instrument_index", 0);

    if (xml.hasAttribute("instrumentIndex"))
        return xml.getIntAttribute("instrumentIndex", 0);

    if (xml.hasAttribute("index"))
        return xml.getIntAttribute("index", 0);

    for (const auto* fallbackAttr : { "inst", "instr", "instrIndex", "piano", "bass" })
    {
        if (xml.hasAttribute(fallbackAttr))
            return xml.getIntAttribute(fallbackAttr, 0);
    }

    return 0;
}

inline bool saveManifestToFile(const juce::File& manifestFile, const PresetManifest& manifest)
{
    return manifestFile.replaceWithText(manifest.toJson());
}

inline bool loadManifestFromFile(const juce::File& manifestFile, PresetManifest& manifest)
{
    if (!manifestFile.existsAsFile())
        return false;

    return PresetManifest::fromJson(manifestFile.loadFileAsString(), manifest);
}

} // namespace musique::preset
