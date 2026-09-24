#include "v0_9_0/surface.hpp"

#include "detail/fp.hpp"
#include "detail/mt19937.hpp"
#include "v0_9_0/base_terrain.hpp"
#include "v0_9_0/blocks.hpp"

#include <cassert>
#include <cstddef>

namespace mcpe::worldgen::v0_9_0 {
namespace {

[[nodiscard]] constexpr std::size_t block_index(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    return (static_cast<std::size_t>(x) << 11U)
        | (static_cast<std::size_t>(z) << 7U)
        | static_cast<std::size_t>(y);
}

void set_top(
    std::span<std::uint8_t> blocks,
    std::span<std::uint8_t> data,
    std::size_t index,
    std::uint8_t tile,
    std::uint8_t metadata) noexcept {
    blocks[index] = tile;
    std::uint8_t& packed = data[index >> 1U];
    if ((index & 1U) == 0U) {
        packed = static_cast<std::uint8_t>((packed & 0xf0U) | (metadata & 0x0fU));
    } else {
        packed = static_cast<std::uint8_t>((packed & 0x0fU) | ((metadata & 0x0fU) << 4U));
    }
}

} // namespace

void apply_default_surface_column(
    detail::Mt19937& random,
    std::span<std::uint8_t> blocks,
    std::span<std::uint8_t> data,
    std::int32_t x,
    std::int32_t z,
    float surface_noise,
    const SurfaceProfile& profile) noexcept {
    assert(blocks.size() >= kChunkBlockCount);
    assert(data.size() >= kChunkBlockCount / 2U);
    assert(x >= 0 && x < kChunkWidth && z >= 0 && z < kChunkWidth);

    // This is one draw before the y=127..0 bedrock draws, exactly as in
    // Biome::buildSurfaceAtDefault.
    float thickness_value = detail::fp::mul(surface_noise, 0.33333334F);
    thickness_value = detail::fp::add(thickness_value, 3.0F);
    thickness_value = detail::fp::mul_add(
        thickness_value, random.next_float(), 0.25F);
    const std::int32_t thickness = detail::fp::trunc_to_i32(thickness_value);

    std::int32_t remaining = -1;
    std::uint8_t top = profile.top_block;
    std::uint8_t top_data = profile.top_data;
    std::uint8_t filler = profile.filler_block;

    for (std::int32_t y = kChunkHeight - 1; y >= 0; --y) {
        const std::size_t index = block_index(x, y, z);
        if (random.next_bounded(5U) >= static_cast<std::uint32_t>(y)) {
            blocks[index] = block::unbreakable;
            continue;
        }

        if (blocks[index] == block::air) {
            remaining = -1;
            continue;
        }
        if (blocks[index] != block::rock) {
            continue;
        }

        if (remaining != -1) {
            if (remaining > 0) {
                --remaining;
                blocks[index] = filler;
                if (remaining == 0 && filler == block::sand) {
                    remaining = static_cast<std::int32_t>(random.next_bounded(4U));
                    if (y >= 63) {
                        remaining += y - 63;
                    }
                    filler = block::sand_stone;
                }
            }
            continue;
        }

        remaining = thickness;
        if (thickness < 1) {
            filler = block::rock;
            if (y < 63) {
                top = profile.temperature < 0.15F ? block::ice : block::calm_water;
                top_data = 0;
            } else {
                top = block::air;
                top_data = 0;
            }
        } else if (y >= 59 && y <= 64) {
            top = profile.top_block;
            top_data = profile.top_data;
            filler = profile.filler_block;
        }

        if (y < 63 && top == block::air) {
            top = profile.temperature < 0.15F ? block::ice : block::calm_water;
            top_data = 0;
        }

        if (y < 62) {
            if (y < 56 - thickness) {
                filler = block::rock;
                top = block::air;
                blocks[index] = block::gravel;
            } else {
                blocks[index] = filler;
            }
            continue;
        }

        set_top(blocks, data, index, top, top_data);
    }
}

} // namespace mcpe::worldgen::v0_9_0
