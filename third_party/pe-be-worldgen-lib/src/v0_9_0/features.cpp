#include "v0_9_0/features.hpp"

#include "detail/fp.hpp"
#include "detail/wrap.hpp"
#include "v0_6_1/legacy_math.hpp"
#include "v0_9_0/blocks.hpp"

#include <array>
#include <cstdint>

namespace mcpe::worldgen::v0_9_0::feature {
namespace {

using detail::fp::add;
using detail::fp::div;
using detail::fp::mul;
using detail::fp::sub;

constexpr float pi = 3.14159274F; // Mth::PI, bits 0x40490fdb

[[nodiscard]] float interpolate_native(
    float first,
    float second,
    std::int32_t step,
    std::int32_t count) noexcept {
    // OreFeature forms 1/count once, then performs two VFP multiplies.
    const float inverse_count = div(1.0F, static_cast<float>(count));
    const float offset = mul(
        mul(static_cast<float>(step), sub(second, first)), inverse_count);
    return add(first, offset);
}

[[nodiscard]] float ore_phase_sine(
    std::int32_t step,
    std::int32_t size) noexcept {
    // The APK expands Mth::sin inline: `(step * PI / size) * 10430.3779`,
    // then VCVT.S32 and a 16-bit table index. Reuse the one shared Mth table
    // implementation rather than duplicate its 65,536 exact entries.
    const float phase = div(
        mul(static_cast<float>(step), pi), static_cast<float>(size));
    return v0_6_1::legacy_math::sin(phase);
}

void decorate_depth_span(
    World& world,
    detail::Mt19937& random,
    std::int32_t base_x,
    std::int32_t base_y,
    std::int32_t base_z,
    std::int32_t count,
    std::uint8_t output_id,
    std::uint8_t output_data,
    std::int32_t size,
    std::int32_t min_y,
    std::int32_t max_y) noexcept {
    for (std::int32_t occurrence = 0; occurrence < count; ++occurrence) {
        // decorateDepthSpan consumes X, then Y, then Z in that exact order.
        const std::int32_t x = detail::wrapping_add(
            base_x, static_cast<std::int32_t>(random.next_u32() & 15U));
        const std::int32_t y = detail::wrapping_add(
            base_y,
            detail::wrapping_add(
                min_y,
                static_cast<std::int32_t>(
                    random.next_bounded(static_cast<std::uint32_t>(max_y - min_y)))));
        const std::int32_t z = detail::wrapping_add(
            base_z, static_cast<std::int32_t>(random.next_u32() & 15U));
        place_ore(world, random, x, y, z, output_id, output_data, size);
    }
}

void decorate_depth_average(
    World& world,
    detail::Mt19937& random,
    std::int32_t base_x,
    std::int32_t base_y,
    std::int32_t base_z,
    std::int32_t count,
    std::uint8_t output_id,
    std::uint8_t output_data,
    std::int32_t size,
    std::int32_t minimum,
    std::int32_t span) noexcept {
    for (std::int32_t occurrence = 0; occurrence < count; ++occurrence) {
        // decorateDepthAverage consumes X, then two Y values, then Z.
        const std::int32_t x = detail::wrapping_add(
            base_x, static_cast<std::int32_t>(random.next_u32() & 15U));
        const std::int32_t y_offset = static_cast<std::int32_t>(
            random.next_bounded(static_cast<std::uint32_t>(span)))
            + static_cast<std::int32_t>(
                random.next_bounded(static_cast<std::uint32_t>(span)));
        const std::int32_t y = detail::wrapping_add(
            base_y, detail::wrapping_add(minimum - span, y_offset));
        const std::int32_t z = detail::wrapping_add(
            base_z, static_cast<std::int32_t>(random.next_u32() & 15U));
        place_ore(world, random, x, y, z, output_id, output_data, size);
    }
}

[[nodiscard]] bool can_place_bush(
    const World& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // Feature call sites already require TileSource::isEmptyTile. Bush's
    // virtual mayPlaceOn accepts exactly these four support tiles in 0.9.0.
    return world.block(x, y, z) == block::air
        && block::bush_support(world.block(x, y - 1, z));
}

[[nodiscard]] bool cactus_can_survive(
    const World& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // CactusTile::canSurvive,.
    if (block::material_is_solid(world.block(x - 1, y, z))
        || block::material_is_solid(world.block(x + 1, y, z))
        || block::material_is_solid(world.block(x, y, z - 1))
        || block::material_is_solid(world.block(x, y, z + 1))) {
        return false;
    }
    const std::uint8_t below = world.block(x, y - 1, z);
    return below == block::cactus || below == block::sand;
}

[[nodiscard]] bool reeds_can_survive(
    const World& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    const std::uint8_t below = world.block(x, y - 1, z);
    if (below == block::reeds) {
        return true;
    }
    if (below != block::grass && below != block::dirt && below != block::sand) {
        return false;
    }
    return block::is_water(world.block(x - 1, y - 1, z))
        || block::is_water(world.block(x + 1, y - 1, z))
        || block::is_water(world.block(x, y - 1, z - 1))
        || block::is_water(world.block(x, y - 1, z + 1));
}

[[nodiscard]] bool mushroom_can_survive(
    const World& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // Mushroom::canSurvive,. Podzol/mycelium bypass the brightness
    // check; every other support tile needs both a raw-brightness value below
    // thirteen and a true Tile::solid[] entry.
    if (static_cast<std::uint32_t>(y) >= static_cast<std::uint32_t>(Chunk::height)) {
        return false;
    }
    const std::uint8_t below = world.block(x, y - 1, z);
    return below == block::podzol || below == block::mycelium
        || (world.raw_brightness(x, y, z) < 13U
            && block::tile_solid_snapshot(below));
}

[[nodiscard]] constexpr bool is_solid_blocking_tile(
    std::uint8_t id) noexcept {
    // TileSource::isSolidBlockingTile,. This must remain distinct
    // from the material-only predicates used by lake and cactus generation.
    return !block::is_liquid(id)
        && block::material_blocks_motion(id)
        && block::tile_solid_snapshot(id);
}

struct ScatterPosition final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
};

[[nodiscard]] ScatterPosition scatter_8_4_8(
    detail::Mt19937& random,
    std::int32_t base_x,
    std::int32_t base_y,
    std::int32_t base_z) noexcept {
    // FlowerFeature, TallGrassFeature, CactusFeature, WaterlilyFeature, and
    // DeadBushFeature all draw their three signed offsets in this sequence.
    const std::uint32_t x_positive = random.next_u32();
    const std::uint32_t x_negative = random.next_u32();
    const std::uint32_t y_positive = random.next_u32();
    const std::uint32_t y_negative = random.next_u32();
    const std::uint32_t z_positive = random.next_u32();
    const std::uint32_t z_negative = random.next_u32();
    return {
        detail::wrapping_add(
            base_x, static_cast<std::int32_t>(x_positive & 7U)
                - static_cast<std::int32_t>(x_negative & 7U)),
        detail::wrapping_add(
            base_y, static_cast<std::int32_t>(y_positive & 3U)
                - static_cast<std::int32_t>(y_negative & 3U)),
        detail::wrapping_add(
            base_z, static_cast<std::int32_t>(z_positive & 7U)
                - static_cast<std::int32_t>(z_negative & 7U)),
    };
}

} // namespace

