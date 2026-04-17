#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include "SinTable.h"

namespace mbs {
namespace fx {

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

inline float hermite(float frac, float y0, float y1, float y2, float y3)
{
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// =============================================================================
// Fractional delay line (Hermite interpolation)
// =============================================================================
class DelayLine
{
public:
    void allocate(int maxSamples)
    {
        buf.assign(static_cast<std::size_t>(maxSamples + 4), 0.0f);
        mask = static_cast<int>(buf.size());
        wp   = 0;
    }

    void clear()
    {
        std::fill(buf.begin(), buf.end(), 0.0f);
        wp = 0;
    }

    void push(float x)
    {
        buf[static_cast<std::size_t>(wp)] = x;
        if (++wp >= mask) wp = 0;
    }

    float read(float delaySamples) const
    {
        const float d = std::max(0.0f, delaySamples);
        const int   di  = static_cast<int>(d);
        const float frac = d - static_cast<float>(di);
        auto idx = [&](int offset) -> std::size_t {
            int i = wp - 1 - offset;
            while (i < 0) i += mask;
            return static_cast<std::size_t>(i % mask);
        };
        const float y0 = buf[idx(di + 1)];
        const float y1 = buf[idx(di)];
        const float y2 = buf[idx(std::max(0, di - 1))];
        const float y3 = buf[idx(std::max(0, di - 2))];
        return hermite(frac, y0, y1, y2, y3);
    }

    float readLinear(float delaySamples) const
    {
        const float d = std::max(0.0f, delaySamples);
        const int   di  = static_cast<int>(d);
        const float frac = d - static_cast<float>(di);
        auto idx = [&](int offset) -> std::size_t {
            int i = wp - 1 - offset;
            while (i < 0) i += mask;
            return static_cast<std::size_t>(i % mask);
        };
        return buf[idx(di)] + frac * (buf[idx(di + 1)] - buf[idx(di)]);
    }

private:
    std::vector<float> buf;
    int mask = 0;
    int wp   = 0;
};

// =============================================================================
// One-pole lowpass
// =============================================================================
struct OnePole
{
    float state = 0.0f;
    float process(float x, float coeff)
    {
        state += coeff * (x - state);
        state += 1e-25f;       // flush denormals
        state -= 1e-25f;
        return state;
    }
    void  clear() { state = 0.0f; }
};

// =============================================================================
// All-pass filter
// =============================================================================
class AllPass
{
public:
    void allocate(int maxLen) { delay.allocate(maxLen + 4); }
    void clear() { delay.clear(); }

    float process(float x, float delaySamples, float coeff)
    {
        const float delayed = delay.readLinear(delaySamples);
        const float y = -coeff * x + delayed;
        delay.push(x + coeff * y);
        return y;
    }

private:
    DelayLine delay;
};

// =============================================================================
// Dattorro Plate Reverb
// =============================================================================
class DattorroPlateReverb
{
public:
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = std::max(1.0, sampleRate);
        const float scale = static_cast<float>(sr / 29761.0);

        preDelay.allocate(static_cast<int>(sr * 0.1) + 16);

        for (int i = 0; i < 4; ++i)
            inDiff[i].allocate(static_cast<int>(kInDiffLen[i] * scale) + 16);

        for (int i = 0; i < 2; ++i)
        {
            tankModApf[i].allocate(static_cast<int>(kTankApfLen[i] * scale * 1.15f) + 16);
            tankDelay[i].allocate(static_cast<int>(kTankDelayLen[i] * scale) + 16);
        }

        scaleFactor = scale;
        reset();
    }

    void reset()
    {
        preDelay.clear();
        for (auto& d : inDiff) d.clear();
        for (auto& a : tankModApf) a.clear();
        for (auto& d : tankDelay) d.clear();
        for (auto& f : tankDamp) f.clear();
        for (auto& f : inBandwidth) f.clear();
        for (auto& s : tankState) s = 0.0f;
        modPhase = 0.0f;
        currentDecay = 0.55f;
        currentDamping = 0.50f;
        currentWidth = 0.80f;
        currentMix = 0.0f;
        currentPreDelayMs = 0.0f;
        paramsInitialised = false;
    }

    struct Params
    {
        float decay      = 0.55f;
        float damping    = 0.50f;
        float width      = 0.80f;
        float mix        = 0.25f;
        float preDelayMs = 0.0f;
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float targetDecay = clamp01(p.decay);
        const float targetDamping = clamp01(p.damping);
        const float targetWidth = clamp01(p.width);
        const float targetMix = clamp01(p.mix);
        const float targetPreDelayMs = juce::jlimit(0.0f, 100.0f, p.preDelayMs);
        if (!paramsInitialised)
        {
            currentDecay = targetDecay;
            currentDamping = targetDamping;
            currentWidth = targetWidth;
            currentMix = targetMix;
            currentPreDelayMs = targetPreDelayMs;
            paramsInitialised = true;
        }
        if (targetMix <= 0.0001f && currentMix <= 0.0001f)
        {
            currentDecay = targetDecay;
            currentDamping = targetDamping;
            currentWidth = targetWidth;
            currentMix = targetMix;
            currentPreDelayMs = targetPreDelayMs;
            return;
        }

        const float invSamples = 1.0f / static_cast<float>(juce::jmax(1, numSamples));
        const float decayStep = (targetDecay - currentDecay) * invSamples;
        const float dampingStep = (targetDamping - currentDamping) * invSamples;
        const float widthStep = (targetWidth - currentWidth) * invSamples;
        const float mixStep = (targetMix - currentMix) * invSamples;
        const float preDelayStep = (targetPreDelayMs - currentPreDelayMs) * invSamples;
        const float modRate = 0.8f / static_cast<float>(sr);
        const float modDepth = 8.0f * scaleFactor;

        const float id0 = kInDiffLen[0] * scaleFactor;
        const float id1 = kInDiffLen[1] * scaleFactor;
        const float id2 = kInDiffLen[2] * scaleFactor;
        const float id3 = kInDiffLen[3] * scaleFactor;

        const float ta0 = kTankApfLen[0] * scaleFactor;
        const float ta1 = kTankApfLen[1] * scaleFactor;

        const float td0 = kTankDelayLen[0] * scaleFactor;
        const float td1 = kTankDelayLen[1] * scaleFactor;

        constexpr float kTwoPi = 6.283185307179586f;

        for (int i = 0; i < numSamples; ++i)
        {
            currentDecay += decayStep;
            currentDamping += dampingStep;
            currentWidth += widthStep;
            currentMix += mixStep;
            currentPreDelayMs += preDelayStep;

            const float decay = 0.25f + currentDecay * 0.73f;
            const float dampCoeff = 1.0f - currentDamping * 0.7f;
            const float bw = 0.9995f - currentDamping * 0.3f;
            const float mix = clamp01(currentMix);
            const float preDelaySamples = clamp01(currentPreDelayMs / 100.0f)
                                           * static_cast<float>(sr) * 0.1f;
            const float width = clamp01(currentWidth);
            const float decayDiff = decay * 0.6f + 0.1f;

            const float dryL = left[i];
            const float dryR = right != nullptr ? right[i] : dryL;

            float input = (dryL + dryR) * 0.5f;
            preDelay.push(input);
            input = preDelay.readLinear(preDelaySamples);

            input = inBandwidth[0].process(input, bw);

            input = inDiff[0].process(input, id0, 0.75f);
            input = inDiff[1].process(input, id1, 0.75f);
            input = inDiff[2].process(input, id2, 0.625f);
            input = inDiff[3].process(input, id3, 0.625f);

            const float lfo = mbs::fastSin(modPhase);
            modPhase += modRate;
            if (modPhase >= 1.0f) modPhase -= 1.0f;

            float t0 = input + tankState[1] * decay;
            t0 = tankModApf[0].process(t0, ta0 + lfo * modDepth, decayDiff);
            tankDelay[0].push(t0);
            t0 = tankDelay[0].readLinear(td0);
            t0 = tankDamp[0].process(t0, dampCoeff) * decay;
            tankState[0] = t0;

            float t1 = input + tankState[0] * decay;
            t1 = tankModApf[1].process(t1, ta1 - lfo * modDepth, decayDiff);
            tankDelay[1].push(t1);
            t1 = tankDelay[1].readLinear(td1);
            t1 = tankDamp[1].process(t1, dampCoeff) * decay;
            tankState[1] = t1;

            // flush tank denormals
            for (auto& s : tankState) { s += 1e-25f; s -= 1e-25f; }

            const float wetL = tankState[0];
            const float wetR = tankState[1];

            const float wetMono = (wetL + wetR) * 0.5f;
            const float outWetL = wetMono + (wetL - wetMono) * width;
            const float outWetR = wetMono + (wetR - wetMono) * width;

            const float dry = 1.0f - mix * 0.5f;
            left[i]  = dryL * dry + outWetL * mix;
            if (right != nullptr)
                right[i] = dryR * dry + outWetR * mix;
        }
    }

private:
    double sr = 44100.0;
    float scaleFactor = 1.0f;

    static constexpr float kInDiffLen[4]    = { 142.0f, 107.0f, 379.0f, 277.0f };
    static constexpr float kTankApfLen[2]   = { 672.0f, 908.0f };
    static constexpr float kTankDelayLen[2] = { 4453.0f, 3720.0f };

    DelayLine preDelay;
    AllPass   inDiff[4];
    AllPass   tankModApf[2];
    DelayLine tankDelay[2];
    OnePole   tankDamp[2];
    OnePole   inBandwidth[1];
    float     tankState[2] = { 0.0f, 0.0f };
    float     modPhase = 0.0f;
    float     currentDecay = 0.55f;
    float     currentDamping = 0.50f;
    float     currentWidth = 0.80f;
    float     currentMix = 0.0f;
    float     currentPreDelayMs = 0.0f;
    bool      paramsInitialised = false;
};

// =============================================================================
// 3-Band Parametric EQ (Low Shelf / Mid Peak / High Shelf)
// =============================================================================
class ParametricEQ3Band
{
public:
    void prepare(double sampleRate)
    {
        sr = std::max(1.0, sampleRate);
        for (auto& s : state) s = {};
    }

