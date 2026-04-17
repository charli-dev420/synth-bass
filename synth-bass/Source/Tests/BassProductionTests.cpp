#include <JuceHeader.h>

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Engine/FactoryPresets.h"
#include "../../../Shared/PresetManifest.h"
#include "../../../Shared/ProductionQa.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
void require(bool condition, const juce::String& message)
{
    if (!condition)
        throw std::runtime_error(message.toStdString());
}

void setParameterValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, float actualValue)
{
    auto* parameter = apvts.getParameter(paramId);
    require(parameter != nullptr, "Missing parameter: " + paramId);

    auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
    require(ranged != nullptr, "Parameter is not ranged: " + paramId);
    parameter->setValueNotifyingHost(ranged->convertTo0to1(actualValue));
}

juce::AudioBuffer<float> renderWithMidi(BassSynthAudioProcessor& processor,
                                        const std::vector<std::pair<int, juce::MidiMessage>>& events,
                                        int totalSamples,
                                        int blockSize = 256)
{
    const int totalChannels = processor.getTotalNumOutputChannels();
    juce::AudioBuffer<float> rendered(juce::jmax(2, totalChannels), totalSamples);
    rendered.clear();
    juce::AudioBuffer<float> block(totalChannels, blockSize);

    std::size_t eventIndex = 0;
    for (int blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
    {
        block.clear();
        juce::MidiBuffer midi;
        while (eventIndex < events.size() && events[eventIndex].first < blockStart + blockSize)
        {
            if (events[eventIndex].first >= blockStart)
                midi.addEvent(events[eventIndex].second, events[eventIndex].first - blockStart);
            ++eventIndex;
        }

        processor.processBlock(block, midi);

        const int samplesToCopy = juce::jmin(blockSize, totalSamples - blockStart);
        for (int ch = 0; ch < juce::jmin(rendered.getNumChannels(), block.getNumChannels()); ++ch)
            rendered.copyFrom(ch, blockStart, block, ch, 0, samplesToCopy);
    }

    return rendered;
}

float bufferPeak(const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    return peak;
}

float bufferDifference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    const int channels = juce::jmin(a.getNumChannels(), b.getNumChannels());
    const int samples = juce::jmin(a.getNumSamples(), b.getNumSamples());
    float diff = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* aData = a.getReadPointer(ch);
        const auto* bData = b.getReadPointer(ch);
        for (int sample = 0; sample < samples; ++sample)
            diff += std::abs(aData[sample] - bData[sample]);
    }
    return diff;
}

double estimateDominantAutocorrelationFrequency(const juce::AudioBuffer<float>& buffer,
                                                double sampleRate,
                                                int startSample,
                                                int analysisSamples,
                                                double minFrequency,
                                                double maxFrequency)
{
    const auto* data = buffer.getReadPointer(0);
    const int safeStart = juce::jlimit(0, juce::jmax(0, buffer.getNumSamples() - 1), startSample);
    const int safeSamples = juce::jlimit(128, buffer.getNumSamples() - safeStart, analysisSamples);
    const int minLag = juce::jlimit(1, safeSamples - 1, static_cast<int>(std::floor(sampleRate / maxFrequency)));
    const int maxLag = juce::jlimit(minLag + 1, safeSamples - 1, static_cast<int>(std::ceil(sampleRate / minFrequency)));

    double bestCorr = -1.0;
    int bestLag = minLag;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double corr = 0.0;
        for (int i = 0; i < safeSamples - lag; ++i)
            corr += static_cast<double>(data[safeStart + i]) * static_cast<double>(data[safeStart + i + lag]);

        if (corr > bestCorr)
        {
            bestCorr = corr;
            bestLag = lag;
        }
    }

    return sampleRate / static_cast<double>(bestLag);
}

juce::AudioBuffer<float> renderBassVoice(const mbs::BassSettings& settings,
                                         const mbs::BassCharacteristics& chars,
                                         int midiNote,
                                         float velocity,
                                         double sampleRate,
                                         int totalSamples)
{
    mbs::BassVoice voice;
    juce::AudioBuffer<float> buffer(2, totalSamples);
    buffer.clear();
    voice.noteOn(settings, chars, midiNote, velocity, sampleRate);
    voice.render(buffer, 0, totalSamples);
    return buffer;
}

bool bufferIsFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (!std::isfinite(data[sample]))
                return false;
    }
    return true;
}

bool stringArrayContainsIgnoreCase(const juce::StringArray& items, const juce::String& target)
{
    for (const auto& item : items)
        if (item.equalsIgnoreCase(target))
            return true;
    return false;
}

juce::StringArray splitCsvTags(const std::vector<std::string>& tags)
{
    juce::StringArray items;
    for (const auto& tag : tags)
        items.add(juce::String(juce::CharPointer_UTF8(tag.c_str())));
    return items;
}

float maxWindowRmsDelta(const juce::AudioBuffer<float>& buffer, int startSample = 0, int windowSize = 64)
{
    const int safeStart = juce::jlimit(0, juce::jmax(0, buffer.getNumSamples() - 1), startSample);
    const int safeWindow = juce::jmax(8, windowSize);
    float maxDelta = 0.0f;
    float previousRms = -1.0f;

    for (int windowStart = safeStart; windowStart < buffer.getNumSamples(); windowStart += safeWindow)
    {
        const int samples = juce::jmin(safeWindow, buffer.getNumSamples() - windowStart);
        double energy = 0.0;
        int count = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer(ch);
            for (int sample = 0; sample < samples; ++sample)
            {
                const auto value = data[windowStart + sample];
                energy += static_cast<double>(value) * static_cast<double>(value);
                ++count;
            }
        }

        if (count <= 0)
            continue;

        const auto rms = static_cast<float>(std::sqrt(energy / static_cast<double>(count)));
        if (previousRms >= 0.0f)
            maxDelta = juce::jmax(maxDelta, std::abs(rms - previousRms));
        previousRms = rms;
    }

    return maxDelta;
}

float channelRangePeak(const juce::AudioBuffer<float>& buffer, int startChannel, int endChannelExclusive)
{
    float peak = 0.0f;
    for (int ch = juce::jmax(0, startChannel); ch < juce::jmin(endChannelExclusive, buffer.getNumChannels()); ++ch)
        peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    return peak;
}

float channelRangeMaxWindowRmsDelta(const juce::AudioBuffer<float>& buffer,
                                    int startChannel,
                                    int endChannelExclusive,
                                    int startSample = 0,
                                    int windowSize = 64)
{
    juce::AudioBuffer<float> subset(juce::jmax(0, endChannelExclusive - startChannel), buffer.getNumSamples());
    subset.clear();

    int dst = 0;
    for (int ch = juce::jmax(0, startChannel); ch < juce::jmin(endChannelExclusive, buffer.getNumChannels()); ++ch, ++dst)
        subset.copyFrom(dst, 0, buffer, ch, 0, buffer.getNumSamples());

    return maxWindowRmsDelta(subset, startSample, windowSize);
}

void setStateValue(juce::ValueTree& state, const juce::String& paramId, const juce::var& value)
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

    throw std::runtime_error(("Missing state node for parameter: " + paramId).toStdString());
}

void setEnvironmentVariable(const juce::String& key, const juce::String& value)
{
#if JUCE_WINDOWS
    require(_putenv_s(key.toRawUTF8(), value.toRawUTF8()) == 0,
            "Failed to set environment variable: " + key);
#else
    if (value.isEmpty())
        require(unsetenv(key.toRawUTF8()) == 0, "Failed to clear environment variable: " + key);
    else
        require(setenv(key.toRawUTF8(), value.toRawUTF8(), 1) == 0,
                "Failed to set environment variable: " + key);
#endif
}

struct ScopedEnvironmentVariable
{
    juce::String key;
    juce::String previousValue;

    ScopedEnvironmentVariable(juce::String variableKey, const juce::String& newValue)
        : key(std::move(variableKey))
    {
        previousValue = juce::SystemStats::getEnvironmentVariable(key, {});
        setEnvironmentVariable(key, newValue);
    }

    ~ScopedEnvironmentVariable()
    {
        setEnvironmentVariable(key, previousValue);
    }
};

struct ScopedBassTestDataRoot
{
    juce::File root;
    ScopedEnvironmentVariable envOverride { "UWDEVST_BASS_DATA_ROOT", {} };

    ScopedBassTestDataRoot()
    {
        root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("uwdevst_bass_tests_" + juce::String(juce::Uuid().toString()));
        require(root.createDirectory(), "Failed to create temporary bass test data root");
        setEnvironmentVariable("UWDEVST_BASS_DATA_ROOT", root.getFullPathName());
    }

    ~ScopedBassTestDataRoot()
    {
        if (root.exists())
            root.deleteRecursively();
    }
};

