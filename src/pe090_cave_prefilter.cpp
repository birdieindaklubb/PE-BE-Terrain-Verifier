#include "pe090_cave_prefilter.hpp"

#include "detail/fp.hpp"
#include "detail/mt19937.hpp"
#include "detail/wrap.hpp"
#include "v0_6_1/legacy_math.hpp"

#include <bit>
#include <cstddef>
#include <vector>

namespace mcpe::terrain_verifier {
namespace {

constexpr std::int32_t kSourceRadius = 8;
constexpr std::int32_t kCaveRange = 8;
constexpr float kPi = std::bit_cast<float>(0x40490fdbU);
constexpr float kTwoPi = std::bit_cast<float>(0x40c90fdbU);
constexpr float kHalfPi = std::bit_cast<float>(0x3fc90fdbU);

struct PointState final {
    CavePoint point;
    bool reached{};
};

struct Context final {
    std::int32_t target_chunk_x{};
    std::int32_t target_chunk_z{};
    std::span<PointState> points;
    std::size_t remaining{};
};

[[nodiscard]] float native_sin(float value) noexcept {
    return worldgen::v0_6_1::legacy_math::bionic_sin(value);
}

[[nodiscard]] float native_cos(float value) noexcept {
    return worldgen::v0_6_1::legacy_math::bionic_cos(value);
}

[[nodiscard]] bool mark_ellipsoid_points(
    Context& context,
    float center_x,
    float center_y,
    float center_z,
    float radius,
    float vertical_radius) noexcept {
    const float inverse_radius = worldgen::detail::fp::div(1.0F, radius);
    const float inverse_vertical_radius =
        worldgen::detail::fp::div(1.0F, vertical_radius);
    const std::int32_t base_x =
        worldgen::detail::wrapping_mul(context.target_chunk_x, 16);
    const std::int32_t base_z =
        worldgen::detail::wrapping_mul(context.target_chunk_z, 16);

    for (PointState& state : context.points) {
        if (state.reached) {
            continue;
        }
        float relative_x = worldgen::detail::fp::sub(0.5F, center_x);
        relative_x = worldgen::detail::fp::add(
            static_cast<float>(worldgen::detail::wrapping_add(
                base_x, state.point.local_x)),
            relative_x);
        relative_x = worldgen::detail::fp::mul(relative_x, inverse_radius);
        const float x_square = worldgen::detail::fp::mul(relative_x, relative_x);
        if (x_square >= 1.0F) {
            continue;
        }

        float relative_z = worldgen::detail::fp::sub(0.5F, center_z);
        relative_z = worldgen::detail::fp::add(
            static_cast<float>(worldgen::detail::wrapping_add(
                base_z, state.point.local_z)),
            relative_z);
        relative_z = worldgen::detail::fp::mul(relative_z, inverse_radius);
        const float horizontal = worldgen::detail::fp::mul_add(
            x_square, relative_z, relative_z);
        if (horizontal >= 1.0F) {
            continue;
        }

        // Native cave geometry tests y, then writes the block at y+1.
        const std::int32_t geometry_y = state.point.y - 1;
        float relative_y = worldgen::detail::fp::sub(0.5F, center_y);
        relative_y = worldgen::detail::fp::add(
            static_cast<float>(geometry_y), relative_y);
        relative_y = worldgen::detail::fp::mul(
            relative_y, inverse_vertical_radius);
        if (static_cast<double>(relative_y) <= -0.7) {
            continue;
        }
        const float distance = worldgen::detail::fp::mul_add(
            horizontal, relative_y, relative_y);
        if (distance < 1.0F) {
            state.reached = true;
            --context.remaining;
        }
    }
    return context.remaining == 0U;
}

[[nodiscard]] bool trace_tunnel(
    worldgen::detail::Mt19937& external_random,
    Context& context,
    float x,
    float y,
    float z,
    float width,
    float yaw,
    float pitch,
    std::int32_t start,
    std::int32_t end,
    float vertical_scale) noexcept {
    worldgen::detail::Mt19937 random(external_random.next_u32() >> 1U);
    if (end < 1) {
        end = (kCaveRange - 1) * 16;
        end -= static_cast<std::int32_t>(random.next_bounded(
            static_cast<std::uint32_t>(end / 4)));
    }

    const bool room = start == -1;
    if (room) {
        start = end / 2;
    }
    const std::int32_t branch_at = static_cast<std::int32_t>(
        random.next_bounded(static_cast<std::uint32_t>(end / 2))) + end / 4;
    const std::uint32_t pitch_decay_selector = random.next_u32();
    float pitch_change = 0.0F;
    float yaw_change = 0.0F;
    const float chunk_center_x = static_cast<float>(
        worldgen::detail::wrapping_add(
            worldgen::detail::wrapping_mul(context.target_chunk_x, 16), 8));
    const float chunk_center_z = static_cast<float>(
        worldgen::detail::wrapping_add(
            worldgen::detail::wrapping_mul(context.target_chunk_z, 16), 8));

    for (std::int32_t step = start; step < end; ++step) {
        const float progress = worldgen::detail::fp::mul(
            static_cast<float>(step),
            worldgen::detail::fp::div(kPi, static_cast<float>(end)));
        float radius = worldgen::detail::fp::mul(native_sin(progress), width);
        radius = worldgen::detail::fp::add(radius, 1.5F);
        const float vertical_radius =
            worldgen::detail::fp::mul(radius, vertical_scale);

        const float sin_pitch = native_sin(pitch);
        const float cos_pitch = native_cos(pitch);
        const float sin_yaw = native_sin(yaw);
        const float cos_yaw = native_cos(yaw);
        y = worldgen::detail::fp::add(y, sin_pitch);
        x = worldgen::detail::fp::mul_add(x, cos_yaw, cos_pitch);
        z = worldgen::detail::fp::mul_add(z, sin_yaw, cos_pitch);

        pitch = worldgen::detail::fp::mul(
            pitch, pitch_decay_selector % 6U == 0U ? 0.92F : 0.7F);
        yaw = worldgen::detail::fp::mul_add(yaw, yaw_change, 0.1F);
        pitch = worldgen::detail::fp::mul_add(pitch, pitch_change, 0.1F);
        pitch_change = worldgen::detail::fp::mul(pitch_change, 0.9F);
        const float pitch_left = random.next_float();
        const float pitch_right = random.next_float();
        const float pitch_scale = worldgen::detail::fp::mul(
            random.next_float(), 2.0F);
        pitch_change = worldgen::detail::fp::add(
            pitch_change,
            worldgen::detail::fp::mul(
                pitch_scale,
                worldgen::detail::fp::sub(pitch_left, pitch_right)));
        yaw_change = worldgen::detail::fp::mul(yaw_change, 0.75F);
        const float yaw_left = random.next_float();
        const float yaw_right = random.next_float();
        const float yaw_scale = worldgen::detail::fp::mul(
            random.next_float(), 4.0F);
        yaw_change = worldgen::detail::fp::add(
            yaw_change,
            worldgen::detail::fp::mul(
                yaw_scale,
                worldgen::detail::fp::sub(yaw_left, yaw_right)));

        if (step == branch_at && width > 1.0F && !room) {
            const float left_width = worldgen::detail::fp::mul_add(
                0.5F, random.next_float(), 0.5F);
            if (trace_tunnel(
                    random, context, x, y, z, left_width,
                    worldgen::detail::fp::sub(yaw, kHalfPi),
                    worldgen::detail::fp::mul(pitch, 0.33333334F),
                    step, end, 1.0F)) {
                return true;
            }
            const float right_width = worldgen::detail::fp::mul_add(
                0.5F, random.next_float(), 0.5F);
            return trace_tunnel(
                random, context, x, y, z, right_width,
                worldgen::detail::fp::add(yaw, kHalfPi),
                worldgen::detail::fp::mul(pitch, 0.33333334F),
                step, end, 1.0F);
        }

        if (!room && (random.next_u32() & 3U) == 0U) {
            continue;
        }
        const std::int32_t remaining = end - step;
        float horizontal_distance = worldgen::detail::fp::sub(x, chunk_center_x);
        horizontal_distance = worldgen::detail::fp::mul(
            horizontal_distance, horizontal_distance);
        const float z_distance = worldgen::detail::fp::sub(z, chunk_center_z);
        horizontal_distance = worldgen::detail::fp::mul_add(
            horizontal_distance, z_distance, z_distance);
        horizontal_distance = worldgen::detail::fp::sub(
            horizontal_distance,
            worldgen::detail::fp::mul(
                static_cast<float>(remaining),
                static_cast<float>(remaining)));
        const float stop_radius = worldgen::detail::fp::add(width, 18.0F);
        if (horizontal_distance
            > worldgen::detail::fp::mul(stop_radius, stop_radius)) {
            return false;
        }

        const float doubled_radius = worldgen::detail::fp::add(radius, radius);
        const float min_chunk_x = worldgen::detail::fp::sub(
            worldgen::detail::fp::sub(chunk_center_x, doubled_radius), 16.0F);
        const float min_chunk_z = worldgen::detail::fp::sub(
            worldgen::detail::fp::sub(chunk_center_z, doubled_radius), 16.0F);
        const float max_chunk_x = worldgen::detail::fp::add(
            worldgen::detail::fp::add(chunk_center_x, 16.0F), doubled_radius);
        const float max_chunk_z = worldgen::detail::fp::add(
            worldgen::detail::fp::add(chunk_center_z, 16.0F), doubled_radius);
        if (x < min_chunk_x || z < min_chunk_z
            || x > max_chunk_x || z > max_chunk_z) {
            continue;
        }

        if (mark_ellipsoid_points(
                context, x, y, z, radius, vertical_radius)) {
            return true;
        }
        // A room may stop here in the full carver. Continuing is deliberate:
        // water could reject this ellipsoid, so later steps remain a safe
        // superset for a necessary prefilter.
    }
    return false;
}

[[nodiscard]] bool trace_source(
    worldgen::detail::Mt19937& random,
    Context& context,
    std::int32_t source_chunk_x,
    std::int32_t source_chunk_z) noexcept {
    const std::uint32_t first_bound = random.next_bounded(40U) + 1U;
    const std::uint32_t second_bound = random.next_bounded(first_bound) + 1U;
    const std::uint32_t cave_systems = random.next_bounded(second_bound);
    if (random.next_bounded(15U) != 0U) {
        return false;
    }

    const std::int32_t source_x =
        worldgen::detail::wrapping_mul(source_chunk_x, 16);
    const std::int32_t source_z =
        worldgen::detail::wrapping_mul(source_chunk_z, 16);
    for (std::uint32_t system = 0; system < cave_systems; ++system) {
        const float x = static_cast<float>(worldgen::detail::wrapping_add(
            source_x, static_cast<std::int32_t>(random.next_u32() & 15U)));
        const std::uint32_t y_bound = random.next_bounded(120U) + 8U;
        const float y = static_cast<float>(random.next_bounded(y_bound));
        const float z = static_cast<float>(worldgen::detail::wrapping_add(
            source_z, static_cast<std::int32_t>(random.next_u32() & 15U)));

        std::uint32_t tunnel_count = 1U;
        if ((random.next_u32() & 3U) == 0U) {
            const float room_width = worldgen::detail::fp::mul_add(
                1.0F, random.next_float(), 6.0F);
            if (trace_tunnel(
                    random, context, x, y, z, room_width,
                    0.0F, 0.0F, -1, -1, 0.5F)) {
                return true;
            }
            tunnel_count = (random.next_u32() & 3U) + 1U;
        }

        for (std::uint32_t tunnel = 0; tunnel < tunnel_count; ++tunnel) {
            const float yaw = worldgen::detail::fp::mul(
                random.next_float(), kTwoPi);
            const float pitch = worldgen::detail::fp::mul(
                worldgen::detail::fp::sub(random.next_float(), 0.5F), 0.25F);
            const float doubled_width_draw = random.next_float();
            const float additive_width_draw = random.next_float();
            const float width = worldgen::detail::fp::add(
                worldgen::detail::fp::mul(doubled_width_draw, 2.0F),
                additive_width_draw);
            if (trace_tunnel(
                    random, context, x, y, z, width,
                    yaw, pitch, 0, 0, 1.0F)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

bool pe090_caves_may_reach_all(
    std::uint32_t world_seed,
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    std::span<const CavePoint> points) noexcept {
    if (points.empty()) {
        return true;
    }
    std::vector<PointState> states;
    states.reserve(points.size());
    for (const CavePoint point : points) {
        states.push_back({point, false});
    }
    Context context{chunk_x, chunk_z, states, states.size()};

    worldgen::detail::Mt19937 world_random(world_seed);
    const std::uint32_t x_multiplier =
        (world_random.next_u32() >> 2U) * 2U + 1U;
    const std::uint32_t z_multiplier =
        (world_random.next_u32() >> 2U) * 2U + 1U;
    for (std::int32_t source_x = chunk_x - kSourceRadius;
         source_x <= chunk_x + kSourceRadius;
         ++source_x) {
        for (std::int32_t source_z = chunk_z - kSourceRadius;
             source_z <= chunk_z + kSourceRadius;
             ++source_z) {
            const std::uint32_t source_seed = world_seed
                ^ (static_cast<std::uint32_t>(source_x) * x_multiplier
                    + static_cast<std::uint32_t>(source_z) * z_multiplier);
            worldgen::detail::Mt19937 source_random(source_seed);
            if (trace_source(source_random, context, source_x, source_z)) {
                return true;
            }
        }
    }
    return context.remaining == 0U;
}

} // namespace mcpe::terrain_verifier
