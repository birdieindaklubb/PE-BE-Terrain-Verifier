#include "v1_1_5_0/hell_caves.hpp"

#include "detail/fp.hpp"
#include "detail/mt19937.hpp"
#include "detail/wrap.hpp"
#include "v0_6_1/legacy_math.hpp"
#include "v1_1_5_0/blocks.hpp"

#include <cstddef>
#include <cstdint>

namespace mcpe::worldgen::v1_1_5_0 {
namespace {

constexpr std::int32_t kInfluenceRadius = 8;
constexpr float kPi = 3.1415927F;
constexpr float kTwoPi = 6.2831855F;
constexpr float kHalfPi = 1.5707964F;

[[nodiscard]] std::int32_t native_floor(float value) noexcept {
    return detail::fp::floor_to_i32(value);
}

[[nodiscard]] float random_float_from_word(std::uint32_t word) noexcept {
    // Random::nextFloat converts the complete unsigned MT word through
    // binary64 before rounding once to binary32.
    return static_cast<float>(static_cast<double>(word) * 0x1p-32);
}

[[nodiscard]] constexpr std::int32_t chunk_origin(
    std::int32_t chunk) noexcept {
    return detail::wrapping_mul(chunk, Chunk::width);
}

[[nodiscard]] bool is_lava(std::uint8_t id) noexcept {
    return id == block::flowing_lava || id == block::still_lava;
}

[[nodiscard]] bool is_carvable(std::uint8_t id) noexcept {
    // Dirt and grass are native cases retained by this otherwise-Netherrack
    // feature. They matter if another load-stage writer exposed either tile.
    return id == block::netherrack || id == 2U || id == 3U;
}

struct Context final {
    Chunk& chunk;
    std::int32_t target_x;
    std::int32_t target_z;
};

[[nodiscard]] bool touches_lava(
    const Context& context,
    std::int32_t min_x,
    std::int32_t max_x,
    std::int32_t min_y,
    std::int32_t max_y,
    std::int32_t min_z,
    std::int32_t max_z) noexcept {
    // LargeHellCaveFeature scans the outer shell only. Do not turn this into
    // an all-volume test: the exact early exit determines later carving.
    for (std::int32_t x = min_x; x < max_x; ++x) {
        for (std::int32_t z = min_z; z < max_z; ++z) {
            for (std::int32_t y = max_y + 1; y >= min_y - 1; --y) {
                if (y >= 0 && y < Chunk::height
                    && is_lava(context.chunk.block(x, y, z))) {
                    return true;
                }
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
    Context context,
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
    // The tunnel trajectory and bounds are binary32 in the ARM routine, but
    // its final ellipsoid predicate promotes the position/radius operands to
    // double.  Retaining that split is important: a float-only predicate
    // changes which cells lying on a cave boundary become air.
    const double inverse_radius = 1.0 / static_cast<double>(radius);
    const double inverse_vertical_radius =
        1.0 / static_cast<double>(vertical_radius);
    const std::int32_t base_x = chunk_origin(context.target_x);
    const std::int32_t base_z = chunk_origin(context.target_z);
    for (std::int32_t x = min_x; x < max_x; ++x) {
        const double relative_x =
            (static_cast<double>(detail::wrapping_add(base_x, x)) + 0.5
                - static_cast<double>(center_x))
            * inverse_radius;
        const double x_square = relative_x * relative_x;
        if (x_square >= 1.0) {
            continue;
        }
        for (std::int32_t z = min_z; z < max_z; ++z) {
            const double relative_z =
                (static_cast<double>(detail::wrapping_add(base_z, z)) + 0.5
                    - static_cast<double>(center_z))
                * inverse_radius;
            const double horizontal = x_square + relative_z * relative_z;
            if (horizontal >= 1.0) {
                continue;
            }
            for (std::int32_t y = max_y - 1; y >= min_y; --y) {
                const double relative_y =
                    (static_cast<double>(y) + 0.5
                        - static_cast<double>(center_y))
                    * inverse_vertical_radius;
                if (relative_y <= -0.7) {
                    continue;
                }
                const double distance =
                    horizontal + relative_y * relative_y;
                if (distance < 1.0
                    && is_carvable(context.chunk.block(x, y, z))) {
                    context.chunk.set_block(x, y, z, block::air);
                }
            }
        }
    }
}

void carve_tunnel(
    detail::Mt19937& parent_random,
    Context context,
    float x,
    float y,
    float z,
    float width,
    float yaw,
    float pitch,
    std::int32_t start,
    std::int32_t end,
    float vertical_scale) noexcept {
    detail::Mt19937 random(parent_random.next_u32() >> 1U);
    if (end < 1) {
        end = 112 - static_cast<std::int32_t>(random.next_bounded(28U));
    }
    const bool room = start == -1;
    if (room) {
        start = end / 2;
    }
    const std::int32_t branch_at = static_cast<std::int32_t>(
        random.next_bounded(static_cast<std::uint32_t>(end / 2))) + end / 4;
    const float pitch_decay = random.next_bounded(6U) == 0U ? 0.92F : 0.7F;
    const float target_center_x = static_cast<float>(
        detail::wrapping_add(chunk_origin(context.target_x), 8));
    const float target_center_z = static_cast<float>(
        detail::wrapping_add(chunk_origin(context.target_z), 8));
    float pitch_change = 0.0F;
    float yaw_change = 0.0F;

    for (std::int32_t step = start; step < end; ++step) {
        // The ARM path forms ``PI * step`` first and divides by end before
        // indexing Math::mSin.  Reassociating this as ``(PI / end) * step``
        // shifts some binary32 tunnel radii at an ellipsoid boundary.
        const float progress = detail::fp::div(
            detail::fp::mul(kPi, static_cast<float>(step)),
            static_cast<float>(end));
        float radius = detail::fp::mul(v0_6_1::legacy_math::sin(progress), width);
        radius = detail::fp::add(radius, 1.5F);
        const float vertical_radius = detail::fp::mul(radius, vertical_scale);

        const float cos_pitch = v0_6_1::legacy_math::cos(pitch);
        x = detail::fp::mul_add(
            x, v0_6_1::legacy_math::cos(yaw), cos_pitch);
        y = detail::fp::add(y, v0_6_1::legacy_math::sin(pitch));
        z = detail::fp::mul_add(
            z, v0_6_1::legacy_math::sin(yaw), cos_pitch);
        pitch = detail::fp::mul_add(
            detail::fp::mul(pitch, pitch_decay), pitch_change, 0.1F);
        yaw = detail::fp::mul_add(yaw, yaw_change, 0.1F);

        pitch_change = detail::fp::mul(pitch_change, 0.9F);
        const float pitch_left = random.next_float();
        const float pitch_right = random.next_float();
        const float pitch_scale = random.next_float();
        // LargeHellCaveFeature doubles the already-subtracted steering
        // difference before applying its third random draw.  The ARM VFP
        // sequence is ``scale * ((left - right) + (left - right))``;
        // preserving that order keeps the reimplementation tied to the
        // observable routine rather than a source-level rearrangement.
        float pitch_delta = detail::fp::sub(pitch_left, pitch_right);
        pitch_delta = detail::fp::add(pitch_delta, pitch_delta);
        pitch_delta = detail::fp::mul(pitch_scale, pitch_delta);
        pitch_change = detail::fp::add(pitch_change, pitch_delta);
        yaw_change = detail::fp::mul(yaw_change, 0.75F);
        const float yaw_left = random.next_float();
        const float yaw_right = random.next_float();
        const float yaw_scale = random.next_float();
        float yaw_delta = detail::fp::mul(
            yaw_scale, detail::fp::sub(yaw_left, yaw_right));
        yaw_delta = detail::fp::mul(yaw_delta, 4.0F);
        yaw_change = detail::fp::add(yaw_change, yaw_delta);

        if (!room) {
            // LargeHellCaveFeature draws this selector before testing the
            // split point.  A split therefore inherits a stream one word
            // later than a superficially equivalent Java-style ordering.
            // The native condition is width > 1, not width < 1.  The ARM
            // decompilation expresses it as the inverse of the combined
            // less-than/equal condition, which is easy to read backwards.
            // Rooms consume no selector and return after their one pass.
            const std::uint32_t carve_selector = random.next_u32();
            if (width > 1.0F && step == branch_at) {
                // addTunnel uses the selector draw itself as the first
                // child's width. It only advances the stream for the second
                // child; discarding this word shifts every recursive cave.
                carve_tunnel(
                    random, context, x, y, z,
                    detail::fp::mul_add(
                        0.5F, random_float_from_word(carve_selector), 0.5F),
                    detail::fp::sub(yaw, kHalfPi),
                    detail::fp::mul(pitch, 0.33333334F), step, end, 1.0F);
                carve_tunnel(
                    random, context, x, y, z,
                    detail::fp::mul_add(0.5F, random.next_float(), 0.5F),
                    detail::fp::add(yaw, kHalfPi),
                    detail::fp::mul(pitch, 0.33333334F), step, end, 1.0F);
                return;
            }
            if ((carve_selector & 3U) == 0U) {
                continue;
            }
        }

        const std::int32_t remaining = end - step;
        float distance = detail::fp::sub(x, target_center_x);
        distance = detail::fp::mul(distance, distance);
        // addTunnel subtracts the remaining-step square from the X term
        // before adding the Z term.  This is a control-flow predicate, not
        // merely a cosmetic reassociation: a binary32 boundary result can
        // decide whether the entire remainder of a tunnel is abandoned.
        const float remaining_square = detail::fp::mul(
            static_cast<float>(remaining), static_cast<float>(remaining));
        distance = detail::fp::sub(distance, remaining_square);
        const float z_distance = detail::fp::sub(z, target_center_z);
        distance = detail::fp::mul_add(distance, z_distance, z_distance);
        const float stop_radius = detail::fp::add(width, 18.0F);
        if (distance > detail::fp::mul(stop_radius, stop_radius)) {
            return;
        }

        // The native routine performs a coarse two-radius X/Z rejection
        // before constructing local bounds.  It is not redundant at the
        // binary32 edge: the following floor/clamp path is deliberately not
        // evaluated for a rejected step.
        const float doubled_radius = detail::fp::add(radius, radius);
        const float minimum_center_x = detail::fp::sub(
            detail::fp::sub(target_center_x, 16.0F), doubled_radius);
        const float minimum_center_z = detail::fp::sub(
            detail::fp::sub(target_center_z, 16.0F), doubled_radius);
        const float maximum_center_x = detail::fp::add(
            detail::fp::add(target_center_x, 16.0F), doubled_radius);
        const float maximum_center_z = detail::fp::add(
            detail::fp::add(target_center_z, 16.0F), doubled_radius);
        if (x < minimum_center_x || z < minimum_center_z
            || x > maximum_center_x || z > maximum_center_z) {
            continue;
        }

        const std::int32_t base_x = chunk_origin(context.target_x);
        const std::int32_t base_z = chunk_origin(context.target_z);
        std::int32_t min_x = detail::wrapping_sub(
            detail::wrapping_sub(native_floor(detail::fp::sub(x, radius)), base_x), 1);
        std::int32_t max_x = detail::wrapping_add(
            detail::wrapping_sub(native_floor(detail::fp::add(x, radius)), base_x), 1);
        std::int32_t min_y = native_floor(detail::fp::sub(y, vertical_radius)) - 1;
        std::int32_t max_y = native_floor(detail::fp::add(y, vertical_radius)) + 1;
        std::int32_t min_z = detail::wrapping_sub(
            detail::wrapping_sub(native_floor(detail::fp::sub(z, radius)), base_z), 1);
        std::int32_t max_z = detail::wrapping_add(
            detail::wrapping_sub(native_floor(detail::fp::add(z, radius)), base_z), 1);
        // The lava guard examines the outer shell separately, but the
        // carve window itself remains allowed to reach the local X/Z edge.
        // The symbolized server's shared LargeHellCaveFeature confirms the
        // native lower clamp is zero.
        min_x = min_x < 0 ? 0 : min_x;
        max_x = max_x > Chunk::width ? Chunk::width : max_x;
        min_y = min_y < 1 ? 1 : min_y;
        max_y = max_y > 120 ? 120 : max_y;
        min_z = min_z < 0 ? 0 : min_z;
        max_z = max_z > Chunk::width ? Chunk::width : max_z;
        if (min_x >= max_x || min_y >= max_y || min_z >= max_z
            || touches_lava(context, min_x, max_x, min_y, max_y, min_z, max_z)) {
            continue;
        }
        carve_ellipsoid(
            context, x, y, z, radius, vertical_radius,
            min_x, max_x, min_y, max_y, min_z, max_z);
        if (room) {
            return;
        }
    }
}

void add_feature(
    detail::Mt19937& random,
    Context context,
    std::int32_t source_chunk_x,
    std::int32_t source_chunk_z) noexcept {
    const std::uint32_t first = random.next_bounded(10U) + 1U;
    const std::uint32_t second = random.next_bounded(first) + 1U;
    const std::uint32_t count = random.next_bounded(second);
    // The gate draw is unconditional in LargeHellCaveFeature::addFeature,
    // even when the nested count is zero. Each source stream is discarded
    // afterwards today, but retaining the draw makes the source trace exact.
    const bool enabled = random.next_bounded(5U) == 0U;
    if (count == 0U || !enabled) {
        return;
    }
    const std::int32_t source_x = chunk_origin(source_chunk_x);
    const std::int32_t source_z = chunk_origin(source_chunk_z);
    for (std::uint32_t system = 0; system < count; ++system) {
        // LargeHellCaveFeature constructs its Vec3 from three Random calls.
        // The ARM call sequence evaluates the source expressions in Z/Y/X
        // order, then stores them as X/Y/Z.  Preserve the observable stream
        // rather than imposing a left-to-right source-language assumption.
        const std::uint32_t z_word = random.next_u32();
        const std::uint32_t y_word = random.next_u32();
        const std::uint32_t x_word = random.next_u32();
        const float x = static_cast<float>(detail::wrapping_add(
            source_x, static_cast<std::int32_t>(x_word & 15U)));
        const float y = static_cast<float>(y_word & 127U);
        const float z = static_cast<float>(detail::wrapping_add(
            source_z, static_cast<std::int32_t>(z_word & 15U)));
        std::uint32_t tunnels = 1U;
        if ((random.next_u32() & 3U) == 0U) {
            carve_tunnel(
                random, context, x, y, z,
                detail::fp::mul_add(1.0F, random.next_float(), 6.0F),
                0.0F, 0.0F, -1, -1, 0.5F);
            tunnels = (random.next_u32() & 3U) + 1U;
        }
        for (std::uint32_t tunnel = 0; tunnel < tunnels; ++tunnel) {
            const float yaw = detail::fp::mul(random.next_float(), kTwoPi);
            const float pitch = detail::fp::mul(
                detail::fp::sub(random.next_float(), 0.5F), 0.25F);
            // The APK computes ``(r3 + r3) + r4``, then doubles that
            // compound width.  Doubling an IEEE binary32 value is exact;
            // spelling the intermediate add keeps the native order clear.
            const float first_width_draw = random.next_float();
            float width = detail::fp::add(first_width_draw, first_width_draw);
            width = detail::fp::add(width, random.next_float());
            width = detail::fp::add(width, width);
            carve_tunnel(random, context, x, y, z, width, yaw, pitch, 0, 0, 1.0F);
        }
    }
}

} // namespace

void HellCaveCarver::carve(std::uint32_t world_seed, Chunk& chunk) const noexcept {
    detail::Mt19937 level_random(world_seed);
    const std::uint32_t x_multiplier = (level_random.next_u32() >> 1U) | 1U;
    const std::uint32_t z_multiplier = (level_random.next_u32() >> 1U) | 1U;
    const Context context{chunk, chunk.x, chunk.z};
    const std::int32_t minimum_x = detail::wrapping_sub(chunk.x, kInfluenceRadius);
    const std::int32_t maximum_x = detail::wrapping_add(chunk.x, kInfluenceRadius);
    const std::int32_t minimum_z = detail::wrapping_sub(chunk.z, kInfluenceRadius);
    const std::int32_t maximum_z = detail::wrapping_add(chunk.z, kInfluenceRadius);
    for (std::int32_t source_x = minimum_x;;
         source_x = detail::wrapping_add(source_x, 1)) {
        for (std::int32_t source_z = minimum_z;;
             source_z = detail::wrapping_add(source_z, 1)) {
            const std::uint32_t coordinate_term = detail::bits(source_x) * x_multiplier
                + detail::bits(source_z) * z_multiplier;
            detail::Mt19937 random(world_seed ^ coordinate_term);
            add_feature(random, context, source_x, source_z);
            if (source_z == maximum_z) {
                break;
            }
        }
        if (source_x == maximum_x) {
            break;
        }
    }
}

} // namespace mcpe::worldgen::v1_1_5_0
