#pragma once

#include "v0_9_0/biome_ids.hpp"

#include <cstdint>

namespace mcpe::worldgen::v0_9_0 {

// The two fields consumed by RandomLevelSource::getHeights. They are copied
// from Biome::HEIGHTS_* into Biome offsets 0x48 and 0x4c by
// Biome::setDepthAndScale ( in the target).
struct BiomeHeight final {
    float depth;
    float scale;
};

namespace biome_height {

// Read from _INIT_3's writes to Biome::HEIGHTS_* in the 0.9.0 ARM binary.
// Keep these grouped as native records: several happen to have equal values,
// but the distinct records matter when following constructor/mutation paths.
inline constexpr BiomeHeight default_ = {0.1F, 0.2F};
inline constexpr BiomeHeight lowlands = {0.125F, 0.05F};
inline constexpr BiomeHeight ocean = {-1.0F, 0.1F};
inline constexpr BiomeHeight deep_ocean = {-1.8F, 0.1F};
inline constexpr BiomeHeight river = {-0.5F, 0.0F};
inline constexpr BiomeHeight swampland = {-0.2F, 0.1F};
inline constexpr BiomeHeight mushroom = {0.2F, 0.3F};
inline constexpr BiomeHeight beach = {0.0F, 0.025F};
inline constexpr BiomeHeight stone_beach = {0.1F, 0.8F};
inline constexpr BiomeHeight mountains = {0.45F, 0.3F};
inline constexpr BiomeHeight extreme = {1.0F, 0.5F};
inline constexpr BiomeHeight highlands = {1.5F, 0.025F};
inline constexpr BiomeHeight taiga = {0.2F, 0.2F};

[[nodiscard]] constexpr BiomeHeight base_for(std::int32_t id) noexcept {
    switch (id) {
    case biome::ocean:
    case biome::frozen_ocean:
        return ocean;
    case biome::deep_ocean:
        return deep_ocean;
    case biome::river:
    case biome::frozen_river:
        return river;
    case biome::swampland:
        return swampland;
    case biome::mushroom_island:
        return mushroom;
    case biome::mushroom_island_shore:
    case biome::beach:
    case biome::cold_beach:
        return beach;
    case biome::stone_beach:
        return stone_beach;
    case biome::desert_hills:
    case biome::forest_hills:
    case biome::taiga_hills:
    case biome::ice_mountains:
    case biome::jungle_hills:
    case biome::birch_forest_hills:
    case biome::cold_taiga_hills:
    case biome::mega_taiga_hills:
        return mountains;
    case biome::extreme_hills:
    case biome::extreme_hills_plus:
        return extreme;
    case biome::smaller_extreme_hills:
        return {extreme.depth * 0.8F, extreme.scale * 0.8F};
    case biome::cold_taiga:
    case biome::mega_taiga:
        return taiga;
    case biome::savanna_plateau:
    case biome::mesa_plateau_forest:
    case biome::mesa_plateau:
        return highlands;
    case biome::plains:
    case biome::desert:
    case biome::ice_flats:
    case biome::savanna:
        return lowlands;
    default:
        return default_;
    }
}

[[nodiscard]] constexpr BiomeHeight for_biome(std::int32_t id) noexcept {
    if (id < 128) {
        return base_for(id);
    }

    const std::int32_t base_id = id - 128;
    const BiomeHeight base = base_for(base_id);
    switch (base_id) {
    // PlainsBiome::createMutatedCopy constructs another PlainsBiome directly,
    // so Sunflower Plains retains HEIGHTS_LOWLANDS.  It does not pass through
    // MutatedBiome's generic terrain adjustment.
    case biome::plains:
        return base;
    // ForestBiome::createMutatedCopy preserves the parent depth and raises
    // only scale by 0.2 for Flower Forest.
    case biome::forest:
        return {base.depth, base.scale + 0.2F};
    // IceBiome::createMutatedCopy first constructs the spikes biome, then
    // overwrites its terrain record with the parent values plus 0.3 depth
    // and 0.4 scale.  This is deliberately not MutatedBiome's generic
    // +0.1/+0.2 adjustment.
    case biome::ice_flats:
        return {base.depth + 0.3F, base.scale + 0.4F};
    // ExtremeHillsBiome::setMutated copies the height record unchanged.
    case biome::extreme_hills:
    case biome::extreme_hills_plus:
        return base;
    // TaigaBiome::createMutatedCopy preserves Mega Taiga's terrain record for
    // Mega Spruce Taiga rather than using MutatedBiome's generic adjustment.
    case biome::mega_taiga:
        return base;
    // MesaBiome::createMutatedCopy leaves Bryce Mesa at Mesa's default height;
    // mutated plateau variants use HEIGHTS_MOUNTAINS.
    case biome::mesa:
        return base;
    case biome::mesa_plateau_forest:
    case biome::mesa_plateau:
        return mountains;
    // MutatedSavannaBiome replaces the generic mutation values with these two
    // parent-derived formulas.
    case biome::savanna:
    case biome::savanna_plateau:
        return {base.depth * 0.5F + 0.3F, base.scale * 0.5F + 1.2F};
    default:
        // MutatedBiome::MutatedBiome adds 0.1 depth and 0.2 scale.
        return {base.depth + 0.1F, base.scale + 0.2F};
    }
}

} // namespace biome_height
} // namespace mcpe::worldgen::v0_9_0
