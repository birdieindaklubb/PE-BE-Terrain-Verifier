#pragma once

#include "detail/mt19937.hpp"
#include "detail/noise.hpp"
#include "mcpe/worldgen/chunk.hpp"

#include <cstdint>
#include <vector>

namespace mcpe::worldgen::v1_1_5_0 {

// PE 1.1.5.0 HellRandomLevelSource's seed-owned solid-terrain and surface
// source. Cave carving and population are deliberately separate passes.
class HellTerrain final {
public:
    explicit HellTerrain(std::uint32_t world_seed);

    [[nodiscard]] Chunk generate_base_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z) const;

    static void recalculate_heightmap(Chunk& chunk) noexcept;

private:
    void density_lattice(
        std::vector<float>& output,
        std::int32_t coarse_x,
        std::int32_t coarse_z) const;
    void build_surfaces(Chunk& chunk) const;

    detail::Mt19937 random_;
    detail::PerlinNoise density_a_;
    detail::PerlinNoise density_b_;
    detail::PerlinNoise density_selector_;
    detail::PerlinNoise surface_noise_;
    detail::PerlinNoise surface_depth_noise_;
    detail::PerlinNoise scale_noise_;
    detail::PerlinNoise depth_noise_;
};

} // namespace mcpe::worldgen::v1_1_5_0