void place_ore(
    detail::BlockAccess& world,
    detail::Mt19937& random,
    std::int32_t base_x,
    std::int32_t base_y,
    std::int32_t base_z,
    std::uint8_t output_id,
    std::uint8_t output_data,
    std::int32_t size,
    std::uint8_t replace_id,
    std::int32_t vertical_offset,
    std::uint8_t alternate_replace_id,
    bool inclusive_steps) noexcept {
    const float angle_index = mul(random.next_float(), 32768.0F);
    const auto angle = static_cast<std::uint16_t>(
        detail::fp::trunc_to_i32(angle_index));
    const float sine = v0_6_1::legacy_math::sine_table_value(angle);
    const float cosine = v0_6_1::legacy_math::sine_table_value(
        static_cast<std::uint16_t>(angle + 16'384U));
    const float extent = mul(static_cast<float>(size), 0.125F);
    const float origin_x = static_cast<float>(detail::wrapping_add(base_x, 8));
    const float origin_z = static_cast<float>(detail::wrapping_add(base_z, 8));
    const float first_x = add(origin_x, mul(extent, sine));
    const float second_x = sub(origin_x, mul(extent, sine));
    const float first_z = add(origin_z, mul(extent, cosine));
    const float second_z = sub(origin_z, mul(extent, cosine));
    const float first_y = static_cast<float>(detail::wrapping_add(
        base_y,
        detail::wrapping_add(
            static_cast<std::int32_t>(random.next_bounded(3U)), vertical_offset)));
    const float second_y = static_cast<float>(detail::wrapping_add(
        base_y,
        detail::wrapping_add(
            static_cast<std::int32_t>(random.next_bounded(3U)), vertical_offset)));

    const std::int32_t step_limit = inclusive_steps ? size + 1 : size;
    for (std::int32_t step = 0; step < step_limit; ++step) {
        const float center_x = interpolate_native(first_x, second_x, step, size);
        const float center_y = interpolate_native(first_y, second_y, step, size);
        const float center_z = interpolate_native(first_z, second_z, step, size);
        const float random_radius = mul(
            mul(static_cast<float>(size), 0.0625F), random.next_float());
        const float sine_phase = ore_phase_sine(step, size);
        // ARM evaluates `(r + sin(phase) * r) *.5 +.5`, not the algebraic
        // reordering `1 + (sin(phase)+1)*r` used by some later ports.
        const float diameter = add(
            mul(add(random_radius, mul(sine_phase, random_radius)), 0.5F),
            0.5F);
        const float inverse_diameter = div(1.0F, diameter);

        const std::int32_t min_x = detail::fp::trunc_to_i32(sub(center_x, diameter));
        const std::int32_t max_x = detail::fp::trunc_to_i32(add(center_x, diameter));
        const std::int32_t min_y = detail::fp::trunc_to_i32(sub(center_y, diameter));
        const std::int32_t max_y = detail::fp::trunc_to_i32(add(center_y, diameter));
        const std::int32_t min_z = detail::fp::trunc_to_i32(sub(center_z, diameter));
        const std::int32_t max_z = detail::fp::trunc_to_i32(add(center_z, diameter));

        for (std::int32_t x = min_x; x <= max_x; ++x) {
            const float normalized_x = mul(
                inverse_diameter,
                add(static_cast<float>(x), sub(0.5F, center_x)));
            const float squared_x = mul(normalized_x, normalized_x);
            if (squared_x >= 1.0F) {
                continue;
            }
            for (std::int32_t y = min_y; y <= max_y; ++y) {
                const float normalized_y = mul(
                    inverse_diameter,
                    add(static_cast<float>(y), sub(0.5F, center_y)));
                const float squared_xy = add(squared_x, mul(normalized_y, normalized_y));
                if (squared_xy >= 1.0F) {
                    continue;
                }
                for (std::int32_t z = min_z; z <= max_z; ++z) {
                    const float normalized_z = mul(
                        inverse_diameter,
                        add(static_cast<float>(z), sub(0.5F, center_z)));
                    if (add(squared_xy, mul(normalized_z, normalized_z)) < 1.0F
                        && (world.block(x, y, z) == replace_id
                            || world.block(x, y, z) == alternate_replace_id)
                        && world.block(x, y, z) != output_id) {
                        world.set_block_and_data(
                            x, y, z, output_id, output_data, false);
                    }
                }
            }
        }
    }
}

void decorate_ores(
    World& world,
    detail::Mt19937& random,
    std::int32_t base_x,
    std::int32_t base_y,
    std::int32_t base_z) noexcept {
    decorate_depth_span(world, random, base_x, base_y, base_z, 10, block::dirt, 0, 32, 0, 128);
    decorate_depth_span(world, random, base_x, base_y, base_z, 8, block::gravel, 0, 32, 0, 128);
    if ((random.next_u32() & 15U) == 0U) {
        decorate_depth_span(world, random, base_x, base_y, base_z, 80, block::gravel, 0, 32, 0, 50);
    }
    decorate_depth_span(world, random, base_x, base_y, base_z, 10, block::rock, 3, 32, 0, 80);
    decorate_depth_span(world, random, base_x, base_y, base_z, 10, block::rock, 1, 32, 0, 80);
    decorate_depth_span(world, random, base_x, base_y, base_z, 10, block::rock, 5, 32, 0, 80);
    decorate_depth_span(world, random, base_x, base_y, base_z, 20, block::coal_ore, 0, 16, 0, 128);
    decorate_depth_span(world, random, base_x, base_y, base_z, 20, block::iron_ore, 0, 8, 0, 64);
    decorate_depth_span(world, random, base_x, base_y, base_z, 2, block::gold_ore, 0, 8, 0, 32);
    decorate_depth_span(world, random, base_x, base_y, base_z, 8, block::redstone_ore, 0, 7, 0, 16);
    decorate_depth_span(world, random, base_x, base_y, base_z, 1, block::diamond_ore, 0, 7, 0, 16);
    decorate_depth_average(world, random, base_x, base_y, base_z, 1, block::lapis_ore, 0, 6, 16, 16);
}

bool place_lake(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t liquid_id) noexcept {
    // LakeFeature subtracts eight in X/Z before locating the first non-air
    // tile in the supplied column. Its later local Y zero is `y - 4`.
    const std::int32_t base_x = detail::wrapping_sub(x, 8);
    const std::int32_t base_z = detail::wrapping_sub(z, 8);
    while (y > 0 && world.block(base_x, y, base_z) == block::air) {
        --y;
    }
    const std::int32_t base_y = detail::wrapping_sub(y, 4);

    std::array<std::uint8_t, 16 * 16 * 8> mask{};
    const auto mask_index = [](std::int32_t local_x,
                               std::int32_t local_z,
                               std::int32_t local_y) {
        return static_cast<std::size_t>(
            (local_x * 16 + local_z) * 8 + local_y);
    };
    const auto occupied = [&mask, &mask_index](
                              std::int32_t local_x,
                              std::int32_t local_z,
                              std::int32_t local_y) {
        return mask[mask_index(local_x, local_z, local_y)] != 0U;
    };

    const std::int32_t ellipsoid_count =
        static_cast<std::int32_t>(random.next_u32() & 3U) + 4;
    for (std::int32_t ellipsoid = 0;
         ellipsoid < ellipsoid_count;
         ++ellipsoid) {
        // Keep these fused-operation boundaries explicit: moving an
        // intermediate result can change binary32 rounding and later terrain.
        const float diameter_x = detail::fp::mul_add(
            3.0F, random.next_float(), 6.0F);
        const float half_x = mul(diameter_x, 0.5F);
        const float diameter_y = detail::fp::mul_add(
            2.0F, random.next_float(), 4.0F);
        const float half_y = mul(diameter_y, 0.5F);
        const float diameter_z = detail::fp::mul_add(
            3.0F, random.next_float(), 6.0F);
        const float half_z = mul(diameter_z, 0.5F);

        const float random_x = random.next_float();
        const float random_y = random.next_float();
        const float random_z = random.next_float();
        const float x_offset = detail::fp::mul_sub(
            sub(-1.0F, half_x), sub(14.0F, diameter_x), random_x);
        const float center_y = detail::fp::mul_add(
            add(half_y, 2.0F), sub(4.0F, diameter_y), random_y);
        const float z_offset = detail::fp::mul_sub(
            sub(-1.0F, half_z), sub(14.0F, diameter_z), random_z);
        const float inverse_half_x = div(1.0F, half_x);
        const float inverse_half_y = div(1.0F, half_y);
        const float inverse_half_z = div(1.0F, half_z);

        for (std::int32_t local_x = 1; local_x < 15; ++local_x) {
            const float normalized_x = mul(
                add(static_cast<float>(local_x), x_offset), inverse_half_x);
            const float x_squared = mul(normalized_x, normalized_x);
            for (std::int32_t local_z = 1; local_z < 15; ++local_z) {
                const float normalized_z = mul(
                    add(static_cast<float>(local_z), z_offset), inverse_half_z);
                const float xz_squared = add(
                    x_squared, mul(normalized_z, normalized_z));
                for (std::int32_t local_y = 1; local_y < 7; ++local_y) {
                    const float normalized_y = mul(
                        sub(static_cast<float>(local_y), center_y), inverse_half_y);
                    if (add(xz_squared, mul(normalized_y, normalized_y)) < 1.0F) {
                        mask[mask_index(local_x, local_z, local_y)] = 1U;
                    }
                }
            }
        }
    }

    const auto boundary = [&occupied](
                              std::int32_t local_x,
                              std::int32_t local_z,
                              std::int32_t local_y) {
        return !occupied(local_x, local_z, local_y)
            && ((local_x < 15 && occupied(local_x + 1, local_z, local_y))
                || (local_x > 0 && occupied(local_x - 1, local_z, local_y))
                || (local_z < 15 && occupied(local_x, local_z + 1, local_y))
                || (local_z > 0 && occupied(local_x, local_z - 1, local_y))
                || (local_y < 7 && occupied(local_x, local_z, local_y + 1))
                || (local_y > 0 && occupied(local_x, local_z, local_y - 1)));
    };

    // Native validation iterates X, then Z, then Y. A liquid wall cannot be
    // cut above the water line; an empty/non-solid lower wall is permitted
    // only when it already contains the lake's own liquid.
    for (std::int32_t local_x = 0; local_x < 16; ++local_x) {
        for (std::int32_t local_z = 0; local_z < 16; ++local_z) {
            for (std::int32_t local_y = 0; local_y < 8; ++local_y) {
                if (!boundary(local_x, local_z, local_y)) {
                    continue;
                }
                const std::uint8_t id = world.block(
                    detail::wrapping_add(base_x, local_x),
                    detail::wrapping_add(base_y, local_y),
                    detail::wrapping_add(base_z, local_z));
                if (local_y >= 4) {
                    if (block::is_liquid(id)) {
                        return false;
                    }
                } else if (!block::material_is_solid(id) && id != liquid_id) {
                    return false;
                }
            }
        }
    }

    for (std::int32_t local_x = 0; local_x < 16; ++local_x) {
        for (std::int32_t local_z = 0; local_z < 16; ++local_z) {
            for (std::int32_t local_y = 0; local_y < 8; ++local_y) {
                if (!occupied(local_x, local_z, local_y)) {
                    continue;
                }
                world.set_block_and_data(
                    detail::wrapping_add(base_x, local_x),
                    detail::wrapping_add(base_y, local_y),
                    detail::wrapping_add(base_z, local_z),
                    local_y < 4 ? liquid_id : block::air,
                    0,
                    false);
            }
        }
    }

    // This is unrolled in the ARM routine for mask layers 4 through 7, not a
    // single conventional surface repair. Preserve its ascending Y order.
    for (std::int32_t local_x = 0; local_x < 16; ++local_x) {
        for (std::int32_t local_z = 0; local_z < 16; ++local_z) {
            for (std::int32_t local_y = 4; local_y < 8; ++local_y) {
                if (!occupied(local_x, local_z, local_y)) {
                    continue;
                }
                const std::int32_t world_x = detail::wrapping_add(base_x, local_x);
                const std::int32_t world_y = detail::wrapping_add(base_y, local_y);
                const std::int32_t world_z = detail::wrapping_add(base_z, local_z);
                if (world.block(world_x, world_y - 1, world_z) == block::dirt
                    && world.sky_brightness(world_x, world_y, world_z) != 0U) {
                    world.set_block_and_data(
                        world_x, world_y - 1, world_z, block::grass, 0, false);
                }
            }
        }
    }

    if (!block::is_lava(liquid_id)) {
        return true;
    }

    // Lava lakes seal solid boundary cells with rock. The random coin flip is
    // only drawn for upper-half cells, matching the native short circuit.
    for (std::int32_t local_x = 0; local_x < 16; ++local_x) {
        for (std::int32_t local_z = 0; local_z < 16; ++local_z) {
            for (std::int32_t local_y = 0; local_y < 8; ++local_y) {
                if (!boundary(local_x, local_z, local_y)
                    || (local_y >= 4 && (random.next_u32() & 1U) == 0U)) {
                    continue;
                }
                const std::int32_t world_x = detail::wrapping_add(base_x, local_x);
                const std::int32_t world_y = detail::wrapping_add(base_y, local_y);
                const std::int32_t world_z = detail::wrapping_add(base_z, local_z);
                if (block::material_is_solid(world.block(world_x, world_y, world_z))) {
                    world.set_block_and_data(
                        world_x, world_y, world_z, block::rock, 0, false);
                }
            }
        }
    }
    return true;
}

void place_flower(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t id,
    std::uint8_t metadata) noexcept {
    // FlowerFeature::placeFlower,.
    for (std::int32_t attempt = 0; attempt < 64; ++attempt) {
        const ScatterPosition target = scatter_8_4_8(random, x, y, z);
        const bool can_place = id == block::brown_mushroom || id == block::red_mushroom
            ? world.block(target.x, target.y, target.z) == block::air
                && mushroom_can_survive(world, target.x, target.y, target.z)
            : can_place_bush(world, target.x, target.y, target.z);
        if (can_place) {
            world.set_block_and_data(
                target.x, target.y, target.z, id, metadata, true);
        }
    }
}

void place_tall_grass(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t metadata) noexcept {
    // TallGrassFeature::place,. Its descent regards both leaf tile
    // families as transparent and occurs before any random value is drawn.
    while (y > 0) {
        const std::uint8_t id = world.block(x, y, z);
        if (id != block::air && !block::is_leaf_material(id)) {
            break;
        }
        --y;
    }
    for (std::int32_t attempt = 0; attempt < 90; ++attempt) {
        const ScatterPosition target = scatter_8_4_8(random, x, y, z);
        if (can_place_bush(world, target.x, target.y, target.z)) {
            world.set_block_and_data(
                target.x, target.y, target.z, block::tall_grass, metadata, false);
        }
    }
}

void place_reeds(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // ReedsFeature::place,. X/Z use a radius-three signed scatter;
    // the Y supplied by BiomeDecorator is retained.
    for (std::int32_t attempt = 0; attempt < 20; ++attempt) {
        const std::int32_t target_x = detail::wrapping_add(
            x, static_cast<std::int32_t>(random.next_u32() & 3U)
                - static_cast<std::int32_t>(random.next_u32() & 3U));
        const std::int32_t target_z = detail::wrapping_add(
            z, static_cast<std::int32_t>(random.next_u32() & 3U)
                - static_cast<std::int32_t>(random.next_u32() & 3U));
        if (world.block(target_x, y, target_z) != block::air
            || !block::is_water(world.block(target_x - 1, y - 1, target_z))
                && !block::is_water(world.block(target_x + 1, y - 1, target_z))
                && !block::is_water(world.block(target_x, y - 1, target_z - 1))
                && !block::is_water(world.block(target_x, y - 1, target_z + 1))) {
            continue;
        }
        const std::uint32_t height_bound = random.next_u32() % 3U + 1U;
        const std::int32_t height = static_cast<std::int32_t>(
            random.next_u32() % height_bound) + 2;
        for (std::int32_t offset = 0; offset < height; ++offset) {
            const std::int32_t target_y = detail::wrapping_add(y, offset);
            if (!reeds_can_survive(world, target_x, target_y, target_z)) {
                break;
            }
            world.set_block_and_data(
                target_x, target_y, target_z, block::reeds, 0, false);
        }
    }
}

void place_cactus(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // CactusFeature::place,.
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const ScatterPosition target = scatter_8_4_8(random, x, y, z);
        if (world.block(target.x, target.y, target.z) != block::air) {
            continue;
        }
        const std::uint32_t height_bound = random.next_u32() % 3U + 1U;
        const std::int32_t height = static_cast<std::int32_t>(
            random.next_u32() % height_bound) + 1;
        for (std::int32_t offset = 0; offset < height; ++offset) {
            const std::int32_t target_y = detail::wrapping_add(target.y, offset);
            if (!cactus_can_survive(world, target.x, target_y, target.z)) {
                break;
            }
            world.set_block_and_data(
                target.x, target_y, target.z, block::cactus, 0, false);
        }
    }
}

void place_waterlilies(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // WaterlilyFeature::place,. WaterlilyTile::mayPlaceOn checks
    // the flowing-water tile specifically, rather than material equality.
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const ScatterPosition target = scatter_8_4_8(random, x, y, z);
        if (world.block(target.x, target.y, target.z) == block::air
            && world.block(target.x, target.y - 1, target.z) == block::water) {
            world.set_block_and_data(
                target.x, target.y, target.z, block::waterlily, 0, true);
        }
    }
}

