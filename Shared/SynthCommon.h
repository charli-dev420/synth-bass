#pragma once
// =============================================================================
// SynthCommon.h — Shared base for UWdeVST synth editors (bass, guitar, perc,
//                 piano, orch, instr).  NOT used by drum or hub.
//
// Provides:
//   • synthcol   — canonical dark palette tokens (accent excluded — per plugin)
//   • SynthLookAndFeel  — single LookAndFeel parameterised with accent colour
//   • SynthFamilyTab    — generic family-selector tab
//   • SynthPresetCard   — generic instrument-card component
//   • SynthEffectTab    — generic effect-section tab
//   • CommonSynthEditor — base AudioProcessorEditor with:
//       – shared preset-browser UI
//       – shared keyboard + octave buttons
//       – gain dial (APVTS "output_gain")
//       – drum-style paint helpers (grain, vignette, header breadcrumb, cards)
//       – dial setup helpers
//       – virtual preset-host bridge (implement in derived)
// =============================================================================
#include <JuceHeader.h>
#include <array>
#include <memory>
#include <functional>

// =============================================================================
// Canonical dark palette — Unicor SoundEngine charte graphique (§2.3 mode sombre)
// Neutral cool darks + clean white text, professional studio design language.
// =============================================================================
namespace synthcol
{
    inline const juce::Colour bg       { 0xff0D0F13 };  // lacquered dark canvas
    inline const juce::Colour surface  { 0xff171A20 };  // main panel surface
    inline const juce::Colour surfHi   { 0xff20242B };  // elevated smoked surface
    inline const juce::Colour border   { 0xff303742 };  // subtle cool border
    inline const juce::Colour text     { 0xffEAEAEA };  // clean bright text
    inline const juce::Colour textDim  { 0xff9BA3AE };  // muted service text
    inline const juce::Colour textSec  { 0xffD6DAE0 };  // secondary text
    inline const juce::Colour graphite { 0xff060608 };  // shadow offset
    inline const juce::Colour ink      { 0xff171A20 };  // border stroke
}

namespace synthAlpha
{
    inline constexpr float subtle = 0.024f;
    inline constexpr float soft   = 0.06f;
    inline constexpr float medium = 0.14f;
    inline constexpr float strong = 0.28f;
    inline constexpr float solid  = 0.52f;
}

namespace synthMetal
{
    inline const juce::Colour capHi   { 0xffCDD4DE };
    inline const juce::Colour capMid  { 0xff6B7580 };
    inline const juce::Colour capLo   { 0xff2A3038 };
    inline const juce::Colour hubHi   { 0xff8A95A4 };
    inline const juce::Colour hubLo   { 0xff343A44 };
    inline const juce::Colour bezelHi { 0xff353C47 };
    inline const juce::Colour bezelLo { 0xff101318 };
}

namespace synthStroke
{
    inline constexpr float thin   = 0.7f;
    inline constexpr float normal = 1.0f;
    inline constexpr float thick  = 1.4f;
}

namespace synthRadius
{
    inline constexpr float panel  = 10.0f;
    inline constexpr float card   = 8.0f;
    inline constexpr float tab    = 6.0f;
    inline constexpr float button = 4.0f;
}

namespace synthShadow
{
    inline constexpr float light = 2.0f;
    inline constexpr float deep  = 3.5f;
}

namespace synthGlow
{
    inline constexpr float normal   = 0.22f;
    inline constexpr float selected = 0.38f;
    inline constexpr float hover    = 0.30f;
}

namespace synthui
{
    enum class TooltipMode { Off, Short, Novice };

    struct InstrumentUiDef
    {
        const char* label = "";
        const char* shortTooltip = "";
        const char* noviceTooltip = "";
    };

    template <std::size_t N>
    using InstrumentUiProfile = std::array<InstrumentUiDef, N>;

    template <std::size_t N>
    using MacroLabelProfile = std::array<const char*, N>;

    struct PhysicalControlUiDef
    {
        const char* label = "";
        const char* suffix = nullptr;
        const char* shortTooltip = "";
        const char* noviceTooltip = "";
    };

    template <std::size_t N>
    using PhysicalControlUiProfile = std::array<PhysicalControlUiDef, N>;

    inline juce::String tooltipForMode(const InstrumentUiDef& def, TooltipMode mode)
    {
        switch (mode)
        {
            case TooltipMode::Short:  return juce::String(def.shortTooltip);
            case TooltipMode::Novice: return juce::String(def.noviceTooltip);
            case TooltipMode::Off:    break;
        }

        return {};
    }

