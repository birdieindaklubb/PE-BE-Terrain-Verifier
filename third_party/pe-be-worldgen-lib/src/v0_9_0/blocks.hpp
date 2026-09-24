#pragma once

#include <cstdint>

namespace mcpe::worldgen::v0_9_0::block {

// Read from Tile::initTiles in the target APK.  These are tile IDs, not block
// states: PE 0.9.0 stores the low four state bits in a separate nibble array.
inline constexpr std::uint8_t air = 0;
inline constexpr std::uint8_t rock = 1;
inline constexpr std::uint8_t grass = 2;
inline constexpr std::uint8_t dirt = 3;
inline constexpr std::uint8_t cobblestone = 4;
inline constexpr std::uint8_t planks = 5;
inline constexpr std::uint8_t cloth = 35;
inline constexpr std::uint8_t thin_glass = 102;
inline constexpr std::uint8_t log = 17;
inline constexpr std::uint8_t farmland = 60;
inline constexpr std::uint8_t unbreakable = 7;
inline constexpr std::uint8_t water = 8;
inline constexpr std::uint8_t calm_water = 9;
inline constexpr std::uint8_t lava = 10;
inline constexpr std::uint8_t calm_lava = 11;
inline constexpr std::uint8_t sand = 12;
inline constexpr std::uint8_t gravel = 13;
inline constexpr std::uint8_t sand_stone = 24;
inline constexpr std::uint8_t stone_slab = 44;
inline constexpr std::uint8_t double_stone_slab = 43;
inline constexpr std::uint8_t obsidian = 49;
inline constexpr std::uint8_t bookshelf = 47;
inline constexpr std::uint8_t workbench = 58;
inline constexpr std::uint8_t furnace = 61;
// The target's historic C++ names are unintuitive: `Tile::stoneBrick` is the
// ordinary cobblestone tile (4), while the stronghold material is the later
// `Tile::stoneBrickSmooth` tile (98). Keep both names explicit so a source
// label cannot silently turn a cobblestone village wall into stone bricks.
inline constexpr std::uint8_t stone_brick = 4;
inline constexpr std::uint8_t stone_brick_smooth = 98;
inline constexpr std::uint8_t monster_egg = 97;
inline constexpr std::uint8_t mossy_cobblestone = 48;
inline constexpr std::uint8_t mob_spawner = 52;
inline constexpr std::uint8_t end_portal = 119;
inline constexpr std::uint8_t end_portal_frame = 120;
inline constexpr std::uint8_t chest = 54;
inline constexpr std::uint8_t rail = 66;
inline constexpr std::uint8_t wood_stairs = 53;
inline constexpr std::uint8_t stone_stairs = 67;
inline constexpr std::uint8_t stone_brick_stairs = 109;
inline constexpr std::uint8_t sandstone_stairs = 128;
inline constexpr std::uint8_t ladder = 65;
inline constexpr std::uint8_t wooden_door = 64;
inline constexpr std::uint8_t sign = 63;
inline constexpr std::uint8_t crops = 59;
inline constexpr std::uint8_t carrots = 141;
inline constexpr std::uint8_t potatoes = 142;
inline constexpr std::uint8_t brown_mushroom_block = 99;
inline constexpr std::uint8_t red_mushroom_block = 100;
inline constexpr std::uint8_t melon = 103;
inline constexpr std::uint8_t leaves = 18;
inline constexpr std::uint8_t leaves2 = 161;
inline constexpr std::uint8_t log2 = 162;
inline constexpr std::uint8_t yellow_flower = 37;
inline constexpr std::uint8_t red_flower = 38;
inline constexpr std::uint8_t brown_mushroom = 39;
inline constexpr std::uint8_t red_mushroom = 40;
inline constexpr std::uint8_t tall_grass = 31;
inline constexpr std::uint8_t dead_bush = 32;
inline constexpr std::uint8_t cactus = 81;
inline constexpr std::uint8_t fence = 85;
inline constexpr std::uint8_t iron_fence = 101;
inline constexpr std::uint8_t reeds = 83;
inline constexpr std::uint8_t clay = 82;
inline constexpr std::uint8_t pumpkin = 86;
inline constexpr std::uint8_t vine = 106;
inline constexpr std::uint8_t web = 30;
inline constexpr std::uint8_t torch = 50;
inline constexpr std::uint8_t cocoa = 127;
inline constexpr std::uint8_t coal_ore = 16;
inline constexpr std::uint8_t lapis_ore = 21;
inline constexpr std::uint8_t gold_ore = 14;
inline constexpr std::uint8_t iron_ore = 15;
inline constexpr std::uint8_t diamond_ore = 56;
inline constexpr std::uint8_t redstone_ore = 73;
inline constexpr std::uint8_t emerald_ore = 129;
// The engine has two distinct snow tiles.  World post-processing writes the
// one-pixel TopSnowTile (78); the ordinary SnowTile (80) is a full block and
// is used by the snowy-biome surface definition.
inline constexpr std::uint8_t top_snow = 78;
inline constexpr std::uint8_t ice = 79;
inline constexpr std::uint8_t snow = 80;
inline constexpr std::uint8_t mycelium = 110;
inline constexpr std::uint8_t waterlily = 111;
inline constexpr std::uint8_t stained_clay = 159;
inline constexpr std::uint8_t hardened_clay = 172;
inline constexpr std::uint8_t packed_ice = 174;
inline constexpr std::uint8_t double_plant = 175;
inline constexpr std::uint8_t wool_carpet = 171;
inline constexpr std::uint8_t podzol = 243;

// Tile::lightBlock values used by LevelChunk::recalcHeightmap.  This is kept
// local to 0.9.0: sharing the 0.6.1 table would make new tiles such as a lily
// pad incorrectly opaque while the chunk's initial height map is built.
[[nodiscard]] constexpr std::uint8_t light_block(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case top_snow:
    case ice:
    case web:
    // LeafTile explicitly clears Tile::lightBlock for both leaf variants.
    // This matters to the final freeze/snow pass, which asks for the first
    // light-blocking tile rather than treating foliage as terrain.
    case leaves:
    case leaves2:
    case waterlily:
    case double_plant:
    // Bush, CactusTile, VineTile and CocoaTile all clear both Tile tables.
    // That covers every generated small plant, including the crop family.
    case yellow_flower:
    case red_flower:
    case brown_mushroom:
    case red_mushroom:
    case tall_grass:
    case dead_bush:
    case cactus:
    case reeds:
    case vine:
    case cocoa:
    // EndPortalFrameTile keeps its stone material but clears the Tile-level
    // opacity flag.  Stronghold portal rooms can therefore be present before
    // the final height/light refresh without behaving as full opaque terrain.
    case end_portal_frame:
    // FenceTile, TorchTile, Bush (and therefore crops/carrot/potato), and
    // WoolCarpetTile each explicitly clear Tile::lightBlock in the target.
    case fence:
    case iron_fence:
    case torch:
    case rail:
    case chest:
    case stone_slab:
    case wood_stairs:
    case stone_stairs:
    case stone_brick_stairs:
    case sandstone_stairs:
    case wooden_door:
    case crops:
    case carrots:
    case potatoes:
    case wool_carpet:
    // ThinGlassTile, DoorTile, ChestTile, StairTile and SlabTile each set
    // their own non-opaque Tile flags. Villages and strongholds place all of
    // these before the final snow/light update.
    case thin_glass:
    case ladder:
        return 0;
    case water:
    case calm_water:
        return 3;
    default:
        // Tile::initTiles uses 255 for the normal full opaque tile default.
        return 255;
    }
}

