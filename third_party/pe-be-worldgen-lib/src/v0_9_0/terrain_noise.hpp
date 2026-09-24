#pragma once

#include "detail/mt19937.hpp"
#include "detail/noise.hpp"
#include "detail/simplex_noise.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace mcpe::worldgen::v0_9_0 {

// RandomLevelSource's seven noise objects and its 5x5x17 base-density
// calculation.  This remains an internal component until the complete 0.9.0
// chunk pipeline, including surfaces, caves, and population, is verified.
class TerrainNoise final {
public:
    explicit TerrainNoise(std::uint32_t world_seed);

    // raw_biomes is BiomeSource::fillRawBiomeData(x - 2, z - 2, 10, 10), in
    // the APK's z-major layer order.  x and z are the coarse coordinates
    // passed to RandomLevelSource::getHeights (chunk coordinate * 4).
    [[nodiscard]] std::span<float> density_lattice(
        std::vector<float>& storage,
        std::span<const std::int32_t> raw_biomes,
        std::int32_t x,
        std::int32_t z) const;

    // RandomLevelSource::buildSurfaces requests this simplex field at absolute
    // block coordinates before dispatching each 16 by 16 column to its biome.
    [[nodiscard]] std::span<float> surface_field(
        std::vector<float>& storage,
        std::int32_t block_x,
        std::int32_t block_z) const;

private:
    detail::Mt19937 random_;
    detail::PerlinNoise density_lower_;
    detail::PerlinNoise density_upper_;
    detail::PerlinNoise density_selector_;
    detail::PerlinSimplexNoise surface_noise_;
    detail::PerlinNoise unused_10_octave_noise_;
    detail::PerlinNoise terrain_modifier_;
    detail::PerlinNoise unused_8_octave_noise_;
};

} // namespace mcpe::worldgen::v0_9_0
