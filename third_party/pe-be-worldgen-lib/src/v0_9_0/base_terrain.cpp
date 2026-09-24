#include "v0_9_0/base_terrain.hpp"

#include "detail/fp.hpp"
#include "v0_9_0/blocks.hpp"

#include <cassert>

namespace mcpe::worldgen::v0_9_0 {
namespace {

constexpr std::int32_t kCoarseWidth = 5;
constexpr std::int32_t kCoarseHeight = 17;

[[nodiscard]] constexpr std::size_t density_index(
    std::int32_t x,
    std::int32_t z,
    std::int32_t y) noexcept {
    return (static_cast<std::size_t>(x) * kCoarseWidth
            + static_cast<std::size_t>(z))
        * kCoarseHeight
        + static_cast<std::size_t>(y);
}

[[nodiscard]] constexpr std::size_t block_index(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    // LevelChunk stores columns as x * 2048 + z * 128 + y.
    return (static_cast<std::size_t>(x) << 11U)
        | (static_cast<std::size_t>(z) << 7U)
        | static_cast<std::size_t>(y);
}

[[nodiscard]] constexpr std::uint8_t base_block(
    float density,
    std::int32_t y,
    BaseTerrainMaterials materials) noexcept {
    if (density > 0.0F) {
        return materials.solid;
    }
    return y < materials.liquid_level ? materials.liquid : block::air;
}

} // namespace

void interpolate_base_terrain(
    std::span<std::uint8_t> blocks,
    std::span<const float> density_lattice,
    BaseTerrainMaterials materials) noexcept {
    assert(blocks.size() >= kChunkBlockCount);
    assert(density_lattice.size()
        >= static_cast<std::size_t>(kCoarseWidth) * kCoarseWidth * kCoarseHeight);

    for (std::int32_t coarse_x = 0; coarse_x < 4; ++coarse_x) {
        for (std::int32_t coarse_z = 0; coarse_z < 4; ++coarse_z) {
            for (std::int32_t coarse_y = 0; coarse_y < 16; ++coarse_y) {
                float density_x0_z0 = density_lattice[density_index(
                    coarse_x, coarse_z, coarse_y)];
                float density_x0_z1 = density_lattice[density_index(
                    coarse_x, coarse_z + 1, coarse_y)];
                float density_x1_z0 = density_lattice[density_index(
                    coarse_x + 1, coarse_z, coarse_y)];
                float density_x1_z1 = density_lattice[density_index(
                    coarse_x + 1, coarse_z + 1, coarse_y)];

                const float density_x0_z0_step = detail::fp::mul(
                    detail::fp::sub(
                        density_lattice[density_index(coarse_x, coarse_z, coarse_y + 1)],
                        density_x0_z0),
                    0.125F);
                const float density_x0_z1_step = detail::fp::mul(
                    detail::fp::sub(
                        density_lattice[density_index(coarse_x, coarse_z + 1, coarse_y + 1)],
                        density_x0_z1),
                    0.125F);
                const float density_x1_z0_step = detail::fp::mul(
                    detail::fp::sub(
                        density_lattice[density_index(coarse_x + 1, coarse_z, coarse_y + 1)],
                        density_x1_z0),
                    0.125F);
                const float density_x1_z1_step = detail::fp::mul(
                    detail::fp::sub(
                        density_lattice[density_index(coarse_x + 1, coarse_z + 1, coarse_y + 1)],
                        density_x1_z1),
                    0.125F);

                for (std::int32_t local_y = 0; local_y < 8; ++local_y) {
                    float density_z0 = density_x0_z0;
                    float density_z1 = density_x0_z1;
                    const float density_x_z0_step = detail::fp::mul(
                        detail::fp::sub(density_x1_z0, density_x0_z0), 0.25F);
                    const float density_x_z1_step = detail::fp::mul(
                        detail::fp::sub(density_x1_z1, density_x0_z1), 0.25F);
                    const std::int32_t y = coarse_y * 8 + local_y;

                    for (std::int32_t local_x = 0; local_x < 4; ++local_x) {
                        float density = density_z0;
                        const float density_z_step = detail::fp::mul(
                            detail::fp::sub(density_z1, density_z0), 0.25F);
                        const std::int32_t x = coarse_x * 4 + local_x;

                        for (std::int32_t local_z = 0; local_z < 4; ++local_z) {
                            const std::int32_t z = coarse_z * 4 + local_z;
                            blocks[block_index(x, y, z)] = base_block(
                                density, y, materials);
                            density = detail::fp::add(density, density_z_step);
                        }

                        density_z0 = detail::fp::add(density_z0, density_x_z0_step);
                        density_z1 = detail::fp::add(density_z1, density_x_z1_step);
                    }

                    density_x0_z0 = detail::fp::add(
                        density_x0_z0, density_x0_z0_step);
                    density_x0_z1 = detail::fp::add(
                        density_x0_z1, density_x0_z1_step);
                    density_x1_z0 = detail::fp::add(
                        density_x1_z0, density_x1_z0_step);
                    density_x1_z1 = detail::fp::add(
                        density_x1_z1, density_x1_z1_step);
                }
            }
        }
    }
}

void interpolate_base_terrain(
    std::span<std::uint8_t> blocks,
    std::span<const float> density_lattice) noexcept {
    interpolate_base_terrain(
        blocks,
        density_lattice,
        {block::rock, block::calm_water, 63});
}

} // namespace mcpe::worldgen::v0_9_0