[[nodiscard]] constexpr bool is_liquid(std::uint8_t id) noexcept {
    return id == water || id == calm_water || id == lava || id == calm_lava;
}

[[nodiscard]] constexpr bool is_water(std::uint8_t id) noexcept {
    return id == water || id == calm_water;
}

[[nodiscard]] constexpr bool is_lava(std::uint8_t id) noexcept {
    return id == lava || id == calm_lava;
}

// Tile::lightEmission entries reached by the implemented world-generation
// path. These are deliberately separate from light opacity: lava remains an
// opaque emitter at the maximum level, while a torch emits fourteen. The
// remaining generated tiles retain Tile's zero default.
[[nodiscard]] constexpr std::uint8_t light_emission(std::uint8_t id) noexcept {
    switch (id) {
    case lava:
    case calm_lava:
        return 15;
    case torch:
        return 14;
    default:
        return 0;
    }
}

// Material::isSolid().  LakeFeature and CactusTile query this field (byte
// 0x0d), which is deliberately distinct from both blocksMotion and Tile::solid.
[[nodiscard]] constexpr bool material_is_solid(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case water:
    case calm_water:
    case lava:
    case calm_lava:
    case top_snow:
    case waterlily:
    case double_plant:
    case tall_grass:
    case dead_bush:
    case reeds:
    case yellow_flower:
    case red_flower:
    case brown_mushroom:
    case red_mushroom:
    case web:
        return false;
    default:
        return true;
    }
}