    inline juce::String tooltipForMode(const PhysicalControlUiDef& def, TooltipMode mode)
    {
        switch (mode)
        {
            case TooltipMode::Short:  return juce::String(def.shortTooltip);
            case TooltipMode::Novice: return juce::String(def.noviceTooltip);
            case TooltipMode::Off:    break;
        }

        return {};
    }

    template <std::size_t N, typename LabelArray>
    inline void applyLabelProfile(const InstrumentUiProfile<N>& profile, LabelArray& labels)
    {
        for (std::size_t i = 0; i < N; ++i)
            labels[i].setText(profile[i].label, juce::dontSendNotification);
    }

    template <std::size_t N, typename LabelArray>
    inline void applyMacroLabelProfile(const MacroLabelProfile<N>& profile, LabelArray& labels)
    {
        for (std::size_t i = 0; i < N; ++i)
            labels[i].setText(profile[i], juce::dontSendNotification);
    }

    template <std::size_t N, typename DialArray>
    inline void applyTooltipProfile(const InstrumentUiProfile<N>& profile, DialArray& dials, TooltipMode mode)
    {
        for (std::size_t i = 0; i < N; ++i)
            dials[i].setTooltip(tooltipForMode(profile[i], mode));
    }
}

// =============================================================================
// SynthLookAndFeel
// =============================================================================
class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum ColourIds
    {
        knobGlowColourId      = 0x2721001,
        knobBezelColourId     = 0x2721002,
        knobCollarColourId    = 0x2721003,
        knobCapAccentColourId = 0x2721004
    };

    explicit SynthLookAndFeel(juce::Colour accent);
    void setAccent(juce::Colour accent);

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour&, bool hi, bool dn) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool,
                      int, int, int, int, juce::ComboBox&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool highlighted, bool down) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

private:
    juce::Colour accent_;
};

// =============================================================================
// SynthFamilyTab — accent-bar underline style
// =============================================================================
class SynthFamilyTab : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    void configure(int familyIndex, const juce::String& name, juce::Colour col);
    void setSelected(bool s);
    void setIcon(juce::Image img);

    std::function<void(int)> onClicked;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    int          idx   = 0;
    juce::String name;
    juce::Colour col   { 0xff888888 };
    bool         sel   = false;
    bool         hover = false;
    juce::Image  icon;
};

// =============================================================================
// SynthPresetCard — image card with icon placeholder
// =============================================================================
class SynthPresetCard : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    void configure(int instrIndex, const juce::String& name, juce::Colour catCol);
    void setSelected(bool s);
    void setIcon(juce::Image img);

    std::function<void(int)> onClicked;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    int          idx   = 0;
    juce::String name;
    juce::Colour cat   { 0xff888888 };
    bool         sel   = false;
    bool         hover = false;
    juce::Image  icon;
};

// =============================================================================
// AmberShakeButton — TextButton with brief amber-highlight jitter on mouseEnter
// =============================================================================
class AmberShakeButton : public juce::TextButton, private juce::Timer
{
public:
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
private:
    void timerCallback() override;
    int shakePhase_ = 0;
};

// =============================================================================
// SynthEffectTab — small clickable tab for effect sections
// =============================================================================
class SynthEffectTab : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
    void configure(int tabIndex, const juce::String& name, juce::Colour accentCol);
    void setAccent(juce::Colour accentCol);
    void setSelected(bool s);

    std::function<void(int)> onClicked;

    void paint(juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    int          idx        = 0;
    juce::String name;
    juce::Colour accent { 0xff888888 };
    bool         sel        = false;
    bool         hover      = false;
    int          shakePhase_= 0;
};

// =============================================================================
// SynthFxRackItem — compact vertical FX rack entry with on/off state
// =============================================================================
class SynthFxRackItem : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    void configure(int itemIndex, const juce::String& name, const juce::String& summary, juce::Colour accentCol);
    void setAccent(juce::Colour accentCol);
    void setSelected(bool s);
    void setEnabledState(bool s);

    std::function<void(int)> onClicked;

    void paint(juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int          idx          = 0;
    juce::String name;
    juce::String summary;
    juce::Colour accent { 0xff888888 };
    bool         sel          = false;
    bool         hover        = false;
    bool         enabledState = true;
};

