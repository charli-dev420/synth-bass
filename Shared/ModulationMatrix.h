#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

// =============================================================================
// ModulationMatrix — configurable source→destination modulation routing.
// Shared across all synths. Works per-block in the audio thread.
//
// Sources: LFO1, LFO2, Envelope, Velocity, ModWheel, Aftertouch, PitchBend
// Destinations: Cutoff, Resonance, Pan, Level, Pitch, Attack, Decay, LFO1Rate
//
// Each slot: source → destination → amount (-1..+1)
// =============================================================================
namespace modmatrix
{

enum class Source : int
{
    None = 0,
    LFO1,
    LFO2,
    Envelope,     // Main amplitude envelope (0..1)
    Velocity,     // Note velocity (0..1, per voice)
    ModWheel,     // CC 1 (0..1)
    Aftertouch,   // Channel pressure (0..1)
    PitchBend,    // Pitch wheel (-1..+1)
    Count
};

enum class Destination : int
{
    None = 0,
    Cutoff,       // Filter cutoff frequency (multiplicative)
    Resonance,    // Filter resonance
    Pan,          // Stereo panning offset
    Level,        // Output level (multiplicative)
    Pitch,        // Pitch offset in semitones
    AttackTime,   // Envelope attack time scale
    DecayTime,    // Envelope decay time scale
    LFO1Rate,     // LFO1 rate modulation
    EqMidFreq,    // EQ mid frequency sweep (±2 octaves)
    EqMidGain,    // EQ mid gain (±12 dB)
    Count
};

constexpr int kMaxSlots = 8;
constexpr int kSourceCount = static_cast<int>(Source::Count);
constexpr int kDestCount   = static_cast<int>(Destination::Count);

inline const char* getSourceName(Source s)
{
    switch (s)
    {
        case Source::None:       return "None";
        case Source::LFO1:       return "LFO 1";
        case Source::LFO2:       return "LFO 2";
        case Source::Envelope:   return "Envelope";
        case Source::Velocity:   return "Velocity";
        case Source::ModWheel:   return "Mod Wheel";
        case Source::Aftertouch: return "Aftertouch";
        case Source::PitchBend:  return "Pitch Bend";
        default:                 return "?";
    }
}

inline const char* getDestinationName(Destination d)
{
    switch (d)
    {
        case Destination::None:       return "None";
        case Destination::Cutoff:     return "Cutoff";
        case Destination::Resonance:  return "Resonance";
        case Destination::Pan:        return "Pan";
        case Destination::Level:      return "Level";
        case Destination::Pitch:      return "Pitch";
        case Destination::AttackTime: return "Attack";
        case Destination::DecayTime:  return "Decay";
        case Destination::LFO1Rate:   return "LFO Rate";
        case Destination::EqMidFreq:  return "EQ Mid Freq";
        case Destination::EqMidGain:  return "EQ Mid Gain";
        default:                      return "?";
    }
}

// =============================================================================
// ModSlot — one routing slot
// =============================================================================
struct ModSlot
{
    Source      source      = Source::None;
    Destination destination = Destination::None;
    float       amount      = 0.0f; // -1..+1
};

struct MatrixState
{
    int pitchBendRange = 2;
    float lfo2Rate = 2.0f;
    int lfo2Wave = 0;
    std::array<ModSlot, kMaxSlots> slots {};
};

// =============================================================================
// ModContext — per-sample source values, filled before matrix processing
// =============================================================================
struct ModContext
{
    float lfo1       = 0.0f;  // -1..+1
    float lfo2       = 0.0f;  // -1..+1
    float envelope   = 0.0f;  // 0..1
    float velocity   = 0.0f;  // 0..1 (per voice)
    float modWheel   = 0.0f;  // 0..1
    float aftertouch = 0.0f;  // 0..1
    float pitchBend  = 0.0f;  // -1..+1

