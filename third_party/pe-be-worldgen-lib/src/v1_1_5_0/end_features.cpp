#include "v1_1_5_0/end_features.hpp"

#include "detail/fp.hpp"
#include "detail/wrap.hpp"
#include "v0_6_1/legacy_math.hpp"
#include "v1_1_5_0/blocks.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace mcpe::worldgen::v1_1_5_0 {
namespace {

[[nodiscard]] std::int32_t trunc_to_i32(float value) noexcept {
    return detail::fp::trunc_to_i32(value);
}

[[nodiscard]] std::int32_t absolute(std::int32_t value) noexcept {
    return value < 0 ? detail::wrapping_sub(0, value) : value;
}

} // namespace

std::array<GeneratedEndPillar, 10> end_pillars(
    std::uint32_t world_seed) noexcept {
    detail::Mt19937 random(world_seed);
    std::array<std::int32_t, 10> order{};
    for (std::int32_t index = 0; index < 10; ++index) {
        order[static_cast<std::size_t>(index)] = index;
    }
    for (std::int32_t index = 1; index < 10; ++index) {
        const std::int32_t selected = static_cast<std::int32_t>(
            random.next_bounded(static_cast<std::uint32_t>(index + 1)));
        const std::int32_t replacement = order[static_cast<std::size_t>(selected)];
        order[static_cast<std::size_t>(selected)] =
            order[static_cast<std::size_t>(index)];
        order[static_cast<std::size_t>(index)] = replacement;
    }

    std::array<GeneratedEndPillar, 10> result{};
    for (std::int32_t index = 0; index < 10; ++index) {
        const float phase = detail::fp::mul_add(
            -3.1415927F,
            detail::fp::mul(static_cast<float>(index), 0.1F),
            3.1415927F);
        const float table_position = detail::fp::mul(phase, 20860.756F);
        const std::uint16_t sine_index = static_cast<std::uint16_t>(
            trunc_to_i32(table_position));
        const std::uint16_t cosine_index = static_cast<std::uint16_t>(
            trunc_to_i32(detail::fp::add(table_position, 16384.0F)));
        const std::int32_t shape = order[static_cast<std::size_t>(index)];
        result[static_cast<std::size_t>(index)] = {
            trunc_to_i32(detail::fp::mul(
                v0_6_1::legacy_math::sine_table_value(cosine_index), 42.0F)),
            trunc_to_i32(detail::fp::mul(
                v0_6_1::legacy_math::sine_table_value(sine_index), 42.0F)),
            shape / 3 + 2,
            shape * 3 + 76,
            shape == 1 || shape == 2,
        };
    }
    return result;
}

void place_end_island(
    EndBlockWriter& writer,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    float radius = static_cast<float>(random.next_bounded(3U) | 4U);
    std::int32_t y_offset = 0;
    while (radius > 0.5F) {
        std::int32_t extent = trunc_to_i32(radius);
        if (static_cast<float>(extent) != radius) {
            ++extent;
        }
        const std::int32_t minimum = -extent;
        const std::int32_t maximum = extent;
        const float radius_squared = detail::fp::mul(
            detail::fp::add(radius, 1.0F), detail::fp::add(radius, 1.0F));
        for (std::int32_t offset_x = minimum; offset_x <= maximum; ++offset_x) {
            for (std::int32_t offset_z = minimum; offset_z <= maximum;
                 ++offset_z) {
                const float squared_distance = static_cast<float>(
                    offset_x * offset_x + offset_z * offset_z);
                // EndIslandFeature includes its circular boundary.  The
                // native comparison is `distanceSquared <= (radius + 1)^2`.
                if (squared_distance <= radius_squared) {
                    writer.set_end_block(
                        detail::wrapping_add(x, offset_x),
                        detail::wrapping_add(y, y_offset),
                        detail::wrapping_add(z, offset_z),
                        block::end_stone);
                }
            }
        }
        --y_offset;
        radius = detail::fp::sub(
            detail::fp::sub(radius, 0.5F),
            static_cast<float>(random.next_u32() & 1U));
    }
}

void place_end_pillar(
    EndBlockWriter& writer,
    const GeneratedEndPillar& pillar) noexcept {
    const std::int32_t radius_squared = pillar.radius * pillar.radius;
    for (std::int32_t x = pillar.center_x - pillar.radius;
         x <= pillar.center_x + pillar.radius;
         ++x) {
        for (std::int32_t z = pillar.center_z - pillar.radius;
             z <= pillar.center_z + pillar.radius;
             ++z) {
            const std::int32_t offset_x = x - pillar.center_x;
            const std::int32_t offset_z = z - pillar.center_z;
            const bool inside = offset_x * offset_x + offset_z * offset_z
                <= radius_squared;
            for (std::int32_t y = 0; y <= pillar.height + 10; ++y) {
                if (inside && y < pillar.height) {
                    writer.set_end_block(x, y, z, block::obsidian);
                } else if (y > 65) {
                    writer.set_end_block(x, y, z, block::air);
                }
            }
        }
    }

    if (pillar.guarded) {
        for (std::int32_t offset_x = -2; offset_x <= 2; ++offset_x) {
            for (std::int32_t offset_z = -2; offset_z <= 2; ++offset_z) {
                const std::int32_t x = pillar.center_x + offset_x;
                const std::int32_t z = pillar.center_z + offset_z;
                const bool outer_x = absolute(offset_x) == 2;
                const bool outer_z = absolute(offset_z) == 2;
                if (outer_x || outer_z) {
                    writer.set_end_block(x, pillar.height, z, block::iron_bars);
                    writer.set_end_block(
                        x, pillar.height + 1, z, block::iron_bars);
                    writer.set_end_block(
                        x, pillar.height + 2, z, block::iron_bars);
                }
                writer.set_end_block(x, pillar.height + 3, z, block::iron_bars);
            }
        }
    }

    // The fresh End fight creates one crystal per spike; its first server tick
    // leaves fire above this bedrock support, so both blocks are part of the
    // player-visible terrain rather than the raw SpikeFeature output.
    writer.set_end_block(
        pillar.center_x, pillar.height, pillar.center_z, block::bedrock, 1U);
    writer.set_end_block(
        pillar.center_x, pillar.height + 1, pillar.center_z, block::fire);
}

