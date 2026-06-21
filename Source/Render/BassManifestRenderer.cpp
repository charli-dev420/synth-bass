#include <JuceHeader.h>

#include "../Engine/BassDefs.h"
#include "../Engine/BassVoice.h"
#include "../Engine/FactoryPresets.h"
#include "../../Shared/ProductionQa.h"
#include "../../../../Shared/AuditionPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
struct RenderJob
{
    juce::String instrument;
    juce::String family;
    juce::String category;
    juce::String subcategory;
    juce::String articulation;
    juce::String key;
    juce::String tempoBpm;
    juce::String durationSeconds;
    juce::String velocityLayer;
    juce::String roundRobin;
    juce::String take;
    juce::String finalRelativePath;
    juce::String presetProfile;
    juce::String fxProfile;
};

struct Event
{
    int sample = 0;
    bool noteOn = false;
    int midiNote = 36;
    float velocity = 0.8f;
};

struct VoiceSlot
{
    mbs::BassVoice voice;
    int midiNote = -1;
};

struct FxSettings
{
    float compThresholdDb = -18.0f;
    float compRatio = 2.0f;
    float compAttackMs = 15.0f;
    float compReleaseMs = 140.0f;
    float compMix = 0.08f;
    float satDrive = 1.15f;
    float satMix = 0.03f;
    float transientAttack = 0.06f;
    float transientSustain = 0.0f;
    float transientMix = 0.06f;
    float targetPeak = 0.94f;
};

struct AudioMetrics
{
    float peakDb = musique::qa::kPeakFloorDb;
    float rmsDb = musique::qa::kPeakFloorDb;
    bool finite = true;
};

constexpr double kSampleRate = 48000.0;
constexpr float kCandidatePeakCeilingDb = -0.8f;

float clamp01(const float value) { return juce::jlimit(0.0f, 1.0f, value); }

juce::String slug(const juce::String& value)
{
    auto ascii = value.toLowerCase().trim();
    ascii = ascii.replace("Ã©", "e").replace("Ã¨", "e").replace("Ãª", "e").replace("Ã«", "e");
    ascii = ascii.replace("Ã ", "a").replace("Ã¢", "a").replace("Ã¤", "a");
    ascii = ascii.replace("Ã¹", "u").replace("Ã»", "u").replace("Ã¼", "u");
    ascii = ascii.replace("Ã´", "o").replace("Ã¶", "o");
    ascii = ascii.replace("Ã®", "i").replace("Ã¯", "i").replace("Ã§", "c");
    ascii = ascii.replaceCharacter(' ', '_').replaceCharacter('-', '_').replaceCharacter('/', '_');
    ascii = ascii.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_");
    while (ascii.contains("__"))
        ascii = ascii.replace("__", "_");
    return ascii.trimCharactersAtStart("_").trimCharactersAtEnd("_");
}

bool containsAny(const juce::String& haystack, std::initializer_list<const char*> needles)
{
    for (const auto* needle : needles)
        if (haystack.contains(needle))
            return true;
    return false;
}

juce::StringArray parseCsvLine(const juce::String& line)
{
    juce::StringArray parts;
    juce::String current;
    bool inQuotes = false;
    for (int i = 0; i < line.length(); ++i)
    {
        const auto ch = line[i];
        if (ch == '"')
        {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"')
            {
                current << '"';
                ++i;
            }
            else
            {
                inQuotes = !inQuotes;
            }
        }
        else if (ch == ',' && !inQuotes)
        {
            parts.add(current);
            current.clear();
        }
        else
        {
            current << ch;
        }
    }
    parts.add(current);
    return parts;
}

int csvColumn(const juce::StringArray& header, const juce::String& name)
{
    const auto index = header.indexOf(name);
    if (index < 0)
        throw std::runtime_error(("Missing CSV column: " + name).toStdString());
    return index;
}

int parseTokenIndex(const juce::String& token, const juce::String& prefix)
{
    if (!token.startsWithIgnoreCase(prefix))
        return 1;
    const auto value = token.fromFirstOccurrenceOf(prefix, false, false).getIntValue();
    return value > 0 ? value : 1;
}

float velocityFromLayer(const juce::String& layer)
{
    switch (parseTokenIndex(layer, "v"))
    {
        case 1: return 0.35f;
        case 2: return 0.50f;
        case 3: return 0.65f;
        case 4: return 0.78f;
        case 5: return 0.90f;
        case 6: return 1.00f;
        default: return 0.78f;
    }
}

FxSettings fxProfileFor(const juce::String& profile)
{
    const auto key = slug(profile);
    if (key == "accentplayable")
        return { -18.0f, 2.8f, 10.0f, 120.0f, 0.18f, 1.35f, 0.08f, 0.28f, -0.04f, 0.18f, 0.93f };
    if (key == "texturedplayable")
        return { -19.0f, 2.5f, 12.0f, 140.0f, 0.20f, 1.60f, 0.11f, 0.16f, 0.06f, 0.14f, 0.92f };
    if (key == "designedloop")
        return { -21.0f, 3.4f, 8.0f, 100.0f, 0.30f, 1.95f, 0.18f, 0.22f, 0.06f, 0.20f, 0.91f };
    if (key == "designedtexture")
        return { -22.0f, 3.0f, 16.0f, 180.0f, 0.28f, 2.20f, 0.22f, 0.05f, 0.18f, 0.18f, 0.90f };
    return {};
}

FxSettings fxSettingsFromPreset(const mbs::GlobalFxSettings& presetFx)
{
    FxSettings fx;
    fx.compThresholdDb = presetFx.compThreshold;
    fx.compRatio = presetFx.compRatio;
    fx.compAttackMs = presetFx.compAttack;
    fx.compReleaseMs = presetFx.compRelease;
    fx.compMix = presetFx.compressorOn ? clamp01(presetFx.compMix) : 0.0f;
    fx.satDrive = juce::jmax(1.0f, presetFx.satDrive);
    fx.satMix = presetFx.saturatorOn ? clamp01(presetFx.satMix) : 0.0f;
    fx.transientAttack = presetFx.transientAttack;
    fx.transientSustain = presetFx.transientSustain;
    fx.transientMix = presetFx.transientOn ? clamp01(presetFx.transientMix) : 0.0f;
    fx.targetPeak = 0.88f;
    return fx;
}

bool isLoopJob(const RenderJob& job)
{
    return job.tempoBpm.isNotEmpty() || job.finalRelativePath.containsIgnoreCase("/Loops/") || job.finalRelativePath.containsIgnoreCase("\\Loops\\");
}

bool isTextureJob(const RenderJob& job)
{
    const auto text = slug(job.category + "_" + job.subcategory + "_" + job.articulation);
    return containsAny(text, { "drone", "ambient", "texture", "subdrone" }) || job.finalRelativePath.containsIgnoreCase("/02_Textures_Drones/");
}

bool isShortJob(const RenderJob& job)
{
    return containsAny(slug(job.category + "_" + job.subcategory + "_" + job.articulation), { "short", "stab", "punch", "pluck", "main", "accent" });
}

int midiNoteFromKey(const juce::String& token, const juce::String& instrument)
{
    const auto inst = slug(instrument);
    const int root = containsAny(inst, { "sub_808", "boom_808", "distorted_808" }) ? 24 : containsAny(inst, { "contrebasse", "basse_fingered", "basse_slap" }) ? 28 : 24;
    if (token.isEmpty())
        return root + 12;

    auto key = token.trim();
    int semitone = 0;
    switch (juce::CharacterFunctions::toUpperCase(key[0]))
    {
        case 'C': semitone = 0; break;
        case 'D': semitone = 2; break;
        case 'E': semitone = 4; break;
        case 'F': semitone = 5; break;
        case 'G': semitone = 7; break;
        case 'A': semitone = 9; break;
        case 'B': semitone = 11; break;
        default: break;
    }

    if (key.length() >= 2 && juce::CharacterFunctions::isDigit(key[key.length() - 1]))
        return 12 * (key.substring(1).getIntValue() + 1) + semitone;

    return 12 * (((root + 12) / 12) - 1 + 1) + semitone;
}

int bassIndexFromName(const juce::String& name)
{
    const auto wanted = slug(name);
    if (wanted.contains("fretless")) return 1;
    if (wanted.contains("fm")) return 6;
    if (wanted.contains("wobble")) return 7;
    if (wanted.contains("acid_303") || wanted.contains("303")) return 8;
    for (int i = 0; i < mbs::kNumBasses; ++i)
    {
        const auto candidate = slug(mbs::getBassName(i));
        if (candidate == wanted || wanted.contains(candidate) || candidate.contains(wanted))
            return i;
    }
    return -1;
}

void applyPresetMacro(const RenderJob& job, const int bassIndex, mbs::BassSettings& s)
{
    const auto preset = slug(job.presetProfile);
    const auto family = mbs::getFamily(bassIndex);
    if (preset == "expressive")
    {
        s.drive = clamp01(s.drive + 0.08f);
        s.cutoffHz = juce::jlimit(120.0f, 12000.0f, s.cutoffHz * 1.08f);
    }
    else if (preset == "dark")
    {
        s.body = clamp01(s.body + 0.16f);
        s.subLevel = clamp01(s.subLevel + 0.12f);
        s.cutoffHz = juce::jlimit(120.0f, 12000.0f, s.cutoffHz * 0.72f);
    }
    else if (preset == "cinematic")
    {
        s.drive = clamp01(s.drive + 0.10f);
        s.body = clamp01(s.body + 0.10f);
        s.decaySeconds = juce::jlimit(0.1f, 10.0f, s.decaySeconds * 1.20f);
    }

    if (family == mbs::Family::Eight08)
        s.pitchEnv = clamp01(s.pitchEnv + 0.08f);
    if (family == mbs::Family::Synth)
        s.character = clamp01(s.character + 0.08f);
}

void adaptSettingsForJob(const RenderJob& job, mbs::BassSettings& s)
{
    const auto text = slug(job.articulation + "_" + job.subcategory + "_" + job.category);
    if (containsAny(text, { "short", "stab", "punch", "pluck", "main", "accent" }))
    {
        s.attackSeconds = juce::jmin(s.attackSeconds, 0.008f);
        s.decaySeconds = juce::jmax(0.10f, s.decaySeconds * 0.45f);
        s.sustainLevel = juce::jmin(s.sustainLevel, 0.18f);
        s.releaseSeconds = juce::jmin(s.releaseSeconds, 0.18f);
    }
    if (containsAny(text, { "slide", "glide" }))
    {
        s.attackSeconds = juce::jmax(0.01f, s.attackSeconds);
        s.releaseSeconds = juce::jmax(0.25f, s.releaseSeconds);
        s.pitchEnv = clamp01(s.pitchEnv + 0.12f);
    }
    if (containsAny(text, { "drone", "ambient", "subdrone" }))
    {
        s.decaySeconds = juce::jmin(10.0f, s.decaySeconds * 1.8f);
        s.sustainLevel = juce::jmax(s.sustainLevel, 0.70f);
        s.releaseSeconds = juce::jmax(s.releaseSeconds, 0.60f);
        s.cutoffHz = juce::jlimit(120.0f, 12000.0f, s.cutoffHz * 0.75f);
    }
}

void applyVariation(const RenderJob& job, mbs::BassSettings& s)
{
    const auto rr = parseTokenIndex(job.roundRobin, "rr");
    const auto take = parseTokenIndex(job.take, "t");
    s.cutoffHz = juce::jlimit(120.0f, 12000.0f, s.cutoffHz * (1.0f + 0.02f * static_cast<float>(rr - 1)));
    s.drive = clamp01(s.drive + 0.02f * static_cast<float>(rr - 1));
    s.pan = juce::jlimit(-0.12f, 0.12f, s.pan + 0.025f * static_cast<float>(take - 2));
}

std::vector<Event> buildEvents(const RenderJob& job, const int midiNote, const float durationSeconds)
{
    const auto totalSamples = static_cast<int>(std::ceil(durationSeconds * kSampleRate));
    const auto velocity = velocityFromLayer(job.velocityLayer);
    std::vector<Event> events;

    if (!isLoopJob(job))
    {
        const auto holdRatio = isTextureJob(job) ? 0.88f : isShortJob(job) ? 0.18f : 0.76f;
        events.push_back({ 0, true, midiNote, velocity });
        events.push_back({ static_cast<int>(holdRatio * totalSamples), false, midiNote, 0.0f });
        if (containsAny(slug(job.articulation), { "slide", "glide" }))
        {
            events.push_back({ totalSamples / 3, true, midiNote + 5, velocity * 0.82f });
            events.push_back({ static_cast<int>(0.82f * totalSamples), false, midiNote + 5, 0.0f });
        }
        return events;
    }

    const auto bpm = juce::jmax(40, job.tempoBpm.getIntValue() > 0 ? job.tempoBpm.getIntValue() : 100);
    const auto stepSamples = static_cast<int>(std::round((60.0 / static_cast<double>(bpm)) * 0.5 * kSampleRate));
    const auto noteLengthSamples = static_cast<int>(stepSamples * 0.58f);
    const float pattern[] = { 1.0f, 0.72f, 0.88f, 0.68f, 0.95f, 0.62f, 0.82f, 0.70f };
    const int intervalPattern[] = { 0, 0, 7, 0, 12, 7, 0, 5 };

    for (int sample = 0, index = 0; sample < totalSamples - noteLengthSamples; sample += stepSamples, ++index)
    {
        const auto noteVel = juce::jlimit(0.08f, 1.0f, velocity * pattern[index % 8]);
        const auto noteNumber = midiNote + intervalPattern[index % 8];
        events.push_back({ sample, true, noteNumber, noteVel });
        events.push_back({ juce::jmin(totalSamples - 1, sample + noteLengthSamples), false, noteNumber, 0.0f });
    }

    return events;
}

