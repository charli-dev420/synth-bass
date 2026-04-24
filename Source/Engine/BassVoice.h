#pragma once

#include <JuceHeader.h>
#include "BassDefs.h"

#include <array>

namespace mbs
{

struct VoiceModulation
{
    float cutoffMul = 1.0f;
    float resonanceAdd = 0.0f;
    float panAdd = 0.0f;
    float attackScale = 1.0f;
    float decayScale = 1.0f;
    float pitchSemi = 0.0f;
    float levelMul = 1.0f;
};

class BassVoice
{
public:
    void noteOn(const BassSettings& settings,
                const BassCharacteristics& chars,
                int midiNote, float velocity, double sampleRate);
    void noteOff();
    void glideToNote(int midiNote, float glideTimeSec);
    void retriggerWithGlide(const BassSettings& settings,
                            const BassCharacteristics& chars,
                            int midiNote, float velocity, double sampleRate);
    void render(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    bool isActive()    const noexcept { return envState != EnvState::Off; }
    bool isReleasing() const noexcept { return envState == EnvState::Release; }
    int  getMidiNote() const noexcept { return midiNote; }
    float getEnvelopeLevel() const noexcept { return envLevel; }

    void setPitchBendFactor(float f) noexcept { pitchBendFactor = f; }
    void setLfoCutoffMod(float mod) noexcept { lfoCutoffMod = mod; }
    void setVoiceModulation(const VoiceModulation& modulation, double sampleRate) noexcept;
    void forceQuickRelease() noexcept;
    void forceReleaseSeconds(float seconds) noexcept;
    void forceStop() noexcept;

private:
    enum class EnvState { Off, Attack, Decay1, Decay2, Sustain, Release };
    enum class FilterEnvState { Off, Attack, Decay };

    float readComb(const float* buf, int bufSize, int writePos, float delaySamples) const;
    static float polyBlep(float t, float dt);
    void updateEnvelopeCoefficients(float sampleRate) noexcept;
    void updatePanFromModulation() noexcept;
    void updateResonanceResponse() noexcept;
    float computeDetuneBlend() const noexcept;

    BassSettings         settings{};
    BassSettings         baseSettings{};
    BassCharacteristics  chars{};
    double sr       = 44100.0;
    float  vel      = 0.0f;
    int    midiNote = -1;

    EnvState envState = EnvState::Off;

    // Multi-oscillator (for saw/sine/square with unison)
    static constexpr int kMaxOsc = 3;
    struct OscState
    {
        float phase    = 0.0f;
        float phaseInc = 0.0f;
    };
    std::array<OscState, kMaxOsc> oscs{};
    int numOscs = 1;

    // Sub oscillator (always sine one octave below the played pitch)
    float subPhase    = 0.0f;
    float subPhaseInc = 0.0f;

    // Additive partials (for acoustic basses)
    static constexpr int kMaxPartials = 12;
    struct PartialState
    {
        float phase     = 0.0f;
        float phaseInc  = 0.0f;
        float amplitude = 0.0f;
        float decayCoeff = 1.0f;
    };
    std::array<PartialState, kMaxPartials> partials{};
    int numActivePartials = 0;

    // Pitch envelope
    float pitchEnvLevel      = 0.0f;
    float pitchEnvDecayCoeff = 1.0f;
    float pitchEnvDepthRatio = 0.0f;   // frequency ratio for full depth
    float baseFreq           = 0.0f;

    // Glide / portamento
    float targetFreq   = 0.0f;
    float glideCoeff   = 1.0f;        // per-sample coefficient (1.0 = no glide)
    bool  glideActive  = false;

    // Amplitude envelope
    float envLevel     = 0.0f;
    float attackCoeff  = 1.0f;
    float decay1Coeff  = 1.0f;
    float decay1Target = 0.0f;
    float decay2Coeff  = 1.0f;
    float releaseCoeff = 1.0f;

    // Pluck transient
    float pluckLevel     = 0.0f;
    float pluckDecayCoeff = 1.0f;

    // SVF filter state (dual-stage for optional 24dB/oct)
    float svfLow     = 0.0f;
    float svfBand    = 0.0f;
    float svfLow2    = 0.0f;    // second stage for 24dB/oct
    float svfBand2   = 0.0f;    // second stage for 24dB/oct
    float filterF    = 0.0f;
    float filterQinv = 0.0f;
    float filterBaseF    = 0.0f;   // resting cutoff coefficient
    float filterEnvDepthOctaves = 0.0f;
    float filterMaxF     = 0.0f;   // stability limit
    bool  filter24dB     = false;  // true = cascaded 24dB/oct
    float lfoCutoffMod   = 0.0f;   // external LFO modulation of cutoff (-1..+1)
    FilterEnvState filterEnvState = FilterEnvState::Off;
    float filterEnvLevel = 0.0f;
    float filterEnvAttackCoeff = 1.0f;
    float filterEnvDecayCoeff = 1.0f;

    // Body resonator (comb filter)
    static constexpr int kBodyBufSize = 8192;
    float bodyBuf[kBodyBufSize] = {};
    int   bodyWritePos     = 0;
    float bodyDelaySamples = 100.0f;
    float bodyFeedback     = 0.0f;
    float bodyDampState    = 0.0f;

    // Pan
    float panL = 0.7071f;
    float panR = 0.7071f;

    // Noise
    juce::Random rng;

    float pitchBendFactor = 1.0f;
    float modPitchFactor  = 1.0f;
    float baseFilterBaseF = 0.0f;
    float baseFilterQinv  = 0.0f;
    float baseBodyFeedback = 0.0f;
    float baseAttackSeconds = 0.005f;
    float baseDecaySeconds = 1.0f;
    float baseReleaseSeconds = 0.2f;
    float basePan = 0.0f;
    float baseLevel = 1.0f;
    VoiceModulation currentModulation {};
    bool quickReleaseForced = false;

    int ageSamples    = 0;
    int maxAgeSamples = 0;
};

} // namespace mbs
