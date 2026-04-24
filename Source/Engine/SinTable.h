#pragma once

#include <array>
#include <cmath>

namespace mbs
{

// =========================================================================
// Pre-computed sine wavetable — replaces per-sample std::sin() calls.
// Phase input is normalised to [0, 1) representing one full cycle.
// Hermite cubic interpolation for low-distortion (<-120 dB THD).
// =========================================================================
class SinTable
{
public:
    static constexpr int kSize = 2048;

    static const SinTable& instance() noexcept
    {
        static const SinTable table;
        return table;
    }

    float lookup(float phase01) const noexcept
    {
        constexpr float sizeF = static_cast<float>(kSize);
        const float idx  = phase01 * sizeF;
        const int   i1   = static_cast<int>(idx) & (kSize - 1);
        const int   i0   = (i1 - 1 + kSize) & (kSize - 1);
        const int   i2   = (i1 + 1) & (kSize - 1);
        const int   i3   = (i1 + 2) & (kSize - 1);
        const float frac = idx - std::floor(idx);

        // Hermite cubic interpolation
        const float y0 = table_[i0];
        const float y1 = table_[i1];
        const float y2 = table_[i2];
        const float y3 = table_[i3];

        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

private:
    SinTable()
    {
        constexpr double twoPi = 6.283185307179586476925286766559;
        for (int i = 0; i < kSize; ++i)
            table_[i] = static_cast<float>(std::sin(static_cast<double>(i) / kSize * twoPi));
    }

    std::array<float, kSize> table_{};
};

/// Fast sine for phase in [0, 1).  Drop-in replacement for
/// std::sin(phase * twoPi) in the per-sample oscillator loop.
inline float fastSin(float phase01) noexcept
{
    return SinTable::instance().lookup(phase01);
}

} // namespace mbs