void place_pumpkins(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // PumpkinFeature::place,. It has the same six scatter draws,
    // but consumes the first two before the MT word used for Y.
    for (std::int32_t attempt = 0; attempt < 64; ++attempt) {
        const ScatterPosition target = scatter_8_4_8(random, x, y, z);
        if (world.block(target.x, target.y, target.z) == block::air
            && world.block(target.x, target.y - 1, target.z) == block::grass) {
            const std::uint8_t facing = static_cast<std::uint8_t>(
                random.next_u32() & 3U);
            world.set_block_and_data(
                target.x, target.y, target.z, block::pumpkin, facing, true);
        }
    }
}

void place_dead_bushes(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // DeadBushFeature::place,.
    while (y > 0) {
        const std::uint8_t id = world.block(x, y, z);
        if (id == block::dead_bush
            || (id != block::air && !block::is_leaf_material(id))) {
            break;
        }
        --y;
    }
    for (std::int32_t attempt = 0; attempt < 4; ++attempt) {
        const ScatterPosition target = scatter_8_4_8(random, x, y, z);
        if (world.block(target.x, target.y, target.z) == block::air
            && block::dead_bush_support(world.block(target.x, target.y - 1, target.z))) {
            world.set_block_and_data(
                target.x, target.y, target.z, block::dead_bush, 0, true);
        }
    }
}