void installModMatrixState(juce::XmlElement& xml,
                           int pitchBendRange = 2,
                           float lfo2Rate = 2.0f,
                           int lfo2Wave = 0)
{
    if (auto* existing = xml.getChildByName("ModMatrix"))
        xml.removeChildElement(existing, true);

    auto* matrix = xml.createNewChildElement("ModMatrix");
    matrix->setAttribute("pbRange", pitchBendRange);
    matrix->setAttribute("lfo2Rate", lfo2Rate);
    matrix->setAttribute("lfo2Wave", lfo2Wave);

    auto* modWheelToLevel = matrix->createNewChildElement("Slot");
    modWheelToLevel->setAttribute("idx", 0);
    modWheelToLevel->setAttribute("src", static_cast<int>(modmatrix::Source::ModWheel));
    modWheelToLevel->setAttribute("dst", static_cast<int>(modmatrix::Destination::Level));
    modWheelToLevel->setAttribute("amt", 0.75);

    auto* aftertouchToPitch = matrix->createNewChildElement("Slot");
    aftertouchToPitch->setAttribute("idx", 1);
    aftertouchToPitch->setAttribute("src", static_cast<int>(modmatrix::Source::Aftertouch));
    aftertouchToPitch->setAttribute("dst", static_cast<int>(modmatrix::Destination::Pitch));
    aftertouchToPitch->setAttribute("amt", 0.40);

    auto* velocityToCutoff = matrix->createNewChildElement("Slot");
    velocityToCutoff->setAttribute("idx", 2);
    velocityToCutoff->setAttribute("src", static_cast<int>(modmatrix::Source::Velocity));
    velocityToCutoff->setAttribute("dst", static_cast<int>(modmatrix::Destination::Cutoff));
    velocityToCutoff->setAttribute("amt", 0.55);
}

const juce::XmlElement* findModMatrixSlot(const juce::XmlElement& xml, int slotIndex)
{
    const auto* matrix = xml.getChildByName("ModMatrix");
    if (matrix == nullptr)
        return nullptr;

    for (auto* slotXml : matrix->getChildWithTagNameIterator("Slot"))
    {
        if (slotXml->getIntAttribute("idx", -1) == slotIndex)
            return slotXml;
    }

    return nullptr;
}

void requireCanonicalPresetXml(const juce::XmlElement& xml, bool expectFactoryIndex)
{
    require(xml.getIntAttribute("format_version", 0) == 3, "Preset XML must use format version 3");
    require(xml.hasAttribute("name"), "Preset XML must store name");
    require(xml.hasAttribute("bass"), "Preset XML must store legacy bass index");
    require(xml.hasAttribute("instrument_index"), "Preset XML must store canonical instrument index");
    require(xml.getIntAttribute("synth_index", -1) == 3, "Preset XML must store synth_index=3");
    if (expectFactoryIndex)
    {
        require(xml.hasAttribute("index"), "Factory preset XML must keep legacy factory index");
        require(xml.hasAttribute("preset_index"), "Factory preset XML must store canonical factory index");
    }

    for (const auto* attr : {
             "level", "tune", "brightness", "attack", "decay", "sustain", "release",
             "body", "drive", "pitch_env", "filter_env", "sub", "character", "cutoff", "pan",
             "resonance", "glide_time", "output",
             "mono_mode", "lfo_rate", "lfo_depth", "lfo_wave", "lfo_dest",
             "macro_fatness", "macro_brillance", "macro_punch", "macro_depth",
             "mod_wheel_target", "pitch_bend_range",
             "sat_drive", "sat_mix", "transient_attack", "transient_sustain", "transient_mix",
             "comp_threshold", "comp_ratio", "comp_attack", "comp_release", "comp_makeup", "comp_mix",
             "eq_low_freq", "eq_low_gain", "eq_mid_freq", "eq_mid_gain", "eq_mid_q", "eq_high_freq", "eq_high_gain",
             "chorus_rate", "chorus_depth", "chorus_mix",
             "delay_time", "delay_feedback", "delay_mix", "delay_sync", "delay_division",
             "reverb_size", "reverb_damping", "reverb_width", "reverb_mix",
             "limiter_threshold", "limiter_release",
             "fx_tab0_en", "fx_tab1_en", "fx_tab2_en", "fx_tab3_en",
             "fx_tab4_en", "fx_tab5_en", "fx_tab6_en", "fx_tab7_en",
             "mix_role", "family", "tags", "nominal_peak_db" })
    {
        require(xml.hasAttribute(attr), "Missing canonical preset attribute: " + juce::String(attr));
    }

    const auto* matrix = xml.getChildByName("ModMatrix");
    require(matrix != nullptr, "Preset XML must include ModMatrix");
    require(matrix->hasAttribute("pbRange"), "ModMatrix must store pbRange");
    require(matrix->hasAttribute("lfo2Rate"), "ModMatrix must store lfo2Rate");
    require(matrix->hasAttribute("lfo2Wave"), "ModMatrix must store lfo2Wave");

    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        const auto* slotXml = findModMatrixSlot(xml, slotIndex);
        require(slotXml != nullptr, "ModMatrix must materialize all 8 slots");
        require(slotXml->hasAttribute("src"), "ModMatrix slot must store source");
        require(slotXml->hasAttribute("dst"), "ModMatrix slot must store destination");
        require(slotXml->hasAttribute("amt"), "ModMatrix slot must store amount");
    }
}

std::unique_ptr<BassSynthAudioProcessor> makeProcessor()
{
    return std::make_unique<BassSynthAudioProcessor>();
}

void testFactoryBankShape()
{
    const auto& banks = mbs::getFactoryPresetBanks();
    require(static_cast<int>(banks.size()) == mbs::kNumBasses, "Factory bank count mismatch");

    int totalPresetCount = 0;
    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        const auto& bank = banks[static_cast<std::size_t>(bassIndex)];
        require(!bank.empty(), "Each bass must expose at least one factory preset");
        totalPresetCount += static_cast<int>(bank.size());

        for (const auto& preset : bank)
        {
            require(!preset.name.empty(), "Preset name cannot be empty");
            require(!preset.metadata.mixRole.empty(), "Preset mixRole metadata cannot be empty");
            require(!preset.metadata.familyLabel.empty(), "Preset family metadata cannot be empty");
            require(!preset.metadata.tags.empty(), "Preset tags metadata cannot be empty");
            require(preset.metadata.nominalPeakDb <= -1.0f && preset.metadata.nominalPeakDb >= -24.0f,
                    "Preset nominal peak metadata must stay in production-safe range");
            require(preset.outputBus >= 0 && preset.outputBus <= BassSynthAudioProcessor::kNumAuxOutputs,
                    "Preset output bus out of range");

            const auto availability = mbs::getFxAvailability(bassIndex);
            require(!preset.fx.saturatorOn || availability.saturator, "Unavailable saturator leaked into factory bank");
            require(!preset.fx.transientOn || availability.transient, "Unavailable transient leaked into factory bank");
            require(!preset.fx.compressorOn || availability.compressor, "Unavailable compressor leaked into factory bank");
            require(!preset.fx.eqOn || availability.eq, "Unavailable EQ leaked into factory bank");
            require(!preset.fx.chorusOn || availability.chorus, "Unavailable chorus leaked into factory bank");
            require(!preset.fx.delayOn || availability.delay, "Unavailable delay leaked into factory bank");
            require(!preset.fx.reverbOn || availability.reverb, "Unavailable reverb leaked into factory bank");
            require(!preset.fx.limiterOn || availability.limiter, "Unavailable limiter leaked into factory bank");
        }
    }

    require(totalPresetCount == static_cast<int>(mbs::getTotalFactoryPresetCount()),
            "Factory preset total must stay derived from the canonical bank definition");
    require(totalPresetCount >= mbs::kNumBasses * 20,
            "Factory bank coverage is unexpectedly low");
}

void testBootPresetMatchesFactory()
{
    auto processor = makeProcessor();
    const auto& bootPreset = mbs::getFactoryPresetBanks()[0][0];

    require(processor->getCurrentFactoryPresetIndex() == 0, "Boot preset index must default to 0");
    require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeBassParamId(0, "level"))->load()
        - bootPreset.settings.level) < 1.0e-4f, "Boot level must match first factory preset");
    require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeBassParamId(0, "output"))->load()
        - static_cast<float>(bootPreset.outputBus)) < 1.0e-4f, "Boot output bus must match first factory preset");
}

void testPresetStoragePaths()
{
    ScopedEnvironmentVariable envOverride("UWDEVST_BASS_DATA_ROOT", {});
    const auto userPresetDir = BassSynthAudioProcessor::getUserPresetsDirectory(0);
    const auto factoryOverrideDir = BassSynthAudioProcessor::getFactoryOverridesDirectory();
    require(userPresetDir.getFullPathName().containsIgnoreCase("MusiqueBassSynth"), "Unexpected bass app-data root");
    require(userPresetDir.getFullPathName().containsIgnoreCase("Presets"), "User presets must live in the preset library");
    require(factoryOverrideDir.getFullPathName().containsIgnoreCase("FactoryOverrides"), "Factory overrides must use FactoryOverrides");
}

void testPresetStoragePathsSupportOverride()
{
    ScopedBassTestDataRoot testDataRoot;
    const auto userPresetDir = BassSynthAudioProcessor::getUserPresetsDirectory(4);
    const auto factoryOverrideDir = BassSynthAudioProcessor::getFactoryOverridesDirectory();

    require(userPresetDir.isAChildOf(testDataRoot.root), "Override user preset directory must stay inside test root");
    require(factoryOverrideDir.isAChildOf(testDataRoot.root), "Override factory override directory must stay inside test root");
    require(userPresetDir.getFullPathName().containsIgnoreCase("Presets"), "Override preset directory must preserve Presets layout");
    require(factoryOverrideDir.getFullPathName().containsIgnoreCase("FactoryOverrides"), "Override factory override directory must preserve layout");
}