    float getSource(Source s) const
    {
        switch (s)
        {
            case Source::LFO1:       return lfo1;
            case Source::LFO2:       return lfo2;
            case Source::Envelope:   return envelope;
            case Source::Velocity:   return velocity;
            case Source::ModWheel:   return modWheel;
            case Source::Aftertouch: return aftertouch;
            case Source::PitchBend:  return pitchBend;
            default:                 return 0.0f;
        }
    }
};

// =============================================================================
// ModResult — accumulated modulation amounts per destination
// =============================================================================
struct ModResult
{
    float cutoffMul     = 1.0f; // Multiplicative: 0.25..4.0 range
    float resonance     = 0.0f; // Additive: -1..+1
    float pan           = 0.0f; // Additive: -1..+1
    float levelMul      = 1.0f; // Multiplicative: 0..2
    float pitchSemi     = 0.0f; // Additive: semitones
    float attackScale   = 1.0f; // Multiplicative: 0.25..4.0
    float decayScale    = 1.0f; // Multiplicative: 0.25..4.0
    float lfo1RateMul   = 1.0f; // Multiplicative: 0.25..4.0
    float eqMidFreqAdd  = 0.0f; // Octave offset for mid freq (±2 oct)
    float eqMidGainAdd  = 0.0f; // dB offset for mid gain (±12 dB)

    void addDestination(Destination d, float modValue)
    {
        switch (d)
        {
            case Destination::Cutoff:
                cutoffMul *= std::exp2(modValue * 2.0f); // ±2 octaves
                break;
            case Destination::Resonance:
                resonance += modValue;
                break;
            case Destination::Pan:
                pan += modValue;
                break;
            case Destination::Level:
                levelMul *= (1.0f + modValue); // modValue -1..+1 → 0..2
                break;
            case Destination::Pitch:
                pitchSemi += modValue * 12.0f; // ±12 semitones at full depth
                break;
            case Destination::AttackTime:
                attackScale *= std::exp2(modValue); // ±1 octave of time
                break;
            case Destination::DecayTime:
                decayScale *= std::exp2(modValue);
                break;
            case Destination::LFO1Rate:
                lfo1RateMul *= std::exp2(modValue * 2.0f);
                break;
            case Destination::EqMidFreq:
                eqMidFreqAdd += modValue * 2.0f;   // ±2 octaves at full depth
                break;
            case Destination::EqMidGain:
                eqMidGainAdd += modValue * 12.0f;  // ±12 dB at full depth
                break;
            default:
                break;
        }
    }

    void clamp()
    {
        cutoffMul   = juce::jlimit(0.0625f, 16.0f, cutoffMul);
        resonance   = juce::jlimit(-1.0f, 1.0f, resonance);
        pan         = juce::jlimit(-1.0f, 1.0f, pan);
        levelMul    = juce::jlimit(0.0f, 4.0f, levelMul);
        pitchSemi   = juce::jlimit(-24.0f, 24.0f, pitchSemi);
        attackScale = juce::jlimit(0.0625f, 16.0f, attackScale);
        decayScale  = juce::jlimit(0.0625f, 16.0f, decayScale);
        lfo1RateMul  = juce::jlimit(0.0625f, 16.0f, lfo1RateMul);
        eqMidFreqAdd = juce::jlimit(-2.0f, 2.0f, eqMidFreqAdd);
        eqMidGainAdd = juce::jlimit(-12.0f, 12.0f, eqMidGainAdd);
    }
};

// =============================================================================
// ModulationMatrix — the main processor
// =============================================================================
class ModulationMatrix
{
public:
    ModulationMatrix() = default;

    // ── Slot access ──
    ModSlot getSlot(int index) const
    {
        const auto safeIndex = static_cast<std::size_t>(juce::jlimit(0, kMaxSlots - 1, index));
        const auto& slot = slots[safeIndex];
        return unpackSlot(slot.packed.load(std::memory_order_acquire));
    }

    void setSlot(int index, Source source, Destination destination, float amount) noexcept
    {
        const auto safeIndex = static_cast<std::size_t>(juce::jlimit(0, kMaxSlots - 1, index));
        auto& slot = slots[safeIndex];
        slot.packed.store(packSlot(source, destination, amount), std::memory_order_release);
    }

    void clearSlots() noexcept
    {
        for (int i = 0; i < kMaxSlots; ++i)
            setSlot(i, Source::None, Destination::None, 0.0f);
    }

    static constexpr int getNumSlots() { return kMaxSlots; }

    // ── Process: compute all modulation for given source context ──
    ModResult process(const ModContext& ctx) const
    {
        ModResult result;
        for (int slotIndex = 0; slotIndex < kMaxSlots; ++slotIndex)
        {
            const auto slot = getSlot(slotIndex);
            if (slot.source == Source::None || slot.destination == Destination::None)
                continue;
            if (std::abs(slot.amount) < 0.0001f)
                continue;

            float sourceVal = ctx.getSource(slot.source);
            float modValue = sourceVal * slot.amount;
            result.addDestination(slot.destination, modValue);
        }
        result.clamp();
        return result;
    }

