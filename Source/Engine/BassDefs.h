#pragma once

#include <array>
#include <string>

namespace mbs
{
constexpr int kNumBasses    = 9;
constexpr int kNumFamilies  = 3;
constexpr int kMaxBassesPerFamily = 3;

constexpr int kFamilySize[]  = { 3, 3, 3 };
constexpr int kFamilyStart[] = { 0, 3, 6 };

// =========================================================================
// Bass families
// =========================================================================
enum class Family { Acoustic = 0, Eight08, Synth };

// =========================================================================
// Oscillator modes
// =========================================================================
enum class OscMode { Additive = 0, Saw, Sine, Square };

// =========================================================================
// Per-bass synthesis character (not user-editable)
// =========================================================================
struct BassCharacteristics
{
    OscMode oscMode;              // oscillator type
    int     numPartials;          // number of partials (additive mode only)
    float   inharmonicity;        // frequency stretch factor
    float   detuneAmount;         // unison detune (Hz ratio offset per osc)
    int     numOscillators;       // unison oscillator count (1–3)

    float   pitchEnvSemitones;    // pitch envelope maximum depth (semitones)
    float   pitchEnvSeconds;      // pitch envelope decay time (seconds)

    float   decay1Ratio;          // fast-decay ratio relative to user decay
    float   decay2Ratio;          // slow-decay ratio relative to user decay
    float   sustainPlatform;      // level for decay1→decay2 transition

    float   bodyDelayRatio;       // body comb delay ratio vs note period
    float   bodyMaxFeedback;      // max body comb feedback
    float   bodyDamping;          // body HF damping (0–1)

    float   pluckAmount;          // pluck transient noise level
    float   pluckSeconds;         // pluck noise decay time

    float   builtInSaturation;    // built-in saturation drive (0 = none)
    bool    isSubBass;            // sub-bass mode (limits partials, boosts low)
};

// =========================================================================
// Per-bass user parameters
// =========================================================================
struct BassSettings
{
    float level          = 0.8f;
    float tuneSemitones  = 0.0f;
    float brightness     = 0.5f;      // harmonic emphasis / timbre
    float attackSeconds  = 0.005f;
    float decaySeconds   = 2.0f;
    float sustainLevel   = 0.3f;
    float releaseSeconds = 0.2f;
    float body           = 0.5f;      // body resonance amount
    float drive          = 0.0f;      // saturation / drive (0–1)
    float pitchEnv       = 0.5f;      // pitch envelope intensity (0–1)
    float subLevel       = 0.5f;      // sub-harmonic level (0–1)
    float character      = 0.5f;      // type-specific character (0–1)
    float cutoffHz       = 2000.0f;   // LP filter cutoff
    float pan            = 0.0f;      // stereo pan (-1..+1)
    float resonance      = 0.3f;      // filter resonance (0–1), independent from brightness
    float glideTime      = 0.0f;      // portamento time in seconds (0 = off)
    float filterEnv      = 0.0f;      // dedicated filter envelope depth (0–1)
};

// =========================================================================
// Global FX settings (stored per-preset alongside BassSettings)
// =========================================================================
struct GlobalFxSettings
{
    // Saturator
    float satDrive         = 1.8f;
    float satMix           = 0.15f;
    bool  saturatorOn      = true;
    // Transient
    float transientAttack  = 0.10f;
    float transientSustain = 0.0f;
    float transientMix     = 0.4f;
    bool  transientOn      = true;
    // Compressor
    float compThreshold = -19.0f;
    float compRatio     = 3.0f;
    float compAttack    = 10.0f;
    float compRelease   = 120.0f;
    float compMakeup    = 0.0f;
    float compMix       = 1.0f;
    bool  compressorOn  = false;  // FIX: Off by default — preserve transients, let user decide
    // EQ
    float eqLowFreq   = 80.0f;
    float eqLowGain   = 0.0f;
    float eqMidFreq   = 600.0f;
    float eqMidGain   = 0.0f;
    float eqMidQ      = 1.0f;
    float eqHighFreq  = 3500.0f;
    float eqHighGain  = 0.0f;
    bool  eqOn        = true;
    // Chorus
    float chorusRate   = 0.8f;
    float chorusDepth  = 0.4f;
    float chorusMix    = 0.0f;
    bool  chorusOn     = false;  // FIX: Off by default — bass must be mono-safe first
    // Delay
    float delayTime     = 300.0f;
    float delayFeedback = 0.25f;
    float delayMix      = 0.0f;
    bool  delaySync     = false;
    int   delayNoteDiv  = 0;
    bool  delayOn       = true;
    // Reverb (Dattorro)
    float reverbSize    = 0.40f;
    float reverbDamping = 0.55f;
    float reverbWidth   = 0.60f;
    float reverbMix     = 0.0f;
    bool  reverbOn      = true;
    // Limiter
    float limiterThreshold = -0.3f;
    float limiterRelease   = 50.0f;
    bool  limiterOn        = true;
};

struct PatchPerformanceSettings
{
    int   monoMode       = 0;
    float lfoRate        = 1.2f;
    float lfoDepth       = 0.0f;
    int   lfoWave        = 0;
    int   lfoDest        = 0;
    float macroFatness   = 0.5f;
    float macroBrillance = 0.5f;
    float macroPunch     = 0.5f;
    float macroDepth     = 0.3f;
    int   modWheelTarget = 1;
    float pitchBendRange = 2.0f;
};

// =========================================================================
// FX availability per bass
// =========================================================================
enum class GlobalFxSlot
{
    Saturator = 0,
    Transient,
    Compressor,
    Eq,
    Chorus,
    Delay,
    Reverb,
    Limiter
};

struct FxAvailability
{
    bool saturator  = true;
    bool transient  = true;
    bool compressor = true;
    bool eq         = true;
    bool chorus     = true;
    bool delay      = true;
    bool reverb     = true;
    bool limiter    = true;
};

// =========================================================================
// Accessors
// =========================================================================
Family      getFamily              (int bassIndex);
int         getFamilyStartIndex    (Family family);
const char* getFamilyName          (int familyIndex);
const char* getBassName            (int bassIndex);
const char* getBassShortName       (int bassIndex);
const BassCharacteristics& getCharacteristics(int bassIndex);
BassSettings               getDefaultSettings(int bassIndex);
const char*                getBassDescription (int bassIndex);
const FxAvailability&      getFxAvailability  (int bassIndex);
bool                       isFxAvailable      (int bassIndex, GlobalFxSlot slot);
void                       maskUnavailableFx  (int bassIndex, GlobalFxSettings& fx);
bool                       supportsBodyControl(int bassIndex);
bool                       supportsPitchEnvControl(int bassIndex);
bool                       supportsFilterEnvControl(int bassIndex);

} // namespace mbs