void testStateSanitization()
{
    auto processor = makeProcessor();
    auto state = processor->getAPVTS().copyState();

    setStateValue(state, "lfo_rate", "nan");
    setStateValue(state, "lfo_depth", 7.5);
    setStateValue(state, "selected_bass", 42);
    setStateValue(state, BassSynthAudioProcessor::makeBassParamId(0, "level"), -5.0);
    setStateValue(state, BassSynthAudioProcessor::makeBassParamId(0, "output"), 99);
    setStateValue(state, "mod_lfo2_rate", 99.0);
    setStateValue(state, "mod_lfo2_wave", 9);

    auto xml = state.createXml();
    require(xml != nullptr, "Failed to snapshot APVTS XML");
    installModMatrixState(*xml);

    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(*xml, block);
    processor->setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    const auto lfoRate = processor->getAPVTS().getRawParameterValue("lfo_rate")->load();
    const auto lfoDepth = processor->getAPVTS().getRawParameterValue("lfo_depth")->load();
    const auto selectedBass = processor->getAPVTS().getRawParameterValue("selected_bass")->load();
    const auto level = processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeBassParamId(0, "level"))->load();
    const auto outputBus = processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeBassParamId(0, "output"))->load();
    const auto modLfo2Rate = processor->getAPVTS().getRawParameterValue("mod_lfo2_rate")->load();
    const auto modLfo2Wave = processor->getAPVTS().getRawParameterValue("mod_lfo2_wave")->load();

    require(std::isfinite(lfoRate), "lfo_rate must remain finite");
    require(lfoDepth >= 0.0f && lfoDepth <= 1.0f, "lfo_depth must be clamped");
    require(selectedBass >= 0.0f && selectedBass < static_cast<float>(mbs::kNumBasses), "selected_bass must be clamped");
    require(level >= 0.0f && level <= 1.0f, "Bass level must be clamped");
    require(outputBus >= 0.0f && outputBus <= static_cast<float>(BassSynthAudioProcessor::kNumAuxOutputs), "Output bus must be clamped");
    require(modLfo2Rate >= 0.05f && modLfo2Rate <= 12.0f, "Mod LFO2 rate must be clamped");
    require(modLfo2Wave >= 0.0f && modLfo2Wave <= 3.0f, "Mod LFO2 wave must be clamped");
}

void testLegacyStateMigratesModMatrixParameters()
{
    auto processor = makeProcessor();
    auto state = processor->getAPVTS().copyState();
    auto xml = state.createXml();
    require(xml != nullptr, "Failed to snapshot APVTS XML");
    installModMatrixState(*xml, 7, 5.5f, 3);

    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(*xml, block);
    processor->setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    require(std::abs(processor->getAPVTS().getRawParameterValue("pitch_bend_range")->load() - 7.0f) < 1.0e-4f,
            "Legacy plugin state must migrate pitch bend range into APVTS");
    require(std::abs(processor->getAPVTS().getRawParameterValue("mod_lfo2_rate")->load() - 5.5f) < 1.0e-4f,
            "Legacy plugin state must migrate mod LFO2 rate into APVTS");
    require(std::abs(processor->getAPVTS().getRawParameterValue("mod_lfo2_wave")->load() - 3.0f) < 1.0e-4f,
            "Legacy plugin state must migrate mod LFO2 wave into APVTS");
    require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeModMatrixParamId(0, "source"))->load()
        - static_cast<float>(static_cast<int>(modmatrix::Source::ModWheel))) < 1.0e-4f,
            "Legacy plugin state must migrate mod matrix slot source into APVTS");
    require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeModMatrixParamId(1, "dest"))->load()
        - static_cast<float>(static_cast<int>(modmatrix::Destination::Pitch))) < 1.0e-4f,
            "Legacy plugin state must migrate mod matrix slot destination into APVTS");
}

void testUserPresetRoundTripWithManifest()
{
    ScopedBassTestDataRoot testDataRoot;
    auto processor = makeProcessor();
    setParameterValue(processor->getAPVTS(), "selected_bass", 3.0f);
    processor->applyFactoryPreset(1);
    const auto savedLevel = processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeBassParamId(3, "level"))->load();
    setParameterValue(processor->getAPVTS(), "mono_mode", 2.0f);
    setParameterValue(processor->getAPVTS(), "lfo_rate", 4.6f);
    setParameterValue(processor->getAPVTS(), "lfo_depth", 0.72f);
    setParameterValue(processor->getAPVTS(), "lfo_wave", 3.0f);
    setParameterValue(processor->getAPVTS(), "lfo_dest", 2.0f);
    setParameterValue(processor->getAPVTS(), "macro_punch", 0.81f);
    setParameterValue(processor->getAPVTS(), "mod_wheel_target", 0.0f);
    setParameterValue(processor->getAPVTS(), "pitch_bend_range", 12.0f);
    setParameterValue(processor->getAPVTS(), "mod_lfo2_rate", 5.5f);
    setParameterValue(processor->getAPVTS(), "mod_lfo2_wave", 2.0f);
    setParameterValue(processor->getAPVTS(), "delay_sync", 1.0f);
    setParameterValue(processor->getAPVTS(), "delay_division", 4.0f);
    setParameterValue(processor->getAPVTS(), "velocity_curve", 1.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(0, "source"), 5.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(0, "dest"), 4.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(0, "amount"), 0.65f);

    const auto uniqueName = "bass_roundtrip_" + juce::String(juce::Time::getCurrentTime().toMilliseconds());
    require(processor->saveUserPreset(uniqueName), "Failed to save bass user preset");
    const auto file = processor->getCurrentUserPresetFile();
    require(file.existsAsFile(), "Saved bass user preset file must exist");
    const auto savedXml = juce::XmlDocument::parse(file);
    require(savedXml != nullptr, "Saved bass user preset XML must parse");
    requireCanonicalPresetXml(*savedXml, false);
    require(savedXml->getStringAttribute("mix_role") == "custom", "User preset XML must default mix_role to custom");
    require(savedXml->getStringAttribute("family") == "808", "User preset XML must derive family from selected bass");
    require(savedXml->getStringAttribute("tags").contains("bass,user,custom,808"), "User preset XML must include canonical bass user tags");

    const auto manifestFile = musique::preset::manifestFileForPresetFile(file);
    require(manifestFile.existsAsFile(), "Saved bass user preset manifest must exist");

    musique::preset::PresetManifest manifest;
    require(musique::preset::loadManifestFromFile(manifestFile, manifest), "Saved bass preset manifest must parse");
    require(manifest.synthType == "bass", "Saved bass preset manifest must identify synth type");
    require(manifest.instrumentIndex == 3, "Saved bass preset manifest must preserve bass index");

    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(3, "level"), 0.17f);
    setParameterValue(processor->getAPVTS(), "mono_mode", 0.0f);
    setParameterValue(processor->getAPVTS(), "lfo_rate", 0.3f);
    setParameterValue(processor->getAPVTS(), "lfo_depth", 0.0f);
    setParameterValue(processor->getAPVTS(), "lfo_wave", 0.0f);
    setParameterValue(processor->getAPVTS(), "lfo_dest", 0.0f);
    setParameterValue(processor->getAPVTS(), "macro_punch", 0.15f);
    setParameterValue(processor->getAPVTS(), "mod_wheel_target", 1.0f);
    setParameterValue(processor->getAPVTS(), "pitch_bend_range", 2.0f);
    setParameterValue(processor->getAPVTS(), "mod_lfo2_rate", 0.5f);
    setParameterValue(processor->getAPVTS(), "mod_lfo2_wave", 0.0f);
    setParameterValue(processor->getAPVTS(), "delay_sync", 0.0f);
    setParameterValue(processor->getAPVTS(), "delay_division", 0.0f);
    setParameterValue(processor->getAPVTS(), "velocity_curve", 5.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(0, "source"), 0.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(0, "dest"), 0.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(0, "amount"), 0.0f);
    require(processor->loadUserPreset(file), "Failed to reload saved bass user preset");
    require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeBassParamId(3, "level"))->load() - savedLevel) < 1.0e-4f,
            "Reloaded bass preset must restore saved level");
    require(std::abs(processor->getAPVTS().getRawParameterValue("mono_mode")->load() - 2.0f) < 1.0e-4f,
            "Reloaded bass preset must restore mono mode");
    require(std::abs(processor->getAPVTS().getRawParameterValue("lfo_rate")->load() - 4.6f) < 1.0e-4f,
            "Reloaded bass preset must restore LFO rate");
    require(std::abs(processor->getAPVTS().getRawParameterValue("lfo_depth")->load() - 0.72f) < 1.0e-4f,
            "Reloaded bass preset must restore LFO depth");
    require(std::abs(processor->getAPVTS().getRawParameterValue("lfo_wave")->load() - 3.0f) < 1.0e-4f,
            "Reloaded bass preset must restore LFO wave");
    require(std::abs(processor->getAPVTS().getRawParameterValue("lfo_dest")->load() - 2.0f) < 1.0e-4f,
            "Reloaded bass preset must restore LFO destination");
    require(std::abs(processor->getAPVTS().getRawParameterValue("macro_punch")->load() - 0.81f) < 1.0e-4f,
            "Reloaded bass preset must restore macro state");
    require(std::abs(processor->getAPVTS().getRawParameterValue("mod_wheel_target")->load() - 0.0f) < 1.0e-4f,
            "Reloaded bass preset must restore mod wheel target");
    require(std::abs(processor->getAPVTS().getRawParameterValue("pitch_bend_range")->load() - 12.0f) < 1.0e-4f,
            "Reloaded bass preset must restore pitch bend range");
    require(std::abs(processor->getAPVTS().getRawParameterValue("mod_lfo2_rate")->load() - 5.5f) < 1.0e-4f,
            "Reloaded bass preset must restore mod LFO2 rate");
    require(std::abs(processor->getAPVTS().getRawParameterValue("mod_lfo2_wave")->load() - 2.0f) < 1.0e-4f,
            "Reloaded bass preset must restore mod LFO2 wave");
    require(std::abs(processor->getAPVTS().getRawParameterValue("delay_sync")->load() - 1.0f) < 1.0e-4f,
            "Reloaded bass preset must restore delay sync");
    require(std::abs(processor->getAPVTS().getRawParameterValue("delay_division")->load() - 4.0f) < 1.0e-4f,
            "Reloaded bass preset must restore delay note division");
    require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeModMatrixParamId(0, "source"))->load() - 5.0f) < 1.0e-4f,
            "Reloaded bass preset must restore mod matrix source");
    require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeModMatrixParamId(0, "dest"))->load() - 4.0f) < 1.0e-4f,
            "Reloaded bass preset must restore mod matrix destination");
    require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeModMatrixParamId(0, "amount"))->load() - 0.65f) < 1.0e-4f,
            "Reloaded bass preset must restore mod matrix amount");
    require(std::abs(processor->getAPVTS().getRawParameterValue("velocity_curve")->load() - 5.0f) < 1.0e-4f,
            "Velocity curve must remain a global preference outside bass presets");

    require(processor->deleteUserPreset(file), "Temporary bass preset cleanup failed");
    require(!manifestFile.existsAsFile(), "Deleting a bass preset must remove its manifest");
}