void place_end_gateway(
    EndBlockWriter& writer,
    BlockPosition center) noexcept {
    // PE 1.1.5 EndGatewayFeature::place.  The feature writes every cell in
    // this volume (including air), so apply it exactly as a complete shell
    // rather than layering a few blocks over existing terrain.
    for (std::int32_t offset_x = -1; offset_x <= 1; ++offset_x) {
        for (std::int32_t offset_y = -2; offset_y <= 2; ++offset_y) {
            for (std::int32_t offset_z = -1; offset_z <= 1; ++offset_z) {
                const bool central = offset_x == 0
                    && offset_y == 0
                    && offset_z == 0;
                const bool cap = offset_x == 0
                    && offset_z == 0
                    && (offset_y == -2 || offset_y == 2);
                const bool cross = (offset_x == 0 || offset_z == 0)
                    && offset_y != -2
                    && offset_y != 2;
                const std::uint8_t block_id = central ? block::end_gateway
                    : (cap || cross) ? block::bedrock
                    : block::air;
                const std::uint8_t data = block_id == block::bedrock ? 1U : 0U;
                writer.set_end_block(
                    detail::wrapping_add(center.x, offset_x),
                    detail::wrapping_add(center.y, offset_y),
                    detail::wrapping_add(center.z, offset_z),
                    block_id,
                    data);
            }
        }
    }
}

void place_end_exit_podium(
    EndBlockWriter& writer,
    BlockPosition center,
    bool active) noexcept {
    constexpr std::int32_t kRadius = 4;
    constexpr std::int32_t kRimRadius = 1;
    constexpr std::int32_t kPillarHeight = 4;
    constexpr float kCornerRounding = 0.5F;

    const float outer_radius = static_cast<float>(kRadius) - kCornerRounding;
    const float inner_radius = static_cast<float>(
        kRadius - kRimRadius) - kCornerRounding;
    const float outer_radius_squared = outer_radius * outer_radius;
    const float inner_radius_squared = inner_radius * inner_radius;

    for (std::int32_t offset_x = -kRadius; offset_x <= kRadius; ++offset_x) {
        for (std::int32_t offset_y = -1; offset_y <= 32; ++offset_y) {
            for (std::int32_t offset_z = -kRadius;
                 offset_z <= kRadius;
                 ++offset_z) {
                const float distance_squared = static_cast<float>(
                    offset_x * offset_x + offset_z * offset_z);
                if (distance_squared > outer_radius_squared) {
                    continue;
                }

                const std::int32_t x = detail::wrapping_add(
                    center.x, offset_x);
                const std::int32_t y = detail::wrapping_add(
                    center.y, offset_y);
                const std::int32_t z = detail::wrapping_add(
                    center.z, offset_z);
                if (offset_y < 0) {
                    writer.set_end_block(
                        x,
                        y,
                        z,
                        distance_squared <= inner_radius_squared
                            ? block::bedrock
                            : block::end_stone,
                        distance_squared <= inner_radius_squared ? 1U : 0U);
                } else if (offset_y == 0) {
                    writer.set_end_block(
                        x,
                        y,
                        z,
                        distance_squared > inner_radius_squared
                            ? block::bedrock
                            : active ? block::end_portal : block::air,
                        distance_squared > inner_radius_squared ? 1U : 0U);
                } else {
                    writer.set_end_block(x, y, z, block::air);
                }
            }
        }
    }

    for (std::int32_t offset_y = 0; offset_y < kPillarHeight; ++offset_y) {
        writer.set_end_block(
            center.x,
            detail::wrapping_add(center.y, offset_y),
            center.z,
            block::bedrock,
            1U);
    }

    const std::int32_t torch_y = detail::wrapping_add(
        center.y, kPillarHeight / 2);
    writer.set_end_block(center.x, torch_y, center.z - 1, block::torch, 4U);
    writer.set_end_block(center.x, torch_y, center.z + 1, block::torch, 3U);
    writer.set_end_block(center.x + 1, torch_y, center.z, block::torch, 1U);
    writer.set_end_block(center.x - 1, torch_y, center.z, block::torch, 2U);
}

void place_end_arrival_platform(EndBlockWriter& writer) noexcept {
    constexpr std::int32_t kCenterX = 100;
    constexpr std::int32_t kCenterY = 49;
    constexpr std::int32_t kCenterZ = 0;

    for (std::int32_t offset_x = -2; offset_x <= 2; ++offset_x) {
        for (std::int32_t offset_y = -1; offset_y <= 2; ++offset_y) {
            for (std::int32_t offset_z = -2; offset_z <= 2; ++offset_z) {
                writer.set_end_block(
                    kCenterX + offset_x,
                    kCenterY + offset_y,
                    kCenterZ + offset_z,
                    offset_y == -1 ? block::obsidian : block::air);
            }
        }
    }
}

} // namespace mcpe::worldgen::v1_1_5_0