int findFreeVoice(std::vector<VoiceSlot>& voices)
{
    for (int i = 0; i < static_cast<int>(voices.size()); ++i)
        if (!voices[static_cast<std::size_t>(i)].voice.isActive())
            return i;
    for (int i = 0; i < static_cast<int>(voices.size()); ++i)
        if (voices[static_cast<std::size_t>(i)].voice.isReleasing())
            return i;
    return 0;
}

void renderTimeline(juce::AudioBuffer<float>& buffer, const std::vector<Event>& events, const mbs::BassSettings& settings, const mbs::BassCharacteristics& chars)
{
    std::vector<VoiceSlot> voices(8);
    int cursor = 0;
    std::size_t eventIndex = 0;

    auto renderSegment = [&](const int start, const int length)
    {
        if (length <= 0)
            return;
        for (auto& slot : voices)
            if (slot.voice.isActive())
                slot.voice.render(buffer, start, length);
    };

    while (eventIndex < events.size())
    {
        const auto eventSample = juce::jlimit(0, buffer.getNumSamples(), events[eventIndex].sample);
        renderSegment(cursor, eventSample - cursor);
        cursor = eventSample;

        while (eventIndex < events.size() && events[eventIndex].sample == eventSample)
        {
            const auto& event = events[eventIndex];
            if (event.noteOn)
            {
                const auto slotIndex = findFreeVoice(voices);
                auto& slot = voices[static_cast<std::size_t>(slotIndex)];
                slot.midiNote = event.midiNote;
                slot.voice.noteOn(settings, chars, event.midiNote, event.velocity, kSampleRate);
            }
            else
            {
                for (auto& slot : voices)
                    if (slot.midiNote == event.midiNote && slot.voice.isActive() && !slot.voice.isReleasing())
                        slot.voice.noteOff();
            }
            ++eventIndex;
        }
    }

    renderSegment(cursor, buffer.getNumSamples() - cursor);
}

void applyTransient(juce::AudioBuffer<float>& buffer, const FxSettings& fx)
{
    if (fx.transientMix <= 0.0001f)
        return;
    std::array<float, 2> fastEnv = { 0.0f, 0.0f };
    std::array<float, 2> slowEnv = { 0.0f, 0.0f };
    const auto fastCoeff = std::exp(-1.0f / (0.0018f * static_cast<float>(kSampleRate)));
    const auto slowCoeff = std::exp(-1.0f / (0.055f * static_cast<float>(kSampleRate)));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        auto& fast = fastEnv[static_cast<std::size_t>(juce::jlimit(0, 1, ch))];
        auto& slow = slowEnv[static_cast<std::size_t>(juce::jlimit(0, 1, ch))];
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto dry = data[i];
            const auto absSample = std::abs(dry);
            fast = fastCoeff * fast + (1.0f - fastCoeff) * absSample;
            slow = slowCoeff * slow + (1.0f - slowCoeff) * absSample;
            const auto transient = fast - slow;
            const auto gain = juce::jlimit(0.2f, 4.0f, 1.0f + fx.transientAttack * std::max(0.0f, transient) * 7.0f + fx.transientSustain * std::max(0.0f, -transient) * 5.0f);
            data[i] = dry + (dry * gain - dry) * fx.transientMix;
        }
    }
}

void applySaturator(juce::AudioBuffer<float>& buffer, const FxSettings& fx)
{
    if (fx.satMix <= 0.0001f)
        return;
    const auto norm = 1.0f / std::max(0.0001f, std::tanh(fx.satDrive));
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto dry = data[i];
            const auto wet = std::tanh(dry * fx.satDrive) * norm;
            data[i] = dry + (wet - dry) * fx.satMix;
        }
    }
}

void applyCompressor(juce::AudioBuffer<float>& buffer, const FxSettings& fx)
{
    if (fx.compMix <= 0.0001f)
        return;
    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer);
    juce::dsp::Compressor<float> compressor;
    juce::dsp::ProcessSpec spec { kSampleRate, static_cast<juce::uint32>(buffer.getNumSamples()), static_cast<juce::uint32>(buffer.getNumChannels()) };
    compressor.prepare(spec);
    compressor.setThreshold(fx.compThresholdDb);
    compressor.setRatio(fx.compRatio);
    compressor.setAttack(fx.compAttackMs);
    compressor.setRelease(fx.compReleaseMs);
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        auto* dryData = dry.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            wet[i] = dryData[i] + (wet[i] - dryData[i]) * fx.compMix;
    }
}

void applyDcHighPass(juce::AudioBuffer<float>& buffer)
{
    const auto alpha = std::exp(-2.0f * juce::MathConstants<float>::pi * 14.0f / static_cast<float>(kSampleRate));
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        float previousInput = 0.0f;
        float previousOutput = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto input = data[i];
            const auto output = alpha * (previousOutput + input - previousInput);
            data[i] = output;
            previousInput = input;
            previousOutput = output;
        }
    }
}

void trimAndProtect(juce::AudioBuffer<float>& buffer, const RenderJob& job, const FxSettings& fx)
{
    if (!isLoopJob(job))
    {
        int first = 0;
        int last = buffer.getNumSamples() - 1;
        auto magnitudeAt = [&buffer](const int sample)
        {
            float mag = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                mag = juce::jmax(mag, std::abs(buffer.getSample(ch, sample)));
            return mag;
        };
        while (first < buffer.getNumSamples() && magnitudeAt(first) < 0.0008f) ++first;
        while (last > first && magnitudeAt(last) < 0.0008f) --last;
        first = juce::jmax(0, first - 96);
        last = juce::jmin(buffer.getNumSamples() - 1, last + 768);
        const auto newLength = juce::jmax(1, last - first + 1);
        juce::AudioBuffer<float> trimmed(buffer.getNumChannels(), newLength);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            trimmed.copyFrom(ch, 0, buffer, ch, first, newLength);
        buffer.makeCopyOf(trimmed);
    }

    const auto fadeIn = juce::jmin(96, buffer.getNumSamples() / 5);
    const auto fadeOut = isLoopJob(job) ? 0 : juce::jmin(isTextureJob(job) ? 512 : 192, buffer.getNumSamples() / 3);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        if (fadeIn > 0)
            buffer.applyGainRamp(ch, 0, fadeIn, 0.0f, 1.0f);
        if (fadeOut > 0)
            buffer.applyGainRamp(ch, buffer.getNumSamples() - fadeOut, fadeOut, 1.0f, 0.0f);
    }

    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    if (peak > 0.0001f)
        buffer.applyGain(fx.targetPeak / peak);
}

bool writeWav(const juce::File& file, juce::AudioBuffer<float>& buffer)
{
    file.getParentDirectory().createDirectory();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (stream == nullptr)
        return false;
    if (auto* writer = format.createWriterFor(stream.get(), kSampleRate, static_cast<unsigned int>(buffer.getNumChannels()), 24, {}, 0))
    {
        stream.release();
        std::unique_ptr<juce::AudioFormatWriter> holder(writer);
        return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    }
    return false;
}

AudioMetrics measureBuffer(const juce::AudioBuffer<float>& buffer)
{
    AudioMetrics metrics;
    float peak = 0.0f;
    double energy = 0.0;
    int sampleCount = 0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const auto sample = data[s];
            if (!std::isfinite(sample))
            {
                metrics.finite = false;
                return metrics;
            }
            peak = juce::jmax(peak, std::abs(sample));
            energy += static_cast<double>(sample) * static_cast<double>(sample);
            ++sampleCount;
        }
    }

    metrics.peakDb = peak > 0.0f ? juce::Decibels::gainToDecibels(peak) : musique::qa::kPeakFloorDb;
    const auto rms = sampleCount > 0 ? std::sqrt(energy / static_cast<double>(sampleCount)) : 0.0;
    metrics.rmsDb = rms > 0.0 ? juce::Decibels::gainToDecibels(static_cast<float>(rms)) : musique::qa::kPeakFloorDb;
    return metrics;
}

void protectCeiling(juce::AudioBuffer<float>& buffer, const float ceilingDb)
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));

    const auto ceiling = juce::Decibels::decibelsToGain(ceilingDb);
    if (peak > ceiling && peak > 0.0001f)
        buffer.applyGain(ceiling / peak);
}

void mixInto(juce::AudioBuffer<float>& destination,
             const juce::AudioBuffer<float>& source,
             const int startSample,
             const float gain)
{
    if (startSample >= destination.getNumSamples())
        return;

    const int samples = juce::jmin(source.getNumSamples(), destination.getNumSamples() - startSample);
    for (int ch = 0; ch < juce::jmin(destination.getNumChannels(), source.getNumChannels()); ++ch)
        destination.addFrom(ch, startSample, source, ch, 0, samples, gain);
}

std::vector<RenderJob> readManifestCsv(const juce::File& file)
{
    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    if (lines.size() < 2)
        throw std::runtime_error("Manifest CSV is empty.");

    const auto header = parseCsvLine(lines[0]);
    const auto idxInstrument = csvColumn(header, "Instrument");
    const auto idxFamily = csvColumn(header, "Family");
    const auto idxCategory = csvColumn(header, "Category");
    const auto idxSubcategory = csvColumn(header, "Subcategory");
    const auto idxArticulation = csvColumn(header, "Articulation");
    const auto idxKey = csvColumn(header, "Key");
    const auto idxTempo = csvColumn(header, "Tempo BPM");
    const auto idxDuration = csvColumn(header, "Duration (s)");
    const auto idxVelocity = csvColumn(header, "Velocity Layer");
    const auto idxRr = csvColumn(header, "Round Robin");
    const auto idxTake = csvColumn(header, "Take");
    const auto idxPath = csvColumn(header, "Final Relative Path");
    const auto idxPreset = csvColumn(header, "Preset Profile");
    const auto idxFx = csvColumn(header, "FX Profile");

    std::vector<RenderJob> jobs;
    for (int lineIndex = 1; lineIndex < lines.size(); ++lineIndex)
    {
        const auto cols = parseCsvLine(lines[lineIndex]);
        if (cols.size() < header.size())
            continue;
        jobs.push_back({ cols[idxInstrument], cols[idxFamily], cols[idxCategory], cols[idxSubcategory], cols[idxArticulation], cols[idxKey], cols[idxTempo], cols[idxDuration], cols[idxVelocity], cols[idxRr], cols[idxTake], cols[idxPath], cols[idxPreset], cols[idxFx] });
    }
    return jobs;
}

struct PresetQaRow
{
    juce::String instrument;
    juce::String preset;
    juce::String family;
    juce::String mixRole;
    juce::String tags;
    float peakDb = musique::qa::kPeakFloorDb;
    float rmsDb = musique::qa::kPeakFloorDb;
    juce::String status = "PASS";
    juce::String issues;
};

struct BenchmarkRow
{
    juce::String instrument;
    juce::String preset;
    int voices = 0;
    double renderSeconds = 0.0;
    double elapsedMs = 0.0;
    double realtimeFactor = 0.0;
    double msPerVoiceSecond = 0.0;
};

struct PitchQaRow
{
    int bassIndex = 0;
    juce::String instrument;
    juce::String family;
    int midiNote = 36;
    double expectedHz = 0.0;
    double estimatedHz = 0.0;
    double centsError = 0.0;
    double stability = 0.0;
    juce::String verdict = "FAIL";
    juce::String issues;
};

struct ReleaseSuiteRow
{
    juce::String file;
    juce::String scenario;
    float peakDb = musique::qa::kPeakFloorDb;
    float rmsDb = musique::qa::kPeakFloorDb;
    juce::String verdict = "FAIL";
    juce::String notes;
};

struct CandidatePresetInfo
{
    juce::File file;
    juce::String relativePath;
    juce::String presetName;
    int bassIndex = 0;
    int presetIndex = 0;
    mbs::BassSettings settings;
    mbs::GlobalFxSettings fx;
    mbs::PatchPerformanceSettings performance;
    mbs::PresetMetadata metadata;
    juce::StringArray warnings;
};

struct CandidateAudioRow
{
    int instrumentIndex = 0;
    juce::String instrument;
    juce::String presetName;
    juce::String presetFile;
    juce::String audioCase;
    juce::String wavFile;
    juce::String importStatus = "PASS";
    int sanitizationWarnings = 0;
    float peakDb = musique::qa::kPeakFloorDb;
    float rmsDb = musique::qa::kPeakFloorDb;
    float crestDb = 0.0f;
    bool clipped = false;
    juce::String status = "PASS";
    juce::String warnings;
};

juce::String csvEscape(const juce::String& text)
{
    auto escaped = text;
    escaped = escaped.replace("\"", "\"\"");
    if (escaped.containsAnyOf(",\"\n\r"))
        return "\"" + escaped + "\"";
    return escaped;
}

bool writeTextFile(const juce::File& file, const juce::String& text)
{
    if (file.getParentDirectory().createDirectory().failed())
        return false;

    juce::FileOutputStream stream(file);
    if (!stream.openedOk())
        return false;

    if (!stream.setPosition(0))
        return false;

    if (stream.truncate().failed())
        return false;

    if (!stream.writeText(text, false, false, nullptr))
        return false;

    stream.flush();
    return stream.getStatus().wasOk();
}