void testFactoryOverrideRewriteUsesCanonicalSchema()
{
    ScopedBassTestDataRoot testDataRoot;
    const auto overrideFile = BassSynthAudioProcessor::getFactoryOverridesDirectory()
                                  .getChildFile("bass_6")
                                  .getChildFile("1.xml");
    require(overrideFile.getParentDirectory().createDirectory(), "Failed to create bass override directory");

    juce::XmlElement xml("FactoryPreset");
    xml.setAttribute("format_version", 1);
    xml.setAttribute("name", "Legacy Bass Override");
    xml.setAttribute("bass", 6);
    xml.setAttribute("index", 1);
    xml.setAttribute("level", 0.79);
    xml.setAttribute("delay_sync", 1);
    require(xml.writeTo(overrideFile), "Failed to seed legacy bass factory override");

    auto processor = makeProcessor();
    const auto rewrittenXml = juce::XmlDocument::parse(overrideFile);
    require(rewrittenXml != nullptr, "Rewritten bass factory override must parse");
    requireCanonicalPresetXml(*rewrittenXml, true);
    require(rewrittenXml->getStringAttribute("family") == "synth", "Factory override rewrite must preserve family metadata");
}

void testFactoryOverrideRoundTripPersistsPerformanceAndModMatrix()
{
    ScopedBassTestDataRoot testDataRoot;

    {
        auto processor = makeProcessor();
        setParameterValue(processor->getAPVTS(), "selected_bass", 6.0f);
        processor->applyFactoryPreset(1);
        setParameterValue(processor->getAPVTS(), "mono_mode", 1.0f);
        setParameterValue(processor->getAPVTS(), "pitch_bend_range", 9.0f);
        setParameterValue(processor->getAPVTS(), "mod_lfo2_rate", 6.25f);
        setParameterValue(processor->getAPVTS(), "mod_lfo2_wave", 3.0f);
        setParameterValue(processor->getAPVTS(), "delay_sync", 1.0f);
        setParameterValue(processor->getAPVTS(), "delay_division", 3.0f);
        setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(0, "source"), 2.0f);
        setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(0, "dest"), 8.0f);
        setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeModMatrixParamId(0, "amount"), -0.4f);
        require(processor->saveFactoryPreset(1), "Failed to save bass factory override");

        const auto overrideFile = BassSynthAudioProcessor::getFactoryOverridesDirectory()
                                      .getChildFile("bass_6")
                                      .getChildFile("1.xml");
        require(overrideFile.existsAsFile(), "Factory override file must be written to disk");
    }

    {
        auto processor = makeProcessor();
        setParameterValue(processor->getAPVTS(), "selected_bass", 6.0f);
        setParameterValue(processor->getAPVTS(), "velocity_curve", 5.0f);
        processor->applyFactoryPreset(1);

        require(std::abs(processor->getAPVTS().getRawParameterValue("mono_mode")->load() - 1.0f) < 1.0e-4f,
                "Factory override must restore mono mode");
        require(std::abs(processor->getAPVTS().getRawParameterValue("pitch_bend_range")->load() - 9.0f) < 1.0e-4f,
                "Factory override must restore pitch bend range");
        require(std::abs(processor->getAPVTS().getRawParameterValue("mod_lfo2_rate")->load() - 6.25f) < 1.0e-4f,
                "Factory override must restore mod LFO2 rate");
        require(std::abs(processor->getAPVTS().getRawParameterValue("mod_lfo2_wave")->load() - 3.0f) < 1.0e-4f,
                "Factory override must restore mod LFO2 wave");
        require(std::abs(processor->getAPVTS().getRawParameterValue("delay_sync")->load() - 1.0f) < 1.0e-4f,
                "Factory override must restore delay sync");
        require(std::abs(processor->getAPVTS().getRawParameterValue("delay_division")->load() - 3.0f) < 1.0e-4f,
                "Factory override must restore delay note division");
        require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeModMatrixParamId(0, "source"))->load() - 2.0f) < 1.0e-4f,
                "Factory override must restore mod matrix source");
        require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeModMatrixParamId(0, "dest"))->load() - 8.0f) < 1.0e-4f,
                "Factory override must restore mod matrix destination");
        require(std::abs(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeModMatrixParamId(0, "amount"))->load() + 0.4f) < 1.0e-4f,
                "Factory override must restore mod matrix amount");
        require(std::abs(processor->getAPVTS().getRawParameterValue("velocity_curve")->load() - 5.0f) < 1.0e-4f,
                "Factory override must preserve global velocity curve preference");
    }
}

void testLegacyUserPresetLoadRegeneratesManifest()
{
    ScopedBassTestDataRoot testDataRoot;
    auto processor = makeProcessor();
    setParameterValue(processor->getAPVTS(), "selected_bass", 6.0f);

    const auto file = BassSynthAudioProcessor::getUserPresetsDirectory(6).getChildFile(
        "bass_legacy_" + juce::String(juce::Time::getCurrentTime().toMilliseconds()) + ".xml");
    const auto manifestFile = musique::preset::manifestFileForPresetFile(file);

    juce::XmlElement xml("BassPreset");
    xml.setAttribute("format_version", 1);
    xml.setAttribute("name", "Legacy");
    xml.setAttribute("bass", 6);
    xml.setAttribute("level", 0.73);
    xml.setAttribute("output", 4);
    require(xml.writeTo(file), "Failed to write temporary legacy bass preset");

    require(processor->loadUserPreset(file), "Failed to load legacy bass preset");
    require(manifestFile.existsAsFile(), "Legacy bass preset load must regenerate manifest");
    const auto rewrittenXml = juce::XmlDocument::parse(file);
    require(rewrittenXml != nullptr, "Legacy bass preset rewrite must stay parseable");
    requireCanonicalPresetXml(*rewrittenXml, false);
    require(processor->getAPVTS().getRawParameterValue(BassSynthAudioProcessor::makeBassParamId(6, "output"))->load()
        <= static_cast<float>(BassSynthAudioProcessor::kNumAuxOutputs), "Legacy preset output must be sanitized");

    require(processor->deleteUserPreset(file), "Temporary legacy bass preset cleanup failed");
}

void testMainAndAuxRouting()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 512);
    setParameterValue(processor->getAPVTS(), "selected_bass", 0.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(0, "output"), 1.0f);

    juce::AudioBuffer<float> buffer(10, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100), 0);
    processor->processBlock(buffer, midi);

    const auto mainMagnitude = juce::jmax(buffer.getMagnitude(0, 0, buffer.getNumSamples()),
                                          buffer.getMagnitude(1, 0, buffer.getNumSamples()));
    const auto auxMagnitude = juce::jmax(buffer.getMagnitude(2, 0, buffer.getNumSamples()),
                                         buffer.getMagnitude(3, 0, buffer.getNumSamples()));

    require(mainMagnitude > 0.0f, "Main bus must contain post-master audio");
    require(auxMagnitude > 0.0f, "Aux bus must contain dry/direct stems");
}