// =============================================================================
// EnvelopeDisplay — interactive ADSR curve linked to existing sliders
// =============================================================================
class EnvelopeDisplay : public juce::Component,
                        private juce::Slider::Listener
{
public:
    ~EnvelopeDisplay() override;

    void setAccent(juce::Colour c) { accent_ = c; repaint(); }
    void setTitle(const juce::String& t) { title_ = t; repaint(); }
    void bindAdsr(juce::Slider* attack,
                  juce::Slider* decay,
                  juce::Slider* sustain,
                  juce::Slider* release);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    enum class DragTarget { None, Attack, DecaySustain, Release };

    void sliderValueChanged(juce::Slider*) override { repaint(); }
    void addSliderListener(juce::Slider* s);
    void removeSliderListener(juce::Slider* s);
    DragTarget hitTestTarget(juce::Point<float> position, bool allowRegionFallback) const;
    void updateCursor(DragTarget target);
    float getNorm(juce::Slider* s) const;
    void  setNorm(juce::Slider* s, float n) const;
    juce::Rectangle<float> getPlotBounds() const;

    juce::Slider* attack_  = nullptr;
    juce::Slider* decay_   = nullptr;
    juce::Slider* sustain_ = nullptr;
    juce::Slider* release_ = nullptr;
    juce::Colour  accent_  = juce::Colour(0xffD4A017);
    juce::String  title_   = "ENVELOPE";
    DragTarget    drag_    = DragTarget::None;
    DragTarget    hover_   = DragTarget::None;
};

// =============================================================================
// LfoModulationDisplay — animated LFO waveform + direct rate/depth control
// =============================================================================
class LfoModulationDisplay : public juce::Component, private juce::Timer
{
public:
    LfoModulationDisplay();

    void setAccent(juce::Colour c) { accent_ = c; repaint(); }
    void setTitle(const juce::String& t) { title_ = t; repaint(); }
    void bindRateDepth(juce::Slider* rate, juce::Slider* depth);
    void setWaveformIndex(int idx);
    int  getWaveformIndex() const { return static_cast<int>(waveform_); }
    std::function<void(int)> onWaveformChanged;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    enum class Waveform { Sine = 0, Triangle, Saw, Square };

    void timerCallback() override;
    float getRateNorm() const;
    float getDepthNorm() const;
    void  setRateNorm(float n);
    void  setDepthNorm(float n);
    float sampleWave(float phase) const;
    juce::Rectangle<float> getHeaderBounds() const;
    juce::Rectangle<float> getWaveChipBounds(int index) const;
    int  hitTestWaveformChip(juce::Point<float> position) const;
    juce::Rectangle<float> getPlotBounds() const;

    juce::Slider* rate_    = nullptr;
    juce::Slider* depth_   = nullptr;
    float         rateMem_ = 0.35f;
    float         depthMem_= 0.50f;
    float         phase_   = 0.0f;
    Waveform      waveform_= Waveform::Sine;
    juce::Colour  accent_  = juce::Colour(0xffD4A017);
    juce::String  title_   = "LFO";
    int           hoverWaveIndex_ = -1;
    bool          hoverPlot_ = false;
};

// =============================================================================
// PlayableRangeKeyboard — shared keyboard with out-of-range note shading
// =============================================================================
class PlayableRangeKeyboard : public juce::MidiKeyboardComponent
{
public:
    PlayableRangeKeyboard(juce::MidiKeyboardState& state,
                          juce::MidiKeyboardComponent::Orientation orientation);

    void setPlayableRange(int lowNote, int highNote);

    void drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                       bool isDown, bool isOver, juce::Colour lineColour,
                       juce::Colour textColour) override;
    void drawBlackNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                       bool isDown, bool isOver, juce::Colour noteFillColour) override;

private:
    bool isPlayable(int midiNoteNumber) const;

    int playableLow_  = 0;
    int playableHigh_ = 127;
};

// =============================================================================
// CommonSynthEditor
// =============================================================================
class CommonSynthEditor : public juce::AudioProcessorEditor
{
public:
    // -------------------------------------------------------------------------
    // Derived classes MUST implement these pure virtuals
    // -------------------------------------------------------------------------

    // Plugin identity (for header breadcrumb)
    virtual juce::String pluginNamespace() const = 0;  // e.g. "MIS"
    virtual juce::String pluginTitle()     const = 0;  // e.g. "Bass Synth"