// Material::blocksMotion() (byte 0x0c).  The only generation-reachable
// difference from Material::isSolid is not a terrain block but this remains a
// separate predicate so native call sites cannot be accidentally conflated.
[[nodiscard]] constexpr bool material_blocks_motion(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case water:
    case calm_water:
    case lava:
    case calm_lava:
    case top_snow:
    case waterlily:
    case double_plant:
    case tall_grass:
    case dead_bush:
    case reeds:
    case yellow_flower:
    case red_flower:
    case brown_mushroom:
    case red_mushroom:
    case web:
        return false;
    default:
        return true;
    }
}

// Tile::solid[] is initialized true by Tile then selectively cleared by tile
// constructors.  This table covers every tile that the implemented generator
// can create or encounter while deciding a generation write.  It must not be
// replaced with a material query: chests, leaves, half slabs, and cacti prove
// that the two engine concepts differ.
[[nodiscard]] constexpr bool tile_solid_snapshot(std::uint8_t id) noexcept {
    switch (id) {
    case air:
    case water:
    case calm_water:
    case lava:
    case calm_lava:
    case top_snow:
    case ice:
    case stone_slab:
    case chest:
    case leaves:
    case leaves2:
    case yellow_flower:
    case red_flower:
    case brown_mushroom:
    case red_mushroom:
    case web:
    case tall_grass:
    case dead_bush:
    case cactus:
    case reeds:
    case waterlily:
    case double_plant:
    case end_portal_frame:
    case vine:
    case cocoa:
    // FenceTile, TorchTile, Bush (including each crop), and
    // WoolCarpetTile explicitly clear Tile::solid in PE 0.9.0.
    case fence:
    case iron_fence:
    case torch:
    case rail:
    case wood_stairs:
    case stone_stairs:
    case stone_brick_stairs:
    case sandstone_stairs:
    case wooden_door:
    case crops:
    case carrots:
    case potatoes:
    case wool_carpet:
    case thin_glass:
    case ladder:
        return false;
    default:
        return true;
    }
}

[[nodiscard]] constexpr bool is_leaf_material(std::uint8_t id) noexcept {
    return id == leaves || id == leaves2;
}

[[nodiscard]] constexpr bool bush_support(std::uint8_t id) noexcept {
    return id == grass || id == dirt || id == farmland || id == podzol;
}

[[nodiscard]] constexpr bool dead_bush_support(std::uint8_t id) noexcept {
    return id == sand || id == hardened_clay || id == stained_clay;
}

} // namespace mcpe::worldgen::v0_9_0::block