    void reset()
    {
        for (auto& s : state) s = {};
    }

    struct Params
    {
        float lowFreq   = 200.0f;
        float lowGainDb = 0.0f;
        float midFreq   = 1000.0f;
        float midGainDb = 0.0f;
        float midQ      = 1.0f;
        float highFreq  = 5000.0f;
        float highGainDb = 0.0f;
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        if (std::abs(p.lowGainDb)  < 0.05f &&
            std::abs(p.midGainDb)  < 0.05f &&
            std::abs(p.highGainDb) < 0.05f)
            return;

        const bool changed = (p.lowFreq   != cachedEqParams.lowFreq   || p.lowGainDb  != cachedEqParams.lowGainDb  ||
                              p.midFreq   != cachedEqParams.midFreq   || p.midGainDb  != cachedEqParams.midGainDb  ||
                              p.midQ      != cachedEqParams.midQ      ||
                              p.highFreq  != cachedEqParams.highFreq  || p.highGainDb != cachedEqParams.highGainDb);
        if (changed)
        {
            coeffs[0] = calcLowShelf(p.lowFreq, p.lowGainDb);
            coeffs[1] = calcPeaking(p.midFreq, p.midGainDb, p.midQ);
            coeffs[2] = calcHighShelf(p.highFreq, p.highGainDb);
            cachedEqParams = { p.lowFreq, p.lowGainDb, p.midFreq, p.midGainDb, p.midQ, p.highFreq, p.highGainDb };
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float L = left[i];
            float R = right != nullptr ? right[i] : 0.0f;
            for (int b = 0; b < 3; ++b)
            {
                L = biquadDF2T(state[b * 2],     coeffs[b], L);
                R = biquadDF2T(state[b * 2 + 1], coeffs[b], R);
            }
            left[i] = L;
            if (right != nullptr) right[i] = R;
        }
    }

private:
    struct BiquadCoeffs { float b0=1, b1=0, b2=0, a1=0, a2=0; };
    struct BiquadState  { float z1=0, z2=0; };

