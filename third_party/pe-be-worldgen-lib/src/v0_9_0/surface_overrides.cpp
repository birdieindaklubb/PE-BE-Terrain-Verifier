#include "v0_9_0/surface_overrides.hpp"

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

[[nodiscard]] constexpr SurfaceProfile grass_dirt(float temperature) noexcept {
    return SurfaceProfile{block::grass, 0U, block::dirt, temperature};
}

} // namespace

SurfaceProfile extreme_hills_surface_profile(
    std::int32_t type,
    float surface_noise,
    float temperature) noexcept {
    // ExtremeHillsBiome::buildSurfaceAt first resets the inherited fields,
    // then applies one of these two overrides before the default routine.
    if (type == 2 && (surface_noise < -1.0F || surface_noise > 2.0F)) {
        return SurfaceProfile{block::gravel, 0U, block::gravel, temperature};
    }
    if (type != 1 && surface_noise > 1.0F) {
        return SurfaceProfile{block::rock, 0U, block::rock, temperature};
    }
    return grass_dirt(temperature);
}

SurfaceProfile taiga_surface_profile(
    std::int32_t type,
    float surface_noise,
    float temperature) noexcept {
    SurfaceProfile profile = grass_dirt(temperature);
    if (type != 1 && type != 2) {
        return profile;
    }
    if (surface_noise > 1.75F) {
        profile.top_block = block::dirt;
        profile.top_data = 1U;
    } else if (surface_noise > -0.95F) {
        profile.top_block = block::podzol;
    }
    return profile;
}

SurfaceProfile mutated_savanna_surface_profile(
    float surface_noise,
    float temperature) noexcept {
    if (surface_noise > 1.75F) {
        return SurfaceProfile{block::rock, 0U, block::rock, temperature};
    }
    if (surface_noise > -0.5F) {
        return SurfaceProfile{block::dirt, 1U, block::dirt, temperature};
    }
    return grass_dirt(temperature);
}

SwampSurface::SwampSurface()
    : noise_([] {
        detail::Mt19937 random(0x929U);
        return detail::PerlinSimplexNoise(random, 1);
    }()) {}

void SwampSurface::apply_prepass(
    std::span<std::uint8_t> blocks,
    std::int32_t x,
    std::int32_t z,
    std::int32_t world_x,
    std::int32_t world_z) const noexcept {
    assert(blocks.size() >= kChunkBlockCount);
    assert(x >= 0 && x < kChunkWidth && z >= 0 && z < kChunkWidth);

    const float noise = noise_.value(
        static_cast<float>(world_x), static_cast<float>(world_z));
    if (noise <= 0.0F) {
        return;
    }

    for (std::int32_t y = kChunkHeight - 1; y >= 0; --y) {
        const std::size_t index = block_index(x, y, z);
        if (blocks[index] == block::air) {
            continue;
        }
        if (y == 62 && blocks[index] != block::calm_water) {
            blocks[index] = block::calm_water;
            if (noise < 0.12F) {
                blocks[block_index(x, 63, z)] = block::waterlily;
            }
        }
        return;
    }
}

} // namespace mcpe::worldgen::v0_9_0
