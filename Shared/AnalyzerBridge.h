#pragma once
// =============================================================================
// AnalyzerBridge.h — Legacy shared-memory bridge kept for migration from the
//                    retired Musique Analyzer standalone to the integrated Hub.
//
// The Hub remains the source of truth. This bridge only exists while a few
// legacy external workflows are still being phased out, and is intended to be
// enabled explicitly only for migration/debug scenarios.
// Uses a Windows memory-mapped file for zero-copy IPC.
// =============================================================================
#include <cstdint>
#include <cstring>
#include <atomic>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace analyzer_bridge
{

namespace detail
{
inline void copyCString(char* dest, size_t destSize, const char* src)
{
    if (dest == nullptr || destSize == 0)
        return;

    if (src == nullptr)
    {
        dest[0] = '\0';
        return;
    }

    size_t index = 0;
    for (; index + 1 < destSize && src[index] != '\0'; ++index)
        dest[index] = src[index];

    dest[index] = '\0';
}

template <size_t N>
inline void copyCString(char (&dest)[N], const char* src)
{
    copyCString(dest, N, src);
}
} // namespace detail

/// Ring buffer capacity in stereo frames. ~1.5 seconds at 48 kHz.
static constexpr int kRingCapacity = 65536;
static constexpr int kParamStateJsonCapacity = 16384;

/// Shared memory layout (header + ring buffer).
struct SharedBlock
{
    std::atomic<uint32_t> writePos { 0 };   // writer increments
    std::atomic<uint32_t> readPos  { 0 };   // reader increments
    std::atomic<double>   sampleRate { 44100.0 };
    std::atomic<int32_t>  hubActive { 0 };  // 1 = hub is alive

    // Synth/preset metadata (written infrequently by Hub)
    std::atomic<uint32_t> metadataVersion { 0 };
    std::atomic<int32_t>  activeSynthIndex { -1 };
    char synthName[64] {};
    char presetName[128] {};
    char instrumentName[128] {};
    std::atomic<uint32_t> parameterStateVersion { 0 };
    char parameterStateJson[kParamStateJsonCapacity] {};

    // Extended plugin identification (v5)
    char pluginUid[64] {};           // fileOrIdentifier of the active VST3
    char pluginManufacturer[64] {};
    int32_t pluginTier = 0;          // cast of hub::PluginTier enum

    // ── Command channel (Analyzer → Hub), N-slot ring buffer ──
    static constexpr uint32_t kCmdQueueSize = 8;

    struct CommandSlot
    {
        int32_t type       = 0;   // 0=noop, 1=setParam, 2=loadPresetPath, 3=saveAsPreset
        char    paramName[64] {};
        float   paramValue = 0.0f;
        char    payload[512] {};
    };

    std::atomic<uint32_t> cmdHead { 0 };   // Analyzer increments after writing
    std::atomic<uint32_t> cmdTail { 0 };   // Hub increments after reading
    CommandSlot cmdSlots[kCmdQueueSize] {};

    float ringLeft[kRingCapacity] {};
    float ringRight[kRingCapacity] {};
};

static constexpr const char* kSharedMemName = "MusiqueAnalyzerBridge_v7";
static constexpr size_t      kSharedMemSize = sizeof(SharedBlock);

// ─────────────────────────────────────────────────────────────
// Writer (used by the Hub plugin)
// ─────────────────────────────────────────────────────────────
class BridgeWriter
{
public:
    BridgeWriter() = default;
    ~BridgeWriter() { close(); }

    bool open()
    {
#ifdef _WIN32
        hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
                                  PAGE_READWRITE, 0, (DWORD)kSharedMemSize,
                                  kSharedMemName);
        if (!hMap) return false;

        ptr = (SharedBlock*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, kSharedMemSize);
        if (!ptr) { CloseHandle(hMap); hMap = nullptr; return false; }

        // Initialize if first creator
        ptr->hubActive.store(1);
        return true;
#else
        return false;
#endif
    }

    void close()
    {
#ifdef _WIN32
        if (ptr) { ptr->hubActive.store(0); UnmapViewOfFile(ptr); ptr = nullptr; }
        if (hMap) { CloseHandle(hMap); hMap = nullptr; }
#endif
    }

    bool isOpen() const { return ptr != nullptr; }

    void setSampleRate(double sr)
    {
        if (ptr) ptr->sampleRate.store(sr);
    }

    /// Update synth/preset/instrument metadata (call on change, not every block).
    void updateMetadata(int synthIndex, const char* synth, const char* preset, const char* instrument)
    {
        if (!ptr) return;
        ptr->metadataVersion.fetch_add(1, std::memory_order_release);
        ptr->activeSynthIndex.store(synthIndex, std::memory_order_relaxed);
        if (synth)      { detail::copyCString(ptr->synthName, synth); }
        if (preset)     { detail::copyCString(ptr->presetName, preset); }
        if (instrument) { detail::copyCString(ptr->instrumentName, instrument); }
        ptr->metadataVersion.fetch_add(1, std::memory_order_release);
    }

    /// Update extended plugin identification (v5 fields).
    void updatePluginIdentity(const char* uid, const char* manufacturer, int tier)
    {
        if (!ptr) return;
        ptr->metadataVersion.fetch_add(1, std::memory_order_release);
        if (uid)          { detail::copyCString(ptr->pluginUid, uid); }
        if (manufacturer) { detail::copyCString(ptr->pluginManufacturer, manufacturer); }
        ptr->pluginTier = tier;
        ptr->metadataVersion.fetch_add(1, std::memory_order_release);
    }

    /// Publish a compact JSON object containing the current parameter state.
    void updateParameterStateJson(const char* json)
    {
        if (!ptr) return;
        ptr->parameterStateVersion.fetch_add(1, std::memory_order_release);
        detail::copyCString(ptr->parameterStateJson, json);
        ptr->parameterStateVersion.fetch_add(1, std::memory_order_release);
    }

    /// Write stereo audio frames into the ring buffer.
    void write(const float* const* channels, int numChannels, int numSamples)
    {
        if (!ptr) return;

        uint32_t wp = ptr->writePos.load(std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            const float left = channels[0] != nullptr ? channels[0][i] : 0.0f;
            const float right = (numChannels > 1 && channels[1] != nullptr) ? channels[1][i] : left;
            ptr->ringLeft[wp % kRingCapacity] = left;
            ptr->ringRight[wp % kRingCapacity] = right;
            ++wp;
        }

        ptr->writePos.store(wp, std::memory_order_release);
    }

    // ── Command polling (Hub reads commands written by Analyzer) ──

    struct Command
    {
        int32_t type = 0;         // 0=noop, 1=setParam, 2=loadPresetPath, 3=saveAsPreset
        char    paramName[64] {};
        float   paramValue = 0.0f;
        char    payload[512] {};
    };

    /// Returns true if a new command is pending. Fills cmd and acknowledges.
    bool pollCommand(Command& cmd)
    {
        if (!ptr) return false;
        uint32_t tail = ptr->cmdTail.load(std::memory_order_relaxed);
        uint32_t head = ptr->cmdHead.load(std::memory_order_acquire);
        if (tail == head) return false; // queue empty

        const auto& slot = ptr->cmdSlots[tail % SharedBlock::kCmdQueueSize];
        cmd.type       = slot.type;
        detail::copyCString(cmd.paramName, slot.paramName);
        cmd.paramValue = slot.paramValue;
        detail::copyCString(cmd.payload, slot.payload);

        ptr->cmdTail.store(tail + 1, std::memory_order_release);
        return true;
    }

private:
#ifdef _WIN32
    HANDLE hMap = nullptr;
#endif
    SharedBlock* ptr = nullptr;
};