    // ── LFO2 generator (self-contained, independent of the synth's main LFO) ──
    struct LFO2State
    {
        float phase = 0.0f;
        std::atomic<float> rate  { 2.0f };  // Hz
        std::atomic<int>   wave  { 0 };     // 0=Sine, 1=Triangle, 2=Saw, 3=Square

        float tickBlock(float sampleRate, int numSamples)
        {
            float val = 0.0f;
            const auto currentWave = juce::jlimit(0, 3, wave.load(std::memory_order_relaxed));
            switch (currentWave)
            {
                case 1: val = 1.0f - 4.0f * std::abs(phase - 0.5f); break;
                case 2: val = phase * 2.0f - 1.0f; break;
                case 3: val = phase < 0.5f ? 1.0f : -1.0f; break;
                default: val = std::sin(phase * juce::MathConstants<float>::twoPi); break;
            }

            if (sampleRate > 0.0f && numSamples > 0)
            {
                const auto currentRate = juce::jmax(0.0f, rate.load(std::memory_order_relaxed));
                phase += (currentRate * static_cast<float>(numSamples)) / sampleRate;
                phase -= std::floor(phase);
            }

            return val;
        }

        float tick(float sampleRate)
        {
            return tickBlock(sampleRate, 1);
        }

        void setRate(float newRate) noexcept
        {
            rate.store(juce::jmax(0.0f, newRate), std::memory_order_relaxed);
        }

        void setWave(int newWave) noexcept
        {
            wave.store(juce::jlimit(0, 3, newWave), std::memory_order_relaxed);
        }

        float getRate() const noexcept
        {
            return rate.load(std::memory_order_relaxed);
        }

        int getWave() const noexcept
        {
            return juce::jlimit(0, 3, wave.load(std::memory_order_relaxed));
        }

        float getPhase() const noexcept { return phase; }

        void reset() { phase = 0.0f; }
    };

    LFO2State lfo2;

    // ── MIDI state (updated from processBlock) ──
    std::atomic<float> modWheelValue   { 0.0f };
    std::atomic<float> aftertouchValue { 0.0f };
    std::atomic<float> pitchBendValue  { 0.0f }; // -1..+1
    std::atomic<int>   pitchBendRange  { 2 };    // semitones

    void handleMidiMessage(const juce::MidiMessage& msg)
    {
        if (msg.isController())
        {
            if (msg.getControllerNumber() == 1) // Mod Wheel
                modWheelValue.store(static_cast<float>(msg.getControllerValue()) / 127.0f, std::memory_order_relaxed);
        }
        else if (msg.isChannelPressure())
        {
            aftertouchValue.store(static_cast<float>(msg.getChannelPressureValue()) / 127.0f, std::memory_order_relaxed);
        }
        else if (msg.isPitchWheel())
        {
            pitchBendValue.store((static_cast<float>(msg.getPitchWheelValue()) - 8192.0f) / 8192.0f,
                                 std::memory_order_relaxed);
        }
    }

    void resetMidiSources() noexcept
    {
        modWheelValue.store(0.0f, std::memory_order_relaxed);
        aftertouchValue.store(0.0f, std::memory_order_relaxed);
        pitchBendValue.store(0.0f, std::memory_order_relaxed);
    }

    MatrixState captureState() const
    {
        MatrixState state;
        state.pitchBendRange = pitchBendRange.load(std::memory_order_relaxed);
        state.lfo2Rate = lfo2.getRate();
        state.lfo2Wave = lfo2.getWave();
        for (int i = 0; i < kMaxSlots; ++i)
            state.slots[static_cast<std::size_t>(i)] = getSlot(i);
        return state;
    }

    void applyState(const MatrixState& state)
    {
        pitchBendRange.store(state.pitchBendRange, std::memory_order_relaxed);
        lfo2.setRate(state.lfo2Rate);
        lfo2.setWave(state.lfo2Wave);
        for (int i = 0; i < kMaxSlots; ++i)
        {
            const auto& slot = state.slots[static_cast<std::size_t>(i)];
            setSlot(i, slot.source, slot.destination, slot.amount);
        }
    }

