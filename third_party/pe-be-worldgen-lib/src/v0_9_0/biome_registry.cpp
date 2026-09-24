#include "v0_9_0/biome_registry.hpp"

#include "v0_9_0/biome_ids.hpp"
#include "v0_9_0/blocks.hpp"

namespace mcpe::worldgen::v0_9_0 {
namespace {

[[nodiscard]] constexpr std::int32_t base_id(std::int32_t id) noexcept {
    return id >= 128 ? id - 128 : id;
}

[[nodiscard]] constexpr SurfaceProfile default_profile(
    float temperature) noexcept {
    return {block::grass, 0, block::dirt, temperature};
}

} // namespace

BiomeProperties biome_properties(std::int32_t id) noexcept {
    const std::int32_t base = base_id(id);
    BiomeProperties result{
        .surface = default_profile(0.5F),
        .rainfall = 0.5F,
        .surface_kind = SurfaceKind::default_surface,
        .subtype = 0,
        .mesa_bryce = false,
        .mesa_forest = false,
    };

    // Values are the r1/r2 bit patterns passed to
    // Biome::setTemperatureAndDownfall by Biome::initBiomes, or the direct
    // constructor calls for plains and forest variants.
    switch (base) {
    case biome::ocean:
    case biome::river:
    case biome::deep_ocean:
        break;
    case biome::plains:
        result.surface.temperature = 0.8F;
        result.rainfall = 0.4F;
        break;
    case biome::desert:
    case biome::desert_hills:
        result.surface = {block::sand, 0, block::sand, 2.0F};
        result.rainfall = 0.0F;
        break;
    case biome::extreme_hills:
    case biome::smaller_extreme_hills:
    case biome::extreme_hills_plus:
        result.surface.temperature = 0.2F;
        result.rainfall = 0.3F;
        result.surface_kind = SurfaceKind::extreme_hills;
        result.subtype = base == biome::extreme_hills ? 0 : 1;
        if (id == biome::extreme_hills + 128) {
            result.subtype = 2;
        }
        break;
    case biome::forest:
    case biome::forest_hills:
    case biome::roofed_forest:
        result.surface.temperature = 0.7F;
        result.rainfall = 0.8F;
        break;
    case biome::birch_forest:
    case biome::birch_forest_hills:
        result.surface.temperature = 0.6F;
        result.rainfall = 0.6F;
        break;
    case biome::taiga:
    case biome::taiga_hills:
        result.surface.temperature = 0.25F;
        result.rainfall = 0.8F;
        break;
    case biome::swampland:
        result.surface.temperature = 0.8F;
        result.rainfall = 0.9F;
        result.surface_kind = SurfaceKind::swamp;
        break;
    case biome::frozen_ocean:
    case biome::frozen_river:
        result.surface.temperature = 0.0F;
        break;
    case biome::ice_flats:
    case biome::ice_mountains:
        result.surface.temperature = 0.0F;
        // IceBiome::createMutatedCopy constructs an IceBiome with its
        // `spikes` flag set.  That constructor changes only the top surface
        // tile to snow; the ordinary ice biomes retain grass/dirt here.
        if (id == biome::ice_flats + 128) {
            result.surface.top_block = block::snow;
        }
        break;
    case biome::mushroom_island:
    case biome::mushroom_island_shore:
        result.surface = {block::mycelium, 0, block::dirt, 0.9F};
        result.rainfall = 1.0F;
        break;
    case biome::beach:
        result.surface = {block::sand, 0, block::sand, 0.8F};
        result.rainfall = 0.4F;
        break;
    case biome::jungle:
    case biome::jungle_hills:
        result.surface.temperature = 0.95F;
        result.rainfall = 0.9F;
        break;
    case biome::jungle_edge:
        result.surface.temperature = 0.95F;
        result.rainfall = 0.8F;
        break;
    case biome::stone_beach:
        result.surface = {block::rock, 0, block::rock, 0.2F};
        result.rainfall = 0.3F;
        break;
    case biome::cold_beach:
        result.surface = {block::sand, 0, block::sand, 0.05F};
        result.rainfall = 0.3F;
        break;
    case biome::cold_taiga:
    case biome::cold_taiga_hills:
        result.surface.temperature = -0.5F;
        result.rainfall = 0.4F;
        break;
    case biome::mega_taiga:
    case biome::mega_taiga_hills:
        result.surface.temperature = 0.3F;
        result.rainfall = 0.8F;
        result.surface_kind = SurfaceKind::taiga;
        // TaigaBiome::createMutatedCopy compares against redwoodTaiga
        // (ID 32), so only ID 160 is the type-2 Mega Spruce variant.
        result.subtype = id == biome::mega_taiga + 128 ? 2 : 1;
        break;
    case biome::savanna:
        result.surface.temperature = 1.2F;
        result.rainfall = 0.0F;
        if (id == biome::savanna + 128) {
            result.surface_kind = SurfaceKind::mutated_savanna;
        }
        break;
    case biome::savanna_plateau:
        result.surface.temperature = 1.0F;
        result.rainfall = 0.0F;
        break;
    case biome::mesa:
    case biome::mesa_plateau_forest:
    case biome::mesa_plateau:
        result.surface = {block::sand, 1, block::hardened_clay, 2.0F};
        result.rainfall = 0.0F;
        result.surface_kind = SurfaceKind::mesa;
        result.mesa_bryce = id == biome::mesa + 128;
        result.mesa_forest = base == biome::mesa_plateau_forest;
        break;
    default:
        break;
    }
    return result;
}

} // namespace mcpe::worldgen::v0_9_0