juce::String joinTags(const std::vector<std::string>& tags)
{
    juce::StringArray items;
    for (const auto& tag : tags)
        items.add(juce::String(juce::CharPointer_UTF8(tag.c_str())));
    return items.joinIntoString(",");
}

juce::String familyLabelForBass(const int bassIndex)
{
    switch (mbs::getFamily(bassIndex))
    {
        case mbs::Family::Acoustic: return "acoustic";
        case mbs::Family::Eight08:  return "808";
        case mbs::Family::Synth:    return "synth";
        default:                    return "unknown";
    }
}

juce::String identitySlugForBass(const int bassIndex)
{
    static constexpr const char* kSlugs[] = {
        "00_contrabass",
        "01_fingered_bass",
        "02_slap_bass",
        "03_sub_808",
        "04_boom_808",
        "05_distorted_808",
        "06_moog_bass",
        "07_reese_bass",
        "08_acid_bass"
    };
    return kSlugs[static_cast<std::size_t>(juce::jlimit(0, mbs::kNumBasses - 1, bassIndex))];
}

juce::String bareInstrumentSlugForBass(const int bassIndex)
{
    return slug(juce::String(juce::CharPointer_UTF8(mbs::getBassName(bassIndex))));
}

juce::String candidateInstrumentSlugForBass(const int bassIndex)
{
    return juce::String::formatted("%02d_", bassIndex) + bareInstrumentSlugForBass(bassIndex);
}

double midiNoteToHz(const int midiNote)
{
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

void writePresetQaReport(const juce::File& file, const std::vector<PresetQaRow>& rows)
{
    file.getParentDirectory().createDirectory();
    juce::String csv = "Instrument,Preset,Family,MixRole,Tags,PeakDb,RmsDb,Status,Issues\n";
    for (const auto& row : rows)
    {
        csv << csvEscape(row.instrument) << ','
            << csvEscape(row.preset) << ','
            << csvEscape(row.family) << ','
            << csvEscape(row.mixRole) << ','
            << csvEscape(row.tags) << ','
            << juce::String(row.peakDb, 3) << ','
            << juce::String(row.rmsDb, 3) << ','
            << csvEscape(row.status) << ','
            << csvEscape(row.issues) << '\n';
    }
    if (!writeTextFile(file, csv))
        throw std::runtime_error(("Failed to write preset QA report: " + file.getFullPathName()).toStdString());
}

void writeBenchmarkReport(const juce::File& file, const std::vector<BenchmarkRow>& rows)
{
    file.getParentDirectory().createDirectory();
    juce::String csv = "Instrument,Preset,Voices,RenderSeconds,ElapsedMs,RealtimeFactor,MsPerVoiceSecond\n";
    for (const auto& row : rows)
    {
        csv << csvEscape(row.instrument) << ','
            << csvEscape(row.preset) << ','
            << juce::String(row.voices) << ','
            << juce::String(row.renderSeconds, 3) << ','
            << juce::String(row.elapsedMs, 3) << ','
            << juce::String(row.realtimeFactor, 3) << ','
            << juce::String(row.msPerVoiceSecond, 6) << '\n';
    }
    if (!writeTextFile(file, csv))
        throw std::runtime_error(("Failed to write benchmark report: " + file.getFullPathName()).toStdString());
}

void writePitchQaReport(const juce::File& file, const std::vector<PitchQaRow>& rows)
{
    file.getParentDirectory().createDirectory();
    juce::String csv = "instrument_index,instrument,family,midi_note,expected_hz,estimated_hz,cents_error,stability,verdict,issues\n";
    for (const auto& row : rows)
    {
        csv << row.bassIndex << ','
            << csvEscape(row.instrument) << ','
            << csvEscape(row.family) << ','
            << row.midiNote << ','
            << juce::String(row.expectedHz, 3) << ','
            << juce::String(row.estimatedHz, 3) << ','
            << juce::String(row.centsError, 2) << ','
            << juce::String(row.stability, 4) << ','
            << csvEscape(row.verdict) << ','
            << csvEscape(row.issues) << '\n';
    }
    if (!writeTextFile(file, csv))
        throw std::runtime_error(("Failed to write pitch QA report: " + file.getFullPathName()).toStdString());
}

void writeReleaseSuiteReport(const juce::File& file, const std::vector<ReleaseSuiteRow>& rows)
{
    file.getParentDirectory().createDirectory();
    juce::String csv = "file,scenario,peak_dbfs,rms_dbfs,verdict,notes\n";
    for (const auto& row : rows)
    {
        csv << csvEscape(row.file) << ','
            << csvEscape(row.scenario) << ','
            << juce::String(row.peakDb, 3) << ','
            << juce::String(row.rmsDb, 3) << ','
            << csvEscape(row.verdict) << ','
            << csvEscape(row.notes) << '\n';
    }
    if (!writeTextFile(file, csv))
        throw std::runtime_error(("Failed to write release suite report: " + file.getFullPathName()).toStdString());
}

std::map<int, double> loadBenchmarkSummaryMsPerVoiceSecond(const juce::File& file)
{
    std::map<int, double> summaryByVoices;
    if (!file.existsAsFile())
        return summaryByVoices;

    const auto lines = juce::StringArray::fromLines(file.loadFileAsString());
    if (lines.size() < 2)
        return summaryByVoices;

    const auto header = parseCsvLine(lines[0]);
    const auto idxInstrument = csvColumn(header, "Instrument");
    const auto idxPreset = csvColumn(header, "Preset");
    const auto idxVoices = csvColumn(header, "Voices");
    const auto idxMsPerVoiceSecond = csvColumn(header, "MsPerVoiceSecond");
    for (int i = 1; i < lines.size(); ++i)
    {
        const auto cols = parseCsvLine(lines[i]);
        if (cols.size() < header.size())
            continue;
        if (cols[idxInstrument] != "ALL" || cols[idxPreset] != "summary")
            continue;

        summaryByVoices[cols[idxVoices].getIntValue()] = cols[idxMsPerVoiceSecond].getDoubleValue();
    }
    return summaryByVoices;
}

int presetIndexByName(const std::vector<mbs::InstrumentPreset>& bank, const juce::String& presetName)
{
    const auto wanted = slug(presetName);
    for (std::size_t i = 0; i < bank.size(); ++i)
    {
        if (slug(juce::String(juce::CharPointer_UTF8(bank[i].name.c_str()))) == wanted)
            return static_cast<int>(i);
    }
    return -1;
}

juce::AudioBuffer<float> renderPresetForBenchmark(const int bassIndex,
                                                  const mbs::InstrumentPreset& preset,
                                                  const int voices,
                                                  const double renderSeconds)
{
    const int totalSamples = static_cast<int>(std::round(renderSeconds * kSampleRate));
    juce::AudioBuffer<float> mixed(2, totalSamples);
    juce::AudioBuffer<float> temp(2, totalSamples);
    mixed.clear();
    temp.clear();

    static constexpr int kIntervals[] = { 0, 7, 12, 19, 24, 31, 36, 43 };
    const auto& chars = mbs::getCharacteristics(bassIndex);
    for (int voiceIndex = 0; voiceIndex < voices; ++voiceIndex)
    {
        temp.clear();
        mbs::BassVoice voice;
        const int midiNote = 36 + kIntervals[voiceIndex % static_cast<int>(std::size(kIntervals))];
        const float velocity = juce::jlimit(0.2f, 1.0f, 0.88f - 0.05f * static_cast<float>(voiceIndex % 4));
        voice.noteOn(preset.settings, chars, midiNote, velocity, kSampleRate);
        voice.render(temp, 0, totalSamples);
        for (int ch = 0; ch < mixed.getNumChannels(); ++ch)
            mixed.addFrom(ch, 0, temp, ch, 0, totalSamples);
    }

    const auto fx = fxSettingsFromPreset(preset.fx);
    applyTransient(mixed, fx);
    applySaturator(mixed, fx);
    applyCompressor(mixed, fx);
    applyDcHighPass(mixed);
    return mixed;
}

juce::AudioBuffer<float> renderFactoryClip(const int bassIndex,
                                           const int presetIndex,
                                           const int midiNote,
                                           const float velocity,
                                           const double holdSeconds,
                                           const double tailSeconds)
{
    const auto& bank = mbs::getFactoryPresetBanks()[static_cast<std::size_t>(bassIndex)];
    const auto& preset = bank[static_cast<std::size_t>(juce::jlimit(0, static_cast<int>(bank.size()) - 1, presetIndex))];
    const int holdSamples = static_cast<int>(std::round(holdSeconds * kSampleRate));
    const int tailSamples = static_cast<int>(std::round(tailSeconds * kSampleRate));
    juce::AudioBuffer<float> buffer(2, juce::jmax(1, holdSamples + tailSamples));
    buffer.clear();

    mbs::BassVoice voice;
    voice.noteOn(preset.settings, mbs::getCharacteristics(bassIndex), midiNote, velocity, kSampleRate);
    voice.render(buffer, 0, holdSamples);
    voice.noteOff();
    voice.render(buffer, holdSamples, tailSamples);

    const auto fx = fxSettingsFromPreset(preset.fx);
    applyTransient(buffer, fx);
    applySaturator(buffer, fx);
    applyCompressor(buffer, fx);
    applyDcHighPass(buffer);
    trimAndProtect(buffer, RenderJob{}, fx);
    return buffer;
}

RenderJob makeAuditionPanelJob(const musique::qa::audition::Variant& variant,
                               const int variantIndex,
                               const juce::String& instrument,
                               const juce::String& family,
                               const juce::String& relativePath)
{
    RenderJob job;
    job.instrument = instrument;
    job.family = family;
    job.category = variant.category;
    job.subcategory = variant.subcategory;
    job.articulation = variant.articulation;
    job.key = variant.key;
    job.tempoBpm = variant.tempoBpm;
    job.durationSeconds = variant.durationSeconds;
    job.velocityLayer = variant.velocityLayer;
    job.roundRobin = "rr" + juce::String((variantIndex % 3) + 1);
    job.take = "t" + juce::String((variantIndex % 4) + 1);
    job.finalRelativePath = relativePath;
    job.presetProfile = "factory";
    job.fxProfile = "factory";
    return job;
}

std::vector<Event> buildAuditionPanelEvents(const RenderJob& job,
                                            const musique::qa::audition::Variant& variant,
                                            const int midiNote,
                                            const float durationSeconds)
{
    const auto kind = juce::String(variant.kind);
    if (kind != "chord" && kind != "note")
        return buildEvents(job, midiNote, durationSeconds);

    const auto totalSamples = static_cast<int>(std::ceil(durationSeconds * kSampleRate));
    const auto velocity = velocityFromLayer(job.velocityLayer);
    const auto holdSamples = kind == "chord"
        ? static_cast<int>(totalSamples * 0.70f)
        : static_cast<int>(totalSamples * (isShortJob(job) ? 0.22f : 0.74f));

    std::vector<Event> events;
    auto add = [&events, holdSamples](const int note, const float vel)
    {
        events.push_back({ 0, true, note, vel });
        events.push_back({ juce::jmax(1, holdSamples), false, note, 0.0f });
    };

    if (kind == "note")
    {
        add(midiNote, velocity);
    }
    else
    {
        const bool minor = juce::String(variant.id).containsIgnoreCase("minor");
        const bool open = juce::String(variant.id).containsIgnoreCase("open");
        const int third = minor ? 3 : 4;
        const std::array<int, 4> intervals = open ? std::array<int, 4> { 0, 7, 12, 19 }
                                                   : std::array<int, 4> { 0, third, 7, 12 };
        for (int interval : intervals)
            add(midiNote + interval, velocity * (interval == 0 ? 0.90f : 0.48f));
    }

    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b)
    {
        if (a.sample == b.sample)
            return static_cast<int>(a.noteOn) > static_cast<int>(b.noteOn);
        return a.sample < b.sample;
    });
    return events;
}

bool renderFactoryAuditionToFile(const RenderJob& job,
                                 const musique::qa::audition::Variant& variant,
                                 const int bassIndex,
                                 const mbs::InstrumentPreset& preset,
                                 const juce::File& outputFile)
{
    const auto duration = juce::jmax(0.12f, job.durationSeconds.getFloatValue());
    const auto extraTail = isLoopJob(job) ? (isTextureJob(job) ? 0.6f : 0.2f)
        : (isTextureJob(job) ? 1.8f : (isShortJob(job) ? 0.25f : 0.8f));
    juce::AudioBuffer<float> buffer(2, static_cast<int>(std::ceil((duration + extraTail) * kSampleRate)));
    buffer.clear();

    renderTimeline(buffer,
                   buildAuditionPanelEvents(job, variant, midiNoteFromKey(job.key, job.instrument), duration),
                   preset.settings,
                   mbs::getCharacteristics(bassIndex));

    const auto fx = fxSettingsFromPreset(preset.fx);
    applyTransient(buffer, fx);
    applySaturator(buffer, fx);
    applyCompressor(buffer, fx);
    applyDcHighPass(buffer);
    trimAndProtect(buffer, job, fx);
    return writeWav(outputFile, buffer);
}