void testModulationMatrixAudibility()
{
    auto processor = makeProcessor();
    auto state = processor->getAPVTS().copyState();
    auto xml = state.createXml();
    require(xml != nullptr, "Failed to snapshot APVTS XML");
    installModMatrixState(*xml);

    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(*xml, block);
    processor->setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);
    setParameterValue(processor->getAPVTS(), "selected_bass", 6.0f);
    processor->applyFactoryPreset(0);

    const std::vector<std::pair<int, juce::MidiMessage>> baselineEvents = {
        { 0, juce::MidiMessage::noteOn(1, 40, (juce::uint8) 70) },
        { 768, juce::MidiMessage::noteOff(1, 40) }
    };
    const std::vector<std::pair<int, juce::MidiMessage>> modWheelEvents = {
        { 0, juce::MidiMessage::controllerEvent(1, 1, 127) },
        { 0, juce::MidiMessage::noteOn(1, 40, (juce::uint8) 70) },
        { 768, juce::MidiMessage::noteOff(1, 40) }
    };
    const std::vector<std::pair<int, juce::MidiMessage>> aftertouchEvents = {
        { 0, juce::MidiMessage::noteOn(1, 40, (juce::uint8) 70) },
        { 128, juce::MidiMessage::channelPressureChange(1, 110) },
        { 768, juce::MidiMessage::noteOff(1, 40) }
    };

    const auto baseline = renderWithMidi(*processor, baselineEvents, 4096);
    const auto modWheel = renderWithMidi(*processor, modWheelEvents, 4096);
    const auto aftertouch = renderWithMidi(*processor, aftertouchEvents, 4096);

    require(bufferIsFinite(modWheel) && bufferIsFinite(aftertouch), "Modulated renders must stay finite");
    require(bufferDifference(baseline, modWheel) > 0.5f, "Mod wheel route must produce an audible render difference");
    require(bufferDifference(baseline, aftertouch) > 0.5f, "Aftertouch route must produce an audible render difference");
}

void testFactoryPresetNamesAreUniquePerBank()
{
    const auto& banks = mbs::getFactoryPresetBanks();
    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        juce::StringArray names;
        const auto& bank = banks[static_cast<std::size_t>(bassIndex)];
        for (const auto& preset : bank)
        {
            const auto key = juce::String(juce::CharPointer_UTF8(preset.name.c_str())).trim().toLowerCase();
            require(!names.contains(key), "Factory bank contains duplicate preset name: " + key);
            names.add(key);
        }
    }
}

void testFactoryPresetReleaseMetadataCoherence()
{
    static const juce::StringArray allowedRoles {
        "organic-foundation", "sub-foundation", "character-bass",
        "lead-bass", "texture-bass", "transient-bass"
    };
    static const std::array<const char*, mbs::kNumBasses> bassTags = {{
        "double-bass", "fingered-bass", "slap-bass",
        "sub-808", "boom-808", "distorted-808",
        "moog-bass", "reese-bass", "acid-bass"
    }};

    const auto& banks = mbs::getFactoryPresetBanks();
    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        const auto familyEnum = mbs::getFamily(bassIndex);
        const auto expectedFamily = familyEnum == mbs::Family::Acoustic ? juce::String("acoustic")
                                 : familyEnum == mbs::Family::Eight08 ? juce::String("808")
                                                                      : juce::String("synth");
        const auto expectedBassTag = juce::String(bassTags[static_cast<std::size_t>(bassIndex)]);
        const auto& bank = banks[static_cast<std::size_t>(bassIndex)];

        for (const auto& preset : bank)
        {
            const auto name = juce::String(juce::CharPointer_UTF8(preset.name.c_str())).toLowerCase();
            const auto role = juce::String(juce::CharPointer_UTF8(preset.metadata.mixRole.c_str()));
            const auto family = juce::String(juce::CharPointer_UTF8(preset.metadata.familyLabel.c_str())).toLowerCase();
            const auto tags = splitCsvTags(preset.metadata.tags);

            require(stringArrayContainsIgnoreCase(allowedRoles, role), "Unsupported factory mix role: " + role);
            require(family == expectedFamily, "Factory preset family metadata must match its bank family");
            require(stringArrayContainsIgnoreCase(tags, "bass"), "Factory preset tags must include bass");
            require(stringArrayContainsIgnoreCase(tags, "factory"), "Factory preset tags must include factory");
            require(stringArrayContainsIgnoreCase(tags, expectedFamily), "Factory preset tags must include family");
            require(stringArrayContainsIgnoreCase(tags, expectedBassTag), "Factory preset tags must include model tag");
            require(stringArrayContainsIgnoreCase(tags, role), "Factory preset tags must include mix role");

            if (name.contains("ambient") || name.contains("pad") || name.contains("drone"))
                require(role == "texture-bass", "Ambient/pad/drone presets must be tagged texture-bass");
            if (name.contains("slap") || name.contains("punch") || name.contains("attack") || name.contains("staccato"))
                require(role == "transient-bass", "Percussive presets must be tagged transient-bass");
        }
    }
}

void testFactoryRoleCoverageByFamily()
{
    const auto& banks = mbs::getFactoryPresetBanks();
    bool acousticFoundation = false;
    bool acousticTransient = false;
    bool eight08Foundation = false;
    bool eight08Transient = false;
    bool synthCharacter = false;
    bool synthLead = false;
    bool synthTexture = false;

    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        const auto family = mbs::getFamily(bassIndex);
        const auto& bank = banks[static_cast<std::size_t>(bassIndex)];
        for (const auto& preset : bank)
        {
            const auto role = juce::String(juce::CharPointer_UTF8(preset.metadata.mixRole.c_str()));
            if (family == mbs::Family::Acoustic)
            {
                acousticFoundation = acousticFoundation || role == "organic-foundation";
                acousticTransient = acousticTransient || role == "transient-bass";
            }
            else if (family == mbs::Family::Eight08)
            {
                eight08Foundation = eight08Foundation || role == "sub-foundation";
                eight08Transient = eight08Transient || role == "transient-bass";
            }
            else
            {
                synthCharacter = synthCharacter || role == "character-bass";
                synthLead = synthLead || role == "lead-bass";
                synthTexture = synthTexture || role == "texture-bass";
            }
        }
    }

    require(acousticFoundation, "Acoustic family must keep at least one organic-foundation preset");
    require(acousticTransient, "Acoustic family must keep at least one transient preset");
    require(eight08Foundation, "808 family must keep at least one sub-foundation preset");
    require(eight08Transient, "808 family must keep at least one transient preset");
    require(synthCharacter, "Synth family must keep at least one character-bass preset");
    require(synthLead, "Synth family must keep at least one lead-bass preset");
    require(synthTexture, "Synth family must keep at least one texture-bass preset");
}

void testSubControlRendersTrueSubOctave()
{
    auto settings = mbs::getDefaultSettings(3);
    settings.level = 1.0f;
    settings.attackSeconds = 0.0f;
    settings.decaySeconds = 2.0f;
    settings.sustainLevel = 1.0f;
    settings.releaseSeconds = 0.1f;
    settings.cutoffHz = 12000.0f;
    settings.resonance = 0.0f;
    settings.brightness = 0.0f;
    settings.drive = 0.0f;
    settings.pitchEnv = 0.0f;
    settings.subLevel = 0.0f;

    const auto chars = mbs::getCharacteristics(3);
    constexpr int totalSamples = 48000;
    constexpr double sampleRate = 48000.0;
    constexpr int midiNote = 48;

    const auto withoutSub = renderBassVoice(settings, chars, midiNote, 1.0f, sampleRate, totalSamples);

    settings.subLevel = 1.0f;
    const auto withSub = renderBassVoice(settings, chars, midiNote, 1.0f, sampleRate, totalSamples);

    juce::AudioBuffer<float> residual(withSub.getNumChannels(), withSub.getNumSamples());
    residual.makeCopyOf(withSub, true);
    for (int ch = 0; ch < residual.getNumChannels(); ++ch)
        residual.addFrom(ch, 0, withoutSub, ch, 0, residual.getNumSamples(), -1.0f);

    const double estimatedHz = estimateDominantAutocorrelationFrequency(
        residual, sampleRate, 12000, 16000, 40.0, 120.0);
    const double expectedHz = juce::MidiMessage::getMidiNoteInHertz(midiNote) * 0.5;

    require(std::abs(estimatedHz - expectedHz) < 6.0,
            "Sub control must add a true octave-below component");
}

void test808LfoTremPanIsAudible()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);
    setParameterValue(processor->getAPVTS(), "selected_bass", 3.0f);
    processor->applyFactoryPreset(0);
    setParameterValue(processor->getAPVTS(), "lfo_rate", 4.0f);
    setParameterValue(processor->getAPVTS(), "lfo_dest", 0.0f);

    const std::vector<std::pair<int, juce::MidiMessage>> events = {
        { 0, juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100) },
        { 2048, juce::MidiMessage::noteOff(1, 40) }
    };

    setParameterValue(processor->getAPVTS(), "lfo_depth", 0.0f);
    const auto baseline = renderWithMidi(*processor, events, 4096);

    setParameterValue(processor->getAPVTS(), "lfo_depth", 1.0f);
    const auto modulated = renderWithMidi(*processor, events, 4096);

    require(bufferDifference(baseline, modulated) > 0.5f,
            "808 LFO trem/pan path must produce an audible difference");
}