    // Preset host bridge — forward each to proc.xxxx():
    virtual juce::StringArray       hostGetFactoryNames()             = 0;
    virtual juce::Array<juce::File> hostScanUserPresets()             = 0;
    virtual bool                    hostIsUserPreset()                = 0;
    virtual juce::File              hostCurrentUserFile()             = 0;
    virtual int                     hostCurrentFactoryIdx()           = 0;
    virtual void                    hostApplyFactory(int idx)         = 0;
    virtual void                    hostLoadUser(const juce::File&)   = 0;
    virtual bool                    hostSaveUser(const juce::String&) = 0;
    virtual void                    hostUpdateUser(const juce::File&) = 0;
    virtual void                    hostSaveFactory(int idx)          = 0;
    virtual void                    hostDeleteUser(const juce::File&) = 0;
    virtual juce::File              hostGetUserPresetsDir()           = 0;
    virtual juce::File              hostGetUserPresetsDirForIndex(int instrumentIndex) = 0;
    virtual juce::String            hostPresetInstrumentAttr() const  = 0;

protected:
    // -------------------------------------------------------------------------
    // Constructor
    //   accent      – per-plugin accent colour
    //   apvts       – processor's APVTS (for gain attachment)
    //   kbState     – processor's MidiKeyboardState
    //   kbLow/High  – initial keyboard MIDI range
    //   kbKeyWidth  – key width in pixels
    // -------------------------------------------------------------------------
    CommonSynthEditor(juce::AudioProcessor&             proc,
                      juce::AudioProcessorValueTreeState& apvts,
                      juce::MidiKeyboardState&            kbState,
                      juce::Colour                        accent,
                      int                                 kbLow      = 36,
                      int                                 kbHigh     = 96,
                      float                               kbKeyWidth = 36.0f);

    ~CommonSynthEditor() override;

    // -------------------------------------------------------------------------
    // Call this at the END of the derived constructor to wire preset box
    // callbacks and start the timer.
    // -------------------------------------------------------------------------
    void initCommon();

    // -------------------------------------------------------------------------
    // Call from derived timerCallback() to keep preset-box in sync
    // -------------------------------------------------------------------------
    void syncPresetBox();

    // -------------------------------------------------------------------------
    // Paint helpers — call from derived paint()
    // -------------------------------------------------------------------------

    /// Fills bg + procedural grain overlay + radial vignette
    void paintBackground(juce::Graphics& g) const;

    /// Draws semi-transparent header bar + bottom border + breadcrumb text
    void paintHeader(juce::Graphics& g, int headerH) const;

    /// Draws a surface-coloured rounded card with title and horizontal divider
    void paintCard(juce::Graphics& g,
                   int x, int y, int cw, int ch,
                   const juce::String& title) const;

    /// Draws a section label with small accent liser + optional panel bg
    void paintSection(juce::Graphics& g,
                      int x, int y, int w, int h,
                      const juce::String& label,
                      bool fillPanel = true) const;

    /// Draws the lower keyboard dock with recessed 3D framing
    void paintKeyboardDock(juce::Graphics& g,
                           int x, int y, int w, int h) const;

    /// Draws a compact metallic status plaque for synth-specific header chrome.
    void paintStatusChip(juce::Graphics& g,
                         juce::Rectangle<int> area,
                         const juce::String& text,
                         juce::Colour fill,
                         juce::Colour outline) const;

    /// Draws a flatter, more explicit mode pill distinct from passive status badges.
    void paintModePill(juce::Graphics& g,
                       juce::Rectangle<int> area,
                       const juce::String& text,
                       juce::Colour fill,
                       juce::Colour outline) const;

    /// Draws a segmented micro-meter used as decorative/utilitarian header chrome.
    void paintMeterBar(juce::Graphics& g,
                       juce::Rectangle<int> area,
                       float level,
                       juce::Colour colour) const;

    // -------------------------------------------------------------------------
    // Dial setup helpers
    // -------------------------------------------------------------------------
    static void setupDial(juce::Slider& s, juce::Colour fill);
    static void setupSmallDial(juce::Slider& s, juce::Colour fill);

    /// suffix — e.g. " Hz", " st" — appended to textbox value
    static void setupGrandDial(juce::Slider& s, juce::Colour fill,
                               const juce::String& suffix = {});

    void setAccentTheme(juce::Colour accent);
    void setChromePalette(juce::Colour headerTint,
                          juce::Colour panelBaseTint,
                          juce::Colour panelCavityTint,
                          juce::Colour panelHeaderTint,
                          juce::Colour keyboardTint = juce::Colours::transparentBlack);
    void setHeaderLogo(juce::Image logo);
    void setKeyboardPlayableRange(int lowNote, int highNote);

    // -------------------------------------------------------------------------
    // Preset management — implemented here, delegating to host* virtuals
    // -------------------------------------------------------------------------
    virtual juce::String hostFormatFactoryPresetLabel(int presetIndex,
                                                      const juce::String& displayName) const
    {
        juce::ignoreUnused(presetIndex);
        return displayName;
    }