int runRenderAuditionPanel(juce::File outputBase, int variantsPerPreset, const bool overwrite)
{
    using namespace musique::qa::audition;

    variantsPerPreset = juce::jlimit(1, kDefaultVariantsPerPreset, variantsPerPreset);
    const auto& variants = factoryVariants();
    const auto& banks = mbs::getFactoryPresetBanks();

    std::vector<Row> rows;
    int total = 0;
    for (const auto& bank : banks)
        total += static_cast<int>(bank.size()) * variantsPerPreset;
    rows.reserve(static_cast<std::size_t>(total));

    int written = 0;
    int visited = 0;
    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        const auto instrumentName = juce::String(juce::CharPointer_UTF8(mbs::getBassName(bassIndex)));
        const auto familyName = juce::String(juce::CharPointer_UTF8(mbs::getFamilyName(static_cast<int>(mbs::getFamily(bassIndex)))));
        const auto& bank = banks[static_cast<std::size_t>(bassIndex)];

        for (int presetIndex = 0; presetIndex < static_cast<int>(bank.size()); ++presetIndex)
        {
            const auto& preset = bank[static_cast<std::size_t>(presetIndex)];
            const auto presetName = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));

            for (int variantIndex = 0; variantIndex < variantsPerPreset; ++variantIndex)
            {
                const auto& variant = variants[static_cast<std::size_t>(variantIndex)];
                const auto relativePath = makeAudioRelativePath(instrumentName, presetIndex, presetName, variantIndex, variant);
                const auto outputFile = outputBase.getChildFile(relativePath.replaceCharacter('/', juce::File::getSeparatorChar()));
                const auto job = makeAuditionPanelJob(variant, variantIndex, instrumentName, familyName, relativePath);

                if (!outputFile.existsAsFile() || overwrite)
                {
                    if (!renderFactoryAuditionToFile(job, variant, bassIndex, preset, outputFile))
                        throw std::runtime_error(("Failed to write WAV: " + outputFile.getFullPathName()).toStdString());
                    ++written;
                }

                rows.push_back({ bassIndex, instrumentName, presetIndex, presetName, variantIndex,
                                 variant.id, variant.kind, relativePath });
                ++visited;
                std::cout << "[" << visited << "/" << total << "] " << instrumentName << " / "
                          << presetName << " / " << variant.id << "\n";
            }
        }
    }

    if (!writePanelFiles(outputBase, "UWdeVST Bass Factory Audition Panel", rows))
        throw std::runtime_error(("Failed to write audition panel files: " + outputBase.getFullPathName()).toStdString());

    std::cout << "Audition panel ready: " << rows.size() << " entries, " << written
              << " WAV files written into " << outputBase.getFullPathName() << "\n";
    return 0;
}

void addFactoryClip(juce::AudioBuffer<float>& destination,
                    const int bassIndex,
                    const int presetIndex,
                    const int midiNote,
                    const double startSeconds,
                    const float gain)
{
    if (bassIndex < 0 || bassIndex >= mbs::kNumBasses)
        return;

    const auto& bank = mbs::getFactoryPresetBanks()[static_cast<std::size_t>(bassIndex)];
    if (bank.empty())
        return;

    auto clip = renderFactoryClip(bassIndex, presetIndex, midiNote, 0.84f, 0.28, 1.20);
    mixInto(destination, clip, static_cast<int>(std::round(startSeconds * kSampleRate)), gain);
}

std::vector<std::string> tagsFromCsv(const juce::String& tags)
{
    juce::StringArray parts;
    parts.addTokens(tags, ",", "");
    std::vector<std::string> result;
    for (auto part : parts)
    {
        part = part.trim();
        if (part.isNotEmpty())
            result.push_back(part.toStdString());
    }
    return result;
}

void writePerformanceXmlAttributes(juce::XmlElement& root, const mbs::PatchPerformanceSettings& performance)
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

void writeFxXmlAttributes(juce::XmlElement& root, const mbs::GlobalFxSettings& fx)
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

void writeModMatrixXml(juce::XmlElement& root, const modmatrix::MatrixState& state)
{
    auto* matrix = root.createNewChildElement("ModMatrix");
    matrix->setAttribute("pbRange", state.pitchBendRange);
    matrix->setAttribute("lfo2Rate", static_cast<double>(state.lfo2Rate));
    matrix->setAttribute("lfo2Wave", state.lfo2Wave);

    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        const auto& slot = state.slots[static_cast<std::size_t>(slotIndex)];
        auto* slotXml = matrix->createNewChildElement("Slot");
        slotXml->setAttribute("idx", slotIndex);
        slotXml->setAttribute("src", static_cast<int>(slot.source));
        slotXml->setAttribute("dst", static_cast<int>(slot.destination));
        slotXml->setAttribute("amt", static_cast<double>(slot.amount));
    }
}

std::unique_ptr<juce::XmlElement> createCandidatePresetXml(const int bassIndex,
                                                           const mbs::InstrumentPreset& preset)
{
    auto xml = std::make_unique<juce::XmlElement>("BassPreset");
    xml->setAttribute("format_version", 3);
    xml->setAttribute("name", juce::String(juce::CharPointer_UTF8(preset.name.c_str())));
    xml->setAttribute("bass", bassIndex);
    xml->setAttribute("instrument_index", bassIndex);
    xml->setAttribute("synth_index", 3);

    const auto& settings = preset.settings;
    xml->setAttribute("level",          static_cast<double>(settings.level));
    xml->setAttribute("tune",           static_cast<double>(settings.tuneSemitones));
    xml->setAttribute("brightness",     static_cast<double>(settings.brightness));
    xml->setAttribute("attack",         static_cast<double>(settings.attackSeconds));
    xml->setAttribute("decay",          static_cast<double>(settings.decaySeconds));
    xml->setAttribute("sustain",        static_cast<double>(settings.sustainLevel));
    xml->setAttribute("release",        static_cast<double>(settings.releaseSeconds));
    xml->setAttribute("body",           static_cast<double>(settings.body));
    xml->setAttribute("drive",          static_cast<double>(settings.drive));
    xml->setAttribute("pitch_env",      static_cast<double>(settings.pitchEnv));
    xml->setAttribute("filter_env",     static_cast<double>(settings.filterEnv));
    xml->setAttribute("sub",            static_cast<double>(settings.subLevel));
    xml->setAttribute("character",      static_cast<double>(settings.character));
    xml->setAttribute("cutoff",         static_cast<double>(settings.cutoffHz));
    xml->setAttribute("pan",            static_cast<double>(settings.pan));
    xml->setAttribute("resonance",      static_cast<double>(settings.resonance));
    xml->setAttribute("glide_time",     static_cast<double>(settings.glideTime));
    xml->setAttribute("pitch_env_time", static_cast<double>(settings.pitchEnvTime));
    xml->setAttribute("snap",           static_cast<double>(settings.snap));
    xml->setAttribute("env_shape",      static_cast<double>(settings.envShape));
    xml->setAttribute("output",         preset.outputBus);

    writePerformanceXmlAttributes(*xml, preset.performance.value_or(mbs::PatchPerformanceSettings {}));
    writeFxXmlAttributes(*xml, preset.fx);
    xml->setAttribute("mix_role", juce::String(juce::CharPointer_UTF8(preset.metadata.mixRole.c_str())));
    xml->setAttribute("family", juce::String(juce::CharPointer_UTF8(preset.metadata.familyLabel.c_str())));
    xml->setAttribute("tags", joinTags(preset.metadata.tags));
    xml->setAttribute("nominal_peak_db", static_cast<double>(preset.metadata.nominalPeakDb));
    writeModMatrixXml(*xml, preset.modMatrix.value_or(modmatrix::MatrixState {}));
    return xml;
}

float readXmlFloat(const juce::XmlElement& xml,
                   const char* attrName,
                   const float fallback,
                   const float minValue,
                   const float maxValue,
                   juce::StringArray& warnings)
{
    if (!xml.hasAttribute(attrName))
    {
        warnings.add("missing " + juce::String(attrName));
        return fallback;
    }

    const auto raw = xml.getStringAttribute(attrName).trim();
    char* end = nullptr;
    const auto parsed = std::strtod(raw.toRawUTF8(), &end);
    if (end == raw.toRawUTF8() || end == nullptr || *end != '\0' || !std::isfinite(parsed))
    {
        warnings.add("invalid " + juce::String(attrName));
        return fallback;
    }

    const auto value = static_cast<float>(parsed);
    const auto clamped = juce::jlimit(minValue, maxValue, value);
    if (std::abs(clamped - value) > 1.0e-5f)
        warnings.add("clamped " + juce::String(attrName));
    return clamped;
}

int readXmlInt(const juce::XmlElement& xml,
               const char* attrName,
               const int fallback,
               const int minValue,
               const int maxValue,
               juce::StringArray& warnings)
{
    if (!xml.hasAttribute(attrName))
    {
        warnings.add("missing " + juce::String(attrName));
        return fallback;
    }

    const auto value = xml.getIntAttribute(attrName, fallback);
    const auto clamped = juce::jlimit(minValue, maxValue, value);
    if (clamped != value)
        warnings.add("clamped " + juce::String(attrName));
    return clamped;
}

bool readXmlBool(const juce::XmlElement& xml, const char* attrName, const bool fallback)
{
    return xml.getIntAttribute(attrName, fallback ? 1 : 0) != 0;
}

mbs::BassSettings readBassSettingsFromXml(const juce::XmlElement& xml,
                                          const int bassIndex,
                                          juce::StringArray& warnings)
{
    auto settings = mbs::getDefaultSettings(bassIndex);
    settings.level          = readXmlFloat(xml, "level",          settings.level,          0.0f,    1.0f, warnings);
    settings.tuneSemitones  = readXmlFloat(xml, "tune",           settings.tuneSemitones, -24.0f,  24.0f, warnings);
    settings.brightness     = readXmlFloat(xml, "brightness",     settings.brightness,     0.0f,    1.0f, warnings);
    settings.attackSeconds  = readXmlFloat(xml, "attack",         settings.attackSeconds,  0.0f,    2.0f, warnings);
    settings.decaySeconds   = readXmlFloat(xml, "decay",          settings.decaySeconds,   0.01f,  10.0f, warnings);
    settings.sustainLevel   = readXmlFloat(xml, "sustain",        settings.sustainLevel,   0.0f,    1.0f, warnings);
    settings.releaseSeconds = readXmlFloat(xml, "release",        settings.releaseSeconds, 0.005f, 10.0f, warnings);
    settings.body           = readXmlFloat(xml, "body",           settings.body,           0.0f,    1.0f, warnings);
    settings.drive          = readXmlFloat(xml, "drive",          settings.drive,          0.0f,    1.0f, warnings);
    settings.pitchEnv       = readXmlFloat(xml, "pitch_env",      settings.pitchEnv,       0.0f,    1.0f, warnings);
    settings.filterEnv      = readXmlFloat(xml, "filter_env",     settings.filterEnv,      0.0f,    1.0f, warnings);
    settings.subLevel       = readXmlFloat(xml, "sub",            settings.subLevel,       0.0f,    1.0f, warnings);
    settings.character      = readXmlFloat(xml, "character",      settings.character,      0.0f,    1.0f, warnings);
    settings.cutoffHz       = readXmlFloat(xml, "cutoff",         settings.cutoffHz,     120.0f,12000.0f, warnings);
    settings.pan            = readXmlFloat(xml, "pan",            settings.pan,           -1.0f,    1.0f, warnings);
    settings.resonance      = readXmlFloat(xml, "resonance",      settings.resonance,      0.0f,    1.0f, warnings);
    settings.glideTime      = readXmlFloat(xml, "glide_time",     settings.glideTime,      0.0f,    1.5f, warnings);
    settings.pitchEnvTime   = readXmlFloat(xml, "pitch_env_time", settings.pitchEnvTime,   0.0f,    0.6f, warnings);
    settings.snap           = readXmlFloat(xml, "snap",           settings.snap,           0.0f,    1.0f, warnings);
    settings.envShape       = readXmlFloat(xml, "env_shape",      settings.envShape,       0.0f,    1.0f, warnings);
    return settings;
}