void test808LfoCutoffIsAudible()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);
    setParameterValue(processor->getAPVTS(), "selected_bass", 3.0f);
    processor->applyFactoryPreset(0);
    setParameterValue(processor->getAPVTS(), "lfo_rate", 3.0f);
    setParameterValue(processor->getAPVTS(), "lfo_dest", 1.0f);

    const std::vector<std::pair<int, juce::MidiMessage>> events = {
        { 0, juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100) },
        { 2048, juce::MidiMessage::noteOff(1, 40) }
    };

    setParameterValue(processor->getAPVTS(), "lfo_depth", 0.0f);
    const auto baseline = renderWithMidi(*processor, events, 4096);

    setParameterValue(processor->getAPVTS(), "lfo_depth", 1.0f);
    const auto modulated = renderWithMidi(*processor, events, 4096);

    require(bufferDifference(baseline, modulated) > 0.2f,
            "808 LFO cutoff path must no longer be inert");
}

void testUiHidesUnsupportedFamilyControls()
{
    juce::ScopedJuceInitialiser_GUI gui;

    auto processor = makeProcessor();
    BassSynthAudioProcessorEditor editor(*processor);
    editor.setSize(1100, 780);

    setParameterValue(processor->getAPVTS(), "selected_bass", 3.0f);
    editor.refreshBassUiForTests();
    require(!editor.isEnvControlVisibleForTests(7), "808 UI must hide unsupported Body control");
    require(editor.isEnvControlVisibleForTests(9), "808 UI must keep Pitch Env visible");

    setParameterValue(processor->getAPVTS(), "selected_bass", 6.0f);
    editor.refreshBassUiForTests();
    require(editor.isEnvControlVisibleForTests(7), "Synth UI must expose a dedicated filter envelope control");
    require(editor.getEnvControlLabelForTests(7).containsIgnoreCase("filter"),
            "Synth UI must relabel the reused slot as Filter Env");
    require(!editor.isEnvControlVisibleForTests(9), "Synth UI must hide unsupported Pitch Env control");

    setParameterValue(processor->getAPVTS(), "selected_bass", 0.0f);
    editor.refreshBassUiForTests();
    require(editor.isEnvControlVisibleForTests(7), "Acoustic UI must keep Body visible");
    require(!editor.isEnvControlVisibleForTests(9), "Acoustic UI must hide unsupported Pitch Env control");
}

void testSynthFilterEnvIsAudible()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);
    setParameterValue(processor->getAPVTS(), "selected_bass", 6.0f);
    processor->applyFactoryPreset(0);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(6, "cutoff"), 450.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(6, "brightness"), 0.45f);

    const std::vector<std::pair<int, juce::MidiMessage>> events = {
        { 0, juce::MidiMessage::noteOn(1, 43, (juce::uint8) 100) },
        { 2048, juce::MidiMessage::noteOff(1, 43) }
    };

    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(6, "filter_env"), 0.0f);
    const auto flat = renderWithMidi(*processor, events, 4096);

    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(6, "filter_env"), 1.0f);
    const auto swept = renderWithMidi(*processor, events, 4096);

    require(bufferDifference(flat, swept) > 4.0f,
            "Synth filter env must audibly change the contour with brightness held constant");
}

void testBrightnessStillChangesTimbreWithZeroFilterEnv()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);
    setParameterValue(processor->getAPVTS(), "selected_bass", 7.0f);
    processor->applyFactoryPreset(0);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(7, "filter_env"), 0.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(7, "cutoff"), 900.0f);

    const std::vector<std::pair<int, juce::MidiMessage>> events = {
        { 0, juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100) },
        { 2048, juce::MidiMessage::noteOff(1, 40) }
    };

    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(7, "brightness"), 0.15f);
    const auto dark = renderWithMidi(*processor, events, 4096);

    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(7, "brightness"), 0.85f);
    const auto bright = renderWithMidi(*processor, events, 4096);

    require(bufferDifference(dark, bright) > 3.0f,
            "Brightness must remain an audible timbre control after filter env decoupling");
}

void testGlideTimeSupportsLongerPortamento()
{
    auto processor = makeProcessor();
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(processor->getAPVTS().getParameter("glide_time"));
    require(parameter != nullptr, "Glide time parameter must exist");
    const auto range = parameter->getNormalisableRange();
    require(range.end >= 1.49f, "Glide time must support longer portamento than the previous 0.5 s ceiling");
}

void testVoiceStealStressRendersCleanly()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);
    setParameterValue(processor->getAPVTS(), "selected_bass", 8.0f);
    processor->applyFactoryPreset(2);

    std::vector<std::pair<int, juce::MidiMessage>> events;
    for (int i = 0; i < 36; ++i)
    {
        const int sample = i * 36;
        events.push_back({ sample, juce::MidiMessage::noteOn(1, 40 + (i % 4), (juce::uint8) 96) });
        events.push_back({ sample + 18, juce::MidiMessage::noteOff(1, 40 + (i % 4)) });
    }

    const auto rendered = renderWithMidi(*processor, events, 4096);
    require(bufferIsFinite(rendered), "Voice steal stress render must stay finite");
}

void testAllFactoryPresetsRenderStable()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);

    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        setParameterValue(processor->getAPVTS(), "selected_bass", static_cast<float>(bassIndex));
        const auto& bank = mbs::getFactoryPresetBanks()[static_cast<std::size_t>(bassIndex)];
        for (int presetIndex = 0; presetIndex < static_cast<int>(bank.size()); ++presetIndex)
        {
            processor->applyFactoryPreset(presetIndex);
            const std::vector<std::pair<int, juce::MidiMessage>> events = {
                { 0, juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100) },
                { 800, juce::MidiMessage::noteOff(1, 40) }
            };
            const auto rendered = renderWithMidi(*processor, events, 4096);
            require(bufferIsFinite(rendered), "Factory preset render must stay finite");
            require(bufferPeak(rendered) >= musique::qa::minimumAudiblePeakLinear(), "Factory preset render must not be silent");
            require(bufferPeak(rendered) <= musique::qa::maximumSafePeakLinear(), "Factory preset render must not clip past QA ceiling");
        }
    }
}

void testDeterministicOfflineRender()
{
    auto first = makeProcessor();
    auto second = makeProcessor();
    first->enableAllBuses();
    second->enableAllBuses();
    first->prepareToPlay(48000.0, 256);
    second->prepareToPlay(48000.0, 256);
    setParameterValue(first->getAPVTS(), "selected_bass", 3.0f);
    setParameterValue(second->getAPVTS(), "selected_bass", 3.0f);
    first->applyFactoryPreset(2);
    second->applyFactoryPreset(2);

    const std::vector<std::pair<int, juce::MidiMessage>> events = {
        { 0, juce::MidiMessage::noteOn(1, 40, (juce::uint8) 92) },
        { 1024, juce::MidiMessage::noteOff(1, 40) }
    };

    const auto renderedA = renderWithMidi(*first, events, 8192);
    const auto renderedB = renderWithMidi(*second, events, 8192);
    require(bufferDifference(renderedA, renderedB) < 1.0e-4f, "Offline renders must remain deterministic");
}

void testRenderStabilityAcrossSampleRatesAndBlocks()
{
    static constexpr double kSampleRates[] = { 44100.0, 48000.0, 96000.0 };
    static constexpr int kBlockSizes[] = { 64, 256, 1024 };

    for (const auto sampleRate : kSampleRates)
    {
        for (const auto blockSize : kBlockSizes)
        {
            auto processor = makeProcessor();
            processor->enableAllBuses();
            processor->prepareToPlay(sampleRate, blockSize);
            setParameterValue(processor->getAPVTS(), "selected_bass", 6.0f);
            processor->applyFactoryPreset(1);

            const std::vector<std::pair<int, juce::MidiMessage>> events = {
                { 0, juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100) },
                { blockSize * 3, juce::MidiMessage::noteOff(1, 40) }
            };

            const auto rendered = renderWithMidi(*processor, events, blockSize * 24, blockSize);
            require(bufferIsFinite(rendered), "Render must remain finite across sample-rate/block-size matrix");
            require(bufferPeak(rendered) > 0.0001f, "Render matrix must remain audible");
        }
    }
}

void testDelayAndReverbTailsFadeSmoothly()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);
    setParameterValue(processor->getAPVTS(), "selected_bass", 0.0f);
    processor->applyFactoryPreset(0);
    setParameterValue(processor->getAPVTS(), "fx_tab5_en", 1.0f);
    setParameterValue(processor->getAPVTS(), "fx_tab6_en", 1.0f);
    setParameterValue(processor->getAPVTS(), "delay_mix", 0.42f);
    setParameterValue(processor->getAPVTS(), "delay_time", 320.0f);
    setParameterValue(processor->getAPVTS(), "reverb_mix", 0.28f);
    setParameterValue(processor->getAPVTS(), "reverb_size", 0.62f);

    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 72;
    const int totalChannels = processor->getTotalNumOutputChannels();
    juce::AudioBuffer<float> rendered(totalChannels, kBlockSize * kBlocks);
    rendered.clear();

    for (int blockIndex = 0; blockIndex < kBlocks; ++blockIndex)
    {
        if (blockIndex == 20)
        {
            setParameterValue(processor->getAPVTS(), "fx_tab5_en", 0.0f);
            setParameterValue(processor->getAPVTS(), "fx_tab6_en", 0.0f);
        }

        juce::AudioBuffer<float> block(totalChannels, kBlockSize);
        block.clear();
        juce::MidiBuffer midi;
        if (blockIndex == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100), 0);
        if (blockIndex == 8)
            midi.addEvent(juce::MidiMessage::noteOff(1, 40), 0);

        processor->processBlock(block, midi);
        for (int ch = 0; ch < totalChannels; ++ch)
            rendered.copyFrom(ch, blockIndex * kBlockSize, block, ch, 0, kBlockSize);
    }

    require(bufferIsFinite(rendered), "Delay/reverb tail fade render must stay finite");
    require(bufferPeak(rendered) > 0.001f, "Delay/reverb tail fade render must remain audible");
    require(maxWindowRmsDelta(rendered, kBlockSize * 18, 64) < 0.20f,
            "Disabling delay/reverb must fade tails without abrupt discontinuities");
}