    static void saveStateToXml(juce::XmlElement& parent, const MatrixState& state)
    {
        auto* matEl = parent.createNewChildElement("ModMatrix");
        matEl->setAttribute("pbRange", state.pitchBendRange);
        matEl->setAttribute("lfo2Rate", static_cast<double>(state.lfo2Rate));
        matEl->setAttribute("lfo2Wave", state.lfo2Wave);

        for (int i = 0; i < kMaxSlots; ++i)
        {
            const auto& slot = state.slots[static_cast<std::size_t>(i)];
            if (slot.source == Source::None && slot.destination == Destination::None)
                continue;

            auto* slotEl = matEl->createNewChildElement("Slot");
            slotEl->setAttribute("idx", i);
            slotEl->setAttribute("src", static_cast<int>(slot.source));
            slotEl->setAttribute("dst", static_cast<int>(slot.destination));
            slotEl->setAttribute("amt", static_cast<double>(slot.amount));
        }
    }

    static bool loadStateFromXml(const juce::XmlElement& parent, MatrixState& state)
    {
        if (auto* matEl = parent.getChildByName("ModMatrix"))
        {
            state = {};
            state.pitchBendRange = matEl->getIntAttribute("pbRange", state.pitchBendRange);
            state.lfo2Rate = static_cast<float>(matEl->getDoubleAttribute("lfo2Rate", state.lfo2Rate));
            state.lfo2Wave = matEl->getIntAttribute("lfo2Wave", state.lfo2Wave);

            for (auto* slotEl : matEl->getChildWithTagNameIterator("Slot"))
            {
                const auto idx = slotEl->getIntAttribute("idx", -1);
                if (idx < 0 || idx >= kMaxSlots)
                    continue;

                auto& slot = state.slots[static_cast<std::size_t>(idx)];
                slot.source = decodeSource(slotEl->getIntAttribute("src", 0));
                slot.destination = decodeDestination(slotEl->getIntAttribute("dst", 0));
                slot.amount = juce::jlimit(-1.0f, 1.0f, static_cast<float>(slotEl->getDoubleAttribute("amt", 0.0)));
            }

            return true;
        }

        state = {};
        return false;
    }

    // ── Persistence ──
    void saveToXml(juce::XmlElement& parent) const
    {
        saveStateToXml(parent, captureState());
    }

    bool loadFromXml(const juce::XmlElement& parent)
    {
        MatrixState state;
        const auto loaded = loadStateFromXml(parent, state);
        applyState(state);
        return loaded;
    }

private:
    struct AtomicModSlot
    {
        std::atomic<std::uint64_t> packed { 0 };
    };

    static std::uint32_t floatToBits(float value) noexcept
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    static float bitsToFloat(std::uint32_t bits) noexcept
    {
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return std::isfinite(value) ? juce::jlimit(-1.0f, 1.0f, value) : 0.0f;
    }

    static std::uint64_t packSlot(Source source, Destination destination, float amount) noexcept
    {
        const auto safeSource = static_cast<std::uint64_t>(
            juce::jlimit(0, kSourceCount - 1, static_cast<int>(source)));
        const auto safeDestination = static_cast<std::uint64_t>(
            juce::jlimit(0, kDestCount - 1, static_cast<int>(destination)));
        const float safeAmount = std::isfinite(amount) ? juce::jlimit(-1.0f, 1.0f, amount) : 0.0f;
        return safeSource
             | (safeDestination << 8)
             | (static_cast<std::uint64_t>(floatToBits(safeAmount)) << 16);
    }

    static ModSlot unpackSlot(std::uint64_t packed) noexcept
    {
        const auto rawSource = static_cast<int>(packed & 0xffu);
        const auto rawDestination = static_cast<int>((packed >> 8) & 0xffu);
        const auto amountBits = static_cast<std::uint32_t>((packed >> 16) & 0xffffffffu);
        return { decodeSource(rawSource), decodeDestination(rawDestination), bitsToFloat(amountBits) };
    }

    static Source decodeSource(int rawValue) noexcept
    {
        return static_cast<Source>(juce::jlimit(0, kSourceCount - 1, rawValue));
    }

    static Destination decodeDestination(int rawValue) noexcept
    {
        return static_cast<Destination>(juce::jlimit(0, kDestCount - 1, rawValue));
    }

    std::array<AtomicModSlot, kMaxSlots> slots;
};

} // namespace modmatrix
