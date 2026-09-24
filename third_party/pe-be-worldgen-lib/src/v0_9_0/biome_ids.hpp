#pragma once

#include <cstdint>

namespace mcpe::worldgen::v0_9_0::biome {

// PE 0.9.0 biome IDs. Mutated variants occupy the original ID plus 128.
inline constexpr std::int32_t ocean = 0;
inline constexpr std::int32_t plains = 1;
inline constexpr std::int32_t desert = 2;
inline constexpr std::int32_t extreme_hills = 3;
inline constexpr std::int32_t forest = 4;
inline constexpr std::int32_t taiga = 5;
inline constexpr std::int32_t swampland = 6;
inline constexpr std::int32_t river = 7;
inline constexpr std::int32_t frozen_ocean = 10;
inline constexpr std::int32_t frozen_river = 11;
inline constexpr std::int32_t ice_flats = 12;
inline constexpr std::int32_t ice_mountains = 13;
inline constexpr std::int32_t mushroom_island = 14;
inline constexpr std::int32_t mushroom_island_shore = 15;
inline constexpr std::int32_t beach = 16;
inline constexpr std::int32_t desert_hills = 17;
inline constexpr std::int32_t forest_hills = 18;
inline constexpr std::int32_t taiga_hills = 19;
inline constexpr std::int32_t smaller_extreme_hills = 20;
inline constexpr std::int32_t jungle = 21;
inline constexpr std::int32_t jungle_hills = 22;
inline constexpr std::int32_t jungle_edge = 23;
inline constexpr std::int32_t deep_ocean = 24;
inline constexpr std::int32_t stone_beach = 25;
inline constexpr std::int32_t cold_beach = 26;
inline constexpr std::int32_t birch_forest = 27;
inline constexpr std::int32_t birch_forest_hills = 28;
inline constexpr std::int32_t roofed_forest = 29;
inline constexpr std::int32_t cold_taiga = 30;
inline constexpr std::int32_t cold_taiga_hills = 31;
inline constexpr std::int32_t mega_taiga = 32;
inline constexpr std::int32_t mega_taiga_hills = 33;
inline constexpr std::int32_t extreme_hills_plus = 34;
inline constexpr std::int32_t savanna = 35;
inline constexpr std::int32_t savanna_plateau = 36;
inline constexpr std::int32_t mesa = 37;
inline constexpr std::int32_t mesa_plateau_forest = 38;
inline constexpr std::int32_t mesa_plateau = 39;

enum class Type : std::int32_t {
    unknown = -1,
    beach = 0,
    desert = 1,
    extreme_hills = 2,
    forest = 4,
    ice = 6,
    jungle = 7,
    mesa = 8,
    mushroom = 9,
    ocean = 10,
    plains = 11,
    river = 12,
    savanna = 13,
    stone_beach = 14,
    swamp = 15,
    taiga = 16,
};

enum class TemperatureCategory : std::int32_t {
    unknown = 0,
    cold = 1,
    medium = 2,
    warm = 3,
};

[[nodiscard]] constexpr bool is_ocean(std::int32_t id) noexcept {
    return id == ocean || id == deep_ocean || id == frozen_ocean;
}

[[nodiscard]] constexpr bool is_snow_covered(std::int32_t id) noexcept {
    const std::int32_t base = id >= 128 ? id - 128 : id;
    return base == frozen_ocean || base == frozen_river || base == ice_flats
        || base == ice_mountains || base == cold_beach || base == cold_taiga
        || base == cold_taiga_hills;
}

[[nodiscard]] constexpr TemperatureCategory temperature_category(
    std::int32_t id) noexcept {
    const std::int32_t base = id >= 128 ? id - 128 : id;
    switch (base) {
    case frozen_ocean:
    case frozen_river:
    case ice_flats:
    case ice_mountains:
    case cold_beach:
    case cold_taiga:
    case cold_taiga_hills:
        return TemperatureCategory::cold;

    case desert:
    case desert_hills:
    case savanna:
    case savanna_plateau:
    case mesa:
    case mesa_plateau_forest:
    case mesa_plateau:
        return TemperatureCategory::warm;

    case ocean:
    case plains:
    case extreme_hills:
    case forest:
    case taiga:
    case swampland:
    case river:
    case mushroom_island:
    case mushroom_island_shore:
    case beach:
    case forest_hills:
    case taiga_hills:
    case smaller_extreme_hills:
    case jungle:
    case jungle_hills:
    case jungle_edge:
    case deep_ocean:
    case stone_beach:
    case birch_forest:
    case birch_forest_hills:
    case roofed_forest:
    case mega_taiga:
    case mega_taiga_hills:
    case extreme_hills_plus:
        return TemperatureCategory::medium;

    default:
        return TemperatureCategory::unknown;
    }
}

[[nodiscard]] constexpr Type type_of(std::int32_t id) noexcept {
    const std::int32_t base = id >= 128 ? id - 128 : id;
    switch (base) {
    case beach:
    case cold_beach:
        return Type::beach;
    case desert:
    case desert_hills:
        return Type::desert;
    case extreme_hills:
    case smaller_extreme_hills:
    case extreme_hills_plus:
        return Type::extreme_hills;
    case forest:
    case forest_hills:
    case birch_forest:
    case birch_forest_hills:
    case roofed_forest:
        return Type::forest;
    case ice_flats:
    case ice_mountains:
        return Type::ice;
    case jungle:
    case jungle_hills:
    case jungle_edge:
        return Type::jungle;
    case mesa:
    case mesa_plateau_forest:
    case mesa_plateau:
        return Type::mesa;
    case mushroom_island:
    case mushroom_island_shore:
        return Type::mushroom;
    case ocean:
    case frozen_ocean:
    case deep_ocean:
        return Type::ocean;
    case plains:
        return Type::plains;
    case river:
    case frozen_river:
        return Type::river;
    case savanna:
    case savanna_plateau:
        return Type::savanna;
    case stone_beach:
        return Type::stone_beach;
    case swampland:
        return Type::swamp;
    case taiga:
    case taiga_hills:
    case cold_taiga:
    case cold_taiga_hills:
    case mega_taiga:
    case mega_taiga_hills:
        return Type::taiga;
    default:
        return Type::unknown;
    }
}

[[nodiscard]] constexpr bool is_same(std::int32_t left, std::int32_t right) noexcept {
    if (left == right) {
        return true;
    }
    // The layer helper checks the left operand for these two plateau forms
    // before it calls Biome::isSame. Preserve that ordered exception: a mesa
    // biome is otherwise compared through its stored biome type below.
    if (left == mesa_plateau_forest) {
        return right == mesa_plateau;
    }
    if (left == mesa_plateau) {
        return right == mesa_plateau_forest;
    }
    const Type left_type = type_of(left);
    return left_type != Type::unknown && left_type == type_of(right);
}

} // namespace mcpe::worldgen::v0_9_0::biome