// ─────────────────────────────────────────────────────────────
// Reader (legacy external analyzer app only)
// ─────────────────────────────────────────────────────────────
class BridgeReader
{
public:
    BridgeReader() = default;
    ~BridgeReader() { close(); }

    bool open()
    {
#ifdef _WIN32
        // Open with read-write: read audio data, write commands back to Hub
        hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, kSharedMemName);
        if (!hMap) return false;

        ptr = (SharedBlock*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, kSharedMemSize);
        if (!ptr) { CloseHandle(hMap); hMap = nullptr; return false; }
        return true;
#else
        return false;
#endif
    }

    void close()
    {
#ifdef _WIN32
        if (ptr) { UnmapViewOfFile(ptr); ptr = nullptr; }
        if (hMap) { CloseHandle(hMap); hMap = nullptr; }
#endif
    }

    bool isOpen() const { return ptr != nullptr; }

    bool isHubActive() const
    {
        return ptr && ptr->hubActive.load(std::memory_order_relaxed) != 0;
    }

    double getSampleRate() const
    {
        return ptr ? ptr->sampleRate.load(std::memory_order_relaxed) : 44100.0;
    }

    int getActiveSynthIndex() const
    {
        return ptr ? ptr->activeSynthIndex.load(std::memory_order_relaxed) : -1;
    }

    /// Read metadata strings with a seqlock to avoid tearing during rare writes.
    void getMetadata(char* synthOut, int synthLen,
                     char* presetOut, int presetLen,
                     char* instrOut, int instrLen) const
    {
        if (!ptr) return;
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto versionBefore = ptr->metadataVersion.load(std::memory_order_acquire);
            if ((versionBefore & 1u) != 0u)
                continue;

            if (synthOut  && synthLen > 0)  { detail::copyCString(synthOut,  static_cast<size_t>(synthLen),  ptr->synthName); }
            if (presetOut && presetLen > 0) { detail::copyCString(presetOut, static_cast<size_t>(presetLen), ptr->presetName); }
            if (instrOut  && instrLen > 0)  { detail::copyCString(instrOut,  static_cast<size_t>(instrLen),  ptr->instrumentName); }

            const auto versionAfter = ptr->metadataVersion.load(std::memory_order_acquire);
            if (versionBefore == versionAfter && (versionAfter & 1u) == 0u)
                return;
        }

        if (synthOut  && synthLen > 0)  synthOut[0] = '\0';
        if (presetOut && presetLen > 0) presetOut[0] = '\0';
        if (instrOut  && instrLen > 0)  instrOut[0] = '\0';
    }

    void getPluginIdentity(char* uidOut, int uidLen,
                           char* manufacturerOut, int manufacturerLen,
                           int* tierOut) const
    {
        if (uidOut && uidLen > 0) uidOut[0] = '\0';
        if (manufacturerOut && manufacturerLen > 0) manufacturerOut[0] = '\0';
        if (tierOut) *tierOut = 0;
        if (!ptr) return;

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto versionBefore = ptr->metadataVersion.load(std::memory_order_acquire);
            if ((versionBefore & 1u) != 0u)
                continue;

            if (uidOut && uidLen > 0) { detail::copyCString(uidOut, static_cast<size_t>(uidLen), ptr->pluginUid); }
            if (manufacturerOut && manufacturerLen > 0) { detail::copyCString(manufacturerOut, static_cast<size_t>(manufacturerLen), ptr->pluginManufacturer); }
            if (tierOut) *tierOut = ptr->pluginTier;

            const auto versionAfter = ptr->metadataVersion.load(std::memory_order_acquire);
            if (versionBefore == versionAfter && (versionAfter & 1u) == 0u)
                return;
        }

        if (uidOut && uidLen > 0) uidOut[0] = '\0';
        if (manufacturerOut && manufacturerLen > 0) manufacturerOut[0] = '\0';
        if (tierOut) *tierOut = 0;
    }

    /// Read the last published parameter-state JSON snapshot.
    void getParameterStateJson(char* jsonOut, int jsonLen) const
    {
        if (jsonOut == nullptr || jsonLen <= 0)
            return;

        jsonOut[0] = '\0';
        if (!ptr)
            return;

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            auto versionBefore = ptr->parameterStateVersion.load(std::memory_order_acquire);
            if ((versionBefore & 1u) != 0u)
                continue;

            detail::copyCString(jsonOut, static_cast<size_t>(jsonLen), ptr->parameterStateJson);

            auto versionAfter = ptr->parameterStateVersion.load(std::memory_order_acquire);
            if (versionBefore == versionAfter && (versionAfter & 1u) == 0u)
                return;
        }

        jsonOut[0] = '\0';
    }

    int getAvailableSamples() const
    {
        if (!ptr) return 0;
        uint32_t wp = ptr->writePos.load(std::memory_order_acquire);
        uint32_t rp = localReadPos;
        int avail = (int)(wp - rp);
        if (avail < 0) avail = 0;
        if (avail > kRingCapacity) avail = kRingCapacity; // overflow: skip
        return avail;
    }

    int readStereo(float* leftDest, float* rightDest, int numSamples)
    {
        if (!ptr || leftDest == nullptr || rightDest == nullptr || numSamples <= 0) return 0;

        uint32_t wp = ptr->writePos.load(std::memory_order_acquire);
        int avail = (int)(wp - localReadPos);
        if (avail < 0) avail = 0;

        // If we're too far behind, skip to most recent data
        if (avail > kRingCapacity)
        {
            localReadPos = wp - (uint32_t)(kRingCapacity / 2);
            avail = (int)(wp - localReadPos);
        }

        int toRead = (numSamples < avail) ? numSamples : avail;
        for (int i = 0; i < toRead; ++i)
        {
            leftDest[i] = ptr->ringLeft[localReadPos % kRingCapacity];
            rightDest[i] = ptr->ringRight[localReadPos % kRingCapacity];
            ++localReadPos;
        }
        return toRead;
    }

    int read(float* dest, int numSamples)
    {
        if (dest == nullptr || numSamples <= 0)
            return 0;

        std::vector<float> left(static_cast<size_t>(numSamples), 0.0f);
        std::vector<float> right(static_cast<size_t>(numSamples), 0.0f);
        const auto total = readStereo(left.data(), right.data(), numSamples);
        for (int i = 0; i < total; ++i)
            dest[i] = (left[(size_t)i] + right[(size_t)i]) * 0.5f;
        return total;
    }

    /// Peek at the last N written samples without advancing the read cursor.
    /// Returns actual count copied (may be less than numSamples if not enough data).
    int peekLastStereo(float* leftDest, float* rightDest, int numSamples) const
    {
        if (!ptr || leftDest == nullptr || rightDest == nullptr || numSamples <= 0) return 0;

        uint32_t wp = ptr->writePos.load(std::memory_order_acquire);
        int avail = juce::jmin(numSamples, (int)juce::jmin((uint32_t)kRingCapacity, wp));
        uint32_t startPos = wp - (uint32_t)avail;

        for (int i = 0; i < avail; ++i)
        {
            leftDest[i] = ptr->ringLeft[(startPos + (uint32_t)i) % kRingCapacity];
            rightDest[i] = ptr->ringRight[(startPos + (uint32_t)i) % kRingCapacity];
        }

        return avail;
    }

    int peekLast(float* dest, int numSamples) const
    {
        if (dest == nullptr || numSamples <= 0)
            return 0;

        std::vector<float> left(static_cast<size_t>(numSamples), 0.0f);
        std::vector<float> right(static_cast<size_t>(numSamples), 0.0f);
        const auto total = peekLastStereo(left.data(), right.data(), numSamples);
        for (int i = 0; i < total; ++i)
            dest[i] = (left[(size_t)i] + right[(size_t)i]) * 0.5f;
        return total;
    }

    // ── Command submission (Analyzer sends commands to Hub) ──

    /// Submit a setParam command. Returns false if queue is full.
    bool submitSetParam(const char* paramName, float normalizedValue)
    {
        if (!ptr) return false;
        uint32_t head = ptr->cmdHead.load(std::memory_order_relaxed);
        uint32_t tail = ptr->cmdTail.load(std::memory_order_acquire);
        if (head - tail >= SharedBlock::kCmdQueueSize) return false; // queue full

        auto& slot = ptr->cmdSlots[head % SharedBlock::kCmdQueueSize];
        slot.type = 1;
        detail::copyCString(slot.paramName, paramName);
        slot.paramValue = normalizedValue;
        slot.payload[0] = '\0';
        ptr->cmdHead.store(head + 1, std::memory_order_release);
        return true;
    }

    /// Submit a loadPreset command (path to XML file).
    bool submitLoadPreset(const char* xmlPath)
    {
        if (!ptr) return false;
        uint32_t head = ptr->cmdHead.load(std::memory_order_relaxed);
        uint32_t tail = ptr->cmdTail.load(std::memory_order_acquire);
        if (head - tail >= SharedBlock::kCmdQueueSize) return false; // queue full

        auto& slot = ptr->cmdSlots[head % SharedBlock::kCmdQueueSize];
        slot.type = 2;
        slot.paramName[0] = '\0';
        slot.paramValue = 0.0f;
        detail::copyCString(slot.payload, xmlPath);
        ptr->cmdHead.store(head + 1, std::memory_order_release);
        return true;
    }

    /// Submit a saveAsPreset command (new preset name in payload).
    bool submitSaveAsPreset(const char* newPresetName)
    {
        if (!ptr) return false;
        uint32_t head = ptr->cmdHead.load(std::memory_order_relaxed);
        uint32_t tail = ptr->cmdTail.load(std::memory_order_acquire);
        if (head - tail >= SharedBlock::kCmdQueueSize) return false;

        auto& slot = ptr->cmdSlots[head % SharedBlock::kCmdQueueSize];
        slot.type = 3;
        slot.paramName[0] = '\0';
        slot.paramValue = 0.0f;
        detail::copyCString(slot.payload, newPresetName);
        ptr->cmdHead.store(head + 1, std::memory_order_release);
        return true;
    }

private:
#ifdef _WIN32
    HANDLE hMap = nullptr;
#endif
    SharedBlock* ptr = nullptr;
    uint32_t localReadPos = 0;
};

} // namespace analyzer_bridge