void testAutomationSweepsRenderCleanly()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);
    setParameterValue(processor->getAPVTS(), "selected_bass", 0.0f);
    processor->applyFactoryPreset(0);
    setParameterValue(processor->getAPVTS(), "fx_tab5_en", 1.0f);
    setParameterValue(processor->getAPVTS(), "fx_tab6_en", 1.0f);
    setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(0, "release"), 0.55f);

    constexpr int kBlockSize = 256;
    constexpr int kBlocks = 96;
    const int totalChannels = processor->getTotalNumOutputChannels();
    juce::AudioBuffer<float> rendered(totalChannels, kBlockSize * kBlocks);
    rendered.clear();

    for (int blockIndex = 0; blockIndex < kBlocks; ++blockIndex)
    {
        const float t = static_cast<float>(blockIndex) / static_cast<float>(kBlocks - 1);
        setParameterValue(processor->getAPVTS(), "output_gain", -18.0f + t * 15.0f);
        setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(0, "cutoff"), 120.0f + t * 4880.0f);
        setParameterValue(processor->getAPVTS(), "delay_mix", 0.02f + t * 0.43f);
        setParameterValue(processor->getAPVTS(), "delay_time", 90.0f + t * 390.0f);
        setParameterValue(processor->getAPVTS(), "reverb_mix", 0.01f + t * 0.34f);

        const int outputBus = blockIndex < kBlocks / 3 ? 0 : (blockIndex < (2 * kBlocks) / 3 ? 1 : 0);
        setParameterValue(processor->getAPVTS(), BassSynthAudioProcessor::makeBassParamId(0, "output"), static_cast<float>(outputBus));

        juce::AudioBuffer<float> block(totalChannels, kBlockSize);
        block.clear();
        juce::MidiBuffer midi;
        if (blockIndex == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, 40, (juce::uint8) 100), 0);
        if (blockIndex == kBlocks - 18)
            midi.addEvent(juce::MidiMessage::noteOff(1, 40), 0);

        processor->processBlock(block, midi);
        for (int ch = 0; ch < totalChannels; ++ch)
            rendered.copyFrom(ch, blockIndex * kBlockSize, block, ch, 0, kBlockSize);
    }

    require(bufferIsFinite(rendered), "Automation sweep render must stay finite");
    require(bufferPeak(rendered) > 0.001f, "Automation sweep render must remain audible");
    require(channelRangePeak(rendered, 0, 2) > 0.001f, "Automation sweep must keep signal on the main output");
    require(channelRangePeak(rendered, 2, 4) > 0.001f, "Automation sweep must keep signal on the routed aux output");
    require(channelRangeMaxWindowRmsDelta(rendered, 0, 2, 2048, 64) < 0.20f,
            "Automation sweeps must avoid coarse envelope discontinuities on the main output");
}

void testEditorLayoutSnapshot()
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto validateSnapshot = [](const BassSynthAudioProcessorEditor::LayoutSnapshot& snapshot,
                                     bool expectCompact)
    {
        require(snapshot.compact == expectCompact, "Layout compact mode expectation mismatch");
        require(snapshot.headerBounds.contains(snapshot.gainBounds), "Output knob must remain inside the header");
        require(snapshot.headerBounds.contains(snapshot.gainSlotBounds), "Output knob slot must remain inside the header");
        require(snapshot.statusPrimaryBounds.contains(snapshot.randBounds), "RAND must stay inside the primary status row");
        require(snapshot.statusSecondaryBounds.contains(snapshot.tooltipBounds), "Tooltip mode button must stay inside the secondary status row");
        require(snapshot.statusSecondaryBounds.contains(snapshot.voiceBounds), "Voice counter must stay inside the secondary status row");
        require(snapshot.statusSecondaryBounds.contains(snapshot.ccBounds), "CC page label must stay inside the secondary status row");
        require(!snapshot.gainBounds.intersects(snapshot.randBounds), "Output knob must not overlap RAND");
        require(!snapshot.gainBounds.intersects(snapshot.tooltipBounds), "Output knob must not overlap tooltip mode");
        require(!snapshot.gainBounds.intersects(snapshot.voiceBounds), "Output knob must not overlap voice count");
        require(!snapshot.gainBounds.intersects(snapshot.ccBounds), "Output knob must not overlap CC page label");
        require(snapshot.selectorPanelBounds.contains(snapshot.familyLabelBounds), "Family label must remain inside selector panel");
        require(snapshot.selectorPanelBounds.contains(snapshot.familyTabsBounds), "Family tabs must remain inside selector panel");
        require(snapshot.selectorPanelBounds.contains(snapshot.modelLabelBounds), "Model label must remain inside selector panel");
        require(snapshot.selectorPanelBounds.contains(snapshot.modelSelectorBounds), "Model selector must remain inside selector panel");
    };

    {
        auto processor = makeProcessor();
        BassSynthAudioProcessorEditor editor(*processor);
        editor.resized();
        validateSnapshot(editor.captureLayoutSnapshotForTests(), true);
    }

    {
        auto processor = makeProcessor();
        BassSynthAudioProcessorEditor editor(*processor);
        editor.setSize(980, 700);
        editor.resized();
        validateSnapshot(editor.captureLayoutSnapshotForTests(), true);
    }

    {
        auto processor = makeProcessor();
        BassSynthAudioProcessorEditor editor(*processor);
        editor.setSize(1220, 780);
        editor.resized();
        validateSnapshot(editor.captureLayoutSnapshotForTests(), false);
    }

    {
        auto processor = makeProcessor();
        BassSynthAudioProcessorEditor editor(*processor);
        editor.setSize(1220, 780);
        editor.setRightPanelSectionForTests(2);
        const auto snapshot = editor.captureLayoutSnapshotForTests();
        require(snapshot.fxLockVisible, "FX lock must be visible in the FX section");
        require(!snapshot.fxLockBounds.isEmpty(), "FX lock must have non-zero bounds in the FX section");
        require(snapshot.editorBounds.contains(snapshot.fxLockBounds), "FX lock must remain inside the editor bounds");
    }
}

void testMacroContextReflectsBassFamily()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto processor = makeProcessor();

    setParameterValue(processor->getAPVTS(), "selected_bass", 3.0f);
    BassSynthAudioProcessorEditor editor(*processor);
    editor.refreshBassUiForTests();
    require(editor.getMacroLabelForTests(0) == "Weight", "808 family must expose Weight as macro 1");
    require(editor.getMacroHintForTests().containsIgnoreCase("Weight"), "808 macro hint must reflect the active family workflow");
    require(editor.getMacroGuardrailForTests().containsIgnoreCase("mono-safe"), "808 guardrail text must protect the low-end contract");

    setParameterValue(processor->getAPVTS(), "selected_bass", 6.0f);
    editor.refreshBassUiForTests();
    require(editor.getMacroLabelForTests(0) == "Drive", "Synth family must expose Drive as macro 1");
    require(editor.getMacroHintForTests().containsIgnoreCase("Edge"), "Synth macro hint must mention the synth workflow");
    require(editor.getMacroGuardrailForTests().containsIgnoreCase("tonal center"), "Synth guardrail text must mention note-center protection");
}

