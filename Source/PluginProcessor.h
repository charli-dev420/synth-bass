#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <vector>

#include "Engine/BassVoice.h"
#include "Engine/FactoryPresets.h"
#include "Engine/FxProcessors.h"
#include "../Shared/PitchBendState.h"
#include "../Shared/ModulationMatrix.h"

class BassSynthAudioProcessor : public juce::AudioProcessor,
                                private juce::AsyncUpdater
{
public:
    static constexpr int kNumAuxOutputs = 4;
    static constexpr int kMaxVoices     = 32;
    static constexpr const char* kProcessorName = "UWdeVST Bass";

    BassSynthAudioProcessor();
    ~BassSynthAudioProcessor() override = default;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String makeBassParamId(int bassIndex, const juce::String& suffix);
    static juce::String makeModMatrixParamId(int slotIndex, const juce::String& suffix);
    static auto createBusLayout() -> BusesProperties;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return kProcessorName; }
    bool acceptsMidi()    const override { return true; }
    bool producesMidi()   const override { return false; }
    bool isMidiEffect()   const override { return false; }
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return parameters; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    // ── FLkey Mini CC page system ──────────────────────────────────────
    static constexpr int kNumCCPages = 7;
    int  getMidiCCPage() const noexcept { return midiCCPage.load(std::memory_order_relaxed); }
    static const char* getCCPageName(int page) noexcept;

    // Per-instrument preset API — all methods operate on the currently selected bass
    juce::StringArray getFactoryPresetNames() const;
    int  getCurrentFactoryPresetIndex() const noexcept;
    void applyFactoryPreset(int presetIndex);
    bool saveFactoryPreset(int presetIndex);
    juce::String getFactoryPresetBrowserLabel(int presetIndex) const;
    juce::String getFactoryPresetBrowserSearchText(int presetIndex) const;
#if defined(UWDEVST_BASS_TEST_BUILD)
    mbs::BassSettings snapshotMacroAppliedSettingsForTests(int bassIndex) const;
#endif

    static juce::File getUserPresetsDirectory(int bassIndex);
    static juce::File getFactoryOverridesDirectory();
    juce::Array<juce::File> scanUserPresets() const;
    bool saveUserPreset(const juce::String& name);
    bool updateUserPreset(const juce::File& file);
    bool deleteUserPreset(const juce::File& file);
    bool loadUserPreset(const juce::File& file);
    bool isCurrentPresetUser() const noexcept;
    juce::File getCurrentUserPresetFile() const noexcept;

    int  getSelectedBassIndex() const;
    bool isFxAvailableForCurrentBass(mbs::GlobalFxSlot slot) const;
    int  getActiveVoiceCount() const noexcept { return activeVoiceCountAtomic.load(std::memory_order_relaxed); }
    void randomizePreset(float amount = 0.15f);
    modmatrix::ModulationMatrix&       getModulationMatrix()       noexcept { return modulationMatrix; }
    const modmatrix::ModulationMatrix& getModulationMatrix() const noexcept { return modulationMatrix; }

    struct ParamBinding
    {
        std::atomic<float>* raw = nullptr;
        juce::RangedAudioParameter* ranged = nullptr;
    };