mbs::GlobalFxSettings readFxFromXml(const juce::XmlElement& xml,
                                    const int bassIndex,
                                    juce::StringArray& warnings)
{
    auto fx = mbs::getDefaultGlobalFx(bassIndex);
    fx.satDrive         = readXmlFloat(xml, "sat_drive",         fx.satDrive,          1.0f,   16.0f, warnings);
    fx.satMix           = readXmlFloat(xml, "sat_mix",           fx.satMix,            0.0f,    1.0f, warnings);
    fx.transientAttack  = readXmlFloat(xml, "transient_attack",  fx.transientAttack,  -1.0f,    1.0f, warnings);
    fx.transientSustain = readXmlFloat(xml, "transient_sustain", fx.transientSustain, -1.0f,    1.0f, warnings);
    fx.transientMix     = readXmlFloat(xml, "transient_mix",     fx.transientMix,      0.0f,    1.0f, warnings);
    fx.compThreshold    = readXmlFloat(xml, "comp_threshold",    fx.compThreshold,   -60.0f,    0.0f, warnings);
    fx.compRatio        = readXmlFloat(xml, "comp_ratio",        fx.compRatio,         1.0f,   20.0f, warnings);
    fx.compAttack       = readXmlFloat(xml, "comp_attack",       fx.compAttack,        0.1f,  100.0f, warnings);
    fx.compRelease      = readXmlFloat(xml, "comp_release",      fx.compRelease,       5.0f,  500.0f, warnings);
    fx.compMakeup       = readXmlFloat(xml, "comp_makeup",       fx.compMakeup,        0.0f,   24.0f, warnings);
    fx.compMix          = readXmlFloat(xml, "comp_mix",          fx.compMix,           0.0f,    1.0f, warnings);
    fx.eqLowFreq        = readXmlFloat(xml, "eq_low_freq",       fx.eqLowFreq,        40.0f,  800.0f, warnings);
    fx.eqLowGain        = readXmlFloat(xml, "eq_low_gain",       fx.eqLowGain,       -12.0f,   12.0f, warnings);
    fx.eqMidFreq        = readXmlFloat(xml, "eq_mid_freq",       fx.eqMidFreq,       200.0f, 8000.0f, warnings);
    fx.eqMidGain        = readXmlFloat(xml, "eq_mid_gain",       fx.eqMidGain,       -12.0f,   12.0f, warnings);
    fx.eqMidQ           = readXmlFloat(xml, "eq_mid_q",          fx.eqMidQ,            0.1f,   10.0f, warnings);
    fx.eqHighFreq       = readXmlFloat(xml, "eq_high_freq",      fx.eqHighFreq,     1000.0f,16000.0f, warnings);
    fx.eqHighGain       = readXmlFloat(xml, "eq_high_gain",      fx.eqHighGain,      -12.0f,   12.0f, warnings);
    fx.chorusRate       = readXmlFloat(xml, "chorus_rate",       fx.chorusRate,        0.1f,    8.0f, warnings);
    fx.chorusDepth      = readXmlFloat(xml, "chorus_depth",      fx.chorusDepth,       0.0f,    1.0f, warnings);
    fx.chorusMix        = readXmlFloat(xml, "chorus_mix",        fx.chorusMix,         0.0f,    1.0f, warnings);
    fx.delayTime        = readXmlFloat(xml, "delay_time",        fx.delayTime,        10.0f, 1500.0f, warnings);
    fx.delayFeedback    = readXmlFloat(xml, "delay_feedback",    fx.delayFeedback,     0.0f,    0.95f, warnings);
    fx.delayMix         = readXmlFloat(xml, "delay_mix",         fx.delayMix,          0.0f,    1.0f, warnings);
    fx.delaySync        = readXmlBool(xml, "delay_sync", fx.delaySync);
    fx.delayNoteDiv     = readXmlInt(xml, "delay_division", fx.delayNoteDiv, 0, 4, warnings);
    fx.reverbSize       = readXmlFloat(xml, "reverb_size",       fx.reverbSize,        0.0f,    1.0f, warnings);
    fx.reverbDamping    = readXmlFloat(xml, "reverb_damping",    fx.reverbDamping,     0.0f,    1.0f, warnings);
    fx.reverbWidth      = readXmlFloat(xml, "reverb_width",      fx.reverbWidth,       0.0f,    1.0f, warnings);
    fx.reverbMix        = readXmlFloat(xml, "reverb_mix",        fx.reverbMix,         0.0f,    1.0f, warnings);
    fx.limiterThreshold = readXmlFloat(xml, "limiter_threshold", fx.limiterThreshold, -12.0f,   0.0f, warnings);
    fx.limiterRelease   = readXmlFloat(xml, "limiter_release",   fx.limiterRelease,    1.0f,  200.0f, warnings);

    fx.saturatorOn  = readXmlBool(xml, "fx_tab0_en", fx.saturatorOn);
    fx.transientOn  = readXmlBool(xml, "fx_tab1_en", fx.transientOn);
    fx.compressorOn = readXmlBool(xml, "fx_tab2_en", fx.compressorOn);
    fx.eqOn         = readXmlBool(xml, "fx_tab3_en", fx.eqOn);
    fx.chorusOn     = readXmlBool(xml, "fx_tab4_en", fx.chorusOn);
    fx.delayOn      = readXmlBool(xml, "fx_tab5_en", fx.delayOn);
    fx.reverbOn     = readXmlBool(xml, "fx_tab6_en", fx.reverbOn);
    fx.limiterOn    = readXmlBool(xml, "fx_tab7_en", fx.limiterOn);

    const auto availability = mbs::getFxAvailability(bassIndex);
    if (fx.saturatorOn && !availability.saturator) warnings.add("unavailable saturator enabled");
    if (fx.transientOn && !availability.transient) warnings.add("unavailable transient enabled");
    if (fx.compressorOn && !availability.compressor) warnings.add("unavailable compressor enabled");
    if (fx.eqOn && !availability.eq) warnings.add("unavailable eq enabled");
    if (fx.chorusOn && !availability.chorus) warnings.add("unavailable chorus enabled");
    if (fx.delayOn && !availability.delay) warnings.add("unavailable delay enabled");
    if (fx.reverbOn && !availability.reverb) warnings.add("unavailable reverb enabled");
    if (fx.limiterOn && !availability.limiter) warnings.add("unavailable limiter enabled");

    mbs::maskUnavailableFx(bassIndex, fx);
    return fx;
}

mbs::PatchPerformanceSettings readPerformanceFromXml(const juce::XmlElement& xml,
                                                      juce::StringArray& warnings)
{
    mbs::PatchPerformanceSettings performance;
    performance.monoMode = readXmlInt(xml, "mono_mode", performance.monoMode, 0, 2, warnings);
    performance.lfoRate = readXmlFloat(xml, "lfo_rate", performance.lfoRate, 0.05f, 12.0f, warnings);
    performance.lfoDepth = readXmlFloat(xml, "lfo_depth", performance.lfoDepth, 0.0f, 1.0f, warnings);
    performance.lfoWave = readXmlInt(xml, "lfo_wave", performance.lfoWave, 0, 3, warnings);
    performance.lfoDest = readXmlInt(xml, "lfo_dest", performance.lfoDest, 0, 2, warnings);
    performance.macroFatness = readXmlFloat(xml, "macro_fatness", performance.macroFatness, 0.0f, 1.0f, warnings);
    performance.macroBrillance = readXmlFloat(xml, "macro_brillance", performance.macroBrillance, 0.0f, 1.0f, warnings);
    performance.macroPunch = readXmlFloat(xml, "macro_punch", performance.macroPunch, 0.0f, 1.0f, warnings);
    performance.macroDepth = readXmlFloat(xml, "macro_depth", performance.macroDepth, 0.0f, 1.0f, warnings);
    performance.modWheelTarget = readXmlInt(xml, "mod_wheel_target", performance.modWheelTarget, 0, 2, warnings);
    performance.pitchBendRange = readXmlFloat(xml, "pitch_bend_range", performance.pitchBendRange, 1.0f, 24.0f, warnings);
    return performance;
}

void validateCandidateXml(const juce::XmlElement& xml,
                          const int bassIndex,
                          juce::StringArray& warnings)
{
    if (!xml.hasTagName("BassPreset"))
        warnings.add("root is not BassPreset");
    if (xml.getIntAttribute("format_version", 0) != 3)
        warnings.add("format_version is not 3");
    if (xml.getIntAttribute("synth_index", -1) != 3)
        warnings.add("synth_index is not 3");
    if (xml.getIntAttribute("bass", bassIndex) != bassIndex
        || xml.getIntAttribute("instrument_index", bassIndex) != bassIndex)
    {
        warnings.add("bass index mismatch");
    }
    if (std::abs(static_cast<float>(xml.getDoubleAttribute("tune", 0.0))) > 1.0e-6f)
        warnings.add("tune is not zero");
    if (xml.getIntAttribute("fx_tab5_en", 0) != 0)
        warnings.add("delay enabled");
    if (std::abs(static_cast<float>(xml.getDoubleAttribute("delay_mix", 0.0))) > 1.0e-6f)
        warnings.add("delay mix not zero");
    if (std::abs(static_cast<float>(xml.getDoubleAttribute("delay_feedback", 0.0))) > 1.0e-6f)
        warnings.add("delay feedback not zero");

    const auto* matrix = xml.getChildByName("ModMatrix");
    if (matrix == nullptr || !matrix->hasAttribute("pbRange") || !matrix->hasAttribute("lfo2Rate") || !matrix->hasAttribute("lfo2Wave"))
    {
        warnings.add("incomplete ModMatrix header");
    }
    else
    {
        std::array<bool, modmatrix::kMaxSlots> seen {};
        int slotCount = 0;
        for (auto* slotXml : matrix->getChildWithTagNameIterator("Slot"))
        {
            const int idx = slotXml->getIntAttribute("idx", -1);
            if (idx < 0 || idx >= modmatrix::kMaxSlots)
            {
                warnings.add("invalid ModMatrix slot");
                continue;
            }
            seen[static_cast<std::size_t>(idx)] = true;
            ++slotCount;
            if (!slotXml->hasAttribute("src") || !slotXml->hasAttribute("dst") || !slotXml->hasAttribute("amt"))
                warnings.add("incomplete ModMatrix slot");
        }
        if (slotCount != modmatrix::kMaxSlots)
            warnings.add("ModMatrix slot count mismatch");
        for (bool exists : seen)
            if (!exists)
                warnings.add("missing ModMatrix slot");
    }
}

CandidatePresetInfo loadCandidatePresetInfo(const juce::File& file, const juce::File& presetDir)
{
    CandidatePresetInfo info;
    info.file = file;
    info.relativePath = file.getRelativePathFrom(presetDir).replaceCharacter('\\', '/');

    juce::XmlDocument document(file);
    auto xml = document.getDocumentElement();
    if (xml == nullptr)
    {
        info.warnings.add("xml parse failed");
        return info;
    }

    info.bassIndex = juce::jlimit(0, mbs::kNumBasses - 1,
                                  xml->getIntAttribute("instrument_index", xml->getIntAttribute("bass", 0)));
    info.presetName = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
    info.presetIndex = info.presetName.containsIgnoreCase("signature") ? 1 : 0;

    const auto& bank = mbs::getFactoryPresetBanks()[static_cast<std::size_t>(info.bassIndex)];
    if (info.presetIndex >= 0 && info.presetIndex < static_cast<int>(bank.size()))
        info.metadata = bank[static_cast<std::size_t>(info.presetIndex)].metadata;
    info.settings = readBassSettingsFromXml(*xml, info.bassIndex, info.warnings);
    info.fx = readFxFromXml(*xml, info.bassIndex, info.warnings);
    info.performance = readPerformanceFromXml(*xml, info.warnings);
    info.metadata.mixRole = xml->getStringAttribute("mix_role", juce::String(juce::CharPointer_UTF8(info.metadata.mixRole.c_str()))).toStdString();
    info.metadata.familyLabel = xml->getStringAttribute("family", juce::String(juce::CharPointer_UTF8(info.metadata.familyLabel.c_str()))).toStdString();
    if (xml->hasAttribute("tags"))
        info.metadata.tags = tagsFromCsv(xml->getStringAttribute("tags"));
    if (xml->hasAttribute("nominal_peak_db"))
        info.metadata.nominalPeakDb = readXmlFloat(*xml, "nominal_peak_db", info.metadata.nominalPeakDb, -24.0f, 0.0f, info.warnings);

    validateCandidateXml(*xml, info.bassIndex, info.warnings);
    return info;
}

std::vector<CandidatePresetInfo> collectCandidatePresetInfos(const juce::File& presetDir)
{
    if (!presetDir.isDirectory())
        throw std::runtime_error(("Preset directory does not exist: " + presetDir.getFullPathName()).toStdString());

    juce::Array<juce::File> found;
    presetDir.findChildFiles(found, juce::File::findFiles, true, "*.xml");
    std::vector<juce::File> files;
    files.reserve(static_cast<std::size_t>(found.size()));
    for (const auto& file : found)
        files.push_back(file);
    std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b)
    {
        return a.getFullPathName() < b.getFullPathName();
    });

    std::vector<CandidatePresetInfo> presets;
    presets.reserve(files.size());
    for (const auto& file : files)
        presets.push_back(loadCandidatePresetInfo(file, presetDir));
    return presets;
}

mbs::GlobalFxSettings makeDryFx(mbs::GlobalFxSettings fx)
{
    fx.saturatorOn = false;
    fx.transientOn = false;
    fx.compressorOn = false;
    fx.eqOn = false;
    fx.chorusOn = false;
    fx.delayOn = false;
    fx.reverbOn = false;
    fx.limiterOn = true;
    fx.satMix = 0.0f;
    fx.transientMix = 0.0f;
    fx.compMix = 0.0f;
    fx.chorusMix = 0.0f;
    fx.delayMix = 0.0f;
    fx.delayFeedback = 0.0f;
    fx.reverbMix = 0.0f;
    return fx;
}

int candidateBaseMidiNote(const int bassIndex)
{
    if (mbs::getFamily(bassIndex) == mbs::Family::Eight08)
        return 36;
    if (mbs::getFamily(bassIndex) == mbs::Family::Acoustic)
        return 31;
    return 36;
}

