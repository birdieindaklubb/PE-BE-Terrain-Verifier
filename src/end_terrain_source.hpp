#pragma once

#include "detail/mt19937.hpp"
#include "mcpe/worldgen/chunk.hpp"
#include "v1_1_5_0/end_terrain.hpp"

#include <cstdint>

namespace mcpe::terrain_verifier {

// Final terrain-only End source: density islands plus the small outer-island
// decorator that writes End stone. Pillars, portals, chorus, gateways, and
// stateful chunk population are intentionally absent.
class EndTerrainSource final {
public:
    explicit EndTerrainSource(std::uint32_t world_seed);

    [[nodiscard]] worldgen::Chunk generate_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z) const;

private:
    void apply_outer_island_source(
        worldgen::Chunk& target,
        std::int32_t target_chunk_x,
        std::int32_t target_chunk_z,
        std::int32_t source_chunk_x,
        std::int32_t source_chunk_z) const;
    std::uint32_t world_seed_{};
    std::uint32_t x_multiplier_{};
    std::uint32_t z_multiplier_{};
    worldgen::v1_1_5_0::EndTerrain terrain_;
};

} // namespace mcpe::terrain_verifier