bool place_huge_mushroom(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::int32_t forced_type) noexcept {
    // HugeMushroomFeature::place,. The first word is consumed even
    // by the three fixed-type instances constructed by BiomeDecorator.
    std::int32_t mushroom_type = forced_type;
    const std::uint32_t type_word = random.next_u32();
    if (mushroom_type < 0) {
        mushroom_type = static_cast<std::int32_t>(type_word & 1U);
    }
    std::int32_t height = static_cast<std::int32_t>(random.next_u32() % 3U) + 4;
    if (random.next_u32() % 12U == 0U) {
        height *= 2;
    }

    const std::int32_t top_y = detail::wrapping_add(y, height);
    if (y <= 0 || top_y >= 127) {
        return false;
    }

    // The clearance volume is deliberately a 1x1 column through y+3, then a
    // 7x7 column. It accepts either leaves material as well as air.
    for (std::int32_t target_y = y;
         target_y <= detail::wrapping_add(top_y, 1);
         ++target_y) {
        const std::int32_t radius = target_y > detail::wrapping_add(y, 3)
            ? 3
            : 0;
        for (std::int32_t target_x = detail::wrapping_add(x, -radius);
             target_x <= detail::wrapping_add(x, radius);
             ++target_x) {
            for (std::int32_t target_z = detail::wrapping_add(z, -radius);
                 target_z <= detail::wrapping_add(z, radius);
                 ++target_z) {
                const std::uint8_t existing = world.block(
                    target_x, target_y, target_z);
                if (existing != block::air && !block::is_leaf_material(existing)) {
                    return false;
                }
            }
        }
    }

    const std::uint8_t ground = world.block(x, detail::wrapping_sub(y, 1), z);
    if (ground != block::dirt && ground != block::grass
        && ground != block::mycelium) {
        return false;
    }

    const std::uint8_t mushroom_block = mushroom_type == 0
        ? block::brown_mushroom_block
        : block::red_mushroom_block;
    const std::int32_t first_cap_y = mushroom_type == 1
        ? detail::wrapping_sub(top_y, 3)
        : top_y;

    for (std::int32_t cap_y = first_cap_y; cap_y <= top_y; ++cap_y) {
        std::int32_t radius = mushroom_type == 0 ? 3 : 1;
        if (mushroom_type == 1 && cap_y < top_y) {
            radius = 2;
        }
        const std::int32_t min_x = detail::wrapping_sub(x, radius);
        const std::int32_t max_x = detail::wrapping_add(x, radius);
        const std::int32_t min_z = detail::wrapping_sub(z, radius);
        const std::int32_t max_z = detail::wrapping_add(z, radius);

        for (std::int32_t target_x = min_x; target_x <= max_x; ++target_x) {
            for (std::int32_t target_z = min_z; target_z <= max_z; ++target_z) {
                // The cap metadata is the 3x3 directional matrix, expanded
                // across the type-zero cap's wider rim.
                std::uint8_t metadata = 5;
                if (target_x == min_x) {
                    metadata = 4;
                } else if (target_x == max_x) {
                    metadata = 6;
                }
                if (target_z == min_z) {
                    metadata = static_cast<std::uint8_t>(metadata - 3U);
                } else if (target_z == max_z) {
                    metadata = static_cast<std::uint8_t>(metadata + 3U);
                }

                if (!(mushroom_type == 1 && cap_y == top_y)) {
                    const bool x_edge = target_x == min_x || target_x == max_x;
                    const bool z_edge = target_z == min_z || target_z == max_z;
                    if (x_edge && z_edge) {
                        continue;
                    }

                    if (target_x == detail::wrapping_add(min_x, 1)
                        && target_z == min_z) {
                        metadata = 1;
                    }
                    if (target_x == min_x
                        && target_z == detail::wrapping_add(min_z, 1)) {
                        metadata = 1;
                    }
                    if (target_x == detail::wrapping_sub(max_x, 1)
                        && target_z == min_z) {
                        metadata = 3;
                    }
                    if (target_x == max_x
                        && target_z == detail::wrapping_add(min_z, 1)) {
                        metadata = 3;
                    }
                    if (target_x == detail::wrapping_add(min_x, 1)
                        && target_z == max_z) {
                        metadata = 7;
                    }
                    if (target_x == min_x
                        && target_z == detail::wrapping_sub(max_z, 1)) {
                        metadata = 7;
                    }
                    if (target_x == detail::wrapping_sub(max_x, 1)
                        && target_z == max_z) {
                        metadata = 9;
                    }
                    if (target_x == max_x
                        && target_z == detail::wrapping_sub(max_z, 1)) {
                        metadata = 9;
                    }
                }

                // On non-top cap layers the central (metadata 5) tile is not
                // written. The source has a degenerate one-block-height path
                // that writes data zero; normal heights always take this skip.
                if (metadata == 5U && cap_y < top_y) {
                    if (y < detail::wrapping_sub(top_y, 1)) {
                        continue;
                    }
                    metadata = 0;
                }
                if (!is_solid_blocking_tile(world.block(target_x, cap_y, target_z))) {
                    (void)world.set_block_and_data(
                        target_x, cap_y, target_z, mushroom_block, metadata, true);
                }
            }
        }
    }

    // The stalk is subsequently applied through the cap centre, replacing
    // only positions which TileSource considers non-solid-blocking.
    for (std::int32_t offset = 0; offset < height; ++offset) {
        const std::int32_t stalk_y = detail::wrapping_add(y, offset);
        if (!is_solid_blocking_tile(world.block(x, stalk_y, z))) {
            (void)world.set_block_and_data(
                x, stalk_y, z, mushroom_block, 10, true);
        }
    }
    return true;
}

