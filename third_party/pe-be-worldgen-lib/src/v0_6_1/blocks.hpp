#pragma once

#include <cstdint>

namespace mcpe::worldgen::v0_6_1::block {

// Numeric IDs read from Tile::initTiles in the PE 0.6.1 ARMv7 binary.
inline constexpr std::uint8_t air = 0;
inline constexpr std::uint8_t stone = 1;
inline constexpr std::uint8_t grass = 2;
inline constexpr std::uint8_t dirt = 3;
inline constexpr std::uint8_t cobblestone = 4;
inline constexpr std::uint8_t wood_planks = 5;
inline constexpr std::uint8_t sapling = 6;
inline constexpr std::uint8_t bedrock = 7;
inline constexpr std::uint8_t water = 8;
inline constexpr std::uint8_t still_water = 9;
inline constexpr std::uint8_t lava = 10;
inline constexpr std::uint8_t still_lava = 11;
inline constexpr std::uint8_t sand = 12;
inline constexpr std::uint8_t gravel = 13;
inline constexpr std::uint8_t gold_ore = 14;
inline constexpr std::uint8_t iron_ore = 15;
inline constexpr std::uint8_t coal_ore = 16;
inline constexpr std::uint8_t log = 17;
inline constexpr std::uint8_t leaves = 18;
inline constexpr std::uint8_t lapis_ore = 21;
inline constexpr std::uint8_t sand_stone = 24;
inline constexpr std::uint8_t tall_grass = 31;
inline constexpr std::uint8_t dandelion = 37;
inline constexpr std::uint8_t rose = 38;
inline constexpr std::uint8_t brown_mushroom = 39;
inline constexpr std::uint8_t red_mushroom = 40;
inline constexpr std::uint8_t obsidian = 49;
inline constexpr std::uint8_t fire = 51;
// The APK's historical C++ symbol calls this emeraldOre, but block ID 56 and
// its texture/drop behavior are the classic diamond ore block.
inline constexpr std::uint8_t diamond_ore = 56;
inline constexpr std::uint8_t farmland = 60;
inline constexpr std::uint8_t sign = 63;
inline constexpr std::uint8_t wooden_door = 64;
inline constexpr std::uint8_t ladder = 65;
inline constexpr std::uint8_t redstone_ore = 73;
inline constexpr std::uint8_t snow_layer = 78;
inline constexpr std::uint8_t ice = 79;
inline constexpr std::uint8_t cactus = 81;
inline constexpr std::uint8_t clay = 82;
inline constexpr std::uint8_t reeds = 83;
inline constexpr std::uint8_t invisible_bedrock = 95;

[[nodiscard]] constexpr bool is_liquid(std::uint8_t id) noexcept {
    return id == water || id == still_water || id == lava || id == still_lava;
}

[[nodiscard]] constexpr bool is_water(std::uint8_t id) noexcept {
    return id == water || id == still_water;
}

[[nodiscard]] constexpr bool is_lava(std::uint8_t id) noexcept {
    return id == lava || id == still_lava;
}

[[nodiscard]] constexpr bool is_dynamic_liquid(std::uint8_t id) noexcept {
    return id == water || id == lava;
}

[[nodiscard]] constexpr bool same_liquid_material(
    std::uint8_t left,
    std::uint8_t right) noexcept {
    return (is_water(left) && is_water(right))
        || (is_lava(left) && is_lava(right));
}

[[nodiscard]] constexpr bool is_leaves(std::uint8_t id) noexcept {
    return id == leaves;
}

// Tile::solid[] is populated from isSolidRender(), not from material motion.
// LeafTile's graphics byte is zero when Tile::init() snapshots this table, so
// leaves are solid here even though they use reduced opacity and are not
// Level::isSolidBlockingTile(). All four tree features and Mushroom::mayPlaceOn
// consult this immutable table.
[[nodiscard]] constexpr bool tile_solid_snapshot(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case water:
    case still_water:
    case lava:
    case still_lava:
    case ice:
    case fire:
    case sapling:
    case tall_grass:
    case dandelion:
    case rose:
    case brown_mushroom:
    case red_mushroom:
    case farmland:
    case sign:
    case wooden_door:
    case ladder:
    case snow_layer:
    case cactus:
    case reeds:
        return false;
    default:
        return true;
    }
}