    static float biquadDF2T(BiquadState& s, const BiquadCoeffs& c, float x)
    {
        const float y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    BiquadCoeffs calcLowShelf(float freq, float gainDb) const
    {
        freq = std::min(freq, static_cast<float>(sr) * 0.48f);
        constexpr float kPi = 3.14159265f;
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * kPi * freq / static_cast<float>(sr);
        const float cosw = std::cos(w0), sinw = std::sin(w0);
        const float alpha = sinw / (2.0f * 0.707f);
        const float sqA = std::sqrt(A);
        const float a0 = (A + 1.0f) + (A - 1.0f) * cosw + 2.0f * sqA * alpha;
        BiquadCoeffs c;
        c.b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw + 2.0f * sqA * alpha) / a0;
        c.b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw) / a0;
        c.b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw - 2.0f * sqA * alpha) / a0;
        c.a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw) / a0;
        c.a2 = ((A + 1.0f) + (A - 1.0f) * cosw - 2.0f * sqA * alpha) / a0;
        return c;
    }

    BiquadCoeffs calcPeaking(float freq, float gainDb, float Q) const
    {
        freq = std::min(freq, static_cast<float>(sr) * 0.48f);
        constexpr float kPi = 3.14159265f;
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * kPi * freq / static_cast<float>(sr);
        const float cosw = std::cos(w0), sinw = std::sin(w0);
        const float alpha = sinw / (2.0f * std::max(0.01f, Q));
        const float a0 = 1.0f + alpha / A;
        BiquadCoeffs c;
        c.b0 = (1.0f + alpha * A) / a0;
        c.b1 = (-2.0f * cosw) / a0;
        c.b2 = (1.0f - alpha * A) / a0;
        c.a1 = c.b1;
        c.a2 = (1.0f - alpha / A) / a0;
        return c;
    }

    BiquadCoeffs calcHighShelf(float freq, float gainDb) const
    {
        freq = std::min(freq, static_cast<float>(sr) * 0.48f);
        constexpr float kPi = 3.14159265f;
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * kPi * freq / static_cast<float>(sr);
        const float cosw = std::cos(w0), sinw = std::sin(w0);
        const float alpha = sinw / (2.0f * 0.707f);
        const float sqA = std::sqrt(A);
        const float a0 = (A + 1.0f) - (A - 1.0f) * cosw + 2.0f * sqA * alpha;
        BiquadCoeffs c;
        c.b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw + 2.0f * sqA * alpha) / a0;
        c.b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw) / a0;
        c.b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw - 2.0f * sqA * alpha) / a0;
        c.a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw) / a0;
        c.a2 = ((A + 1.0f) - (A - 1.0f) * cosw - 2.0f * sqA * alpha) / a0;
        return c;
    }

    double sr = 44100.0;
    BiquadCoeffs coeffs[3];
    BiquadState  state[6];

    struct EqParamsCache {
        float lowFreq = -1.f, lowGainDb = 999.f;
        float midFreq = -1.f, midGainDb = 999.f, midQ = -1.f;
        float highFreq = -1.f, highGainDb = 999.f;
    } cachedEqParams;
};

