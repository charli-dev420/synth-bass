#pragma once

#include "BassDefs.h"
#include "../../Shared/ModulationMatrix.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace mbs
{

struct PresetMetadata
{
    std::string mixRole = "production";
    std::vector<std::string> tags { "bass", "factory" };
    std::string familyLabel = "bass";
    float nominalPeakDb = -12.0f;
};

// ---------------------------------------------------------------------------
// Per-instrument preset: a named BassSettings for a single bass instrument.
// ---------------------------------------------------------------------------
struct InstrumentPreset
{
    std::string      name;
    BassSettings     settings;
    GlobalFxSettings fx;
    int              outputBus = 0;
    std::optional<PatchPerformanceSettings> performance;
    std::optional<modmatrix::MatrixState> modMatrix;
    PresetMetadata   metadata {};
};

// ---------------------------------------------------------------------------
// Returns a curated bank for every bass. The actual bank sizes are the source
// of truth and intentionally not duplicated in comments.
// ---------------------------------------------------------------------------
const std::array<std::vector<InstrumentPreset>, kNumBasses>& getFactoryPresetBanks();
std::size_t getTotalFactoryPresetCount();

} // namespace mbs