// LevelRenderer::allChanged copies Options::fancyGraphics into LeafTile's
// mutable graphics byte without rebuilding Tile::solid[]. LeafTile then
// returns !fancyGraphics from its virtual isSolidRender(). Calls which dispatch
// that virtual (notably Dimension::isValidSpawn and TopSnowTile::mayPlace) must
// therefore use this runtime predicate instead of tile_solid_snapshot().
[[nodiscard]] constexpr bool runtime_is_solid_render(
    std::uint8_t id,
    bool fancy_graphics) noexcept {
    return id == leaves ? !fancy_graphics : tile_solid_snapshot(id);
}

// Material::blocksMotion() differs from Tile::isSolidRender() for transparent
// full blocks (notably leaves and ice).
[[nodiscard]] constexpr bool blocks_motion(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case water:
    case still_water:
    case lava:
    case still_lava:
    case fire:
    case sapling:
    case tall_grass:
    case dandelion:
    case rose:
    case brown_mushroom:
    case red_mushroom:
    case snow_layer:
    case reeds:
        return false;
    default:
        return true;
    }
}

// Material::isSolid(), used by cactus survival, is distinct from both
// blocksMotion() and Tile::isSolidRender().  It is false for gas, liquid, and
// decoration materials but remains true for leaves, ice, and cactus.
[[nodiscard]] constexpr bool material_is_solid(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case water:
    case still_water:
    case lava:
    case still_lava:
    case fire:
    case sapling:
    case tall_grass:
    case dandelion:
    case rose:
    case brown_mushroom:
    case red_mushroom:
    case snow_layer:
    case reeds:
        return false;
    default:
        return true;
    }
}

// Material::isReplaceable(), used by Tile::mayPlace.  Of the PE 0.6.1 tile
// materials, only gas air/fire, liquids, replaceable plants, and top snow set
// the replaceable flag. TallGrass uses the replaceable-plant material.
[[nodiscard]] constexpr bool material_is_replaceable(
    std::uint8_t id) noexcept {
    return id == air || is_liquid(id) || id == fire || id == tall_grass
        || id == snow_layer;
}

// LiquidTileDynamic::isWaterBlocking first handles these four Tile singletons
// explicitly, then returns Material::blocksMotion() || Material::isSolid().
// Unknown IDs are solid in the original Tile/Material tables, so the default
// deliberately remains blocking.
[[nodiscard]] constexpr bool blocks_liquid_flow(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case water:
    case still_water:
    case lava:
    case still_lava:
    case fire:
    case sapling:
    case tall_grass:
    case dandelion:
    case rose:
    case brown_mushroom:
    case red_mushroom:
    case snow_layer:
        return false;
    case wooden_door:
    case sign:
    case ladder:
    case reeds:
        return true;
    default:
        return true;
    }
}

// Level::isSolidBlockingTile combines Material::isSolidBlocking() with
// Tile::isCubeShaped(). Leaves and ice set Material's non-blocking flag even
// though both materials block motion and their tiles are cube-shaped.
[[nodiscard]] constexpr bool is_solid_blocking(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case water:
    case still_water:
    case lava:
    case still_lava:
    case leaves:
    case ice:
    case fire:
    case sapling:
    case tall_grass:
    case dandelion:
    case rose:
    case brown_mushroom:
    case red_mushroom:
    case farmland:
    case sign:
    case wooden_door:
    case ladder:
    case snow_layer:
    case cactus:
    case reeds:
        return false;
    default:
        return true;
    }
}

// Values used by LevelChunk height and skylight maintenance. Generated block
// IDs not listed here retain the default fully-opaque value from Tile::initTiles.
[[nodiscard]] constexpr std::uint8_t light_block(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case fire:
    case sapling:
    case tall_grass:
    case dandelion:
    case rose:
    case brown_mushroom:
    case red_mushroom:
    case farmland:
    case sign:
    case wooden_door:
    case ladder:
    case snow_layer:
    case cactus:
    case reeds:
        return 0;
    case water:
    case still_water:
    case ice:
        return 3;
    case leaves:
        return 1;
    default:
        // Tile::init writes 255 for isSolidRender() tiles.  Dynamic and static
        // lava explicitly override their non-solid-render default to 255 too.
        return 255;
    }
}

// Tile::lightEmission[] stores trunc(15 * emission).  Only lava and the brown
// mushroom can be introduced by the 0.6.1 generation pipeline.
[[nodiscard]] constexpr std::uint8_t light_emission(
    std::uint8_t id) noexcept {
    switch (id) {
    case lava:
    case still_lava:
    case fire:
        return 15;
    case brown_mushroom:
        return 1;
    default:
        return 0;
    }
}

} // namespace mcpe::worldgen::v0_6_1::block
