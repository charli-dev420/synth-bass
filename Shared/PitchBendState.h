#pragma once

#include <cmath>
#include <algorithm>

// =============================================================================
// Shared pitch bend + velocity curve utilities for all synths
// =============================================================================

// Pitch bend range in semitones (standard +-2, configurable up to 24 semitones)
struct PitchBendState
{
    float bendSemitones   = 2.0f;    // range (1-24 semitones)
    float currentBend     = 0.0f;    // -1..+1 from MIDI pitch wheel
    float pitchBendFactor = 1.0f;    // frequency multiplier (updated each block)

    // Call from processBlock when a pitch wheel message is received
    // pitchWheelValue: 0..16383 (center = 8192)
    void setPitchWheel(int pitchWheelValue)
    {
        currentBend = static_cast<float>(pitchWheelValue - 8192) / 8192.0f;
        updateFactor();
    }

    // Call after changing bendSemitones
    void updateFactor()
    {
        pitchBendFactor = std::pow(2.0f, currentBend * bendSemitones / 12.0f);
    }

    // Reset to center
    void reset()
    {
        currentBend = 0.0f;
        pitchBendFactor = 1.0f;
    }
};

// =============================================================================
// Velocity curve: transforms raw MIDI velocity (0-1) into shaped velocity
// 7 modes covering lightweight synth-action to weighted hammer-action feels.
// =============================================================================
enum class VelocityCurve
{
    Linear,      // v              — neutral, unweighted keyboards
    Soft,        // v^0.5          — more dynamics at low velocities (typical synth action)
    Softer,      // v^0.33         — very gentle, full dynamic range from light touch
    Hard,        // v^2            — requires stronger playing (weighted, compact keyboards)
    Harder,      // v^3            — very aggressive, studio weighted-action feel
    Fixed,       // always 1.0     — ignores velocity (organ-like)
    Touch        // v^0.25 clamped — ultra-sensitive, maximum dynamic range at feather touch
};

inline float applyVelocityCurve(float velocity, VelocityCurve curve)
{
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    switch (curve)
    {
        case VelocityCurve::Linear:  return velocity;
        case VelocityCurve::Soft:    return std::sqrt(velocity);
        case VelocityCurve::Softer:  return std::pow(velocity, 0.33f);
        case VelocityCurve::Hard:    return velocity * velocity;
        case VelocityCurve::Harder:  return velocity * velocity * velocity;
        case VelocityCurve::Fixed:   return 1.0f;
        case VelocityCurve::Touch:   return std::pow(velocity, 0.25f);
    }
    return velocity;
}

// Convert velocity curve enum to/from int for parameter storage
inline int velocityCurveToInt(VelocityCurve c) { return static_cast<int>(c); }
inline VelocityCurve intToVelocityCurve(int v)
{
    if (v >= 0 && v <= 6) return static_cast<VelocityCurve>(v);
    return VelocityCurve::Linear;
}