std::vector<Event> makeCandidateEvents(const juce::String& audioCase,
                                       const int bassIndex,
                                       const double durationSeconds)
{
    const int totalSamples = static_cast<int>(std::ceil(durationSeconds * kSampleRate));
    const int base = candidateBaseMidiNote(bassIndex);
    std::vector<Event> events;
    const auto addNote = [&events, totalSamples](const double startSeconds,
                                                 const double holdSeconds,
                                                 const int note,
                                                 const float velocity)
    {
        const auto start = juce::jlimit(0, totalSamples - 1, static_cast<int>(std::round(startSeconds * kSampleRate)));
        const auto end = juce::jlimit(start + 1, totalSamples - 1, start + static_cast<int>(std::round(holdSeconds * kSampleRate)));
        events.push_back({ start, true, note, velocity });
        events.push_back({ end, false, note, 0.0f });
    };

    if (audioCase == "preview")
    {
        addNote(0.00, 0.54, base, 0.78f);
        addNote(0.72, 0.48, base + 7, 0.70f);
        addNote(1.34, 0.42, base + 12, 0.62f);
    }
    else if (audioCase == "repeated_fx")
    {
        const int notes[] = { base, base + 7, base + 3, base + 12 };
        for (int i = 0; i < 4; ++i)
            addNote(0.10 + 0.43 * static_cast<double>(i), 0.24, notes[i], 0.72f - 0.04f * static_cast<float>(i));
    }
    else if (audioCase == "strict_chord_dry" || audioCase == "voicing_open_dry")
    {
        const int intervals[] = { 0, 7, 12, 19 };
        const float velocities[] = { 0.52f, 0.30f, 0.24f, 0.18f };
        for (int i = 0; i < 4; ++i)
            addNote(0.02, audioCase == "strict_chord_dry" ? 1.05 : 0.90, base + intervals[i], velocities[i]);
    }
    else if (audioCase == "sustain_long_dry")
    {
        addNote(0.02, 2.75, base, 0.70f);
    }
    else
    {
        addNote(0.02, 0.78, base, 0.74f);
    }

    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b)
    {
        if (a.sample == b.sample)
            return static_cast<int>(a.noteOn) > static_cast<int>(b.noteOn);
        return a.sample < b.sample;
    });
    return events;
}

double durationForCandidateCase(const juce::String& audioCase)
{
    if (audioCase == "sustain_long_dry") return 3.80;
    if (audioCase == "repeated_fx") return 2.25;
    if (audioCase == "strict_chord_dry") return 1.85;
    if (audioCase == "voicing_open_dry") return 1.70;
    if (audioCase == "preview") return 2.05;
    return 1.45;
}

void applyCandidateFades(juce::AudioBuffer<float>& buffer)
{
    const int fadeIn = juce::jmin(96, buffer.getNumSamples() / 5);
    const int fadeOut = juce::jmin(384, buffer.getNumSamples() / 4);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        if (fadeIn > 0)
            buffer.applyGainRamp(ch, 0, fadeIn, 0.0f, 1.0f);
        if (fadeOut > 0)
            buffer.applyGainRamp(ch, buffer.getNumSamples() - fadeOut, fadeOut, 1.0f, 0.0f);
    }
}

CandidateAudioRow renderCandidateCase(const CandidatePresetInfo& preset,
                                      const juce::String& audioCase,
                                      const bool dry,
                                      const juce::File& outputBase,
                                      const bool overwrite)
{
    const auto instrumentSlug = candidateInstrumentSlugForBass(preset.bassIndex);
    const auto wavFile = outputBase
        .getChildFile(instrumentSlug)
        .getChildFile(slug(preset.presetName) + "_" + audioCase + ".wav");
    const auto duration = durationForCandidateCase(audioCase);
    juce::AudioBuffer<float> buffer(2, static_cast<int>(std::ceil(duration * kSampleRate)));
    buffer.clear();

    renderTimeline(buffer,
                   makeCandidateEvents(audioCase, preset.bassIndex, duration),
                   preset.settings,
                   mbs::getCharacteristics(preset.bassIndex));

    const auto fx = fxSettingsFromPreset(dry ? makeDryFx(preset.fx) : preset.fx);
    applyTransient(buffer, fx);
    applySaturator(buffer, fx);
    applyCompressor(buffer, fx);
    applyDcHighPass(buffer);
    applyCandidateFades(buffer);
    protectCeiling(buffer, kCandidatePeakCeilingDb);

    CandidateAudioRow row;
    row.instrumentIndex = preset.bassIndex;
    row.instrument = mbs::getBassName(preset.bassIndex);
    row.presetName = preset.presetName;
    row.presetFile = preset.relativePath;
    row.audioCase = audioCase;
    row.wavFile = wavFile.getRelativePathFrom(outputBase).replaceCharacter('\\', '/');
    row.sanitizationWarnings = preset.warnings.size();
    row.importStatus = preset.warnings.isEmpty() ? "PASS" : "WARN";
    row.warnings = preset.warnings.joinIntoString("; ");

    const auto metrics = measureBuffer(buffer);
    row.peakDb = metrics.peakDb;
    row.rmsDb = metrics.rmsDb;
    row.crestDb = metrics.peakDb - metrics.rmsDb;
    row.clipped = metrics.peakDb > -0.05f;

    juce::StringArray issues;
    if (!metrics.finite) issues.add("NaN/Inf");
    if (metrics.peakDb < musique::qa::kMinimumAudiblePeakDb) issues.add("silent");
    if (metrics.peakDb > kCandidatePeakCeilingDb + 0.05f) issues.add("peak above candidate ceiling");
    if (row.clipped) issues.add("clipping");
    if (!preset.warnings.isEmpty()) issues.add("import warnings");
    row.status = issues.isEmpty() ? "PASS" : "FAIL";
    if (row.warnings.isEmpty())
        row.warnings = issues.joinIntoString("; ");
    else if (!issues.isEmpty())
        row.warnings << "; " << issues.joinIntoString("; ");

    if (!wavFile.existsAsFile() || overwrite)
    {
        if (!writeWav(wavFile, buffer))
            throw std::runtime_error(("Failed to write WAV: " + wavFile.getFullPathName()).toStdString());
    }

    return row;
}

void writeCandidateAudioReport(const juce::File& reportFile,
                               const std::vector<CandidateAudioRow>& rows)
{
    reportFile.getParentDirectory().createDirectory();
    juce::String csv = "instrument_index,instrument,preset_name,preset_file,case,wav_file,import_status,sanitization_warnings,peak_dbfs,rms_dbfs,crest_db,clipped,status,warnings\n";
    for (const auto& row : rows)
    {
        csv << row.instrumentIndex << ','
            << csvEscape(row.instrument) << ','
            << csvEscape(row.presetName) << ','
            << csvEscape(row.presetFile) << ','
            << csvEscape(row.audioCase) << ','
            << csvEscape(row.wavFile) << ','
            << csvEscape(row.importStatus) << ','
            << row.sanitizationWarnings << ','
            << juce::String(row.peakDb, 3) << ','
            << juce::String(row.rmsDb, 3) << ','
            << juce::String(row.crestDb, 3) << ','
            << (row.clipped ? "true" : "false") << ','
            << csvEscape(row.status) << ','
            << csvEscape(row.warnings) << '\n';
    }
    if (!writeTextFile(reportFile, csv))
        throw std::runtime_error(("Failed to write candidate audio report: " + reportFile.getFullPathName()).toStdString());
}

int runExportCandidatePresets(const juce::File& outputBase, const bool overwrite)
{
    const auto presetRoot = outputBase.getChildFile("bass_candidate_presets");
    if (overwrite && presetRoot.exists())
        presetRoot.deleteRecursively();
    presetRoot.createDirectory();

    const auto& banks = mbs::getFactoryPresetBanks();
    int written = 0;
    for (int bassIndex = 0; bassIndex < mbs::kNumBasses; ++bassIndex)
    {
        const auto instrumentDir = presetRoot.getChildFile(candidateInstrumentSlugForBass(bassIndex));
        instrumentDir.createDirectory();
        const auto bareSlug = bareInstrumentSlugForBass(bassIndex);
        const auto& bank = banks[static_cast<std::size_t>(bassIndex)];
        for (int presetIndex = 0; presetIndex < static_cast<int>(bank.size()); ++presetIndex)
        {
            const auto roleSuffix = presetIndex == 0 ? "a" : "b";
            const auto fileName = juce::String::formatted("%02d_", presetIndex + 1) + bareSlug + "_" + roleSuffix + ".xml";
            const auto file = instrumentDir.getChildFile(fileName);
            if (file.existsAsFile() && !overwrite)
                continue;

            auto xml = createCandidatePresetXml(bassIndex, bank[static_cast<std::size_t>(presetIndex)]);
            if (!xml->writeTo(file))
                throw std::runtime_error(("Failed to write XML: " + file.getFullPathName()).toStdString());
            ++written;
        }
    }

    std::cout << "Exported " << written << " candidate preset XML files into "
              << presetRoot.getFullPathName() << "\n";
    return 0;
}

int runRenderXmlPreviews(const juce::File& presetDir,
                         const juce::File& outputBase,
                         const bool overwrite)
{
    if (overwrite && outputBase.exists())
        outputBase.deleteRecursively();
    const auto presets = collectCandidatePresetInfos(presetDir);
    int rendered = 0;
    for (const auto& preset : presets)
    {
        renderCandidateCase(preset, "preview", false, outputBase, overwrite);
        ++rendered;
    }
    std::cout << "Rendered " << rendered << " preview WAV files into "
              << outputBase.getFullPathName() << "\n";
    return 0;
}

int runStrictChordAudit(const juce::File& presetDir,
                        const juce::File& outputBase,
                        const juce::File& reportFile,
                        const bool overwrite)
{
    if (overwrite && outputBase.exists())
        outputBase.deleteRecursively();
    const auto presets = collectCandidatePresetInfos(presetDir);
    std::vector<CandidateAudioRow> rows;
    rows.reserve(presets.size());
    for (const auto& preset : presets)
        rows.push_back(renderCandidateCase(preset, "strict_chord_dry", true, outputBase, overwrite));

    writeCandidateAudioReport(reportFile, rows);
    std::cout << "Strict chord audit: " << rows.size() << " WAV files into "
              << outputBase.getFullPathName() << "\nReport: "
              << reportFile.getFullPathName() << "\n";
    return std::any_of(rows.begin(), rows.end(), [](const CandidateAudioRow& row) { return row.status != "PASS"; }) ? 1 : 0;
}

int runAuditAudio(const juce::File& presetDir,
                  const juce::File& outputBase,
                  const juce::File& reportFile,
                  const bool overwrite)
{
    if (overwrite && outputBase.exists())
        outputBase.deleteRecursively();
    const auto presets = collectCandidatePresetInfos(presetDir);
    static constexpr const char* kCases[] = {
        "note_mid_dry",
        "voicing_open_dry",
        "sustain_long_dry",
        "repeated_fx"
    };

    std::vector<CandidateAudioRow> rows;
    rows.reserve(presets.size() * std::size(kCases));
    for (const auto& preset : presets)
        for (const auto* audioCase : kCases)
            rows.push_back(renderCandidateCase(preset, audioCase, juce::String(audioCase).endsWith("_dry"), outputBase, overwrite));

    writeCandidateAudioReport(reportFile, rows);
    std::cout << "Audio audit: " << rows.size() << " WAV files into "
              << outputBase.getFullPathName() << "\nReport: "
              << reportFile.getFullPathName() << "\n";
    return std::any_of(rows.begin(), rows.end(), [](const CandidateAudioRow& row) { return row.status != "PASS"; }) ? 1 : 0;
}

