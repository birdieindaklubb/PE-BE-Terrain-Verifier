#pragma once

#include "detail/mt19937.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace mcpe::worldgen::detail {

class ImprovedNoise final {
public:
    explicit ImprovedNoise(Mt19937& random) noexcept;

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
    [[nodiscard]] static float fade(float value) noexcept;
    [[nodiscard]] static float lerp(float amount, float a, float b) noexcept;
    [[nodiscard]] static float grad(
        std::uint32_t hash,
        float x,
        float y,
        float z) noexcept;
    [[nodiscard]] static float grad2(
        std::uint32_t hash,
        float x,
        float z) noexcept;

    float x_offset_{};
    float y_offset_{};
    float z_offset_{};
    std::array<std::uint32_t, 512> permutation_{};
};

class PerlinNoise final {
public:
    PerlinNoise(Mt19937& random, std::int32_t octaves);

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
        float amplitude) const;

private:
    std::vector<ImprovedNoise> octaves_;
};

} // namespace mcpe::worldgen::detail