// =============================================================================
// Stereo Chorus (modulated delay with quadrature LFO)
// =============================================================================
class StereoChorus
{
public:
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = std::max(1.0, sampleRate);
        const int maxDelaySamples = static_cast<int>(sr * 0.05) + 16;
        for (auto& d : delay) d.allocate(maxDelaySamples);
        reset();
    }

    void reset()
    {
        for (auto& d : delay) d.clear();
        lfoPhase[0] = 0.0f;
        lfoPhase[1] = 0.25f;
    }

    struct Params
    {
        float rateHz = 1.0f;
        float depth  = 0.5f;
        float mix    = 0.0f;
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float mix = clamp01(p.mix);
        if (mix <= 0.0001f) return;

        const float rate = std::max(0.01f, p.rateHz);
        const float depth = clamp01(p.depth);
        const float phaseInc = rate / static_cast<float>(sr);
        const float baseDelay = 0.007f * static_cast<float>(sr);
        const float modAmt    = depth * 0.003f * static_cast<float>(sr);
        constexpr float kTwoPi = 6.283185307179586f;

        const int numCh = (right != nullptr) ? 2 : 1;
        float* ch[2] = { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                const float lfo = mbs::fastSin(lfoPhase[c]);
                const float delaySamples = baseDelay + lfo * modAmt;

                delay[c].push(ch[c][i]);
                const float wet = delay[c].read(delaySamples);
                ch[c][i] = ch[c][i] * (1.0f - mix) + wet * mix;
            }

            lfoPhase[0] += phaseInc;
            if (lfoPhase[0] >= 1.0f) lfoPhase[0] -= 1.0f;
            lfoPhase[1] += phaseInc;
            if (lfoPhase[1] >= 1.0f) lfoPhase[1] -= 1.0f;
        }
    }

private:
    double sr = 44100.0;
    DelayLine delay[2];
    float lfoPhase[2] = { 0.0f, 0.25f };
};