double estimateFundamentalHz(const juce::AudioBuffer<float>& buffer,
                             const double minHz,
                             const double maxHz,
                             double& stability)
{
    stability = 0.0;
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return 0.0;

    const int start = juce::jlimit(0, buffer.getNumSamples() - 1, static_cast<int>(0.35 * kSampleRate));
    const int maxWindow = static_cast<int>(0.75 * kSampleRate);
    const int window = juce::jmin(maxWindow, buffer.getNumSamples() - start);
    if (window < static_cast<int>(0.18 * kSampleRate))
        return 0.0;

    std::vector<float> mono(static_cast<std::size_t>(window), 0.0f);
    double mean = 0.0;
    for (int i = 0; i < window; ++i)
    {
        float sample = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            sample += buffer.getSample(ch, start + i);
        sample /= static_cast<float>(buffer.getNumChannels());
        mono[static_cast<std::size_t>(i)] = sample;
        mean += sample;
    }
    mean /= static_cast<double>(window);
    for (auto& sample : mono)
        sample -= static_cast<float>(mean);

    const int minLag = juce::jmax(1, static_cast<int>(std::floor(kSampleRate / maxHz)));
    const int maxLag = juce::jmin(window / 2, static_cast<int>(std::ceil(kSampleRate / minHz)));

    double bestCorr = 0.0;
    int bestLag = 0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double corr = 0.0;
        double e0 = 0.0;
        double e1 = 0.0;
        const int samples = window - lag;
        for (int i = 0; i < samples; ++i)
        {
            const auto a = static_cast<double>(mono[static_cast<std::size_t>(i)]);
            const auto b = static_cast<double>(mono[static_cast<std::size_t>(i + lag)]);
            corr += a * b;
            e0 += a * a;
            e1 += b * b;
        }

        const auto norm = std::sqrt(e0 * e1);
        const auto normalized = norm > 1.0e-12 ? corr / norm : 0.0;
        if (normalized > bestCorr)
        {
            bestCorr = normalized;
            bestLag = lag;
        }
    }

    stability = bestCorr;
    return bestLag > 0 ? kSampleRate / static_cast<double>(bestLag) : 0.0;
}
// --validate-presets : render one note per bass/preset, check peak / NaN / Inf
// =============================================================================
static int runValidatePresets(const juce::File& reportFile)
{
    constexpr float kMinPeakDb = musique::qa::kMinimumAudiblePeakDb;
    constexpr float kMaxPeakDb = musique::qa::kMaximumPeakDb + musique::qa::kClippingToleranceDb;
    constexpr float kMinRmsDb  = -48.0f;

    const auto& banks = mbs::getFactoryPresetBanks();
    int fails = 0;
    int checks = 0;
    std::vector<PresetQaRow> rows;

    for (int bass = 0; bass < mbs::kNumBasses; ++bass)
    {
        const auto& chars = mbs::getCharacteristics(bass);
        const auto& bank  = banks[static_cast<std::size_t>(bass)];
        for (std::size_t p = 0; p < bank.size(); ++p)
        {
            ++checks;
            const auto& preset = bank[p];
            const auto fx = fxSettingsFromPreset(preset.fx);
            const auto renderSeconds = preset.metadata.mixRole == "texture-bass" ? 1.75f : 1.0f;
            const auto minRmsDb = preset.metadata.mixRole == "sub-foundation" ? -54.0f : kMinRmsDb;
            const auto renderSamples = static_cast<int>(kSampleRate * renderSeconds);
            mbs::BassVoice voice;
            voice.noteOn(preset.settings, chars, 36, 0.8f, kSampleRate);

            juce::AudioBuffer<float> buf(2, renderSamples);
            buf.clear();
            voice.render(buf, 0, renderSamples);
            applyTransient(buf, fx);
            applySaturator(buf, fx);
            applyCompressor(buf, fx);
            applyDcHighPass(buf);
            trimAndProtect(buf, RenderJob{}, fx);
            const auto analysedSamples = buf.getNumSamples();

            juce::ignoreUnused(analysedSamples);
            const auto metrics = measureBuffer(buf);

            PresetQaRow row;
            row.instrument = mbs::getBassName(bass);
            row.preset = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
            row.family = juce::String(juce::CharPointer_UTF8(preset.metadata.familyLabel.c_str()));
            row.mixRole = juce::String(juce::CharPointer_UTF8(preset.metadata.mixRole.c_str()));
            row.tags = joinTags(preset.metadata.tags);
            row.peakDb = metrics.peakDb;
            row.rmsDb = metrics.rmsDb;

            juce::StringArray issues;
            if (!metrics.finite) issues.add("NaN/Inf");
            if (row.family.isEmpty()) issues.add("missing family");
            if (row.mixRole.isEmpty()) issues.add("missing mix role");
            if (row.tags.isEmpty()) issues.add("missing tags");
            if (!std::isfinite(preset.metadata.nominalPeakDb)) issues.add("invalid nominal peak");
            if (metrics.peakDb < kMinPeakDb) issues.add("silent");
            if (metrics.peakDb > kMaxPeakDb) issues.add("clipping");
            if (metrics.rmsDb < minRmsDb) issues.add("RMS too low");

            if (!issues.isEmpty())
            {
                ++fails;
                row.status = "FAIL";
                row.issues = issues.joinIntoString("; ");
                std::cout << "[FAIL] bass " << bass << " / " << preset.name
                          << " : " << row.issues << "\n";
            }

            rows.push_back(std::move(row));
        }
    }

    writePresetQaReport(reportFile, rows);
    std::cout << "Preset validation: " << (checks - fails) << "/" << checks << " passed";
    if (fails > 0) std::cout << "  (" << fails << " failed)";
    std::cout << "\nReport: " << reportFile.getFullPathName() << "\n";
    return fails > 0 ? 1 : 0;
}

static int runBenchmarkCpu(const juce::File& reportFile,
                           const juce::File& baselineFile,
                           const std::optional<int> requestedVoices,
                           const double thresholdFraction)
{
    struct BenchmarkPreset
    {
        int bassIndex;
        const char* presetName;
    };

    static constexpr BenchmarkPreset kBenchmarkPresets[] = {
        { 0, "Contrebasse Signature" },
        { 4, "Boom 808 Signature" },
        { 5, "Distorted 808 Signature" },
        { 7, "Reese Bass Signature" },
        { 8, "Acid Bass Signature" },
    };

    const auto& banks = mbs::getFactoryPresetBanks();
    const std::vector<int> voiceCounts = requestedVoices.has_value()
        ? std::vector<int> { juce::jmax(1, *requestedVoices) }
        : std::vector<int> { 8, 16, 32 };
    constexpr double kRenderSeconds = 1.5;
    std::vector<BenchmarkRow> rows;

    for (const int voices : voiceCounts)
    {
        double totalMsPerVoiceSecond = 0.0;
        int totalCount = 0;
        for (const auto& spec : kBenchmarkPresets)
        {
            const auto& bank = banks[static_cast<std::size_t>(spec.bassIndex)];
            const int presetIndex = presetIndexByName(bank, spec.presetName);
            if (presetIndex < 0)
                throw std::runtime_error(("Missing bass benchmark preset: " + juce::String(spec.presetName)).toStdString());

            const auto& preset = bank[static_cast<std::size_t>(presetIndex)];
            const auto startedMs = juce::Time::getMillisecondCounterHiRes();
            auto rendered = renderPresetForBenchmark(spec.bassIndex, preset, voices, kRenderSeconds);
            ignoreUnused(rendered);
            const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - startedMs;

            BenchmarkRow row;
            row.instrument = mbs::getBassName(spec.bassIndex);
            row.preset = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
            row.voices = voices;
            row.renderSeconds = kRenderSeconds;
            row.elapsedMs = elapsedMs;
            row.realtimeFactor = (kRenderSeconds * 1000.0) / juce::jmax(0.001, elapsedMs);
            row.msPerVoiceSecond = elapsedMs / (static_cast<double>(voices) * kRenderSeconds);
            totalMsPerVoiceSecond += row.msPerVoiceSecond;
            ++totalCount;
            rows.push_back(row);
        }

        BenchmarkRow summary;
        summary.instrument = "ALL";
        summary.preset = "summary";
        summary.voices = voices;
        summary.renderSeconds = kRenderSeconds;
        summary.msPerVoiceSecond = totalCount > 0 ? totalMsPerVoiceSecond / static_cast<double>(totalCount) : 0.0;
        rows.push_back(summary);
    }

    writeBenchmarkReport(reportFile, rows);
    std::cout << "Benchmark report: " << reportFile.getFullPathName() << "\n";

    if (baselineFile.existsAsFile())
    {
        const auto baselineSummary = loadBenchmarkSummaryMsPerVoiceSecond(baselineFile);
        for (const int voices : voiceCounts)
        {
            const auto current = std::find_if(rows.begin(), rows.end(), [voices](const BenchmarkRow& row)
            {
                return row.instrument == "ALL" && row.preset == "summary" && row.voices == voices;
            });
            if (current == rows.end())
                continue;

            const auto baselineIt = baselineSummary.find(voices);
            if (baselineIt == baselineSummary.end())
                continue;

            const double allowed = baselineIt->second * (1.0 + thresholdFraction);
            if (current->msPerVoiceSecond > allowed)
            {
                std::cout << "[FAIL] voices " << voices
                          << " summary regression: " << current->msPerVoiceSecond
                          << " ms/voice/s > allowed " << allowed << "\n";
                return 1;
            }
        }
    }

    return 0;
}

static int runValidatePitch(const juce::File& reportFile)
{
    static constexpr int kTestNotes[] = { 24, 29, 33, 36, 40, 45, 48 };
    const auto& banks = mbs::getFactoryPresetBanks();
    std::vector<PitchQaRow> rows;
    int failures = 0;

    for (int bass = 0; bass < mbs::kNumBasses; ++bass)
    {
        const auto& bank = banks[static_cast<std::size_t>(bass)];
        if (bank.empty())
        {
            PitchQaRow row;
            row.bassIndex = bass;
            row.instrument = mbs::getBassName(bass);
            row.family = familyLabelForBass(bass);
            row.verdict = "FAIL";
            row.issues = "empty factory bank";
            rows.push_back(row);
            ++failures;
            continue;
        }

        auto settings = bank.front().settings;
        settings.pitchEnv = 0.0f;
        settings.glideTime = 0.0f;
        settings.drive = juce::jmin(settings.drive, bass == 5 ? 0.55f : 0.35f);
        settings.releaseSeconds = juce::jmin(settings.releaseSeconds, 0.20f);
        // Pitch QA measures the main oscillator center; octave/subharmonic layers can bias autocorrelation on C1.
        settings.subLevel = 0.0f;

        for (const int midiNote : kTestNotes)
        {
            juce::AudioBuffer<float> buffer(2, static_cast<int>(1.25 * kSampleRate));
            buffer.clear();
            mbs::BassVoice voice;
            voice.noteOn(settings, mbs::getCharacteristics(bass), midiNote, 0.82f, kSampleRate);
            voice.render(buffer, 0, buffer.getNumSamples());
            applyDcHighPass(buffer);

            const auto expectedHz = midiNoteToHz(midiNote);
            double stability = 0.0;
            auto estimatedHz = estimateFundamentalHz(buffer, 24.0, 220.0, stability);
            while (estimatedHz > expectedHz * 1.55)
                estimatedHz *= 0.5;
            while (estimatedHz > 0.0 && estimatedHz < expectedHz * 0.65)
                estimatedHz *= 2.0;
            if (estimatedHz > 0.0)
            {
                double bestHz = estimatedHz;
                auto bestCents = std::abs(1200.0 * std::log2(bestHz / expectedHz));
                static constexpr double kCandidateScales[] = { 0.5, 1.0, 1.5, 2.0, 3.0 };
                for (const auto scale : kCandidateScales)
                {
                    const auto candidateHz = estimatedHz * scale;
                    if (candidateHz < 20.0 || candidateHz > 260.0)
                        continue;
                    const auto candidateCents = std::abs(1200.0 * std::log2(candidateHz / expectedHz));
                    if (candidateCents < bestCents)
                    {
                        bestCents = candidateCents;
                        bestHz = candidateHz;
                    }
                }
                estimatedHz = bestHz;
            }

            PitchQaRow row;
            row.bassIndex = bass;
            row.instrument = mbs::getBassName(bass);
            row.family = familyLabelForBass(bass);
            row.midiNote = midiNote;
            row.expectedHz = expectedHz;
            row.estimatedHz = estimatedHz;
            row.stability = stability;
            row.centsError = estimatedHz > 0.0 ? 1200.0 * std::log2(estimatedHz / expectedHz)
                                               : std::numeric_limits<double>::infinity();

            const auto absCents = std::abs(row.centsError);
            double centsTolerance = 80.0;
            double minStability = 0.035;
            if (mbs::getFamily(bass) == mbs::Family::Eight08)
                centsTolerance = bass == 5 ? 140.0 : 110.0;
            else if (mbs::getFamily(bass) == mbs::Family::Synth)
                centsTolerance = bass == 7 ? 140.0 : 115.0;

            juce::StringArray issues;
            if (estimatedHz <= 0.0 || !std::isfinite(estimatedHz)) issues.add("pitch not estimated");
            if (absCents > centsTolerance) issues.add("cents error above tolerance");
            if (stability < minStability) issues.add("unstable center");

            row.verdict = issues.isEmpty() ? "PASS" : "FAIL";
            row.issues = issues.joinIntoString("; ");
            if (row.verdict == "FAIL")
            {
                ++failures;
                std::cout << "[FAIL] pitch " << row.instrument << " MIDI " << midiNote
                          << " : " << row.issues << " ("
                          << juce::String(row.centsError, 2) << " cents, stability "
                          << juce::String(row.stability, 4) << ")\n";
            }

            rows.push_back(row);
        }
    }

    writePitchQaReport(reportFile, rows);
    std::cout << "Pitch validation: " << (static_cast<int>(rows.size()) - failures)
              << "/" << rows.size() << " passed\nReport: "
              << reportFile.getFullPathName() << "\n";
    return failures > 0 ? 1 : 0;
}

bool appendReleaseSuiteRow(std::vector<ReleaseSuiteRow>& rows,
                           const juce::File& file,
                           const juce::String& scenario,
                           juce::AudioBuffer<float>& buffer)
{
    const auto metrics = measureBuffer(buffer);
    ReleaseSuiteRow row;
    row.file = file.getFileName();
    row.scenario = scenario;
    row.peakDb = metrics.peakDb;
    row.rmsDb = metrics.rmsDb;

    if (!metrics.finite)
    {
        row.verdict = "FAIL";
        row.notes = "NaN/Inf";
    }
    else if (metrics.peakDb < musique::qa::kMinimumAudiblePeakDb)
    {
        row.verdict = "FAIL";
        row.notes = "silent";
    }
    else if (metrics.peakDb > -0.05f)
    {
        row.verdict = "FAIL";
        row.notes = "clip risk";
    }
    else
    {
        row.verdict = "PASS";
    }

    const bool wrote = writeWav(file, buffer);
    if (!wrote)
    {
        row.verdict = "FAIL";
        row.notes = row.notes.isEmpty() ? "write failed" : row.notes + "; write failed";
    }

    rows.push_back(row);
    return row.verdict == "PASS";
}

