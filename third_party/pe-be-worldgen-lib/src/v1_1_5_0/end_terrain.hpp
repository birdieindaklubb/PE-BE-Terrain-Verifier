#pragma once

#include "detail/mt19937.hpp"
#include "detail/noise.hpp"
#include "detail/simplex_noise.hpp"
#include "mcpe/worldgen/chunk.hpp"

#include <cstdint>
#include <vector>

namespace mcpe::worldgen::v1_1_5_0 {

// The End source's seed-owned density sources. It has no biome mixer: every
// column belongs to the End biome and the island field directly controls the
// terrain density.
class EndTerrain final {
public:
    explicit EndTerrain(std::uint32_t world_seed);

    [[nodiscard]] Chunk generate_base_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z) const;

    // Chunk-space island height used both by the density lattice and the
    // far-island population predicate.
    [[nodiscard]] float island_height_value(
        std::int32_t chunk_x,
        std::int32_t chunk_z,
        std::int32_t local_x,
        std::int32_t local_z) const noexcept;

    static void recalculate_heightmap(Chunk& chunk) noexcept;

private:
    void density_lattice(
        std::vector<float>& output,
        std::int32_t coarse_x,
        std::int32_t coarse_z) const;

    detail::Mt19937 random_;
    detail::PerlinNoise density_lower_;
    detail::PerlinNoise density_upper_;
    detail::PerlinNoise density_selector_;
    detail::SimplexNoise island_noise_;
};

} // namespace mcpe::worldgen::v1_1_5_0