bool place_spring_source(
    World& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t liquid_id) noexcept {
    // SpringFeature::place,. Its random parameter is forwarded to
    // LiquidTileDynamic::tick only after a successful write, so placement
    // itself consumes no MT output.
    if (world.block(x, detail::wrapping_add(y, 1), z) != block::rock
        || world.block(x, detail::wrapping_sub(y, 1), z) != block::rock) {
        return false;
    }
    const std::uint8_t center = world.block(x, y, z);
    if (center != block::air && center != block::rock) {
        return false;
    }

    std::int32_t rock_sides = 0;
    std::int32_t air_sides = 0;
    constexpr std::array<std::array<std::int32_t, 2>, 4> horizontal{{
        {{-1, 0}}, {{1, 0}}, {{0, -1}}, {{0, 1}},
    }};
    for (const auto& offset : horizontal) {
        const std::uint8_t side = world.block(
            detail::wrapping_add(x, offset[0]), y,
            detail::wrapping_add(z, offset[1]));
        rock_sides += side == block::rock ? 1 : 0;
        air_sides += side == block::air ? 1 : 0;
    }
    if (rock_sides != 3 || air_sides != 1) {
        return true;
    }
    return world.set_block_and_data(x, y, z, liquid_id, 0, true);
}

bool place_ice_patch(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t output_id,
    std::int32_t maximum_radius) noexcept {
    // IcePatchFeature::place,. It stops at y=2 rather than using a
    // generic height-map helper, so preserve that boundary here.
    while (y > 2 && world.block(x, y, z) == block::air) {
        y = detail::wrapping_sub(y, 1);
    }
    if (world.block(x, y, z) != block::snow) {
        return false;
    }

    const std::int32_t radius = maximum_radius == 2
        ? 2
        : static_cast<std::int32_t>(random.next_u32()
            % static_cast<std::uint32_t>(maximum_radius - 2)) + 2;
    const std::int32_t radius_squared = radius * radius;
    for (std::int32_t target_x = detail::wrapping_sub(x, radius);
         target_x <= detail::wrapping_add(x, radius);
         ++target_x) {
        const std::int32_t dx = target_x - x;
        for (std::int32_t target_z = detail::wrapping_sub(z, radius);
             target_z <= detail::wrapping_add(z, radius);
             ++target_z) {
            const std::int32_t dz = target_z - z;
            if (dx * dx + dz * dz > radius_squared) {
                continue;
            }
            for (std::int32_t target_y = detail::wrapping_sub(y, 1);
                 target_y <= detail::wrapping_add(y, 1);
                 ++target_y) {
                const std::uint8_t existing = world.block(
                    target_x, target_y, target_z);
                if (existing == block::dirt || existing == block::snow
                    || existing == block::ice) {
                    (void)world.set_block_and_data(
                        target_x, target_y, target_z, output_id, 0, true);
                }
            }
        }
    }
    return true;
}

