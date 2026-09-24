#include "v1_1_5_0/hell_features.hpp"

#include "detail/wrap.hpp"
#include "mcpe/worldgen/chunk.hpp"
#include "v1_1_5_0/blocks.hpp"

#include <array>

namespace mcpe::worldgen::v1_1_5_0::hell_feature {
namespace {

using detail::wrapping_add;
using detail::wrapping_sub;

[[nodiscard]] constexpr bool is_air(
    const detail::BlockAccess& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    return world.block(x, y, z) == block::air;
}

[[nodiscard]] constexpr bool mushroom_support(std::uint8_t id) noexcept {
    // MushroomPlantBlock::canBePlacedOn reads Tile::solid[]. All blocks that
    // can occur here but clear that flag are listed explicitly; the tile
    // table's default remains solid for terrain and fortress construction.
    switch (id) {
    case block::air:
    case block::flowing_lava:
    case block::still_lava:
    case block::fire:
    case block::brown_mushroom:
    case block::red_mushroom:
    case block::nether_brick_fence:
    case block::nether_wart:
        return false;
    default:
        return true;
    }
}

[[nodiscard]] constexpr std::array<std::array<std::int32_t, 3>, 6>
    neighbor_offsets{{
        {{-1, 0, 0}}, {{1, 0, 0}}, {{0, -1, 0}},
        {{0, 1, 0}}, {{0, 0, -1}}, {{0, 0, 1}},
    }};

} // namespace

bool place_spring(
    detail::LiquidAccess& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    bool inside_rock_only) noexcept {
    // HellSpringFeature::place. The two native instances differ only in the
    // boolean at object offset +9: the lower pass requires all five exposed
    // sides to be netherrack, whereas the upper pass also permits a one-air
    // opening in a four-netherrack shell.
    if (world.block(x, wrapping_add(y, 1), z) != block::netherrack) {
        return false;
    }
    const std::uint8_t here = world.block(x, y, z);
    if (here != block::air && here != block::netherrack) {
        return false;
    }

    constexpr std::array<std::array<std::int32_t, 3>, 5> sides{{
        {{-1, 0, 0}}, {{1, 0, 0}}, {{0, 0, -1}},
        {{0, 0, 1}}, {{0, -1, 0}},
    }};
    std::int32_t netherrack_sides{};
    std::int32_t air_sides{};
    for (const auto& offset : sides) {
        const std::uint8_t id = world.block(
            wrapping_add(x, offset[0]),
            wrapping_add(y, offset[1]),
            wrapping_add(z, offset[2]));
        netherrack_sides += id == block::netherrack ? 1 : 0;
        air_sides += id == block::air ? 1 : 0;
    }
    if (netherrack_sides != 5
        && (inside_rock_only || netherrack_sides != 4 || air_sides != 1)) {
        return false;
    }
    (void)world.set_block_and_data(
        x, y, z, block::flowing_lava, 0, true);
    return true;
}

bool place_glowstone(
    detail::BlockAccess& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // GlowStoneFeature::place. Both glowstone loops in this APK dispatch to
    // this implementation; their different attempt counts are selected by
    // HellRandomLevelSource rather than by a separate approximation.
    if (!is_air(world, x, y, z)
        || world.block(x, wrapping_add(y, 1), z) != block::netherrack) {
        return false;
    }
    (void)world.set_block_and_data(x, y, z, block::glowstone, 0, true);
    for (std::int32_t attempt = 0; attempt < 1500; ++attempt) {
        // ARM constructs BlockPos right-to-left: Z offsets are consumed
        // first, then Y, then X. The order is seed-visible even though the
        // resulting coordinate expression is symmetric.
        const std::int32_t candidate_z = wrapping_add(
            z,
            static_cast<std::int32_t>(random.next_u32() & 7U)
                - static_cast<std::int32_t>(random.next_u32() & 7U));
        const std::int32_t candidate_y = wrapping_sub(
            y,
            static_cast<std::int32_t>(random.next_bounded(12U)));
        const std::int32_t candidate_x = wrapping_add(
            x,
            static_cast<std::int32_t>(random.next_u32() & 7U)
                - static_cast<std::int32_t>(random.next_u32() & 7U));
        if (!is_air(world, candidate_x, candidate_y, candidate_z)) {
            continue;
        }
        std::int32_t glowstone_neighbors{};
        for (const auto& offset : neighbor_offsets) {
            glowstone_neighbors += world.block(
                wrapping_add(candidate_x, offset[0]),
                wrapping_add(candidate_y, offset[1]),
                wrapping_add(candidate_z, offset[2])) == block::glowstone
                ? 1
                : 0;
        }
        if (glowstone_neighbors == 1) {
            (void)world.set_block_and_data(
                candidate_x, candidate_y, candidate_z, block::glowstone, 0,
                true);
        }
    }
    return true;
}

void place_fire(
    detail::BlockAccess& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // HellFireFeature::place. ARM evaluates the BlockPos fields from Z to X,
    // so its six MT words are Z+, Z-, Y+, Y-, X+, X-.
    for (std::int32_t attempt = 0; attempt < 64; ++attempt) {
        const std::int32_t candidate_z = wrapping_add(
            z,
            static_cast<std::int32_t>(random.next_u32() & 7U)
                - static_cast<std::int32_t>(random.next_u32() & 7U));
        const std::int32_t candidate_y = wrapping_add(
            y,
            static_cast<std::int32_t>(random.next_u32() & 3U)
                - static_cast<std::int32_t>(random.next_u32() & 3U));
        const std::int32_t candidate_x = wrapping_add(
            x,
            static_cast<std::int32_t>(random.next_u32() & 7U)
                - static_cast<std::int32_t>(random.next_u32() & 7U));
        if (is_air(world, candidate_x, candidate_y, candidate_z)
            && world.block(candidate_x, wrapping_sub(candidate_y, 1), candidate_z)
                == block::netherrack) {
            (void)world.set_block_and_data(
                candidate_x, candidate_y, candidate_z, block::fire, 0, true);
        }
    }
}

void place_mushroom(
    detail::BlockAccess& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t mushroom) noexcept {
    // PlantFeature::place with MushroomPlantBlock::canSurvive. The Nether
    // has no generation-time sky light, so raw light is below the mushroom's
    // threshold; Tile::solid[] of the support is the remaining predicate.
    for (std::int32_t attempt = 0; attempt < 64; ++attempt) {
        const std::int32_t candidate_z = wrapping_add(
            z,
            static_cast<std::int32_t>(random.next_u32() & 7U)
                - static_cast<std::int32_t>(random.next_u32() & 7U));
        const std::int32_t candidate_y = wrapping_add(
            y,
            static_cast<std::int32_t>(random.next_u32() & 3U)
                - static_cast<std::int32_t>(random.next_u32() & 3U));
        const std::int32_t candidate_x = wrapping_add(
            x,
            static_cast<std::int32_t>(random.next_u32() & 7U)
                - static_cast<std::int32_t>(random.next_u32() & 7U));
        if (candidate_y < 0 || candidate_y >= Chunk::height
            || !is_air(world, candidate_x, candidate_y, candidate_z)
            || !mushroom_support(world.block(
                candidate_x, wrapping_sub(candidate_y, 1), candidate_z))) {
            continue;
        }
        (void)world.set_block_and_data(
            candidate_x, candidate_y, candidate_z, mushroom, 0, false);
    }
}

} // namespace mcpe::worldgen::v1_1_5_0::hell_feature