private:
    // Dying voice pool -- stolen voices fade out here
    static constexpr int kMaxDyingVoices = 8;

    struct VoiceSlot
    {
        mbs::BassVoice* voice = nullptr;
        int midiNote   = -1;
        int bassIndex  = 0;
        int outputBus  = 0;
        float velocity = 0.0f;
        uint64_t activationAge = 0;
        std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, kNumAuxOutputs> auxSendGains;
    };

    struct DyingVoiceSlot
    {
        mbs::BassVoice* voice = nullptr;
        int  outputBus = 0;
        float velocity = 0.0f;
        bool inUse     = false;
        uint64_t activationAge = 0;
    };
    std::array<DyingVoiceSlot, kMaxDyingVoices> dyingVoices{};

    struct BassParamRefs
    {
        ParamBinding level;
        ParamBinding tune;
        ParamBinding brightness;
        ParamBinding attack;
        ParamBinding decay;
        ParamBinding sustain;
        ParamBinding release;
        ParamBinding body;
        ParamBinding drive;
        ParamBinding pitchEnv;
        ParamBinding filterEnv;
        ParamBinding sub;
        ParamBinding character;
        ParamBinding cutoff;
        ParamBinding pan;
        ParamBinding resonance;
        ParamBinding pitchEnvTime;
        ParamBinding snap;
        ParamBinding envShape;
        ParamBinding output;
    };

    struct GlobalParamRefs
    {
        ParamBinding selectedBass;
        ParamBinding monoMode;
        ParamBinding glideTime;
        ParamBinding lfoRate;
        ParamBinding lfoDepth;
        ParamBinding lfoWave;
        ParamBinding lfoDest;
        ParamBinding outputGain;
        ParamBinding macroFatness;
        ParamBinding macroBrillance;
        ParamBinding macroPunch;
        ParamBinding macroDepth;
        ParamBinding modWheelTarget;
        ParamBinding satDrive;
        ParamBinding satMix;
        ParamBinding transientAttack;
        ParamBinding transientSustain;
        ParamBinding transientMix;
        ParamBinding compThreshold;
        ParamBinding compRatio;
        ParamBinding compAttack;
        ParamBinding compRelease;
        ParamBinding compMakeup;
        ParamBinding compMix;
        ParamBinding eqLowFreq;
        ParamBinding eqLowGain;
        ParamBinding eqMidFreq;
        ParamBinding eqMidGain;
        ParamBinding eqMidQ;
        ParamBinding eqHighFreq;
        ParamBinding eqHighGain;
        ParamBinding chorusRate;
        ParamBinding chorusDepth;
        ParamBinding chorusMix;
        ParamBinding delayTime;
        ParamBinding delayFeedback;
        ParamBinding delayMix;
        ParamBinding delaySync;
        ParamBinding delayNoteDiv;
        ParamBinding reverbSize;
        ParamBinding reverbDamping;
        ParamBinding reverbWidth;
        ParamBinding reverbMix;
        ParamBinding limiterThreshold;
        ParamBinding limiterRelease;
        ParamBinding fxSatEnable;
        ParamBinding fxTransientEnable;
        ParamBinding fxCompEnable;
        ParamBinding fxEqEnable;
        ParamBinding fxChorusEnable;
        ParamBinding fxDelayEnable;
        ParamBinding fxReverbEnable;
        ParamBinding fxLimiterEnable;
        ParamBinding velocityCurveParam;
        ParamBinding pitchBendRange;
        ParamBinding modLfo2Rate;
        ParamBinding modLfo2Wave;
        ParamBinding fxLock;
    };

    struct ModMatrixParamRefs
    {
        ParamBinding source;
        ParamBinding destination;
        ParamBinding amount;
    };

    struct GlobalBlockState
    {
        int selectedBass = 0;
        int monoMode = 0;
        float glideTime = 0.0f;
        float outputGainDb = 0.0f;
        float lfoRate = 0.0f;
        float lfoDepth = 0.0f;
        int lfoWave = 0;
        int lfoDest = 0;
        modmatrix::ModContext baseModContext;
        modmatrix::ModResult sharedModResult;
        mbs::GlobalFxSettings fx;
    };

    using LinearSmoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    struct GlobalFxSmoothers
    {
        LinearSmoother satDrive;
        LinearSmoother satMix;
        LinearSmoother transientAttack;
        LinearSmoother transientSustain;
        LinearSmoother transientMix;
        LinearSmoother compThreshold;
        LinearSmoother compRatio;
        LinearSmoother compAttack;
        LinearSmoother compRelease;
        LinearSmoother compMakeup;
        LinearSmoother compMix;
        LinearSmoother eqLowFreq;
        LinearSmoother eqLowGain;
        LinearSmoother eqMidFreq;
        LinearSmoother eqMidGain;
        LinearSmoother eqMidQ;
        LinearSmoother eqHighFreq;
        LinearSmoother eqHighGain;
        LinearSmoother chorusRate;
        LinearSmoother chorusDepth;
        LinearSmoother chorusMix;
        LinearSmoother delayTime;
        LinearSmoother delayFeedback;
        LinearSmoother delayMix;
        LinearSmoother reverbSize;
        LinearSmoother reverbDamping;
        LinearSmoother reverbWidth;
        LinearSmoother reverbMix;
        LinearSmoother limiterThreshold;
        LinearSmoother limiterRelease;
    };

    float getParamValue(const juce::String& paramId) const;
    float readCachedParamValue(const ParamBinding& binding, float fallback = 0.0f) const noexcept;
    void  setParamValue(const juce::String& paramId, float value);
    void  setParamValueInternal(const juce::String& paramId, float value, bool notifyHost);
    float sanitizeParameterValue(const juce::String& paramId, float value, float fallback, int* warningCount = nullptr) const;
    void  resolveParameterPointers();
    void  sanitizeAllParameters();
    mbs::BassSettings sanitizeBassSettings(int bassIndex, const mbs::BassSettings& settings) const;
    mbs::GlobalFxSettings sanitizeFxSettings(const mbs::GlobalFxSettings& fx) const;
    mbs::PatchPerformanceSettings sanitizePerformanceSettings(const mbs::PatchPerformanceSettings& settings) const;
    GlobalBlockState buildGlobalBlockState(int numSamples);
    void initializeGlobalFxSmoothers();
    void setGlobalFxSmootherTargets(const GlobalBlockState& state);
    GlobalBlockState advanceSmoothedGlobalBlockState(const GlobalBlockState& state, int numSamples);
    mbs::BassSettings snapshotBassSettings(int bassIndex) const;
    int  captureBassOutputBus(int bassIndex) const;
    mbs::GlobalFxSettings snapshotGlobalFxSettings() const;
    mbs::PatchPerformanceSettings snapshotPerformanceSettings() const;
    modmatrix::MatrixState captureModMatrixStateFromParams() const;
    void applyPerformanceMacros(int bassIndex, mbs::BassSettings& s) const;
    int  findFreeVoice() const;
    void triggerNoteOn(int bassIndex, int midiNote, float velocity);
    void triggerNoteOff(int bassIndex, int midiNote);
    void panicAllVoices();
    void releaseVoices(int midiChannel, bool immediate);
    void resetGlobalTailState() noexcept;
    void handleMidiCC(int ccNumber, int ccValue, int bassIndex);
    void updateGlobalEffectParameters(const GlobalBlockState& blockState);
    void processMasterFxChain(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalTransient(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalSaturator(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalCompressor(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalEQ(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalChorus(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalDelay(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalReverb(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void processGlobalLimiter(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void applyGlobalLfo(juce::AudioBuffer<float>& mainBuffer, const GlobalBlockState& blockState);
    void applyGlobalHpf(juce::AudioBuffer<float>& mainBuffer);
    static juce::File getDataRootDirectory();
    void loadFactoryOverrides();
    void applyGlobalFxSettings(const mbs::GlobalFxSettings& fx, bool notifyHost = false);
    void applyPerformanceSettings(const mbs::PatchPerformanceSettings& settings, bool notifyHost = false);
    void applyModMatrixStateToParams(const modmatrix::MatrixState& state, bool notifyHost = false);
    void syncModMatrixFromParams();
    void applyBassSettingsToParams(int bassIndex, const mbs::BassSettings& settings, bool notifyHost = false);
    bool writePresetManifest(const juce::File& presetFile, const juce::String& presetName,
                             int bassIndex, const juce::String& sourceModel) const;

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;
    std::array<BassParamRefs, mbs::kNumBasses> bassParamRefs {};
    GlobalParamRefs globalParamRefs {};
    std::array<ModMatrixParamRefs, modmatrix::kMaxSlots> modMatrixParamRefs {};

    // Per-bass preset banks (set once from factory data + user overrides)
    std::array<std::vector<mbs::InstrumentPreset>, mbs::kNumBasses> factoryPresetBanks;

    // Per-bass preset state (-1 = modified / user preset active)
    std::array<int,        mbs::kNumBasses> currentPresetIndices;
    std::array<juce::File, mbs::kNumBasses> currentUserPresetFiles;

    std::array<mbs::BassVoice, kMaxVoices + kMaxDyingVoices> voicePool;
    std::array<VoiceSlot, kMaxVoices> voices;
    uint64_t voiceAgeCounter = 0;

    juce::dsp::Compressor<float> compressor;
    juce::AudioBuffer<float> fxDryBuffer;
    juce::AudioBuffer<float> mainDryBuffer;
    juce::AudioBuffer<float> voiceRenderBuffer;
    juce::AudioBuffer<float> fxChunkBuffer;
    std::array<float, 2> transientFastEnv = { 0.0f, 0.0f };
    std::array<float, 2> transientSlowEnv = { 0.0f, 0.0f };
    double preparedSampleRate = 44100.0;
    float lfoPhase = 0.0f;
    float outputGainCurrent = juce::Decibels::decibelsToGain(-3.0f);
    std::array<int, mbs::kNumBasses> outputBusCache {};
    GlobalFxSmoothers globalFxSmoothers {};

    struct CompressorCache
    {
        float threshold =  1.0f;
        float ratio     = -1.0f;
        float attack    = -1.0f;
        float release   = -1.0f;
    } compCache;

    PitchBendState pitchBend;
    modmatrix::ModulationMatrix modulationMatrix;
    modmatrix::ModResult cachedModResult;
    VelocityCurve  velocityCurve = VelocityCurve::Linear;
    std::atomic<int> midiCCPage { 0 };  // FLkey Mini CC page (0..kNumCCPages-1)
    std::atomic<int> activeVoiceCountAtomic { 0 };

    // Mono mode state
    static constexpr int kMaxHeldNotes = 16;
    struct MonoState
    {
        int  voiceSlot = -1;
        int  heldNotes[kMaxHeldNotes] = {};
        int  heldCount = 0;

        void pushNote(int note) {
            if (heldCount < kMaxHeldNotes)
                heldNotes[heldCount++] = note;
        }
        void removeNote(int note) {
            for (int i = 0; i < heldCount; ++i) {
                if (heldNotes[i] == note) {
                    for (int j = i; j < heldCount - 1; ++j)
                        heldNotes[j] = heldNotes[j + 1];
                    --heldCount;
                    return;
                }
            }
        }
        int topNote() const { return heldCount > 0 ? heldNotes[heldCount - 1] : -1; }
    };
    std::array<MonoState, mbs::kNumBasses> monoStates{};

    // New FX processors
    mbs::fx::ParametricEQ3Band  eqProcessor;
    mbs::fx::StereoChorus       chorusProcessor;
    mbs::fx::StereoDelay        delayProcessor;
    mbs::fx::DattorroPlateReverb reverbProcessor;
    mbs::fx::OutputLimiter      limiterProcessor;

    // Global HPF state (2nd order Butterworth)
    float hpfX1[2] = {};   // input history per channel
    float hpfX2[2] = {};
    float hpfY1[2] = {};   // output history per channel
    float hpfY2[2] = {};

    // 2x oversampling for saturation
    juce::dsp::Oversampling<float> satOversampling { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false };

    // RT-safe deferred parameter updates from MIDI CC handlers
    struct PendingParamUpdate
    {
        juce::RangedAudioParameter* param = nullptr;
        float normalisedValue = 0.0f;
    };
    static constexpr int kFxControlChunkSize = 32;
    static constexpr int kPendingParamQueueSize = 32;
    juce::AbstractFifo pendingParamFifo { kPendingParamQueueSize };
    std::array<PendingParamUpdate, kPendingParamQueueSize> pendingParamQueue;
    void queueParamUpdate(juce::RangedAudioParameter* param, float normalisedValue);
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassSynthAudioProcessor)
};