    virtual juce::String hostFormatUserPresetLabel(const juce::File& presetFile,
                                                   const juce::String& displayName) const
    {
        juce::ignoreUnused(presetFile);
        return displayName;
    }

    virtual juce::String hostFactoryPresetSearchText(int presetIndex,
                                                     const juce::String& displayName) const
    {
        juce::ignoreUnused(presetIndex);
        return displayName;
    }

    virtual juce::String hostUserPresetSearchText(const juce::File& presetFile,
                                                  const juce::String& displayName) const
    {
        juce::ignoreUnused(presetFile);
        return displayName;
    }

    virtual bool hostShouldIncludeFactoryPreset(int presetIndex) const
    {
        juce::ignoreUnused(presetIndex);
        return true;
    }

    virtual bool hostShouldIncludeUserPreset(const juce::File& presetFile) const
    {
        juce::ignoreUnused(presetFile);
        return true;
    }

    void refreshPresetList();
    void applyPresetFilter(const juce::String& query);
    void showSaveAsDialog(const juce::String& defaultName = "Mon Preset");
    void saveCurrentPreset();
    void deleteCurrentUserPreset();
    void navigatePreset(int direction);
    void importPresetsFromZip();

    // -------------------------------------------------------------------------
    // Common chrome layout helper
    // Returns the geometry so derived resized() can continue from bodyY.
    // -------------------------------------------------------------------------
    struct ChromeLayout
    {
        int headerH;      // height of header bar
        int selectorY;    // top of selector row
        int selectorH;    // height of selector row
        int bodyY;        // top of instrument-specific body area
        int kbY;          // top of keyboard strip
        int kbH;          // keyboard strip height
        int margin;       // horizontal page margin
        int gutter;       // column gutter
    };

    struct HeaderZones
    {
        juce::Rectangle<int> headerBounds;
        juce::Rectangle<int> contentBounds;
        juce::Rectangle<int> identityZone;
        juce::Rectangle<int> presetZone;
        juce::Rectangle<int> statusZone;
        juce::Rectangle<int> presetPrimaryRow;
        juce::Rectangle<int> presetSecondaryRow;
        juce::Rectangle<int> statusPrimaryRow;
        juce::Rectangle<int> statusSecondaryRow;
    };

    /// Positions preset-bar widgets, gain dial, keyboard.
    /// Call resizeChrome() first, then position plugin-specific knobs.
    ChromeLayout resizeChrome(int presetsStartX = 260,
                              int selectorH     = 66,
                              int kbH           = 110);

    /// Returns the shared three-zone header geometry used by V2 layouts.
    HeaderZones computeHeaderZones(int headerH) const;

    // -------------------------------------------------------------------------
    // Members shared with derived classes
    // -------------------------------------------------------------------------
    juce::Colour         accent_;
    SynthLookAndFeel     lnf_;
    juce::Image          bgTexture_;        // procedural grain (generated in ctor)
    juce::Image          backgroundImage_;  // per-synth background fond (loaded by derived ctor)
    juce::Image          headerLogoImage_;  // optional plugin logo shown in header
    struct ChromePalette
    {
        juce::Colour headerTint;
        juce::Colour panelBaseTint;
        juce::Colour panelCavityTint;
        juce::Colour panelHeaderTint;
        juce::Colour keyboardTint;
    } chromePalette_;

    // Preset bar
    juce::ComboBox       presetBox;
    juce::TextEditor     presetSearch;
    juce::TextButton     prevPresetBtn, nextPresetBtn;
    AmberShakeButton     savePresetBtn, saveAsPresetBtn, deletePresetBtn;
    AmberShakeButton     importPresetsBtn;

    // Gain (APVTS bound to "output_gain")
    juce::Slider         gainDial;

    // Optional single-note toggle (add to parent if plugin uses it)
    juce::ToggleButton   singleBtn;

    // Instrument selector (family + model combos)
    juce::Label          familySelectorLbl, modelSelectorLbl;
    juce::ComboBox       familySelector, modelSelector;

    // Keyboard
    std::unique_ptr<PlayableRangeKeyboard> keyboard;
    juce::TextButton     octaveDownBtn, octaveUpBtn;

private:
    using APVTS          = juce::AudioProcessorValueTreeState;
    using SliderAttach   = APVTS::SliderAttachment;

    std::unique_ptr<SliderAttach> gainAtt_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    int               factoryPresetCount_ = 0;
    juce::StringArray factoryPresetNames_;
    juce::Array<juce::File> userPresetFiles_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommonSynthEditor)
};