// =============================================================================
// Stereo Delay with optional BPM sync
// =============================================================================
class StereoDelay
{
public:
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = std::max(1.0, sampleRate);
        const int maxDelaySamples = static_cast<int>(sr * 2.0) + 16;
        for (auto& d : delay) d.allocate(maxDelaySamples);
        reset();
    }

    void reset()
    {
        for (auto& d : delay) d.clear();
        currentDelaySamples = 300.0f;
        currentFeedback = 0.30f;
        currentMix = 0.0f;
        paramsInitialised = false;
    }

    struct Params
    {
        float timeMs     = 300.0f;
        float feedback   = 0.30f;
        float mix        = 0.0f;
        bool  syncToBpm  = false;
        float bpm        = 120.0f;
        int   noteDiv    = 0;
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        float targetDelaySamples;
        if (p.syncToBpm && p.bpm > 20.0f)
        {
            const float beatSec = 60.0f / std::max(20.0f, p.bpm);
            float mult = 1.0f;
            switch (p.noteDiv)
            {
                case 1: mult = 0.5f;    break;   // 1/8
                case 2: mult = 0.25f;   break;   // 1/16
                case 3: mult = 0.75f;   break;   // dotted 1/8
                case 4: mult = 1.0f/3.0f; break; // triplet 1/8
                default: mult = 1.0f;   break;   // 1/4
            }
            targetDelaySamples = beatSec * mult * static_cast<float>(sr);
        }
        else
        {
            targetDelaySamples = std::max(1.0f, p.timeMs) * 0.001f * static_cast<float>(sr);
        }

        const float maxDelay = static_cast<float>(sr) * 2.0f - 2.0f;
        targetDelaySamples = std::min(targetDelaySamples, maxDelay);
        const float targetFeedback = std::min(0.95f, std::max(0.0f, p.feedback));
        const float targetMix = clamp01(p.mix);
        if (!paramsInitialised)
        {
            currentDelaySamples = targetDelaySamples;
            currentFeedback = targetFeedback;
            currentMix = targetMix;
            paramsInitialised = true;
        }
        if (targetMix <= 0.0001f && currentMix <= 0.0001f)
        {
            currentDelaySamples = targetDelaySamples;
            currentFeedback = targetFeedback;
            currentMix = targetMix;
            return;
        }

        const float invSamples = 1.0f / static_cast<float>(juce::jmax(1, numSamples));
        const float delayStep = (targetDelaySamples - currentDelaySamples) * invSamples;
        const float feedbackStep = (targetFeedback - currentFeedback) * invSamples;
        const float mixStep = (targetMix - currentMix) * invSamples;
        const int numCh = (right != nullptr) ? 2 : 1;
        float* ch[2] = { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            currentDelaySamples += delayStep;
            currentFeedback += feedbackStep;
            currentMix += mixStep;

            for (int c = 0; c < numCh; ++c)
            {
                const float delayed = delay[c].readLinear(currentDelaySamples);
                delay[c].push(ch[c][i] + delayed * currentFeedback);
                ch[c][i] = ch[c][i] * (1.0f - currentMix) + delayed * currentMix;
            }
        }
    }

private:
    double sr = 44100.0;
    DelayLine delay[2];
    float currentDelaySamples = 300.0f;
    float currentFeedback = 0.30f;
    float currentMix = 0.0f;
    bool paramsInitialised = false;
};

// =============================================================================
// Output Limiter (feed-forward brick-wall)
// =============================================================================
class OutputLimiter
{
public:
    void prepare(double sampleRate)
    {
        sr = std::max(1.0, sampleRate);
        reset();
    }

    void reset()
    {
        envL = 0.0f;
        envR = 0.0f;
    }

    struct Params
    {
        float thresholdDb = -0.3f;
        float releaseMs   = 50.0f;
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float thresh = std::pow(10.0f, std::min(0.0f, p.thresholdDb) / 20.0f);
        if (thresh >= 0.9999f) return;

        const float relCoeff = std::exp(-1.0f / (std::max(1.0f, p.releaseMs) * 0.001f
                                                  * static_cast<float>(sr)));

        for (int i = 0; i < numSamples; ++i)
        {
            const float absL = std::abs(left[i]);
            if (absL > envL)
                envL = absL;
            else
                envL = relCoeff * envL + (1.0f - relCoeff) * absL;

            if (right != nullptr)
            {
                const float absR = std::abs(right[i]);
                if (absR > envR)
                    envR = absR;
                else
                    envR = relCoeff * envR + (1.0f - relCoeff) * absR;
            }

            // Linked gain — use the loudest channel to preserve stereo image
            const float env = std::max(envL, envR);
            if (env > thresh)
            {
                const float gain = thresh / env;
                left[i] *= gain;
                if (right != nullptr)
                    right[i] *= gain;
            }
        }
    }

private:
    double sr = 44100.0;
    float envL = 0.0f;
    float envR = 0.0f;
};

} // namespace fx
} // namespace mbs
