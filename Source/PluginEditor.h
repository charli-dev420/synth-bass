#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "../Shared/SynthCommon.h"

// =============================================================================
// Bass Synth editor — inherits CommonSynthEditor from Shared/SynthCommon.h
// =============================================================================
class BassSynthAudioProcessorEditor : public CommonSynthEditor,
                                      private juce::Timer
{
public:
    explicit BassSynthAudioProcessorEditor(BassSynthAudioProcessor&);

    // --- CommonSynthEditor pure virtuals ---
    juce::String            pluginNamespace()  const override { return {}; }
    juce::String            pluginTitle()      const override { return "UWdeVST Bass"; }
    juce::StringArray       hostGetFactoryNames()              override;
    juce::Array<juce::File> hostScanUserPresets()              override;
    bool                    hostIsUserPreset()                 override;
    juce::File              hostCurrentUserFile()              override;
    int                     hostCurrentFactoryIdx()            override;
    void                    hostApplyFactory(int idx)          override;
    void                    hostLoadUser(const juce::File& f)  override;
    bool                    hostSaveUser(const juce::String& n)override;
    void                    hostUpdateUser(const juce::File& f)override;
    void                    hostSaveFactory(int idx)           override;
    void                    hostDeleteUser(const juce::File& f)override;
    juce::File              hostGetUserPresetsDir()            override;
    juce::File              hostGetUserPresetsDirForIndex(int instrumentIndex) override;
    juce::String            hostPresetInstrumentAttr() const   override;
    juce::String            hostFormatFactoryPresetLabel(int presetIndex,
                                                         const juce::String& displayName) const override;
    juce::String            hostFactoryPresetSearchText(int presetIndex,
                                                        const juce::String& displayName) const override;
    juce::String            hostFormatUserPresetLabel(const juce::File& presetFile,
                                                      const juce::String& displayName) const override;
    juce::String            hostUserPresetSearchText(const juce::File& presetFile,
                                                     const juce::String& displayName) const override;

    void paint(juce::Graphics&) override;
    void resized() override;

#if defined(UWDEVST_BASS_TEST_BUILD)
    struct LayoutSnapshot
    {
        bool compact = false;
        juce::Rectangle<int> editorBounds;
        juce::Rectangle<int> headerBounds;
        juce::Rectangle<int> selectorPanelBounds;
        juce::Rectangle<int> statusPrimaryBounds;
        juce::Rectangle<int> statusSecondaryBounds;
        juce::Rectangle<int> gainSlotBounds;
        juce::Rectangle<int> gainBounds;
        juce::Rectangle<int> randBounds;
        juce::Rectangle<int> tooltipBounds;
        juce::Rectangle<int> voiceBounds;
        juce::Rectangle<int> ccBounds;
        juce::Rectangle<int> fxLockBounds;
        juce::Rectangle<int> keyboardBounds;
        juce::Rectangle<int> rightPanelBounds;
        juce::Rectangle<int> performanceStripBounds;
        juce::Rectangle<int> pitchBendRangeBounds;
        juce::Rectangle<int> glideTimeBounds;
        juce::Rectangle<int> modWheelTargetBounds;
        juce::Rectangle<int> lfoVisualBounds;
        juce::Rectangle<int> cutoffBounds;
        juce::Rectangle<int> resonanceBounds;
        juce::Rectangle<int> monoModeBounds;
        juce::String cutoffDisplayText;
        juce::String glideTimeDisplayText;
        juce::Rectangle<int> familyLabelBounds;
        juce::Rectangle<int> familyTabsBounds;
        juce::Rectangle<int> modelLabelBounds;
        juce::Rectangle<int> modelSelectorBounds;
        bool fxLockVisible = false;
        bool lfoVisualVisible = false;
    };

    LayoutSnapshot captureLayoutSnapshotForTests() const;
    void setRightPanelSectionForTests(int sectionIndex);
    void refreshBassUiForTests();
    bool isEnvControlVisibleForTests(int index) const;
    juce::String getEnvControlLabelForTests(int index) const;
    juce::String getMacroLabelForTests(int index) const;
    juce::String getMacroHintForTests() const;
    juce::String getMacroGuardrailForTests() const;
    juce::String getFactoryPresetBrowserLabelForTests(int presetIndex) const;
    juce::String getFactoryPresetBrowserSearchTextForTests(int presetIndex) const;
#endif

private:
    using APVTS          = juce::AudioProcessorValueTreeState;
    using SliderAttach   = APVTS::SliderAttachment;
    using ComboBoxAttach = APVTS::ComboBoxAttachment;

    struct CtrlDef { const char* label; const char* suffix; };
    struct FxDef   { const char* label; const char* paramId; };

    void timerCallback() override;
    void rebuildBassAttachments();
    void rebuildModelSelectorForFamily(int familyIndex, int preferredBass = -1);
    void syncSelectionUiFromBass();
    bool isFxTabAvailable(int tabIndex) const;
    int  firstAvailableFxTab() const;
    void syncFxAvailability();
    void switchEffectTab(int tabIndex);
    void syncFxRackState();
    void switchRightPanelSection(int sectionIndex);
    void syncFamilyControlVisibility();
    void updateMacroContext(int bassIndex);
    int  selectedBassFromParam() const;
    void applyBassTheme(int bassIndex);
    void configureValueDisplays();
    void updateKeyboardPresentation();

    struct VisualLayoutSnapshot
    {
        bool compact = false;
        bool roomy = false;
        int headerH = 0;
        int contentX = 0;
        int contentW = 0;
        int selectorY = 0;
        int selectorH = 0;
        int bodyY = 0;
        int bodyH = 0;
        int kbY = 0;
        int kbH = 0;
        int col1X = 0;
        int col2X = 0;
        int col3X = 0;
        int colW = 0;
        HeaderZones headerZones;
        juce::Rectangle<int> headerBounds;
        juce::Rectangle<int> selectorPanelBounds;
        juce::Rectangle<int> statusPrimaryRow;
        juce::Rectangle<int> statusSecondaryRow;
        juce::Rectangle<int> gainSlotBounds;
        int gainSize = 0;
        int statusReserve = 0;
    };

    VisualLayoutSnapshot computeVisualLayoutSnapshot(int width, int height) const;

    static juce::Colour familyColour(int familyIndex);
    static juce::Colour bassCatColour(int bassIndex);

    BassSynthAudioProcessor& proc;

    static constexpr int kEnvN         = 14;
    static constexpr int kFxN          = 30;
    static constexpr int kMacroTotal   = 4;
    static constexpr int kMacroVisible = 4;
    static constexpr int kFxPerTab     = 7;
    static constexpr int kFxTabs       = 8;
    static constexpr int kRightPanelSections = 3;

    std::array<SynthFamilyTab,  mbs::kNumFamilies> familyTabs;
    std::array<SynthPresetCard, mbs::kNumBasses>   presetCards;

    juce::ComboBox bassSelector;
    std::unique_ptr<ComboBoxAttach> selBassAtt;

    std::array<juce::Slider, kEnvN> envDials;
    std::array<juce::Label,  kEnvN> envLabels;
    std::array<std::unique_ptr<SliderAttach>, kEnvN> envAttach;
    EnvelopeDisplay envVisual;
    LfoModulationDisplay lfoVisual;
    juce::Slider lfoRateDial, lfoDepthDial;
    juce::ComboBox lfoWaveSelector;
    juce::ComboBox lfoDestSelector;
    juce::Label    lfoDestLabel, lfoWaveLabel, lfoRateLabel, lfoDepthLabel;
    juce::Slider glideTimeDial, resonanceDial;
    juce::ComboBox monoModeSelector;
    juce::Label glideTimeLabel, resonanceLabel, monoModeLabel;
    std::unique_ptr<SliderAttach> lfoRateAtt, lfoDepthAtt;
    std::unique_ptr<SliderAttach> glideTimeAtt, resonanceAtt;
    std::unique_ptr<ComboBoxAttach> lfoWaveAtt;
    std::unique_ptr<ComboBoxAttach> lfoDestAtt;
    std::unique_ptr<ComboBoxAttach> monoModeAtt;

    // ── Mod Matrix Panel ───────────────────────────────────────────────
    struct ModMatrixRow {
        juce::ComboBox srcCombo;
        juce::ComboBox dstCombo;
        juce::Slider   amtSlider;
    };
    std::array<ModMatrixRow, 8> modRows;
    std::array<std::unique_ptr<ComboBoxAttach>, 8> modSrcAtts;
    std::array<std::unique_ptr<ComboBoxAttach>, 8> modDstAtts;
    std::array<std::unique_ptr<SliderAttach>, 8> modAmtAtts;
    juce::Label modLfo2RateLabel;
    juce::Label modLfo2WaveLabel;
    juce::Slider modLfo2RateDial;
    juce::ComboBox modLfo2WaveSelector;
    std::unique_ptr<SliderAttach> modLfo2RateAtt;
    std::unique_ptr<ComboBoxAttach> modLfo2WaveAtt;
    juce::Label modMatrixTitle;

    std::array<juce::Slider, kMacroTotal> macroDials;
    std::array<juce::Label,  kMacroTotal> macroLbls;
    std::array<std::unique_ptr<SliderAttach>, kMacroTotal> macroAtt;
    juce::Label macroHintLabel;
    juce::Label macroGuardrailLabel;
    std::array<SynthEffectTab, kRightPanelSections> rightPanelTabs;

    std::array<juce::Slider, kFxN> fxDials;
    std::array<juce::Label,  kFxN> fxLbls;
    std::array<std::unique_ptr<SliderAttach>, kFxN> fxAtt;
    std::array<SynthFxRackItem, kFxTabs> fxRackItems;
    std::array<juce::ToggleButton, kFxTabs> fxBypassBtns;
    using BtnAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::array<std::unique_ptr<BtnAttach>, kFxTabs> fxBypassAtts;
    juce::Label fxDetailTitle;
    juce::Label fxUnavailableLbl;

    int activeFamilyIndex = 0;
    int activeFxTab       = -1;
    int activeRightPanelSection = 0;
    int cachedBassIdx     = -1;

    static constexpr int kFxTabMap[kFxTabs][kFxPerTab] = {
        {  0,  1, -1, -1, -1, -1, -1 },   // Tab 0: Sat (Drive, Mix)
        {  2,  3,  4, -1, -1, -1, -1 },   // Tab 1: Transient (Attack, Sustain, Mix)
        {  5,  6,  7,  8,  9, 10, -1 },   // Tab 2: Comp (Threshold, Ratio, Attack, Release, Makeup, Mix)
        { 11, 12, 13, 14, 15, 16, 17 },   // Tab 3: EQ (LowF, LowG, MidF, MidG, MidQ, HighF, HighG)
        { 18, 19, 20, -1, -1, -1, -1 },   // Tab 4: Chorus (Rate, Depth, Mix)
        { 21, 22, 23, -1, -1, -1, -1 },   // Tab 5: Delay (Time, Feedback, Mix)
        { 24, 25, 26, 27, -1, -1, -1 },   // Tab 6: Reverb (Size, Damping, Width, Mix)
        { 28, 29, -1, -1, -1, -1, -1 }    // Tab 7: Limiter (Threshold, Release)
    };

    static const std::array<CtrlDef, kEnvN>       kEnvCtrls;
    static const std::array<FxDef,   kMacroTotal> kMacroCtrls;
    static const std::array<FxDef,   kFxN>        kFxCtrls;

    static const char* kFxTabNames[kFxTabs];
    static const char* kFxRackSummaries[kFxTabs];
    static const char* kFxBypassParamIds[kFxTabs];

    // ── Performance controls ───────────────────────────────────────────
    juce::Label    velocityCurveLabel;
    juce::ComboBox velocityCurveSelector;
    juce::Label    modWheelTargetLabel;
    juce::ComboBox modWheelTargetSelector;
    juce::Label    pitchBendRangeLabel;
    juce::Slider   pitchBendRangeDial;
    std::unique_ptr<ComboBoxAttach> velocityCurveAtt;
    std::unique_ptr<ComboBoxAttach> modWheelTargetAtt;
    std::unique_ptr<SliderAttach>   pitchBendRangeAtt;
    juce::TextButton randButton;
    juce::Label      voiceCountLabel;

    // ── Delay BPM sync ─────────────────────────────────────────────────
    static constexpr int kDelayFxTab = 5;
    juce::ToggleButton delaySyncButton;
    juce::ComboBox     delayNoteDivSelector;
    juce::Label        delayNoteDivLabel;
    std::unique_ptr<BtnAttach>      delaySyncAtt;
    std::unique_ptr<ComboBoxAttach> delayNoteDivAtt;

    // ── FX Lock ────────────────────────────────────────────────────────
    juce::ToggleButton fxLockButton;
    std::unique_ptr<BtnAttach> fxLockAtt;

    // ── MIDI CC page indicator (FLkey Mini) ────────────────────────────
    juce::Label midiCCPageLabel;
    int cachedMidiCCPage = -1;

    // ── Tooltip mode system ────────────────────────────────────────────
    enum class TooltipMode { Off, Short, Novice };
    TooltipMode tooltipMode = TooltipMode::Short;

    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::TextButton    tooltipModeBtn;

    void cycleTooltipMode();
    void applyTooltips();

    static constexpr int kTooltipCount = kEnvN + 2 + kMacroTotal + kFxN + 1; // env(14)+lfo(2)+macro(4)+fx(30)+gain(1) = 51
    static const char* kTooltipsShort[kTooltipCount];
    static const char* kTooltipsNovice[kTooltipCount];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BassSynthAudioProcessorEditor)
};
