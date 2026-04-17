#pragma once

#include <JuceHeader.h>

#include <cmath>

namespace musique::qa
{
constexpr float kMinimumAudiblePeakDb = -60.0f;
constexpr float kMaximumPeakDb = 0.0f;
constexpr float kClippingToleranceDb = 0.1f;
constexpr float kPeakFloorDb = -100.0f;

inline float minimumAudiblePeakLinear()
{
    return juce::Decibels::decibelsToGain(kMinimumAudiblePeakDb);
}

inline float maximumSafePeakLinear()
{
    return juce::Decibels::decibelsToGain(kMaximumPeakDb + kClippingToleranceDb);
}

inline float peakToDb(const float peak)
{
    return peak > 0.0f ? juce::Decibels::gainToDecibels(peak) : kPeakFloorDb;
}

inline float bufferPeak(const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        peak = juce::jmax(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));

    return peak;
}

inline bool bufferIsFinite(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* data = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            if (!std::isfinite(data[sample]))
                return false;
        }
    }

    return true;
}
} // namespace musique::qa
