#include <JuceHeader.h>

#include "../Engine/BassDefs.h"
#include "../Engine/BassVoice.h"
#include "../Engine/FactoryPresets.h"
#include "../../Shared/ProductionQa.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
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

constexpr double kSampleRate = 48000.0;

float clamp01(const float value) { return juce::jlimit(0.0f, 1.0f, value); }

juce::String slug(const juce::String& value)
{
    auto ascii = value.toLowerCase();
    ascii = ascii.replaceCharacters("Ã©Ã¨ÃªÃ«Ã Ã¢Ã¤Ã¹Ã»Ã¼Ã´Ã¶Ã®Ã¯Ã§ ", "eeeeaaauuuooiic_");
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
    juce::String status = "ok";
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

juce::String csvEscape(const juce::String& text)
{
    auto escaped = text;
    escaped = escaped.replace("\"", "\"\"");
    if (escaped.containsAnyOf(",\"\n\r"))
        return "\"" + escaped + "\"";
    return escaped;
}

juce::String joinTags(const std::vector<std::string>& tags)
{
    juce::StringArray items;
    for (const auto& tag : tags)
        items.add(juce::String(juce::CharPointer_UTF8(tag.c_str())));
    return items.joinIntoString(",");
}

void writePresetQaReport(const juce::File& file, const std::vector<PresetQaRow>& rows)
{
    file.getParentDirectory().createDirectory();
    juce::String csv = "Instrument,Preset,Family,MixRole,Tags,PeakDb,RmsDb,Status\n";
    for (const auto& row : rows)
    {
        csv << csvEscape(row.instrument) << ','
            << csvEscape(row.preset) << ','
            << csvEscape(row.family) << ','
            << csvEscape(row.mixRole) << ','
            << csvEscape(row.tags) << ','
            << juce::String(row.peakDb, 3) << ','
            << juce::String(row.rmsDb, 3) << ','
            << csvEscape(row.status) << '\n';
    }
    file.replaceWithText(csv);
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
    file.replaceWithText(csv);
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

            float peak = 0.0f;
            double energy = 0.0;
            int sampleCount = 0;
            bool hasNaN = false;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                const float* data = buf.getReadPointer(ch);
                for (int s = 0; s < analysedSamples; ++s)
                {
                    if (std::isnan(data[s]) || std::isinf(data[s]))
                        hasNaN = true;
                    peak = std::max(peak, std::abs(data[s]));
                    energy += static_cast<double>(data[s]) * static_cast<double>(data[s]);
                    ++sampleCount;
                }
            }

            const float peakDb = peak > 0.0f
                ? 20.0f * std::log10(peak)
                : -100.0f;
            const auto rms = sampleCount > 0 ? std::sqrt(energy / static_cast<double>(sampleCount)) : 0.0;
            const float rmsDb = rms > 0.0
                ? 20.0f * std::log10(static_cast<float>(rms))
                : -100.0f;

            PresetQaRow row;
            row.instrument = mbs::getBassName(bass);
            row.preset = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
            row.family = juce::String(juce::CharPointer_UTF8(preset.metadata.familyLabel.c_str()));
            row.mixRole = juce::String(juce::CharPointer_UTF8(preset.metadata.mixRole.c_str()));
            row.tags = joinTags(preset.metadata.tags);
            row.peakDb = peakDb;
            row.rmsDb = rmsDb;

            if (hasNaN)
            {
                ++fails;
                row.status = "nan_or_inf";
                std::cout << "[FAIL] bass " << bass << " / " << preset.name
                          << " : NaN or Inf in output\n";
            }
            else if (peakDb < kMinPeakDb)
            {
                ++fails;
                row.status = "silent";
                std::cout << "[FAIL] bass " << bass << " / " << preset.name
                          << " : silent (peak " << peakDb << " dBFS)\n";
            }
            else if (peakDb > kMaxPeakDb)
            {
                ++fails;
                row.status = "clipping";
                std::cout << "[FAIL] bass " << bass << " / " << preset.name
                          << " : clipping (peak " << peakDb << " dBFS)\n";
            }
            else if (rmsDb < minRmsDb)
            {
                ++fails;
                row.status = "rms_too_low";
                std::cout << "[FAIL] bass " << bass << " / " << preset.name
                          << " : RMS too low (" << rmsDb << " dBFS)\n";
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
        { 0, "Warm Pad Bass" },
        { 4, "Boom Film Score" },
        { 5, "Distort Ambient" },
        { 7, "Reese Pad Lush" },
        { 8, "Acid Sustain" },
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

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: UWdeVST_bass_renderer <manifest.csv> [--output-base <dir>] [--limit <n>] [--overwrite]\n"
                     "       UWdeVST_bass_renderer --validate-presets [--report <csv>]\n"
                     "       UWdeVST_bass_renderer --benchmark-cpu [--report <csv>] [--baseline <csv>] [--voices <n>] [--threshold <fraction>]\n";
        return 1;
    }

    const juce::String firstArg(argv[1]);
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
                throw std::runtime_error(("Unknown bass: " + job.instrument).toStdString());

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