bool place_ground_bush(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t metadata) noexcept {
    // GroundBushFeature::place,. The starting TilePos normally
    // comes from the tree position helper; this descent makes it robust to
    // leaf/air columns in exactly the same way as the target feature.
    while (y > 0) {
        const std::uint8_t existing = world.block(x, y, z);
        if (existing != block::air && !block::is_leaf_material(existing)) {
            break;
        }
        y = detail::wrapping_sub(y, 1);
    }
    const std::uint8_t ground = world.block(x, y, z);
    if (ground != block::dirt && ground != block::grass) {
        return true;
    }
    (void)world.set_block_and_data(x, y, z, block::log, metadata, true);

    for (std::int32_t layer = 0; layer < 3; ++layer) {
        const std::int32_t radius = 2 - layer;
        const std::int32_t leaf_y = detail::wrapping_add(y, layer);
        for (std::int32_t target_x = detail::wrapping_sub(x, radius);
             target_x <= detail::wrapping_add(x, radius);
             ++target_x) {
            const std::int32_t dx = target_x - x;
            const std::int32_t absolute_x = dx < 0 ? -dx : dx;
            for (std::int32_t target_z = detail::wrapping_sub(z, radius);
                 target_z <= detail::wrapping_add(z, radius);
                 ++target_z) {
                const std::int32_t dz = target_z - z;
                const std::int32_t absolute_z = dz < 0 ? -dz : dz;
                if (absolute_x == radius && absolute_z == radius
                    && (random.next_u32() & 1U) == 0U) {
                    continue;
                }
                if (!block::tile_solid_snapshot(
                        world.block(target_x, leaf_y, target_z))) {
                    (void)world.set_block_and_data(
                        target_x, leaf_y, target_z,
                        block::leaves, metadata, true);
                }
            }
        }
    }
    return true;
}

