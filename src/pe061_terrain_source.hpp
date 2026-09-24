#pragma once

#include "detail/mt19937.hpp"
#include "detail/noise.hpp"
#include "mcpe/worldgen/chunk.hpp"
#include "v0_6_1/biome.hpp"

#include <cstdint>
#include <array>
#include <span>
#include <vector>

namespace mcpe::terrain_verifier {

// Terrain-only PE 0.6.1 source. The general library source also owns Level,
// population, lifecycle, and mob state; none of those participates in the
// base terrain queried by this executable.
class Pe061TerrainSource final {
public:
    explicit Pe061TerrainSource(std::uint32_t world_seed);

    [[nodiscard]] worldgen::Chunk generate_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z);
    [[nodiscard]] std::array<std::uint8_t, worldgen::Chunk::height>
        generate_first_column(
            std::int32_t chunk_x,
            std::int32_t chunk_z);

private:
    void get_heights(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::int32_t x_size,
        std::int32_t y_size,
        std::int32_t z_size);
    void prepare_heights(
        std::int32_t chunk_x,
        std::int32_t chunk_z,
        std::span<const float> temperatures,
        worldgen::Chunk& chunk);
    void build_surfaces(
        std::int32_t chunk_x,
        std::int32_t chunk_z,
        std::span<const worldgen::v0_6_1::BiomeId> biomes,
        worldgen::Chunk& chunk);

    worldgen::detail::Mt19937 random_;
    worldgen::detail::PerlinNoise lower_noise_;
    worldgen::detail::PerlinNoise upper_noise_;
    worldgen::detail::PerlinNoise selector_noise_;
    worldgen::detail::PerlinNoise surface_noise_;
    worldgen::detail::PerlinNoise surface_depth_noise_;
    worldgen::detail::PerlinNoise depth_noise_;
    worldgen::detail::PerlinNoise scale_noise_;
    worldgen::v0_6_1::BiomeSource biome_source_;

    std::vector<float> heights_;
    std::vector<float> selector_field_;
    std::vector<float> lower_field_;
    std::vector<float> upper_field_;
    std::vector<float> depth_field_;
    std::vector<float> scale_field_;
    std::vector<float> sand_field_;
    std::vector<float> gravel_field_;
    std::vector<float> surface_depth_field_;
};

} // namespace mcpe::terrain_verifier
