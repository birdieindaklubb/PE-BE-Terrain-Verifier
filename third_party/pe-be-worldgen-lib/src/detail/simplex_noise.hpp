#pragma once

#include "detail/mt19937.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace mcpe::worldgen::detail {

// PE 0.9.0's simplex implementation is separate from ImprovedNoise.  In
// particular, its region routine is three-dimensional even when a caller
// asks for a two-dimensional surface field.
class SimplexNoise final {
public:
    explicit SimplexNoise(Mt19937& random) noexcept;

    [[nodiscard]] float value(float x, float z) const noexcept;
    [[nodiscard]] float value(float x, float y, float z) const noexcept;

    void add(
        std::span<float> output,
        float x,
        float y,
        float z,
        std::int32_t x_size,
        std::int32_t y_size,
        std::int32_t z_size,
        float x_scale,
        float y_scale,
        float z_scale,
        float amplitude) const noexcept;

private:
    [[nodiscard]] float sample3d(
        float x,
        float y,
        float z,
        float skew) const noexcept;

    [[nodiscard]] float gradient2(
        std::uint32_t hash,
        float x,
        float z) const noexcept;

    [[nodiscard]] float gradient(
        std::uint32_t hash,
        float x,
        float y,
        float z) const noexcept;

    float x_offset_{};
    float y_offset_{};
    float z_offset_{};
    std::array<std::uint32_t, 512> permutation_{};
};

class PerlinSimplexNoise final {
public:
    PerlinSimplexNoise(Mt19937& random, std::int32_t octaves);

    [[nodiscard]] float value(float x, float z) const noexcept;
    [[nodiscard]] float value(float x, float y, float z) const noexcept;

    std::span<float> region(
        std::vector<float>& storage,
        float x,
        float y,
        float z,
        std::int32_t x_size,
        std::int32_t y_size,
        std::int32_t z_size,
        float x_scale,
        float y_scale,
        float z_scale) const;

    std::span<float> region2d(
        std::vector<float>& storage,
        std::int32_t x,
        std::int32_t z,
        std::int32_t x_size,
        std::int32_t z_size,
        float x_scale,
        float z_scale,
        float unused_amplitude) const;

private:
    std::vector<SimplexNoise> octaves_;
};

} // namespace mcpe::worldgen::detail