bool place_desert_well(
    World& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // DesertWellFeature::place,. Its native Random argument is
    // intentionally unused: this feature has no local entropy consumption.
    while (y > 2 && world.block(x, y, z) == block::air) {
        --y;
    }
    if (world.block(x, y, z) != block::sand) {
        return false;
    }

    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dz = -2; dz <= 2; ++dz) {
            const std::int32_t target_x = detail::wrapping_add(x, dx);
            const std::int32_t target_z = detail::wrapping_add(z, dz);
            if (world.block(target_x, detail::wrapping_sub(y, 1), target_z)
                    == block::air
                && world.block(target_x, detail::wrapping_sub(y, 2), target_z)
                    == block::air) {
                return false;
            }
        }
    }

    const auto set = [&world](
                         std::int32_t target_x,
                         std::int32_t target_y,
                         std::int32_t target_z,
                         std::uint8_t id,
                         std::uint8_t metadata = 0) noexcept {
        world.set_block_and_data(
            target_x, target_y, target_z, id, metadata, true);
    };

    // The well base is a solid five-by-five sandstone slab at y-1 and y.
    for (std::int32_t dy = -1; dy <= 0; ++dy) {
        for (std::int32_t dx = -2; dx <= 2; ++dx) {
            for (std::int32_t dz = -2; dz <= 2; ++dz) {
                set(detail::wrapping_add(x, dx), detail::wrapping_add(y, dy),
                    detail::wrapping_add(z, dz), block::sand_stone);
            }
        }
    }

    // One central source and its cardinal neighbours form the water cross.
    set(x, y, z, block::calm_water);
    set(detail::wrapping_sub(x, 1), y, z, block::calm_water);
    set(detail::wrapping_add(x, 1), y, z, block::calm_water);
    set(x, y, detail::wrapping_sub(z, 1), block::calm_water);
    set(x, y, detail::wrapping_add(z, 1), block::calm_water);

    const std::int32_t rim_y = detail::wrapping_add(y, 1);
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dz = -2; dz <= 2; ++dz) {
            if (dx == -2 || dx == 2 || dz == -2 || dz == 2) {
                set(detail::wrapping_add(x, dx), rim_y,
                    detail::wrapping_add(z, dz), block::sand_stone);
            }
        }
    }

    // The four rim midpoints are top-half stone slabs (metadata 1).
    set(x, rim_y, detail::wrapping_add(z, 2), block::stone_slab, 1);
    set(x, rim_y, detail::wrapping_sub(z, 2), block::stone_slab, 1);
    set(detail::wrapping_add(x, 2), rim_y, z, block::stone_slab, 1);
    set(detail::wrapping_sub(x, 2), rim_y, z, block::stone_slab, 1);

    const std::int32_t roof_y = detail::wrapping_add(y, 4);
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            set(detail::wrapping_add(x, dx), roof_y,
                detail::wrapping_add(z, dz),
                dx == 0 && dz == 0 ? block::sand_stone : block::stone_slab,
                dx == 0 && dz == 0 ? 0 : 1);
        }
    }

    // Four sandstone supports rise from the rim through the roof height - 1.
    for (std::int32_t dy = 1; dy <= 3; ++dy) {
        const std::int32_t support_y = detail::wrapping_add(y, dy);
        set(detail::wrapping_sub(x, 1), support_y,
            detail::wrapping_sub(z, 1), block::sand_stone);
        set(detail::wrapping_sub(x, 1), support_y,
            detail::wrapping_add(z, 1), block::sand_stone);
        set(detail::wrapping_add(x, 1), support_y,
            detail::wrapping_sub(z, 1), block::sand_stone);
        set(detail::wrapping_add(x, 1), support_y,
            detail::wrapping_add(z, 1), block::sand_stone);
    }
    return true;
}

