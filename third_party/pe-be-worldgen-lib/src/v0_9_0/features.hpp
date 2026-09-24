#pragma once

#include "detail/block_access.hpp"
#include "detail/mt19937.hpp"
#include "v0_9_0/blocks.hpp"
#include "v0_9_0/world.hpp"

#include <cstdint>

namespace mcpe::worldgen::v0_9_0::feature {

// OreFeature::place at. `base_*` is the TilePos supplied by the
// decorator; coordinates of the individual vein are selected inside it.
void place_ore(
    detail::BlockAccess& world,
    detail::Mt19937& random,
    std::int32_t base_x,
    std::int32_t base_y,
    std::int32_t base_z,
    std::uint8_t output_id,
    std::uint8_t output_data,
    std::int32_t size,
    std::uint8_t replace_id = block::rock,
    std::int32_t vertical_offset = 2,
    std::uint8_t alternate_replace_id = block::air,
    bool inclusive_steps = true) noexcept;

// BiomeDecorator::decorateOres at. This remains a separate phase
// because BiomeDecorator::decorate performs the non-ore feature passes after
// it using the same random stream.
void decorate_ores(
    World& world,
    detail::Mt19937& random,
    std::int32_t base_x,
    std::int32_t base_y,
    std::int32_t base_z) noexcept;

// LakeFeature::place at. In 0.9.0 RandomLevelSource constructs
// water and lava instances with an air upper half; `liquid_id` is therefore
// the only varying output tile.
[[nodiscard]] bool place_lake(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t liquid_id) noexcept;

// The small decoration features below retain their own native attempt counts
// and PRNG draw order; they are not normalized through a shared scatterer.
void place_flower(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z,
    std::uint8_t id, std::uint8_t metadata = 0) noexcept;
void place_tall_grass(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z,
    std::uint8_t metadata) noexcept;
void place_reeds(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z) noexcept;
void place_cactus(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z) noexcept;
void place_waterlilies(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z) noexcept;
void place_pumpkins(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z) noexcept;
void place_dead_bushes(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z) noexcept;

// New 0.9 decoration generators retained outside features.cpp so the older,
// already dense collection of lake/ore/scatter code stays readable.
[[nodiscard]] bool place_double_plant(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z,
    std::uint8_t type) noexcept;
void place_melons(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z) noexcept;
void place_vines(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z) noexcept;

// HugeMushroomFeature::place,. `forced_type` is the feature's
// constructor field: -1 selects brown/red from the consumed random word;
// 0 and 1 select brown and red respectively while still consuming that word.
[[nodiscard]] bool place_huge_mushroom(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::int32_t forced_type = -1) noexcept;

// SpringFeature::place validates this exact one-air/three-rock side pattern.
// Its immediate dynamic-liquid tick is modeled separately by the liquid
// phase, because water and lava have different state-transition rules.
[[nodiscard]] bool place_spring_source(
    World& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t liquid_id) noexcept;

// IcePatchFeature::place and GroundBushFeature::place, used respectively by
// ice-spike biomes and jungle tree selection.
[[nodiscard]] bool place_ice_patch(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t output_id,
    std::int32_t maximum_radius) noexcept;
// IceSpikeFeature::place,. Its lower support columns deliberately
// retain the APK's draw-dependent gaps rather than using a simplified cone.
[[nodiscard]] bool place_ice_spike(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept;
[[nodiscard]] bool place_ground_bush(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t metadata) noexcept;

// TileBlobFeature::place, used for the moss-stone blobs in forest decoration.
[[nodiscard]] bool place_tile_blob(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t output_id,
    std::int32_t radius) noexcept;

// DesertWellFeature::place at. The feature has a Random parameter
// in the native virtual interface, but this build does not read it.
[[nodiscard]] bool place_desert_well(
    World& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept;

[[nodiscard]] bool place_clay_patch(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z,
    std::int32_t maximum_radius) noexcept;
[[nodiscard]] bool place_sand_patch(
    World& world, detail::Mt19937& random,
    std::int32_t x, std::int32_t y, std::int32_t z,
    std::uint8_t output_id, std::int32_t maximum_radius) noexcept;

} // namespace mcpe::worldgen::v0_9_0::feature
