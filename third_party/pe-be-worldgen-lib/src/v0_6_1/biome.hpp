#pragma once

#include "detail/mt19937.hpp"
#include "detail/noise.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace mcpe::worldgen::v0_6_1 {

enum class BiomeId : std::uint8_t {
    rain_forest,
    swampland,
    seasonal_forest,
    forest,
    savanna,
    shrubland,
    taiga,
    desert,
    plains,
    ice_desert,
    tundra,
};

struct Biome final {
    BiomeId id;
    std::uint8_t top;
    std::uint8_t filler;
};

[[nodiscard]] const Biome& biome(BiomeId id) noexcept;

class BiomeSource final {
public:
    explicit BiomeSource(std::uint32_t world_seed);

    [[nodiscard]] std::span<const BiomeId> biomes(
        std::int32_t x,
        std::int32_t z,
        std::int32_t width,
        std::int32_t height);

    [[nodiscard]] std::span<const float> temperatures(
        std::int32_t x,
        std::int32_t z,
        std::int32_t width,
        std::int32_t height);

    [[nodiscard]] std::span<const float> temperature_map() const noexcept {
        return temperature_;
    }

    [[nodiscard]] std::span<const float> rainfall_map() const noexcept {
        return rainfall_;
    }

private:
    [[nodiscard]] static BiomeId select_direct(float temperature, float rainfall) noexcept;
    [[nodiscard]] static BiomeId select_quantized(float temperature, float rainfall) noexcept;
    [[nodiscard]] static const std::array<BiomeId, 64 * 64>& lookup() noexcept;

    detail::Mt19937 temperature_random_;
    detail::Mt19937 rainfall_random_;
    detail::Mt19937 detail_random_;
    detail::PerlinNoise temperature_noise_;
    detail::PerlinNoise rainfall_noise_;
    detail::PerlinNoise detail_noise_;

    std::vector<float> temperature_;
    std::vector<float> rainfall_;
    std::vector<float> detail_;
    std::vector<BiomeId> biomes_;
};

} // namespace mcpe::worldgen::v0_6_1