bool place_clay_patch(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::int32_t maximum_radius) noexcept {
    // ClayFeature::place,. It accepts a pre-existing clay tile as
    // well as water at the probe coordinate, which matters for overlapping
    // patches in the same decorator pass.
    const std::uint8_t probe = world.block(x, y, z);
    if (!block::is_water(probe) && probe != block::clay) {
        return false;
    }
    const std::int32_t radius = maximum_radius == 2
        ? 2
        : static_cast<std::int32_t>(
            random.next_u32() % static_cast<std::uint32_t>(maximum_radius - 2)) + 2;
    const std::int32_t radius_squared = radius * radius;
    for (std::int32_t target_x = detail::wrapping_sub(x, radius);
         target_x <= detail::wrapping_add(x, radius);
         ++target_x) {
        const std::int32_t dx = target_x - x;
        for (std::int32_t target_z = detail::wrapping_sub(z, radius);
             target_z <= detail::wrapping_add(z, radius);
             ++target_z) {
            const std::int32_t dz = target_z - z;
            if (dx * dx + dz * dz > radius_squared) {
                continue;
            }
            for (std::int32_t target_y = detail::wrapping_sub(y, 1);
                 target_y <= detail::wrapping_add(y, 1);
                 ++target_y) {
                if (world.block(target_x, target_y, target_z) == block::dirt) {
                    world.set_block_and_data(
                        target_x, target_y, target_z, block::clay, 0, true);
                }
            }
        }
    }
    return true;
}

bool place_sand_patch(
    World& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t output_id,
    std::int32_t maximum_radius) noexcept {
    // SandFeature::place,. The native vertical loop stops the
    // moment a column ceases to be dirt/grass; it does not resume above it.
    if (!block::is_water(world.block(x, y, z))) {
        return false;
    }
    const std::int32_t radius = maximum_radius == 2
        ? 2
        : static_cast<std::int32_t>(
            random.next_u32() % static_cast<std::uint32_t>(maximum_radius - 2)) + 2;
    const std::int32_t radius_squared = radius * radius;
    for (std::int32_t target_x = detail::wrapping_sub(x, radius);
         target_x <= detail::wrapping_add(x, radius);
         ++target_x) {
        const std::int32_t dx = target_x - x;
        for (std::int32_t target_z = detail::wrapping_sub(z, radius);
             target_z <= detail::wrapping_add(z, radius);
             ++target_z) {
            const std::int32_t dz = target_z - z;
            if (dx * dx + dz * dz > radius_squared) {
                continue;
            }
            for (std::int32_t target_y = detail::wrapping_sub(y, 2);
                 target_y <= detail::wrapping_add(y, 2);
                 ++target_y) {
                const std::uint8_t existing = world.block(target_x, target_y, target_z);
                if (existing != block::dirt && existing != block::grass) {
                    break;
                }
                world.set_block_and_data(
                    target_x, target_y, target_z, output_id, 0, true);
            }
        }
    }
    return true;
}

} // namespace mcpe::worldgen::v0_9_0::feature