static int runRenderReleaseSuite(const juce::File& outputBase, const juce::File& reportFile)
{
    static constexpr double kSuiteSeconds = 8.0;
    const int totalSamples = static_cast<int>(std::ceil(kSuiteSeconds * kSampleRate));
    juce::AudioBuffer<float> acoustic(2, totalSamples);
    juce::AudioBuffer<float> eight08(2, totalSamples);
    juce::AudioBuffer<float> synth(2, totalSamples);
    juce::AudioBuffer<float> main(2, totalSamples);
    acoustic.clear();
    eight08.clear();
    synth.clear();
    main.clear();

    addFactoryClip(acoustic, 0, 0, 28, 0.00, 0.74f);
    addFactoryClip(acoustic, 1, 1, 31, 0.55, 0.70f);
    addFactoryClip(acoustic, 2, 1, 36, 1.10, 0.62f);
    addFactoryClip(acoustic, 0, 1, 33, 2.05, 0.58f);

    addFactoryClip(eight08, 3, 0, 24, 0.00, 0.78f);
    addFactoryClip(eight08, 4, 1, 24, 1.00, 0.70f);
    addFactoryClip(eight08, 5, 1, 26, 2.00, 0.58f);
    addFactoryClip(eight08, 3, 1, 31, 3.25, 0.54f);

    addFactoryClip(synth, 6, 0, 31, 0.00, 0.66f);
    addFactoryClip(synth, 7, 1, 28, 1.00, 0.54f);
    addFactoryClip(synth, 8, 1, 31, 2.00, 0.58f);
    addFactoryClip(synth, 8, 1, 36, 3.20, 0.46f);

    protectCeiling(acoustic, -1.5f);
    protectCeiling(eight08, -1.5f);
    protectCeiling(synth, -1.5f);

    mixInto(main, acoustic, 0, 0.74f);
    mixInto(main, eight08, 0, 0.74f);
    mixInto(main, synth, 0, 0.64f);
    protectCeiling(main, -1.0f);

    std::vector<ReleaseSuiteRow> rows;
    int failures = 0;
    const auto stemsDir = outputBase.getChildFile("stems");
    const auto identityDir = outputBase.getChildFile("identity");

    failures += appendReleaseSuiteRow(rows, outputBase.getChildFile("main.wav"), "mini-mix drum/bass", main) ? 0 : 1;
    failures += appendReleaseSuiteRow(rows, stemsDir.getChildFile("acoustic.wav"), "acoustic basses", acoustic) ? 0 : 1;
    failures += appendReleaseSuiteRow(rows, stemsDir.getChildFile("808.wav"), "808 legato/sub patterns", eight08) ? 0 : 1;
    failures += appendReleaseSuiteRow(rows, stemsDir.getChildFile("synth.wav"), "synth bass patterns", synth) ? 0 : 1;

    for (int bass = 0; bass < mbs::kNumBasses; ++bass)
    {
        auto identity = renderFactoryClip(bass, 0, bass >= 3 && bass <= 5 ? 24 : 31, 0.84f, 0.36, 1.3);
        protectCeiling(identity, -1.0f);
        const auto identityFile = identityDir.getChildFile(identitySlugForBass(bass) + ".wav");
        failures += appendReleaseSuiteRow(rows, identityFile, "identity " + juce::String(mbs::getBassName(bass)), identity) ? 0 : 1;
    }

    writeReleaseSuiteReport(reportFile, rows);
    const int checks = static_cast<int>(rows.size());
    std::cout << "Bass release suite: " << (checks - failures) << "/" << checks
              << " checks passed\nReport: " << reportFile.getFullPathName()
              << "\nOutput: " << outputBase.getFullPathName() << "\n";
    return failures > 0 ? 1 : 0;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: UWdeVST_bass_renderer <manifest.csv> [--output-base <dir>] [--limit <n>] [--overwrite]\n"
                     "       UWdeVST_bass_renderer --export-candidate-presets [--output-base <dir>] [--overwrite]\n"
                     "       UWdeVST_bass_renderer --render-xml-previews [--preset-dir <dir>] [--output-base <dir>] [--overwrite]\n"
                     "       UWdeVST_bass_renderer --strict-chord-audit [--preset-dir <dir>] [--output-base <dir>] [--report <csv>] [--overwrite]\n"
                     "       UWdeVST_bass_renderer --audit-audio [--preset-dir <dir>] [--output-base <dir>] [--report <csv>] [--overwrite]\n"
                     "       UWdeVST_bass_renderer --validate-presets [--report <csv>]\n"
                     "       UWdeVST_bass_renderer --benchmark-cpu [--report <csv>] [--baseline <csv>] [--voices <n>] [--threshold <fraction>]\n"
                     "       UWdeVST_bass_renderer --validate-pitch [--report <csv>]\n"
                     "       UWdeVST_bass_renderer --render-release-suite [--output-base <dir>] [--report <csv>]\n"
                     "       UWdeVST_bass_renderer --render-audition-panel [--output-base <dir>] [--variants <1-10>] [--overwrite]\n";
        return 1;
    }

    const juce::String firstArg(argv[1]);
    if (firstArg == "--export-candidate-presets")
    {
        auto outputBase = juce::File::getCurrentWorkingDirectory().getChildFile("build/codex_audit");
        bool overwrite = false;
        for (int i = 2; i < argc; ++i)
        {
            const juce::String key(argv[i]);
            if (key == "--output-base" && i + 1 < argc)
                outputBase = juce::File(argv[++i]);
            else if (key == "--overwrite")
                overwrite = true;
        }
        return runExportCandidatePresets(outputBase, overwrite);
    }
    if (firstArg == "--render-xml-previews")
    {
        auto presetDir = juce::File::getCurrentWorkingDirectory().getChildFile("build/codex_audit/bass_candidate_presets");
        auto outputBase = juce::File::getCurrentWorkingDirectory().getChildFile("qa/current_candidate_preview_renders_2026-05-31");
        bool overwrite = false;
        for (int i = 2; i < argc; ++i)
        {
            const juce::String key(argv[i]);
            if (key == "--preset-dir" && i + 1 < argc)
                presetDir = juce::File(argv[++i]);
            else if (key == "--output-base" && i + 1 < argc)
                outputBase = juce::File(argv[++i]);
            else if (key == "--overwrite")
                overwrite = true;
        }
        return runRenderXmlPreviews(presetDir, outputBase, overwrite);
    }
    if (firstArg == "--strict-chord-audit")
    {
        auto presetDir = juce::File::getCurrentWorkingDirectory().getChildFile("build/codex_audit/bass_candidate_presets");
        auto outputBase = juce::File::getCurrentWorkingDirectory().getChildFile("qa/current_candidate_chords_2026-05-31");
        auto reportFile = juce::File::getCurrentWorkingDirectory().getChildFile("qa/current_candidate_strict_chord_2026-05-31.csv");
        bool overwrite = false;
        for (int i = 2; i < argc; ++i)
        {
            const juce::String key(argv[i]);
            if (key == "--preset-dir" && i + 1 < argc)
                presetDir = juce::File(argv[++i]);
            else if (key == "--output-base" && i + 1 < argc)
                outputBase = juce::File(argv[++i]);
            else if (key == "--report" && i + 1 < argc)
                reportFile = juce::File(argv[++i]);
            else if (key == "--overwrite")
                overwrite = true;
        }
        return runStrictChordAudit(presetDir, outputBase, reportFile, overwrite);
    }
    if (firstArg == "--audit-audio")
    {
        auto presetDir = juce::File::getCurrentWorkingDirectory().getChildFile("build/codex_audit/bass_candidate_presets");
        auto outputBase = juce::File::getCurrentWorkingDirectory().getChildFile("qa/current_candidate_audit_audio_2026-05-31");
        auto reportFile = juce::File::getCurrentWorkingDirectory().getChildFile("qa/current_candidate_audit_audio_2026-05-31.csv");
        bool overwrite = false;
        for (int i = 2; i < argc; ++i)
        {
            const juce::String key(argv[i]);
            if (key == "--preset-dir" && i + 1 < argc)
                presetDir = juce::File(argv[++i]);
            else if (key == "--output-base" && i + 1 < argc)
                outputBase = juce::File(argv[++i]);
            else if (key == "--report" && i + 1 < argc)
                reportFile = juce::File(argv[++i]);
            else if (key == "--overwrite")
                overwrite = true;
        }
        return runAuditAudio(presetDir, outputBase, reportFile, overwrite);
    }
    if (firstArg == "--validate-presets")
    {
        auto reportFile = juce::File::getCurrentWorkingDirectory().getChildFile("qa/bass_preset_qa_report.csv");
        for (int i = 2; i < argc; ++i)
        {
            const juce::String key(argv[i]);
            if (key == "--report" && i + 1 < argc)
                reportFile = juce::File(argv[++i]);
        }
        return runValidatePresets(reportFile);
    }
    if (firstArg == "--benchmark-cpu")
    {
        auto reportFile = juce::File::getCurrentWorkingDirectory().getChildFile("qa/bass_cpu_benchmark.csv");
        juce::File baselineFile;
        std::optional<int> voices;
        double thresholdFraction = 0.20;
        for (int i = 2; i < argc; ++i)
        {
            const juce::String key(argv[i]);
            if (key == "--report" && i + 1 < argc)
                reportFile = juce::File(argv[++i]);
            else if (key == "--baseline" && i + 1 < argc)
                baselineFile = juce::File(argv[++i]);
            else if (key == "--voices" && i + 1 < argc)
                voices = juce::String(argv[++i]).getIntValue();
            else if (key == "--threshold" && i + 1 < argc)
                thresholdFraction = juce::String(argv[++i]).getDoubleValue();
        }
        return runBenchmarkCpu(reportFile, baselineFile, voices, thresholdFraction);
    }
    if (firstArg == "--validate-pitch")
    {
        auto reportFile = juce::File::getCurrentWorkingDirectory().getChildFile("qa/bass_pitch_qa_report.csv");
        for (int i = 2; i < argc; ++i)
        {
            const juce::String key(argv[i]);
            if (key == "--report" && i + 1 < argc)
                reportFile = juce::File(argv[++i]);
        }
        return runValidatePitch(reportFile);
    }
    if (firstArg == "--render-release-suite")
    {
        auto outputBase = juce::File::getCurrentWorkingDirectory().getChildFile("qa/bass_release_suite");
        auto reportFile = juce::File::getCurrentWorkingDirectory().getChildFile("qa/bass_release_suite_report.csv");
        for (int i = 2; i < argc; ++i)
        {
            const juce::String key(argv[i]);
            if (key == "--output-base" && i + 1 < argc)
                outputBase = juce::File(argv[++i]);
            else if (key == "--report" && i + 1 < argc)
                reportFile = juce::File(argv[++i]);
        }
        return runRenderReleaseSuite(outputBase, reportFile);
    }
    if (firstArg == "--render-audition-panel")
    {
        juce::File outputBase = juce::File::getCurrentWorkingDirectory().getChildFile("bass_audition_panel");
        int variantsPerPreset = musique::qa::audition::kDefaultVariantsPerPreset;
        bool overwrite = false;

        for (int i = 2; i < argc; ++i)
        {
            const juce::String key(argv[i]);
            if (key == "--output-base" && i + 1 < argc) outputBase = juce::File(argv[++i]);
            else if (key == "--variants" && i + 1 < argc) variantsPerPreset = juce::String(argv[++i]).getIntValue();
            else if (key == "--overwrite") overwrite = true;
        }

        return runRenderAuditionPanel(outputBase, variantsPerPreset, overwrite);
    }

    const juce::File manifestFile(argv[1]);
    juce::File outputBase = juce::File::getCurrentWorkingDirectory().getChildFile("render_output_bass");
    int limit = -1;
    bool overwrite = false;

    for (int i = 2; i < argc; ++i)
    {
        const juce::String key(argv[i]);
        if (key == "--output-base" && i + 1 < argc) outputBase = juce::File(argv[++i]);
        else if (key == "--limit" && i + 1 < argc) limit = juce::String(argv[++i]).getIntValue();
        else if (key == "--overwrite") overwrite = true;
    }

    try
    {
        auto jobs = readManifestCsv(manifestFile);
        if (limit > 0 && limit < static_cast<int>(jobs.size()))
            jobs.resize(static_cast<std::size_t>(limit));

        int rendered = 0;
        for (const auto& job : jobs)
        {
            const auto bassIndex = bassIndexFromName(job.instrument);
            if (bassIndex < 0)
            {
                std::cerr << "[WARN] Unknown bass instrument in manifest: " << job.instrument << " -> skipped\n";
                continue;
            }

            const auto outputFile = outputBase.getChildFile(job.finalRelativePath.replaceCharacter('/', juce::File::getSeparatorChar()));
            if (outputFile.existsAsFile() && !overwrite)
                continue;

            auto settings = mbs::getDefaultSettings(bassIndex);
            applyPresetMacro(job, bassIndex, settings);
            adaptSettingsForJob(job, settings);
            applyVariation(job, settings);

            const auto duration = juce::jmax(0.1f, job.durationSeconds.getFloatValue());
            const auto extraTail = isLoopJob(job) ? (isTextureJob(job) ? 0.6f : 0.2f) : (isTextureJob(job) ? 1.8f : (isShortJob(job) ? 0.25f : 0.8f));
            juce::AudioBuffer<float> buffer(2, static_cast<int>(std::ceil((duration + extraTail) * kSampleRate)));
            buffer.clear();

            renderTimeline(buffer, buildEvents(job, midiNoteFromKey(job.key, job.instrument), duration), settings, mbs::getCharacteristics(bassIndex));
            const auto fx = fxProfileFor(job.fxProfile);
            applyTransient(buffer, fx);
            applySaturator(buffer, fx);
            applyCompressor(buffer, fx);
            applyDcHighPass(buffer);
            trimAndProtect(buffer, job, fx);

            if (!writeWav(outputFile, buffer))
                throw std::runtime_error(("Failed to write WAV: " + outputFile.getFullPathName()).toStdString());

            ++rendered;
            std::cout << "[" << rendered << "/" << jobs.size() << "] " << job.instrument << " / " << job.articulation << " / " << job.finalRelativePath << "\n";
        }

        std::cout << "Rendered " << rendered << " WAV files into " << outputBase.getFullPathName() << "\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Renderer error: " << e.what() << "\n";
        return 1;
    }
}