void testPerformanceMacrosStayFamilyAware()
{
    auto processor = makeProcessor();

    const auto resetMacros = [&]()
    {
        setParameterValue(processor->getAPVTS(), "macro_fatness", 0.5f);
        setParameterValue(processor->getAPVTS(), "macro_brillance", 0.5f);
        setParameterValue(processor->getAPVTS(), "macro_punch", 0.5f);
        setParameterValue(processor->getAPVTS(), "macro_depth", 0.5f);
    };

    setParameterValue(processor->getAPVTS(), "selected_bass", 0.0f);
    processor->applyFactoryPreset(0);
    resetMacros();
    const auto acousticBase = processor->snapshotMacroAppliedSettingsForTests(0);
    setParameterValue(processor->getAPVTS(), "macro_fatness", 1.0f);
    const auto acousticBoom = processor->snapshotMacroAppliedSettingsForTests(0);
    require(acousticBoom.body > acousticBase.body + 0.05f, "Acoustic Boom must favor body growth");
    require(acousticBoom.subLevel <= acousticBase.subLevel + 0.08f, "Acoustic Boom must avoid fake-sub inflation");

    setParameterValue(processor->getAPVTS(), "selected_bass", 3.0f);
    processor->applyFactoryPreset(0);
    resetMacros();
    const auto eight08Base = processor->snapshotMacroAppliedSettingsForTests(3);
    setParameterValue(processor->getAPVTS(), "macro_punch", 1.0f);
    const auto eight08Punch = processor->snapshotMacroAppliedSettingsForTests(3);
    require(eight08Punch.pitchEnv > eight08Base.pitchEnv + 0.05f, "808 Punch must increase pitch contour");
    setParameterValue(processor->getAPVTS(), "macro_punch", 0.5f);
    setParameterValue(processor->getAPVTS(), "macro_depth", 1.0f);
    const auto eight08Space = processor->snapshotMacroAppliedSettingsForTests(3);
    require(eight08Space.releaseSeconds > eight08Base.releaseSeconds, "808 Space must extend the tail");
    require(eight08Space.releaseSeconds <= 1.2f, "808 Space must keep release inside groove-safe bounds");

    setParameterValue(processor->getAPVTS(), "selected_bass", 6.0f);
    processor->applyFactoryPreset(0);
    resetMacros();
    const auto synthBase = processor->snapshotMacroAppliedSettingsForTests(6);
    setParameterValue(processor->getAPVTS(), "macro_fatness", 1.0f);
    setParameterValue(processor->getAPVTS(), "macro_brillance", 1.0f);
    const auto synthDriven = processor->snapshotMacroAppliedSettingsForTests(6);
    require(synthDriven.drive > synthBase.drive + 0.05f, "Synth Drive must add harmonic density");
    require(synthDriven.filterEnv > synthBase.filterEnv + 0.05f, "Synth Edge must increase filter movement");
}

void testFactoryPresetBrowserMetadataFormatting()
{
    auto processor = makeProcessor();
    juce::ScopedJuceInitialiser_GUI gui;

    setParameterValue(processor->getAPVTS(), "selected_bass", 3.0f);
    BassSynthAudioProcessorEditor editor(*processor);

    const auto label = editor.getFactoryPresetBrowserLabelForTests(0);
    const auto searchText = editor.getFactoryPresetBrowserSearchTextForTests(0);

    require(label.contains("|"), "Factory preset label must expose a role prefix for browser scanning");
    require(searchText.containsIgnoreCase("sub-foundation"), "Factory preset search text must include mix role metadata");
    require(searchText.containsIgnoreCase("808"), "Factory preset search text must include family metadata");
    require(searchText.containsIgnoreCase("sub"), "Factory preset search text must include model/tag context");
}

void testControllerPanicAllSoundOff()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);

    // Play a note
    renderWithMidi(*processor, {
        { 0, juce::MidiMessage::noteOn(1, 36, (juce::uint8) 100) }
    }, 512, 256);

    // Verify voice is active
    bool hasActive = false;
    for (int i = 0; i < 32; ++i)
    {
        juce::AudioBuffer<float> block(processor->getTotalNumOutputChannels(), 256);
        block.clear();
        juce::MidiBuffer midi;
        processor->processBlock(block, midi);
        if (bufferPeak(block) > 0.0001f) { hasActive = true; break; }
    }
    require(hasActive, "Note-on fixture must produce audible output before CC120");

    // Send CC120 (All Sound Off)
    {
        juce::AudioBuffer<float> block(processor->getTotalNumOutputChannels(), 256);
        block.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 120, 0), 0);
        processor->processBlock(block, midi);
    }

    // Render a few blocks and verify silence
    auto tail = renderWithMidi(*processor, {}, 2048, 256);
    require(bufferPeak(tail) < 0.001f, "CC120 must silence all voices within a short tail");
    require(processor->getActiveVoiceCount() == 0, "CC120 must clear active voice accounting immediately");
}

void testControllerPanicAllNotesOff()
{
    auto processor = makeProcessor();
    processor->enableAllBuses();
    processor->prepareToPlay(48000.0, 256);

    // Play a note
    renderWithMidi(*processor, {
        { 0, juce::MidiMessage::noteOn(1, 36, (juce::uint8) 100) }
    }, 512, 256);

    // Send CC123 (All Notes Off) — should release, not hard-kill
    {
        juce::AudioBuffer<float> block(processor->getTotalNumOutputChannels(), 256);
        block.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 123, 0), 0);
        processor->processBlock(block, midi);
    }

    // After enough time, voices should have faded out
    auto tail = renderWithMidi(*processor, {}, 48000, 256);
    require(bufferPeak(tail) < 0.01f, "CC123 must release all voices so they eventually fade to silence");
    require(processor->getActiveVoiceCount() == 0, "CC123 must eventually clear all active voices");
}

} // namespace

int main()
{
    try
    {
        const auto testFilter = juce::SystemStats::getEnvironmentVariable("UWDEVST_BASS_TEST_FILTER", {}).trim();
        const auto runTest = [](const char* name, auto&& test)
        {
            std::cout << "[bass-tests] " << name << '\n' << std::flush;
            test();
        };
        const auto maybeRunTest = [&runTest, &testFilter](const char* name, auto&& test)
        {
            if (testFilter.isNotEmpty() && !juce::String(name).containsIgnoreCase(testFilter))
                return;

            runTest(name, std::forward<decltype(test)>(test));
        };

        maybeRunTest("testFactoryBankShape", testFactoryBankShape);
        maybeRunTest("testFactoryPresetNamesAreUniquePerBank", testFactoryPresetNamesAreUniquePerBank);
        maybeRunTest("testFactoryPresetReleaseMetadataCoherence", testFactoryPresetReleaseMetadataCoherence);
        maybeRunTest("testFactoryRoleCoverageByFamily", testFactoryRoleCoverageByFamily);
        maybeRunTest("testBootPresetMatchesFactory", testBootPresetMatchesFactory);
        maybeRunTest("testPresetStoragePaths", testPresetStoragePaths);
        maybeRunTest("testPresetStoragePathsSupportOverride", testPresetStoragePathsSupportOverride);
        maybeRunTest("testStateSanitization", testStateSanitization);
        maybeRunTest("testLegacyStateMigratesModMatrixParameters", testLegacyStateMigratesModMatrixParameters);
        maybeRunTest("testUserPresetRoundTripWithManifest", testUserPresetRoundTripWithManifest);
        maybeRunTest("testFactoryOverrideRewriteUsesCanonicalSchema", testFactoryOverrideRewriteUsesCanonicalSchema);
        maybeRunTest("testFactoryOverrideRoundTripPersistsPerformanceAndModMatrix", testFactoryOverrideRoundTripPersistsPerformanceAndModMatrix);
        maybeRunTest("testLegacyUserPresetLoadRegeneratesManifest", testLegacyUserPresetLoadRegeneratesManifest);
        maybeRunTest("testMainAndAuxRouting", testMainAndAuxRouting);
        maybeRunTest("testModulationMatrixAudibility", testModulationMatrixAudibility);
        maybeRunTest("testSubControlRendersTrueSubOctave", testSubControlRendersTrueSubOctave);
        maybeRunTest("test808LfoTremPanIsAudible", test808LfoTremPanIsAudible);
        maybeRunTest("test808LfoCutoffIsAudible", test808LfoCutoffIsAudible);
        maybeRunTest("testUiHidesUnsupportedFamilyControls", testUiHidesUnsupportedFamilyControls);
        maybeRunTest("testSynthFilterEnvIsAudible", testSynthFilterEnvIsAudible);
        maybeRunTest("testBrightnessStillChangesTimbreWithZeroFilterEnv", testBrightnessStillChangesTimbreWithZeroFilterEnv);
        maybeRunTest("testGlideTimeSupportsLongerPortamento", testGlideTimeSupportsLongerPortamento);
        maybeRunTest("testVoiceStealStressRendersCleanly", testVoiceStealStressRendersCleanly);
        maybeRunTest("testAllFactoryPresetsRenderStable", testAllFactoryPresetsRenderStable);
        maybeRunTest("testDeterministicOfflineRender", testDeterministicOfflineRender);
        maybeRunTest("testRenderStabilityAcrossSampleRatesAndBlocks", testRenderStabilityAcrossSampleRatesAndBlocks);
        maybeRunTest("testDelayAndReverbTailsFadeSmoothly", testDelayAndReverbTailsFadeSmoothly);
        maybeRunTest("testAutomationSweepsRenderCleanly", testAutomationSweepsRenderCleanly);
        maybeRunTest("testEditorLayoutSnapshot", testEditorLayoutSnapshot);
        maybeRunTest("testMacroContextReflectsBassFamily", testMacroContextReflectsBassFamily);
        maybeRunTest("testPerformanceMacrosStayFamilyAware", testPerformanceMacrosStayFamilyAware);
        maybeRunTest("testFactoryPresetBrowserMetadataFormatting", testFactoryPresetBrowserMetadataFormatting);
        maybeRunTest("testControllerPanicAllSoundOff", testControllerPanicAllSoundOff);
        maybeRunTest("testControllerPanicAllNotesOff", testControllerPanicAllNotesOff);
    }
    catch (const std::exception& e)
    {
        std::cerr << "UWdeVST bass production tests failed: " << e.what() << '\n';
        return 1;
    }

    std::cout << "UWdeVST bass production tests: OK\n";
    return 0;
}
