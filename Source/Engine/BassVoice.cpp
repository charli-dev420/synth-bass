#include "BassVoice.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mbs
{

void BassVoice::updateEnvelopeCoefficients(float sampleRate) noexcept
{
    const float fsr = std::max(1.0f, sampleRate);
    // FIX: Minimum attack 1ms to prevent clicks from random oscillator phases
    const float attackSeconds = std::max(baseAttackSeconds * std::clamp(currentModulation.attackScale, 0.0625f, 16.0f), 0.001f);
    const float decaySeconds = std::max(baseDecaySeconds * std::clamp(currentModulation.decayScale, 0.0625f, 16.0f), 0.01f);

    if (attackSeconds > 0.0001f)
        attackCoeff = std::exp(std::log(1.0f / 0.0001f) / (attackSeconds * fsr));
    else
        attackCoeff = 1.0f;

    const float d1Time = std::max(0.01f, decaySeconds * chars.decay1Ratio);
    decay1Coeff  = std::exp(-1.0f / (d1Time * fsr));
    decay1Target = chars.sustainPlatform * settings.sustainLevel;

    const float d2Time = std::min(8.0f, std::max(0.05f, decaySeconds * chars.decay2Ratio));
    decay2Coeff = std::exp(-1.0f / (d2Time * fsr));

    if (!quickReleaseForced)
        releaseCoeff = std::exp(-1.0f / (std::max(0.005f, baseReleaseSeconds) * fsr));
}

void BassVoice::updatePanFromModulation() noexcept
{
    const auto pan = juce::jlimit(-1.0f, 1.0f, basePan + currentModulation.panAdd);
    panL = std::sqrt(0.5f * (1.0f - pan));
    panR = std::sqrt(0.5f * (1.0f + pan));
}

void BassVoice::updateResonanceResponse() noexcept
{
    const float effectiveResonance = juce::jlimit(0.0f, 1.0f, baseSettings.resonance + currentModulation.resonanceAdd * 0.5f);
    const float q = 0.5f + effectiveResonance * 11.5f;
    filterQinv = 1.0f / q;
    baseFilterQinv = filterQinv;
    filterMaxF = (-filterQinv + std::sqrt(filterQinv * filterQinv + 4.0f)) * 0.95f;
    filterBaseF = juce::jmin(baseFilterBaseF, filterMaxF);
    bodyFeedback = juce::jlimit(0.0f, 0.9995f,
                                baseBodyFeedback * std::clamp(1.0f + currentModulation.resonanceAdd * 0.4f,
                                                              0.2f, 2.0f));
}

float BassVoice::computeDetuneBlend() const noexcept
{
    const bool reeseStyle = chars.oscMode == OscMode::Saw && chars.numOscillators >= 3;
    const float floor = reeseStyle ? 0.28f : 0.0f;
    const float boost = reeseStyle ? 1.15f : 1.0f;
    return std::clamp(std::max(settings.character, floor) * boost, 0.0f, 1.25f);
}

void BassVoice::setVoiceModulation(const VoiceModulation& modulation, double sampleRate) noexcept
{
    currentModulation = modulation;
    modPitchFactor = std::exp2(modulation.pitchSemi / 12.0f);
    settings.level = juce::jlimit(0.0f, 4.0f, baseLevel * std::clamp(modulation.levelMul, 0.0f, 4.0f));
    updateEnvelopeCoefficients(static_cast<float>(sampleRate));
    updatePanFromModulation();
    updateResonanceResponse();
}

// =========================================================================
// PolyBLEP anti-aliasing for saw/square
// =========================================================================
float BassVoice::polyBlep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// =========================================================================
// Linear-interpolating comb read
// =========================================================================
float BassVoice::readComb(const float* buf, const int bufSize,
                          const int writePos, const float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int   idx0 = static_cast<int>(std::floor(readPos));
    const float frac = readPos - static_cast<float>(idx0);

    auto wrap = [bufSize](int i) -> int {
        return ((i % bufSize) + bufSize) % bufSize;
    };

    const float s0 = buf[wrap(idx0)];
    const float s1 = buf[wrap(idx0 + 1)];
    return s0 + frac * (s1 - s0);
}

// =========================================================================
// noteOn
// =========================================================================
void BassVoice::noteOn(const BassSettings& s,
                       const BassCharacteristics& c,
                       const int note, const float velocity,
                       const double sampleRate)
{
    baseSettings = s;
    settings = s;
    chars    = c;
    sr       = std::max(1.0, sampleRate);
    vel      = juce::jlimit(0.0f, 1.0f, velocity);
    midiNote = note;
    ageSamples = 0;
    currentModulation = {};
    modPitchFactor = 1.0f;
    quickReleaseForced = false;
    // FIX: Guard attack to prevent click with random oscillator phases on unison
    baseAttackSeconds = std::max(settings.attackSeconds, 0.001f);
    baseDecaySeconds = settings.decaySeconds;
    baseReleaseSeconds = settings.releaseSeconds;
    basePan = settings.pan;
    baseLevel = settings.level;

    const auto fsr = static_cast<float>(sr);

    // ----- Base frequency with tuning -----
    baseFreq = 440.0f * std::pow(2.0f,
        (static_cast<float>(note) - 69.0f + settings.tuneSemitones) / 12.0f);
    targetFreq = baseFreq;
    glideActive = false;
    glideCoeff = 1.0f;

    // ----- Oscillator setup based on mode -----
    numOscs = 0;
    numActivePartials = 0;

        if (chars.oscMode == OscMode::Additive)
        {
            // Additive synthesis (acoustic basses)
            const int np = std::min(chars.numPartials, kMaxPartials);
            numActivePartials = np;

            const float brightMult = 0.5f + settings.brightness * 1.5f;

            for (int n = 0; n < np; ++n)
            {
                const int harmonic = n + 1;
                const float fn = static_cast<float>(harmonic) * baseFreq *
                    std::sqrt(1.0f + chars.inharmonicity *
                              static_cast<float>(harmonic * harmonic));

                if (fn >= fsr * 0.48f)
                {
                    numActivePartials = n;
                    break;
                }

                // Amplitude: 1/n rolloff, shaped by brightness
                const float baseAmp = 1.0f / static_cast<float>(harmonic);
                const float rolloff = 1.0f / (1.0f + (fn / (baseFreq * 4.0f * brightMult))
                                      * (fn / (baseFreq * 4.0f * brightMult)));
                const float amp = baseAmp * (0.2f + 0.8f * rolloff);

                // Per-partial decay: higher partials decay faster (power law)
                const float qHarm = 1.0f + (1.0f - settings.brightness) * 2.0f;
                const float dScale = 1.0f /
                    (1.0f + qHarm * std::pow(static_cast<float>(harmonic), 1.5f) * 0.05f);
                const float dTime = std::min(8.0f, std::max(0.02f,
                    settings.decaySeconds * chars.decay2Ratio * dScale));

                auto& p = partials[static_cast<std::size_t>(n)];
                p.phase     = 0.0f;
                p.phaseInc  = fn / fsr;
                p.amplitude = amp;
                p.decayCoeff = std::exp(-1.0f / (dTime * fsr));
            }
        }
        else
        {
            // Oscillator modes (saw/sine/square) with optional unison
            // FIX: Use cents for detune to maintain consistent perception across registers
            // C1 (65Hz): 1.5 cents = 0.75 Hz (subtle)
            // C4 (261Hz): 1.5 cents = 3.0 Hz (same perceived width)
            numOscs = std::clamp(chars.numOscillators, 1, kMaxOsc);
            const float detuneCents = chars.detuneAmount * 100.0f * computeDetuneBlend();
            const float detuneRatio = std::pow(2.0f, detuneCents / 1200.0f);

            for (int o = 0; o < numOscs; ++o)
            {
                auto& osc = oscs[static_cast<std::size_t>(o)];
                osc.phase = (numOscs > 1) ? rng.nextFloat() : 0.0f;

                float freqRatio = 1.0f;
                if (numOscs == 2)
                    freqRatio = (o == 0) ? (2.0f / detuneRatio) : detuneRatio;
                else if (numOscs == 3)
                    freqRatio = std::pow(detuneRatio, static_cast<float>(o - 1));

                osc.phaseInc = (baseFreq * freqRatio) / fsr;
            }
        }

        // ----- Sub oscillator -----
        subPhase    = 0.0f;
        subPhaseInc = (baseFreq * 0.5f) / fsr;

    // ----- Pitch envelope -----
    const float pitchSemis = chars.pitchEnvSemitones * settings.pitchEnv;
    pitchEnvDepthRatio = std::pow(2.0f, pitchSemis / 12.0f) - 1.0f;
    pitchEnvLevel = 1.0f;
    if (chars.pitchEnvSeconds > 0.0001f)
        pitchEnvDecayCoeff = std::exp(-1.0f / (chars.pitchEnvSeconds * fsr));
    else
        pitchEnvDecayCoeff = 0.0f;

    // Override pitch envelope time if set by preset (>0 = explicit override in seconds)
    if (settings.pitchEnvTime > 0.005f)
        pitchEnvDecayCoeff = std::exp(-1.0f / (settings.pitchEnvTime * fsr));

    // ----- Amplitude envelope (exponential attack) -----
    if (baseAttackSeconds > 0.0001f)
    {
        envLevel = 0.0001f;
        attackCoeff = std::exp(std::log(1.0f / 0.0001f) / (baseAttackSeconds * fsr));
    }
    else
    {
        envLevel = 1.0f;
        attackCoeff = 1.0f;
    }

    // Env shape: modulate decay ratios (0 = punchy: fast d1 long d2, 1 = tail: slow d1 short d2)
    const float shape    = juce::jlimit(0.0f, 1.0f, settings.envShape);
    const float d1RatioEff = juce::jmap(shape, 0.0f, 1.0f, chars.decay1Ratio * 1.4f, chars.decay1Ratio * 0.6f);
    const float d2RatioEff = juce::jmap(shape, 0.0f, 1.0f, chars.decay2Ratio * 0.5f, chars.decay2Ratio * 1.4f);

    const float d1Time = std::max(0.01f, baseDecaySeconds * d1RatioEff);
    decay1Coeff  = std::exp(-1.0f / (d1Time * fsr));
    decay1Target = chars.sustainPlatform * settings.sustainLevel;

    const float d2Time = std::min(8.0f, std::max(0.05f, baseDecaySeconds * d2RatioEff));
    decay2Coeff  = std::exp(-1.0f / (d2Time * fsr));

    releaseCoeff = std::exp(-1.0f / (std::max(0.005f, baseReleaseSeconds) * fsr));

    envState = (baseAttackSeconds > 0.0001f) ? EnvState::Attack : EnvState::Decay1;

    // ----- Pluck transient -----
    pluckLevel = chars.pluckAmount * vel * (0.5f + settings.brightness * 0.5f);
    pluckLevel *= juce::jlimit(0.0f, 2.0f, settings.snap);  // Snap: 0 = no pluck, 1 = chars default, 2 = double
    if (chars.pluckSeconds > 0.0001f)
        pluckDecayCoeff = std::exp(-1.0f / (chars.pluckSeconds * fsr));
    else
        pluckDecayCoeff = 0.0f;

    // ----- SVF filter (with stability guard) -----
    const float safeFreq = juce::jlimit(20.0f, fsr * 0.45f, settings.cutoffHz);
    const bool synthFamily = chars.oscMode == OscMode::Saw || chars.oscMode == OscMode::Square;
    const float brightnessCutoffMul = synthFamily
        ? std::exp2((settings.brightness - 0.5f) * 1.2f)
        : (chars.isSubBass ? std::exp2((settings.brightness - 0.5f) * 0.55f) : 1.0f);
    const float voicedCutoff = juce::jlimit(20.0f, fsr * 0.45f, safeFreq * brightnessCutoffMul);
    {
        // FIX Phase 2.2: Resonance guard — limit Q to prevent filter self-oscillation
        // 12dB/oct (single): max Q = 10.0 (safe)
        // 24dB/oct (double): max Q = 8.0 (cascade is more prone to instability)
        const bool singleStage = !(chars.oscMode == OscMode::Saw || chars.oscMode == OscMode::Square);
        const float maxQ = singleStage ? 10.0f : 8.0f;
        const float clampedResonance = std::min(settings.resonance, 1.0f);
        const float Q = 0.5f + clampedResonance * (maxQ - 0.5f);
        filterQinv = 1.0f / Q;
    }
    const float rawF = 2.0f * std::sin(juce::MathConstants<float>::pi * voicedCutoff / fsr);
    filterMaxF = (-filterQinv + std::sqrt(filterQinv * filterQinv + 4.0f)) * 0.95f;
    filterF = juce::jmin(rawF, filterMaxF);
    filterBaseF = filterF;
    baseFilterBaseF = filterBaseF;
    baseFilterQinv = filterQinv;
    const float filterEnvMaxOctaves = synthFamily ? 3.0f : (chars.isSubBass ? 1.5f : 0.8f);
    filterEnvDepthOctaves = settings.filterEnv * filterEnvMaxOctaves;
    if (filterEnvDepthOctaves > 0.001f)
    {
        filterEnvLevel = 0.0f;
        const float filterAttackSeconds = juce::jlimit(0.001f, 0.040f, 0.001f + baseAttackSeconds * 0.55f);
        const float filterDecaySeconds = juce::jmax(0.025f, baseDecaySeconds * (synthFamily ? 0.45f : 0.30f));
        filterEnvAttackCoeff = std::exp(-1.0f / (filterAttackSeconds * fsr));
        filterEnvDecayCoeff = std::exp(-1.0f / (filterDecaySeconds * fsr));
        filterEnvState = FilterEnvState::Attack;
    }
    else
    {
        filterEnvLevel = 0.0f;
        filterEnvAttackCoeff = 1.0f;
        filterEnvDecayCoeff = 1.0f;
        filterEnvState = FilterEnvState::Off;
    }
    svfLow  = 0.0f;
    svfBand = 0.0f;
    svfLow2  = 0.0f;
    svfBand2 = 0.0f;
    hpfState      = 0.0f;
    pluckLpState  = 0.0f;
    unisonLpState = 0.0f;
    // Reese (≥3 oscs): precompute 1-pole LP coeff at min(4000, cutoffHz × 1.4) Hz
    unisonLpCoeff = (numOscs >= 3)
        ? 1.0f - std::exp(-juce::MathConstants<float>::twoPi
                          * std::min(4000.0f, settings.cutoffHz * 1.4f) / fsr)
        : 1.0f;

    // Enable 24dB/oct for synth family (Moog, Reese, Acid) — steeper filter
    filter24dB = (chars.oscMode == OscMode::Saw || chars.oscMode == OscMode::Square);

    // ----- Body resonator -----
    if (chars.bodyDelayRatio > 0.01f && settings.body > 0.01f)
    {
        const float bodyFreq = baseFreq * chars.bodyDelayRatio;
        bodyDelaySamples = juce::jlimit(2.0f, static_cast<float>(kBodyBufSize - 2),
                                        fsr / std::max(20.0f, bodyFreq));
        bodyFeedback   = settings.body * chars.bodyMaxFeedback;
        baseBodyFeedback = bodyFeedback;
        bodyDampState  = 0.0f;
        bodyWritePos   = 0;
        std::memset(bodyBuf, 0, sizeof(bodyBuf));
    }
    else
    {
        bodyFeedback = 0.0f;
        baseBodyFeedback = 0.0f;
    }

    // ----- Pan -----
    const auto pan = juce::jlimit(-1.0f, 1.0f, basePan);
    panL = std::sqrt(0.5f * (1.0f - pan));
    panR = std::sqrt(0.5f * (1.0f + pan));

    // ----- Max duration guard -----
    maxAgeSamples = static_cast<int>(sr * std::max(1.0f,
        settings.decaySeconds * 6.0f + settings.releaseSeconds * 3.0f));
    if (maxAgeSamples > static_cast<int>(sr * 30.0))
        maxAgeSamples = static_cast<int>(sr * 30.0);

    setVoiceModulation(currentModulation, sr);
}

// =========================================================================
// noteOff
// =========================================================================
void BassVoice::noteOff()
{
    if (envState != EnvState::Off && envState != EnvState::Release)
    {
        envState = EnvState::Release;
    }
}

void BassVoice::forceQuickRelease() noexcept
{
    if (envState == EnvState::Off)
        return;
    quickReleaseForced = true;
    envState = EnvState::Release;
    releaseCoeff = std::exp(-1.0f / 64.0f);
}

void BassVoice::forceReleaseSeconds(float seconds) noexcept
{
    if (envState == EnvState::Off)
        return;

    quickReleaseForced = true;
    envState = EnvState::Release;
    const float safeSeconds = std::max(0.01f, seconds);
    releaseCoeff = std::exp(-1.0f / (safeSeconds * static_cast<float>(std::max(1.0, sr))));
}

void BassVoice::forceStop() noexcept
{
    envState = EnvState::Off;
    filterEnvState = FilterEnvState::Off;
    envLevel = 0.0f;
    filterEnvLevel = 0.0f;
    pitchEnvLevel = 0.0f;
    glideActive = false;
    quickReleaseForced = false;
}

// =========================================================================
// Glide: smoothly change pitch without retriggering envelope
// =========================================================================
void BassVoice::glideToNote(int note, float glideTimeSec)
{
    midiNote = note;
    targetFreq = 440.0f * std::pow(2.0f,
        (static_cast<float>(note) - 69.0f + settings.tuneSemitones) / 12.0f);

    if (glideTimeSec > 0.001f)
    {
        const float glideTimeSamples = glideTimeSec * static_cast<float>(sr);
        // Exponential glide: ratio per sample to reach target
        const float ratio = targetFreq / std::max(0.01f, baseFreq);
        
        // FIX Phase 2.1: Glide guard — limit glide ratio to prevent "slap" effect
        // Large intervals with short glide times can create harsh portamento
        // Limit ratio to max 2.0 (one octave) and ensure minimum glide time of 50ms
        const float maxGlideRatio = 2.0f;  // Max one octave compression
        const float safeGlideTimeSec = std::max(glideTimeSec, 0.05f);  // Min 50ms
        const float safeRatio = std::min(ratio, maxGlideRatio);
        const float safeGlideTimeSamples = safeGlideTimeSec * static_cast<float>(sr);
        glideCoeff = std::pow(safeRatio, 1.0f / std::max(1.0f, safeGlideTimeSamples));
        glideActive = true;
    }
    else
    {
        baseFreq = targetFreq;
        glideActive = false;
        glideCoeff = 1.0f;
    }
}

// =========================================================================
// Retrigger with glide: new note with envelope retrigger + pitch glide
// =========================================================================
void BassVoice::retriggerWithGlide(const BassSettings& s,
                                   const BassCharacteristics&,
                                   int note, float velocity,
                                   double)
{
    const float oldBaseFreq = baseFreq;
    baseSettings = s;
    settings = s;
    vel = juce::jlimit(0.0f, 1.0f, velocity);
    midiNote = note;
    ageSamples = 0;
    // FIX: Guard attack to prevent click with random oscillator phases on unison
    baseAttackSeconds = std::max(settings.attackSeconds, 0.001f);
    baseDecaySeconds = settings.decaySeconds;
    baseReleaseSeconds = settings.releaseSeconds;
    basePan = settings.pan;
    baseLevel = settings.level;
    quickReleaseForced = false;

    const auto fsr = static_cast<float>(sr);

    targetFreq = 440.0f * std::pow(2.0f,
        (static_cast<float>(note) - 69.0f + settings.tuneSemitones) / 12.0f);

    // Setup glide from old pitch
    if (settings.glideTime > 0.001f)
    {
        baseFreq = oldBaseFreq;
        const float glideTimeSamples = settings.glideTime * fsr;
        const float ratio = targetFreq / std::max(0.01f, baseFreq);
        // FIX Phase 2.1: Same glide guard as glideToNote()
        const float maxGlideRatio = 2.0f;
        const float safeGlideTimeSec = std::max(settings.glideTime, 0.05f);
        const float safeRatio = std::min(ratio, maxGlideRatio);
        const float safeGlideTimeSamples = safeGlideTimeSec * fsr;
        glideCoeff = std::pow(safeRatio, 1.0f / std::max(1.0f, safeGlideTimeSamples));
        glideActive = true;
    }
    else
    {
        baseFreq = targetFreq;
        glideActive = false;
        glideCoeff = 1.0f;
    }

    // Retrigger envelope
    if (baseAttackSeconds > 0.0001f)
    {
        envLevel = 0.0001f;  // C2: reset cleanly — avoid click on legato retrigger
        attackCoeff = std::exp(std::log(1.0f / 0.0001f) / (baseAttackSeconds * fsr));
        envState = EnvState::Attack;
    }
    else
    {
        envLevel = 1.0f;
        envState = EnvState::Decay1;
    }

    // Update oscillator phase increments
    if (chars.oscMode != OscMode::Additive)
    {
        // FIX: Use same cents-based detune as noteOn for consistency
        const float detuneCents = chars.detuneAmount * 100.0f * computeDetuneBlend();
        const float detuneRatio = std::pow(2.0f, detuneCents / 1200.0f);
        for (int o = 0; o < numOscs; ++o)
        {
            float freqRatio = 1.0f;
            if (numOscs == 2)
                freqRatio = (o == 0) ? (2.0f / detuneRatio) : detuneRatio;
            else if (numOscs == 3)
                freqRatio = std::pow(detuneRatio, static_cast<float>(o - 1));
            oscs[static_cast<std::size_t>(o)].phaseInc = (baseFreq * freqRatio) / fsr;
        }
    }
    subPhaseInc = (baseFreq * 0.5f) / fsr;

    maxAgeSamples = static_cast<int>(sr * std::max(1.0f,
        settings.decaySeconds * 6.0f + settings.releaseSeconds * 3.0f));
    if (maxAgeSamples > static_cast<int>(sr * 30.0))
        maxAgeSamples = static_cast<int>(sr * 30.0);

    setVoiceModulation(currentModulation, sr);
}

// =========================================================================
// Per-block render
// =========================================================================
void BassVoice::render(juce::AudioBuffer<float>& buffer,
                       const int startSample, const int numSamples)
{
    if (envState == EnvState::Off)
        return;

    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    if (numChannels <= 0)
        return;

    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    juce::ignoreUnused(sr);

    auto* left  = buffer.getWritePointer(0);
    auto* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    // HPF post-saturation: removes DC asymmetry from tanh on high-drive sub-bass
    const bool needsHpf = chars.isSubBass && chars.builtInSaturation > 1.5f;
    const float hpfAlpha = needsHpf
        ? static_cast<float>(juce::MathConstants<double>::twoPi * 35.0 / std::max(1.0, sr))
        : 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        if (envState == EnvState::Off)
            break;

        // ---- Glide / portamento ----
        if (glideActive)
        {
            baseFreq *= glideCoeff;
            // Check if we've reached (or passed) the target
            if ((glideCoeff >= 1.0f && baseFreq >= targetFreq) ||
                (glideCoeff <= 1.0f && baseFreq <= targetFreq) ||
                std::abs(baseFreq - targetFreq) < 0.01f)
            {
                baseFreq = targetFreq;
                glideActive = false;
                glideCoeff = 1.0f;
            }
            // Update phase increments for new frequency
            const auto fsr = static_cast<float>(sr);
            if (chars.oscMode != OscMode::Additive)
            {
                // FIX: Use same cents-based detune as noteOn for consistency
                const float detuneCents = chars.detuneAmount * 100.0f * computeDetuneBlend();
                const float detuneRatio = std::pow(2.0f, detuneCents / 1200.0f);
                for (int o = 0; o < numOscs; ++o)
                {
                    float freqRatio = 1.0f;
                    if (numOscs == 2)
                        freqRatio = (o == 0) ? (2.0f / detuneRatio) : detuneRatio;
                    else if (numOscs == 3)
                        freqRatio = std::pow(detuneRatio, static_cast<float>(o - 1));
                    oscs[static_cast<std::size_t>(o)].phaseInc = (baseFreq * freqRatio) / fsr;
                }
            }
            else
            {
                // For additive, update fundamental partials proportionally
                for (int n = 0; n < numActivePartials; ++n)
                {
                    const int harmonic = n + 1;
                    const float fn = static_cast<float>(harmonic) * baseFreq *
                        std::sqrt(1.0f + chars.inharmonicity *
                                  static_cast<float>(harmonic * harmonic));
                    partials[static_cast<std::size_t>(n)].phaseInc = fn / fsr;
                }
            }
            subPhaseInc = (baseFreq * 0.5f) / static_cast<float>(sr);
        }

        // ---- Pitch envelope + pitch bend ----
        pitchEnvLevel *= pitchEnvDecayCoeff;
        const float pitchMult = (1.0f + pitchEnvLevel * pitchEnvDepthRatio) * pitchBendFactor * modPitchFactor;

        switch (filterEnvState)
        {
        case FilterEnvState::Attack:
            filterEnvLevel = 1.0f + (filterEnvLevel - 1.0f) * filterEnvAttackCoeff;
            if (filterEnvLevel >= 0.999f)
            {
                filterEnvLevel = 1.0f;
                filterEnvState = FilterEnvState::Decay;
            }
            break;
        case FilterEnvState::Decay:
            filterEnvLevel *= filterEnvDecayCoeff;
            if (filterEnvLevel <= 0.0005f)
            {
                filterEnvLevel = 0.0f;
                filterEnvState = FilterEnvState::Off;
            }
            break;
        case FilterEnvState::Off:
            break;
        }

        // ---- ADSR envelope ----
        switch (envState)
        {
        case EnvState::Attack:
            envLevel *= attackCoeff;
            if (envLevel >= 1.0f)
            {
                envLevel = 1.0f;
                envState = EnvState::Decay1;
            }
            break;

        case EnvState::Decay1:
            envLevel = decay1Target + (envLevel - decay1Target) * decay1Coeff;
            if (envLevel <= decay1Target + 0.002f)
            {
                envLevel = decay1Target;
                envState = EnvState::Decay2;
            }
            break;

        case EnvState::Decay2:
            envLevel *= decay2Coeff;
            if (envLevel <= decay1Target * settings.sustainLevel + 0.001f)
            {
                envLevel = std::max(0.0001f, decay1Target * settings.sustainLevel);
                envState = EnvState::Sustain;
            }
            break;

        case EnvState::Sustain:
            // Hold at sustain level while key is held
            break;

        case EnvState::Release:
            envLevel *= releaseCoeff;
            if (envLevel < 0.0001f)
            {
                envLevel = 0.0f;
                envState = EnvState::Off;
                break;
            }
            break;

        case EnvState::Off:
            break;
        }

        if (envState == EnvState::Off)
            break;

        // ---- Generate oscillator signal ----
        float signal = 0.0f;

        if (chars.oscMode == OscMode::Additive)
        {
            // Sum partials with individual decay
            for (int n = 0; n < numActivePartials; ++n)
            {
                auto& p = partials[static_cast<std::size_t>(n)];
                signal += p.amplitude * std::sin(p.phase * twoPi);
                p.phase += p.phaseInc * pitchMult;
                if (p.phase >= 1.0f) p.phase -= 1.0f;
                p.amplitude *= p.decayCoeff;
            }
        }
        else
        {
            // Oscillator modes: saw/sine/square
            for (int o = 0; o < numOscs; ++o)
            {
                auto& osc = oscs[static_cast<std::size_t>(o)];
                const float inc = osc.phaseInc * pitchMult;

                switch (chars.oscMode)
                {
                case OscMode::Sine:
                    signal += std::sin(osc.phase * twoPi);
                    break;

                case OscMode::Saw:
                {
                    float saw = 2.0f * osc.phase - 1.0f;
                    saw -= polyBlep(osc.phase, inc);
                    signal += saw;
                    break;
                }

                case OscMode::Square:
                {
                    // FIX: PolyBLEP at both discontinuities of square wave
                    // First discontinuity at phase=0: subtract blep
                    // Second discontinuity at phase=0.5: add blep (because square jumps from -1 to +1)
                    float sq = (osc.phase < 0.5f) ? 1.0f : -1.0f;
                    sq -= polyBlep(osc.phase, inc);
                    float shifted = osc.phase + 0.5f;
                    if (shifted >= 1.0f) shifted -= 1.0f;
                    sq += polyBlep(shifted, inc);
                    signal += sq;
                    break;
                }

                default:
                    break;
                }

                osc.phase += inc;
                if (osc.phase >= 1.0f) osc.phase -= 1.0f;
            }

            // Normalise unison
            if (numOscs > 1)
            {
                signal /= static_cast<float>(numOscs);
                // Reese: 1-pole LP reduces HF unison beating; coeff = 1.0 on other voices (bypass)
                if (unisonLpCoeff < 0.999f)
                {
                    unisonLpState += unisonLpCoeff * (signal - unisonLpState);
                    signal = unisonLpState;
                }
            }
        }

        // ---- Sub oscillator ----
        // FIX: Reduce sub level by 25% to minimize beating with fundamental
        // The octave sub (0.5x) can create audible interference with the fundamental
        // especially on low notes. This reduces the constructive/destructive phase interaction.
        if (settings.subLevel > 0.001f)
        {
            constexpr float subLevelCorrection = 0.75f;  // -25% to reduce beating
            const float sub = std::sin(subPhase * twoPi) * settings.subLevel * subLevelCorrection;
            signal += sub;
            subPhase += subPhaseInc * pitchMult;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
        }

        // ---- Pluck transient (noise burst) ----
        if (pluckLevel > 0.001f)
        {
            const float noise = rng.nextFloat() * 2.0f - 1.0f;
            // Slap (pluckAmount ≥ 0.75): 1-pole LP ~8 kHz softens pluck HF before hi-hats collide
            pluckLpState += 0.68f * (noise - pluckLpState);
            const float pluckNoise = (chars.pluckAmount >= 0.75f) ? pluckLpState : noise;
            signal += pluckNoise * pluckLevel * 0.25f;
            pluckLevel *= pluckDecayCoeff;
        }

        // ---- Body resonator ----
        if (bodyFeedback > 0.001f)
        {
            const float delayed = readComb(bodyBuf, kBodyBufSize,
                                           bodyWritePos, bodyDelaySamples);
            bodyDampState += chars.bodyDamping * 0.4f * (delayed - bodyDampState);
            bodyBuf[bodyWritePos] = signal * 0.3f + bodyDampState * bodyFeedback;
            bodyWritePos = (bodyWritePos + 1) % kBodyBufSize;
            signal += delayed * settings.body * 0.4f;
        }

        // ---- SVF low-pass filter (with envelope modulation, optional 24dB/oct) ----
        {
            // Dedicated filter env depth is separate from brightness.
            const float lfoMod = std::pow(2.0f, lfoCutoffMod * 2.0f);
            const float cutoffMod = std::clamp(currentModulation.cutoffMul, 0.0625f, 16.0f);
            const float filterEnvMul = std::exp2(filterEnvLevel * filterEnvDepthOctaves);
            const float envModF = juce::jmin(
                filterBaseF * cutoffMod * filterEnvMul * lfoMod,
                filterMaxF);
            const float hp = signal - svfLow - filterQinv * svfBand;
            svfBand += envModF * hp;
            svfLow  += envModF * svfBand;
            signal = svfLow;

            // Second stage for 24dB/oct (cascaded SVF)
            if (filter24dB)
            {
                const float hp2 = signal - svfLow2 - filterQinv * svfBand2;
                svfBand2 += envModF * hp2;
                svfLow2  += envModF * svfBand2;
                signal = svfLow2;
            }
        }

        // ---- Saturation / Drive ----
        {
            const float totalDrive = chars.builtInSaturation
                                   + settings.drive * 6.0f;
            if (totalDrive > 0.01f)
            {
                const float drv = 1.0f + totalDrive;
                signal = std::tanh(signal * drv) / std::max(0.01f, std::tanh(drv));
            }
        }

        // ---- Post-saturation HPF (35 Hz) — sub-bass with high built-in drive ----
        if (needsHpf)
        {
            hpfState += hpfAlpha * (signal - hpfState);
            signal -= hpfState;
        }

        // ---- Character processing ----
        if (settings.character > 0.01f)
        {
            // Acoustic (Additive): warmth/growl at 0.12 — more harmonic body
            // Synth (Saw/Square): slightly lighter (0.09) — avoids distortion on already-rich oscillators
            const float charScale = (chars.oscMode == OscMode::Additive) ? 0.12f : 0.09f;
            signal += std::tanh(signal * 2.0f) * (settings.character * charScale);
        }

        // ---- Apply envelope, velocity, level ----
        signal *= envLevel * vel * settings.level;

        // ---- Stereo pan & accumulate ----
        const int idx = startSample + i;
        left[idx] += signal * panL;
        if (right != nullptr)
            right[idx] += signal * panR;

        ++ageSamples;
        if (ageSamples >= maxAgeSamples)
        {
            envState = EnvState::Off;
            break;
        }
    }
}

} // namespace mbs
