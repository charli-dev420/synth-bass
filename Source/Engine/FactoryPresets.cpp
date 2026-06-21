#include "FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace mbs
{
namespace
{
constexpr std::array<const char*, kNumBasses> kBassTags = {{
    "double-bass", "fingered-bass", "slap-bass",
    "sub-808", "boom-808", "distorted-808",
    "moog-bass", "reese-bass", "acid-bass"
}};

const char* familyLabelFor(const int bassIndex)
{
    switch (getFamily(bassIndex))
    {
        case Family::Acoustic: return "acoustic";
        case Family::Eight08:  return "808";
        case Family::Synth:    return "synth";
        default:               return "bass";
    }
}

const char* roleFor(const int bassIndex, const bool signature)
{
    switch (bassIndex)
    {
        case 0: return "organic-foundation";
        case 1: return "organic-foundation";
        case 2: return "transient-bass";
        case 3: return "sub-foundation";
        case 4: return signature ? "transient-bass" : "sub-foundation";
        case 5: return signature ? "transient-bass" : "sub-foundation";
        case 6: return signature ? "lead-bass" : "character-bass";
        case 7: return "texture-bass";
        case 8: return signature ? "lead-bass" : "character-bass";
        default: return "character-bass";
    }
}

void addTag(std::vector<std::string>& tags, const std::string& tag)
{
    if (std::find(tags.begin(), tags.end(), tag) == tags.end())
        tags.push_back(tag);
}

PresetMetadata makeMetadata(const int bassIndex, const bool signature)
{
    PresetMetadata metadata;
    metadata.familyLabel = familyLabelFor(bassIndex);
    metadata.mixRole = roleFor(bassIndex, signature);
    metadata.nominalPeakDb = signature ? -9.5f : -11.0f;

    addTag(metadata.tags, metadata.familyLabel);
    addTag(metadata.tags, kBassTags[static_cast<std::size_t>(bassIndex)]);
    addTag(metadata.tags, metadata.mixRole);
    addTag(metadata.tags, signature ? "signature" : "reference");
    addTag(metadata.tags, signature ? "curated" : "dry");

    if (bassIndex == 7)
        addTag(metadata.tags, "reese");
    if (bassIndex == 8)
        addTag(metadata.tags, "acid");
    if (bassIndex == 6)
        addTag(metadata.tags, "moog");

    return metadata;
}

int outputBusFor(const int bassIndex)
{
    switch (getFamily(bassIndex))
    {
        case Family::Acoustic: return 0;
        case Family::Eight08:  return 1;
        case Family::Synth:    return 2;
        default:               return 0;
    }
}

void clearUnavailableFxValues(const int bassIndex, GlobalFxSettings& fx)
{
    const auto& availability = getFxAvailability(bassIndex);
    if (!availability.saturator)
        fx.satMix = 0.0f;
    if (!availability.transient)
        fx.transientMix = 0.0f;
    if (!availability.compressor)
        fx.compMix = 0.0f;
    if (!availability.eq)
    {
        fx.eqLowGain = 0.0f;
        fx.eqMidGain = 0.0f;
        fx.eqHighGain = 0.0f;
    }
    if (!availability.chorus)
        fx.chorusMix = 0.0f;
    if (!availability.delay)
    {
        fx.delayMix = 0.0f;
        fx.delayFeedback = 0.0f;
    }
    if (!availability.reverb)
        fx.reverbMix = 0.0f;
}

GlobalFxSettings makeFx(const int bassIndex, const bool signature)
{
    auto fx = getDefaultGlobalFx(bassIndex);
    fx.saturatorOn = false;
    fx.transientOn = false;
    fx.compressorOn = false;
    fx.eqOn = true;
    fx.chorusOn = false;
    fx.delayOn = false;
    fx.reverbOn = false;
    fx.limiterOn = true;

    fx.satDrive = 1.35f;
    fx.satMix = 0.0f;
    fx.transientAttack = 0.08f;
    fx.transientSustain = 0.0f;
    fx.transientMix = 0.0f;
    fx.compThreshold = -20.0f;
    fx.compRatio = 2.2f;
    fx.compAttack = 14.0f;
    fx.compRelease = 120.0f;
    fx.compMakeup = 0.0f;
    fx.compMix = 0.0f;
    fx.eqLowFreq = 80.0f;
    fx.eqLowGain = 0.0f;
    fx.eqMidFreq = 650.0f;
    fx.eqMidGain = 0.0f;
    fx.eqMidQ = 1.0f;
    fx.eqHighFreq = 3600.0f;
    fx.eqHighGain = 0.0f;
    fx.chorusRate = 0.65f;
    fx.chorusDepth = 0.24f;
    fx.chorusMix = 0.0f;
    fx.delayTime = 300.0f;
    fx.delayFeedback = 0.0f;
    fx.delayMix = 0.0f;
    fx.delaySync = false;
    fx.delayNoteDiv = 0;
    fx.reverbSize = 0.34f;
    fx.reverbDamping = 0.62f;
    fx.reverbWidth = 0.58f;
    fx.reverbMix = 0.0f;
    fx.limiterThreshold = -0.8f;
    fx.limiterRelease = 55.0f;

    if (!signature)
    {
        if (bassIndex <= 1)
        {
            fx.reverbOn = true;
            fx.reverbMix = 0.012f;
        }
        maskUnavailableFx(bassIndex, fx);
        clearUnavailableFxValues(bassIndex, fx);
        return fx;
    }

    switch (bassIndex)
    {
        case 0:
            fx.transientOn = true;
            fx.transientAttack = 0.08f;
            fx.transientMix = 0.08f;
            fx.compressorOn = true;
            fx.compThreshold = -24.0f;
            fx.compRatio = 1.8f;
            fx.compMix = 0.14f;
            fx.reverbOn = true;
            fx.reverbMix = 0.025f;
            break;
        case 1:
            fx.transientOn = true;
            fx.transientAttack = 0.06f;
            fx.transientMix = 0.06f;
            fx.compressorOn = true;
            fx.compThreshold = -22.0f;
            fx.compRatio = 2.0f;
            fx.compMix = 0.16f;
            fx.chorusOn = true;
            fx.chorusDepth = 0.16f;
            fx.chorusMix = 0.035f;
            fx.reverbOn = true;
            fx.reverbMix = 0.018f;
            break;
        case 2:
            fx.saturatorOn = true;
            fx.satDrive = 1.50f;
            fx.satMix = 0.055f;
            fx.transientOn = true;
            fx.transientAttack = 0.18f;
            fx.transientSustain = -0.05f;
            fx.transientMix = 0.16f;
            fx.compressorOn = true;
            fx.compThreshold = -20.0f;
            fx.compRatio = 2.4f;
            fx.compAttack = 8.0f;
            fx.compMix = 0.18f;
            break;
        case 3:
            fx.saturatorOn = true;
            fx.satDrive = 1.30f;
            fx.satMix = 0.025f;
            fx.compressorOn = true;
            fx.compThreshold = -18.0f;
            fx.compRatio = 2.2f;
            fx.compMix = 0.38f;
            break;
        case 4:
            fx.saturatorOn = true;
            fx.satDrive = 1.45f;
            fx.satMix = 0.045f;
            fx.transientOn = true;
            fx.transientAttack = 0.14f;
            fx.transientMix = 0.12f;
            fx.compressorOn = true;
            fx.compThreshold = -18.5f;
            fx.compRatio = 2.4f;
            fx.compMix = 0.42f;
            break;
        case 5:
            fx.saturatorOn = true;
            fx.satDrive = 1.70f;
            fx.satMix = 0.060f;
            fx.compressorOn = true;
            fx.compThreshold = -20.0f;
            fx.compRatio = 2.8f;
            fx.compMix = 0.32f;
            break;
        case 6:
            fx.saturatorOn = true;
            fx.satDrive = 1.55f;
            fx.satMix = 0.055f;
            fx.transientOn = true;
            fx.transientAttack = 0.08f;
            fx.transientMix = 0.06f;
            fx.compressorOn = true;
            fx.compThreshold = -21.0f;
            fx.compRatio = 2.0f;
            fx.compMix = 0.18f;
            fx.reverbOn = true;
            fx.reverbMix = 0.015f;
            break;
        case 7:
            fx.saturatorOn = true;
            fx.satDrive = 1.35f;
            fx.satMix = 0.035f;
            fx.compressorOn = true;
            fx.compThreshold = -19.0f;
            fx.compRatio = 2.5f;
            fx.compMix = 0.38f;
            fx.chorusOn = true;
            fx.chorusDepth = 0.24f;
            fx.chorusMix = 0.075f;
            fx.reverbOn = true;
            fx.reverbMix = 0.018f;
            break;
        case 8:
            fx.saturatorOn = true;
            fx.satDrive = 1.55f;
            fx.satMix = 0.060f;
            fx.transientOn = true;
            fx.transientAttack = 0.12f;
            fx.transientMix = 0.10f;
            fx.compressorOn = true;
            fx.compThreshold = -21.0f;
            fx.compRatio = 2.2f;
            fx.compMix = 0.18f;
            fx.reverbOn = true;
            fx.reverbMix = 0.010f;
            break;
        default:
            break;
    }

    maskUnavailableFx(bassIndex, fx);
    clearUnavailableFxValues(bassIndex, fx);
    return fx;
}

BassSettings makeSettings(const int bassIndex, const bool signature)
{
    auto settings = getDefaultSettings(bassIndex);
    settings.tuneSemitones = 0.0f;
    settings.pan = 0.0f;

    switch (bassIndex)
    {
        case 0:
            settings.level = signature ? 0.58f : 0.62f;
            settings.brightness = signature ? 0.43f : 0.34f;
            settings.decaySeconds = signature ? 3.40f : 2.90f;
            settings.sustainLevel = signature ? 0.25f : 0.20f;
            settings.releaseSeconds = signature ? 0.40f : 0.32f;
            settings.body = signature ? 0.68f : 0.58f;
            settings.cutoffHz = signature ? 2600.0f : 2200.0f;
            settings.snap = signature ? 0.85f : 0.95f;
            settings.envShape = signature ? 0.58f : 0.52f;
            break;
        case 1:
            settings.level = signature ? 0.56f : 0.60f;
            settings.brightness = signature ? 0.62f : 0.52f;
            settings.decaySeconds = signature ? 2.35f : 2.20f;
            settings.sustainLevel = signature ? 0.28f : 0.23f;
            settings.releaseSeconds = signature ? 0.26f : 0.22f;
            settings.body = signature ? 0.50f : 0.44f;
            settings.character = signature ? 0.55f : 0.46f;
            settings.cutoffHz = signature ? 4500.0f : 3600.0f;
            settings.snap = signature ? 0.92f : 0.98f;
            break;
        case 2:
            settings.level = signature ? 0.30f : 0.38f;
            settings.brightness = signature ? 0.74f : 0.62f;
            settings.attackSeconds = 0.001f;
            settings.decaySeconds = signature ? 1.12f : 1.25f;
            settings.sustainLevel = signature ? 0.12f : 0.14f;
            settings.releaseSeconds = signature ? 0.12f : 0.14f;
            settings.body = signature ? 0.34f : 0.36f;
            settings.drive = signature ? 0.14f : 0.06f;
            settings.cutoffHz = signature ? 5600.0f : 4700.0f;
            settings.snap = signature ? 1.00f : 0.95f;
            settings.envShape = signature ? 0.35f : 0.42f;
            break;
        case 3:
            settings.level = signature ? 0.44f : 0.48f;
            settings.brightness = signature ? 0.24f : 0.18f;
            settings.decaySeconds = signature ? 5.20f : 4.40f;
            settings.sustainLevel = signature ? 0.38f : 0.30f;
            settings.releaseSeconds = signature ? 0.32f : 0.25f;
            settings.pitchEnv = signature ? 0.76f : 0.62f;
            settings.subLevel = signature ? 0.82f : 0.78f;
            settings.cutoffHz = signature ? 900.0f : 700.0f;
            settings.envShape = signature ? 0.62f : 0.55f;
            break;
        case 4:
            settings.level = signature ? 0.38f : 0.42f;
            settings.brightness = signature ? 0.32f : 0.25f;
            settings.decaySeconds = signature ? 5.70f : 4.80f;
            settings.sustainLevel = signature ? 0.40f : 0.34f;
            settings.releaseSeconds = signature ? 0.34f : 0.28f;
            settings.drive = signature ? 0.18f : 0.08f;
            settings.pitchEnv = signature ? 0.72f : 0.62f;
            settings.subLevel = signature ? 0.78f : 0.72f;
            settings.cutoffHz = signature ? 1300.0f : 1000.0f;
            settings.snap = signature ? 1.00f : 0.95f;
            break;
        case 5:
            settings.level = signature ? 0.32f : 0.36f;
            settings.brightness = signature ? 0.42f : 0.32f;
            settings.decaySeconds = signature ? 2.70f : 2.40f;
            settings.sustainLevel = signature ? 0.20f : 0.18f;
            settings.releaseSeconds = signature ? 0.22f : 0.18f;
            settings.drive = signature ? 0.58f : 0.40f;
            settings.pitchEnv = signature ? 0.86f : 0.75f;
            settings.character = signature ? 0.70f : 0.58f;
            settings.cutoffHz = signature ? 1400.0f : 1000.0f;
            settings.envShape = signature ? 0.42f : 0.50f;
            break;
        case 6:
            settings.level = signature ? 0.26f : 0.30f;
            settings.brightness = signature ? 0.64f : 0.50f;
            settings.decaySeconds = signature ? 2.20f : 2.00f;
            settings.sustainLevel = signature ? 0.30f : 0.25f;
            settings.releaseSeconds = signature ? 0.22f : 0.20f;
            settings.drive = signature ? 0.24f : 0.12f;
            settings.character = signature ? 0.58f : 0.48f;
            settings.cutoffHz = signature ? 2600.0f : 1800.0f;
            settings.resonance = signature ? 0.52f : 0.40f;
            settings.filterEnv = signature ? 0.70f : 0.30f;
            settings.glideTime = signature ? 0.03f : 0.0f;
            break;
        case 7:
            settings.level = signature ? 0.22f : 0.24f;
            settings.brightness = signature ? 0.42f : 0.34f;
            settings.decaySeconds = signature ? 3.80f : 3.20f;
            settings.sustainLevel = signature ? 0.40f : 0.32f;
            settings.releaseSeconds = signature ? 0.55f : 0.40f;
            settings.drive = signature ? 0.12f : 0.06f;
            settings.character = signature ? 0.70f : 0.55f;
            settings.cutoffHz = signature ? 1300.0f : 1000.0f;
            settings.resonance = signature ? 0.30f : 0.24f;
            settings.filterEnv = signature ? 0.38f : 0.20f;
            settings.envShape = signature ? 0.65f : 0.58f;
            break;
        case 8:
            settings.level = signature ? 0.28f : 0.32f;
            settings.brightness = signature ? 0.72f : 0.56f;
            settings.attackSeconds = signature ? 0.001f : 0.002f;
            settings.decaySeconds = signature ? 1.00f : 1.20f;
            settings.sustainLevel = signature ? 0.12f : 0.16f;
            settings.releaseSeconds = signature ? 0.10f : 0.12f;
            settings.drive = signature ? 0.24f : 0.10f;
            settings.character = signature ? 0.70f : 0.52f;
            settings.cutoffHz = signature ? 3400.0f : 2200.0f;
            settings.resonance = signature ? 0.68f : 0.50f;
            settings.filterEnv = signature ? 0.82f : 0.55f;
            settings.glideTime = signature ? 0.025f : 0.0f;
            settings.envShape = signature ? 0.35f : 0.45f;
            break;
        default:
            break;
    }

    return settings;
}

InstrumentPreset makePreset(const int bassIndex, const bool signature)
{
    InstrumentPreset preset;
    preset.name = std::string(getBassName(bassIndex)) + (signature ? " Signature" : " Reference");
    preset.settings = makeSettings(bassIndex, signature);
    preset.fx = makeFx(bassIndex, signature);
    preset.outputBus = outputBusFor(bassIndex);
    preset.performance = PatchPerformanceSettings {};
    preset.modMatrix = modmatrix::MatrixState {};
    preset.metadata = makeMetadata(bassIndex, signature);
    return preset;
}

#include "Curated8FactoryPresets.inc"

std::array<std::vector<InstrumentPreset>, kNumBasses> buildBanks()
{
    std::array<std::vector<InstrumentPreset>, kNumBasses> banks {};
    for (int bassIndex = 0; bassIndex < kNumBasses; ++bassIndex)
    {
        auto& bank = banks[static_cast<std::size_t>(bassIndex)];
        bank.reserve(10);
        bank.push_back(makePreset(bassIndex, false));
        bank.push_back(makePreset(bassIndex, true));
    }
    appendCurated8FactoryPresets(banks);
    return banks;
}
} // namespace

const std::array<std::vector<InstrumentPreset>, kNumBasses>& getFactoryPresetBanks()
{
    static const auto banks = buildBanks();
    return banks;
}

std::size_t getTotalFactoryPresetCount()
{
    std::size_t total = 0;
    for (const auto& bank : getFactoryPresetBanks())
        total += bank.size();
    return total;
}

} // namespace mbs
