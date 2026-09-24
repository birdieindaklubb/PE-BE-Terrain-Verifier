#include "v0_9_0/caves.hpp"

#include "detail/fp.hpp"
#include "detail/mt19937.hpp"
#include "v0_6_1/legacy_math.hpp"
#include "v0_9_0/base_terrain.hpp"
#include "v0_9_0/blocks.hpp"

#include <cassert>
#include <bit>
#include <cmath>
#include <cstddef>

namespace mcpe::worldgen::v0_9_0 {
namespace {

constexpr std::int32_t kSourceRadius = 8;
constexpr std::int32_t kCaveRange = 8;
constexpr float kPi = std::bit_cast<float>(0x40490fdbU);
constexpr float kTwoPi = std::bit_cast<float>(0x40c90fdbU);
constexpr float kHalfPi = std::bit_cast<float>(0x3fc90fdbU);

[[nodiscard]] constexpr std::size_t block_index(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    return (static_cast<std::size_t>(x) << 11U)
        | (static_cast<std::size_t>(z) << 7U)
        | static_cast<std::size_t>(y);
}

[[nodiscard]] std::int32_t wrapping_add(
    std::int32_t left,
    std::int32_t right) noexcept {
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}

[[nodiscard]] std::int32_t wrapping_mul(
    std::int32_t left,
    std::int32_t right) noexcept {
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
}

[[nodiscard]] std::int32_t wrapping_mul_add(
    std::int32_t left,
    std::int32_t multiplier,
    std::int32_t right) noexcept {
    return wrapping_add(wrapping_mul(left, multiplier), right);
}

[[nodiscard]] std::int32_t wrapping_sub(
    std::int32_t left,
    std::int32_t right) noexcept {
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
}

[[nodiscard]] std::int32_t native_floor(float value) noexcept {
    return detail::fp::trunc_to_i32(std::floor(value));
}

[[nodiscard]] float native_sin(float value) noexcept {
    // The ARM implementation imports Android bionic's sinf/cosf here.  The
    // same bionic kernels already underpin PE 0.6.1's reconstructed Mth
    // table, so use that shared implementation rather than the host CRT.
    return v0_6_1::legacy_math::bionic_sin(value);
}

[[nodiscard]] float native_cos(float value) noexcept {
    return v0_6_1::legacy_math::bionic_cos(value);
}

[[nodiscard]] bool is_water(std::uint8_t tile) noexcept {
    return tile == block::water || tile == block::calm_water;
}

[[nodiscard]] bool is_diggable(
    std::uint8_t tile,
    std::uint8_t tile_above) noexcept {
    return tile == block::rock
        || tile == block::dirt
        || tile == block::grass
        || tile == block::hardened_clay
        || tile == block::stained_clay
        || tile == block::sand_stone
        || tile == block::mycelium
        || tile == block::podzol
        || (tile == block::sand && tile_above == block::calm_water);
}

struct CarveContext final {
    std::span<std::uint8_t> blocks;
    std::int32_t target_chunk_x;
    std::int32_t target_chunk_z;
    std::vector<DeferredLightEmitter>* deferred_light_emitters;
};

[[nodiscard]] bool touches_water(
    const CarveContext& context,
    std::int32_t min_x,
    std::int32_t max_x,
    std::int32_t min_y,
    std::int32_t max_y,
    std::int32_t min_z,
    std::int32_t max_z) noexcept {
    for (std::int32_t x = min_x; x < max_x; ++x) {
        for (std::int32_t z = min_z; z < max_z; ++z) {
            for (std::int32_t y = max_y + 1; y >= min_y - 1; --y) {
                if (y >= 0 && y < kChunkHeight
                    && is_water(context.blocks[block_index(x, y, z)])) {
                    return true;
                }

                // The native perimeter scan skips the interior of the box
                // after the first y sample.  This is not an optimization we
                // may replace: it changes which water blocks reject a carve.
                if (y != min_y - 1
                    && x != min_x && x != max_x - 1
                    && z != min_z && z != max_z - 1) {
                    y = min_y;
                }
            }
        }
    }
    return false;
}

void carve_ellipsoid(
    CarveContext context,
    float center_x,
    float center_y,
    float center_z,
    float radius,
    float vertical_radius,
    std::int32_t min_x,
    std::int32_t max_x,
    std::int32_t min_y,
    std::int32_t max_y,
    std::int32_t min_z,
    std::int32_t max_z) noexcept {
    const float inverse_radius = detail::fp::div(1.0F, radius);
    const float inverse_vertical_radius = detail::fp::div(1.0F, vertical_radius);
    const std::int32_t base_x = wrapping_mul(context.target_chunk_x, 16);
    const std::int32_t base_z = wrapping_mul(context.target_chunk_z, 16);

    for (std::int32_t x = min_x; x < max_x; ++x) {
        // VFP forms 0.5-center first and then adds the integer coordinate.
        // The apparently equivalent (coordinate+0.5)-center expression can
        // round differently at large world coordinates.
        float relative_x = detail::fp::sub(0.5F, center_x);
        relative_x = detail::fp::add(
            static_cast<float>(wrapping_add(base_x, x)), relative_x);
        relative_x = detail::fp::mul(relative_x, inverse_radius);
        const float x_square = detail::fp::mul(relative_x, relative_x);

        if (x_square >= 1.0F) {
            continue;
        }

        for (std::int32_t z = min_z; z < max_z; ++z) {
            float relative_z = detail::fp::sub(0.5F, center_z);
            relative_z = detail::fp::add(
                static_cast<float>(wrapping_add(base_z, z)), relative_z);
            relative_z = detail::fp::mul(relative_z, inverse_radius);
            const float horizontal = detail::fp::mul_add(
                x_square, relative_z, relative_z);
            if (horizontal >= 1.0F) {
                continue;
            }

            for (std::int32_t y = max_y - 1; y >= min_y; --y) {
                float relative_y = detail::fp::sub(0.5F, center_y);
                relative_y = detail::fp::add(static_cast<float>(y), relative_y);
                relative_y = detail::fp::mul(relative_y, inverse_vertical_radius);
                // VCVT.F64.F32 promotes the sample before a double-precision
                // comparison with -0.7, and BLE rejects equality as well.
                if (static_cast<double>(relative_y) <= -0.7) {
                    continue;
                }

                const float distance = detail::fp::mul_add(
                    horizontal, relative_y, relative_y);
                if (distance >= 1.0F) {
                    continue;
                }

                // PE 0.9.0's loop coordinate and block pointer are offset by
                // one: the pointer starts at maxY while the geometric test
                // starts at maxY-1. Preserve that observable native quirk;
                // it makes the carved block one Y level above the ellipsoid
                // sample (and the deferred-light coordinate below the lava).
                const std::size_t index = block_index(x, y + 1, z);
                const std::uint8_t tile = context.blocks[index];
                const std::uint8_t tile_above = context.blocks[index + 1U];
                const bool was_grass = tile == block::grass;
                if (!is_diggable(tile, tile_above)) {
                    continue;
                }

                if (y < 10) {
                    context.blocks[index] = block::lava;
                    if (context.deferred_light_emitters != nullptr) {
                        context.deferred_light_emitters->push_back({
                            wrapping_add(
                                wrapping_mul(context.target_chunk_x, 16), x),
                            y,
                            wrapping_add(
                                wrapping_mul(context.target_chunk_z, 16), z),
                        });
                    }
                    continue;
                }

                if (tile_above == block::sand
                    && context.blocks[index + 2U] == block::sand
                    && context.blocks[index + 3U] == block::sand) {
                    context.blocks[index + 1U] = block::sand_stone;
                }
                context.blocks[index] = block::air;
                if (was_grass && context.blocks[index - 1U] == block::dirt) {
                    context.blocks[index - 1U] = block::grass;
                }
            }
        }
    }
}

void carve_tunnel(
    detail::Mt19937& external_random,
    CarveContext context,
    float x,
    float y,
    float z,
    float width,
    float yaw,
    float pitch,
    std::int32_t start,
    std::int32_t end,
    float vertical_scale) noexcept {
    // LargeCaveFeature creates a fresh native MT from one half-width draw of
    // its caller's stream for each root and recursive tunnel.
    detail::Mt19937 random(external_random.next_u32() >> 1U);

    if (end < 1) {
        end = (kCaveRange - 1) * 16;
        end -= static_cast<std::int32_t>(random.next_bounded(
            static_cast<std::uint32_t>(end / 4)));
    }

    const bool room = start == -1;
    if (room) {
        start = end / 2;
    }

    const std::int32_t branch_at = static_cast<std::int32_t>(random.next_bounded(
        static_cast<std::uint32_t>(end / 2))) + end / 4;
    const std::uint32_t pitch_decay_selector = random.next_u32();
    float pitch_change = 0.0F;
    float yaw_change = 0.0F;

    const std::int32_t center_x = wrapping_mul_add(context.target_chunk_x, 16, 8);
    const std::int32_t center_z = wrapping_mul_add(context.target_chunk_z, 16, 8);
    const float chunk_center_x = static_cast<float>(center_x);
    const float chunk_center_z = static_cast<float>(center_z);

    for (std::int32_t step = start; step < end; ++step) {
        const float progress = detail::fp::mul(
            static_cast<float>(step),
            detail::fp::div(kPi, static_cast<float>(end)));
        float radius = detail::fp::mul(native_sin(progress), width);
        radius = detail::fp::add(radius, 1.5F);
        const float vertical_radius = detail::fp::mul(radius, vertical_scale);

        // Native uses pitch from the horizontal plane: sin(pitch) advances Y,
        // while cos(pitch) is projected onto X/Z by yaw.
        const float sin_pitch = native_sin(pitch);
        const float cos_pitch = native_cos(pitch);
        const float sin_yaw = native_sin(yaw);
        const float cos_yaw = native_cos(yaw);
        y = detail::fp::add(y, sin_pitch);
        x = detail::fp::mul_add(x, cos_yaw, cos_pitch);
        z = detail::fp::mul_add(z, sin_yaw, cos_pitch);

        pitch = detail::fp::mul(
            pitch, pitch_decay_selector % 6U == 0U ? 0.92F : 0.7F);
        yaw = detail::fp::mul_add(yaw, yaw_change, 0.1F);
        pitch = detail::fp::mul_add(pitch, pitch_change, 0.1F);

        pitch_change = detail::fp::mul(pitch_change, 0.9F);
        const float pitch_left = random.next_float();
        const float pitch_right = random.next_float();
        const float pitch_scale = detail::fp::mul(random.next_float(), 2.0F);
        // The APK uses exactly three draws here: the final one is doubled.
        const float pitch_delta = detail::fp::mul(
            pitch_scale, detail::fp::sub(pitch_left, pitch_right));
        pitch_change = detail::fp::add(pitch_change, pitch_delta);

        yaw_change = detail::fp::mul(yaw_change, 0.75F);
        const float yaw_left = random.next_float();
        const float yaw_right = random.next_float();
        const float yaw_scale = detail::fp::mul(random.next_float(), 4.0F);
        const float yaw_delta = detail::fp::mul(
            yaw_scale, detail::fp::sub(yaw_left, yaw_right));
        yaw_change = detail::fp::add(yaw_change, yaw_delta);

        // The specialized root-tunnel routine and the shared recursive
        // routine both split only wide tunnels. ARM's BGT follows the
        // comparison with 1.0F; inverting it changes the descendant graph.
        if (step == branch_at && width > 1.0F && !room) {
            const float left_width = detail::fp::mul_add(
                0.5F, random.next_float(), 0.5F);
            carve_tunnel(
                random,
                context,
                x,
                y,
                z,
                left_width,
                detail::fp::sub(yaw, kHalfPi),
                detail::fp::mul(pitch, 0.33333334F),
                step,
                end,
                1.0F);

            const float right_width = detail::fp::mul_add(
                0.5F, random.next_float(), 0.5F);
            carve_tunnel(
                random,
                context,
                x,
                y,
                z,
                right_width,
                detail::fp::add(yaw, kHalfPi),
                detail::fp::mul(pitch, 0.33333334F),
                step,
                end,
                1.0F);
            return;
        }

        if (!room && (random.next_u32() & 3U) == 0U) {
            continue;
        }

        const std::int32_t remaining = end - step;
        float horizontal_distance = detail::fp::sub(x, chunk_center_x);
        horizontal_distance = detail::fp::mul(horizontal_distance, horizontal_distance);
        const float z_distance = detail::fp::sub(z, chunk_center_z);
        horizontal_distance = detail::fp::mul_add(
            horizontal_distance, z_distance, z_distance);
        horizontal_distance = detail::fp::sub(
            horizontal_distance,
            detail::fp::mul(static_cast<float>(remaining), static_cast<float>(remaining)));
        const float stop_radius = detail::fp::add(width, 18.0F);
        if (horizontal_distance > detail::fp::mul(stop_radius, stop_radius)) {
            return;
        }

        const float doubled_radius = detail::fp::add(radius, radius);
        const float min_chunk_x = detail::fp::sub(
            detail::fp::sub(chunk_center_x, doubled_radius), 16.0F);
        const float min_chunk_z = detail::fp::sub(
            detail::fp::sub(chunk_center_z, doubled_radius), 16.0F);
        const float max_chunk_x = detail::fp::add(
            detail::fp::add(chunk_center_x, 16.0F), doubled_radius);
        const float max_chunk_z = detail::fp::add(
            detail::fp::add(chunk_center_z, 16.0F), doubled_radius);
        // Equality at the upper boundary remains eligible for floor/clamp.
        if (x < min_chunk_x || z < min_chunk_z || x > max_chunk_x || z > max_chunk_z) {
            continue;
        }

        const std::int32_t base_x = wrapping_mul(context.target_chunk_x, 16);
        const std::int32_t base_z = wrapping_mul(context.target_chunk_z, 16);
        std::int32_t min_x = native_floor(detail::fp::sub(x, radius));
        min_x = wrapping_sub(wrapping_sub(min_x, base_x), 1);
        if (min_x < 0) {
            min_x = 0;
        }
        std::int32_t max_x = native_floor(detail::fp::add(x, radius));
        max_x = wrapping_add(wrapping_sub(max_x, base_x), 1);
        if (max_x > kChunkWidth) {
            max_x = kChunkWidth;
        }

        std::int32_t min_y = native_floor(detail::fp::sub(y, vertical_radius)) - 1;
        if (min_y < 1) {
            min_y = 1;
        }
        std::int32_t max_y = native_floor(detail::fp::add(y, vertical_radius)) + 1;
        if (max_y > 120) {
            max_y = 120;
        }

        std::int32_t min_z = native_floor(detail::fp::sub(z, radius));
        min_z = wrapping_sub(wrapping_sub(min_z, base_z), 1);
        if (min_z < 0) {
            min_z = 0;
        }
        std::int32_t max_z = native_floor(detail::fp::add(z, radius));
        max_z = wrapping_add(wrapping_sub(max_z, base_z), 1);
        if (max_z > kChunkWidth) {
            max_z = kChunkWidth;
        }

        if (min_x >= max_x || min_y >= max_y || min_z >= max_z) {
            // The native generic routine exits a room on its first
            // geometrically eligible step even when the clamped box is
            // empty. Ordinary tunnels continue. Water rejection is handled
            // separately below and does not terminate a room.
            if (room) {
                return;
            }
            continue;
        }
        if (touches_water(context, min_x, max_x, min_y, max_y, min_z, max_z)) {
            continue;
        }

        carve_ellipsoid(
            context,
            x,
            y,
            z,
            radius,
            vertical_radius,
            min_x,
            max_x,
            min_y,
            max_y,
            min_z,
            max_z);

        if (room) {
            return;
        }
    }
}

void add_feature(
    detail::Mt19937& random,
    CarveContext context,
    std::int32_t source_chunk_x,
    std::int32_t source_chunk_z) noexcept {
    const std::uint32_t first_bound = random.next_bounded(40U) + 1U;
    const std::uint32_t second_bound = random.next_bounded(first_bound) + 1U;
    const std::uint32_t cave_systems = random.next_bounded(second_bound);
    if (random.next_bounded(15U) != 0U) {
        return;
    }

    const std::int32_t source_x = wrapping_mul(source_chunk_x, 16);
    const std::int32_t source_z = wrapping_mul(source_chunk_z, 16);

    for (std::uint32_t system = 0; system < cave_systems; ++system) {
        const float x = static_cast<float>(wrapping_add(
            source_x, static_cast<std::int32_t>(random.next_u32() & 15U)));
        const std::uint32_t y_bound = random.next_bounded(120U) + 8U;
        const float y = static_cast<float>(random.next_bounded(y_bound));
        const float z = static_cast<float>(wrapping_add(
            source_z, static_cast<std::int32_t>(random.next_u32() & 15U)));

        std::uint32_t tunnel_count = 1U;
        if ((random.next_u32() & 3U) == 0U) {
            const float room_width = detail::fp::mul_add(
                1.0F, random.next_float(), 6.0F);
            carve_tunnel(
                random,
                context,
                x,
                y,
                z,
                room_width,
                0.0F,
                0.0F,
                -1,
                -1,
                0.5F);
            tunnel_count = (random.next_u32() & 3U) + 1U;
        }

        for (std::uint32_t tunnel = 0; tunnel < tunnel_count; ++tunnel) {
            const float yaw = detail::fp::mul(random.next_float(), kTwoPi);
            const float pitch = detail::fp::mul(
                detail::fp::sub(random.next_float(), 0.5F), 0.25F);
            // C++ does not specify function-argument evaluation order.  The
            // native routine consumes the doubled draw first and the additive
            // draw second, so materialize both before composing the width.
            const float doubled_width_draw = random.next_float();
            const float additive_width_draw = random.next_float();
            const float width = detail::fp::add(
                detail::fp::mul(doubled_width_draw, 2.0F),
                additive_width_draw);
            carve_tunnel(
                random,
                context,
                x,
                y,
                z,
                width,
                yaw,
                pitch,
                0,
                0,
                1.0F);
        }
    }
}

} // namespace

void CaveCarver::carve(
    std::uint32_t world_seed,
    std::span<std::uint8_t> blocks,
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    detail::Mt19937* post_carve_random,
    std::vector<DeferredLightEmitter>* deferred_light_emitters) const noexcept {
    assert(blocks.size() >= kChunkBlockCount);

    // LargeFeature::apply derives two odd multipliers from a fresh level-seed
    // MT, visits the 17 by 17 source-chunk square, then reseeds for each
    // source before dispatching LargeCaveFeature::addFeature.
    detail::Mt19937 world_random(world_seed);
    const std::uint32_t x_multiplier = (world_random.next_u32() >> 2U) * 2U + 1U;
    const std::uint32_t z_multiplier = (world_random.next_u32() >> 2U) * 2U + 1U;
    const CarveContext context{
        blocks, chunk_x, chunk_z, deferred_light_emitters};

    for (std::int32_t source_x = chunk_x - kSourceRadius;
         source_x <= chunk_x + kSourceRadius;
         ++source_x) {
        for (std::int32_t source_z = chunk_z - kSourceRadius;
             source_z <= chunk_z + kSourceRadius;
             ++source_z) {
            const std::uint32_t source_seed = world_seed
                ^ (static_cast<std::uint32_t>(source_x) * x_multiplier
                    + static_cast<std::uint32_t>(source_z) * z_multiplier);
            detail::Mt19937 source_random(source_seed);
            add_feature(source_random, context, source_x, source_z);
            // LargeFeature::apply reuses this same Random object for every
            // source chunk.  RandomLevelSource::postProcess subsequently
            // starts its lake prepass from the state left by the final
            // (southeast) source, rather than a fresh chunk seed.
            if (post_carve_random != nullptr
                && source_x == chunk_x + kSourceRadius
                && source_z == chunk_z + kSourceRadius) {
                *post_carve_random = source_random;
            }
        }
    }
}

} // namespace mcpe::worldgen::v0_9_0
