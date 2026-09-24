#include "v1_1_5_0/end_terrain.hpp"

#include "detail/fp.hpp"
#include "detail/wrap.hpp"
#include "v1_1_5_0/blocks.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mcpe::worldgen::v1_1_5_0 {
namespace {

constexpr std::int32_t kLatticeWidth = 3;
constexpr std::int32_t kLatticeHeight = 33;
constexpr std::uint8_t kEndBiome = 9;

constexpr float kSelectorScaleX = 17.1103F;
constexpr float kSelectorScaleY = 4.277575F;
constexpr float kSelectorScaleZ = 17.1103F;
constexpr float kDensityScaleX = 1368.824F;
constexpr float kDensityScaleY = 684.412F;
constexpr float kDensityScaleZ = 1368.824F;

[[nodiscard]] constexpr std::size_t lattice_index(
    std::int32_t x,
    std::int32_t z,
    std::int32_t y) noexcept {
    return (static_cast<std::size_t>(x) * kLatticeWidth
            + static_cast<std::size_t>(z))
        * kLatticeHeight
        + static_cast<std::size_t>(y);
}

[[nodiscard]] constexpr std::uint32_t absolute_bits(
    std::int32_t value) noexcept {
    const std::uint32_t bits = detail::bits(value);
    return value < 0 ? 0U - bits : bits;
}

[[nodiscard]] float clamp_island_height(float value) noexcept {
    float result = -100.0F;
    if (-100.0F < value) {
        result = value;
    }
    if (80.0F < value) {
        result = 80.0F;
    }
    return result;
}

[[nodiscard]] float distance(
    float x,
    float z) noexcept {
    return std::sqrt(detail::fp::add(
        detail::fp::mul(x, x), detail::fp::mul(z, z)));
}

} // namespace

EndTerrain::EndTerrain(std::uint32_t world_seed)
    : random_(world_seed),
      density_lower_(random_, 16),
      density_upper_(random_, 16),
      density_selector_(random_, 8),
      island_noise_(random_) {
}

void EndTerrain::density_lattice(
    std::vector<float>& output,
    std::int32_t coarse_x,
    std::int32_t coarse_z) const {
    std::vector<float> selector;
    std::vector<float> lower;
    std::vector<float> upper;

    density_selector_.region(
        selector,
        static_cast<float>(coarse_x),
        0.0F,
        static_cast<float>(coarse_z),
        kLatticeWidth,
        kLatticeHeight,
        kLatticeWidth,
        kSelectorScaleX,
        kSelectorScaleY,
        kSelectorScaleZ);
    density_lower_.region(
        lower,
        static_cast<float>(coarse_x),
        0.0F,
        static_cast<float>(coarse_z),
        kLatticeWidth,
        kLatticeHeight,
        kLatticeWidth,
        kDensityScaleX,
        kDensityScaleY,
        kDensityScaleZ);
    density_upper_.region(
        upper,
        static_cast<float>(coarse_x),
        0.0F,
        static_cast<float>(coarse_z),
        kLatticeWidth,
        kLatticeHeight,
        kLatticeWidth,
        kDensityScaleX,
        kDensityScaleY,
        kDensityScaleZ);

    output.resize(static_cast<std::size_t>(kLatticeWidth)
        * kLatticeWidth * kLatticeHeight);
    for (std::int32_t lattice_x = 0; lattice_x < kLatticeWidth; ++lattice_x) {
        for (std::int32_t lattice_z = 0; lattice_z < kLatticeWidth;
             ++lattice_z) {
            const float island_height = island_height_value(
                // prepareHeights supplies doubled chunk coordinates to
                // getHeights; native getHeights divides them before this
                // call (the inputs are always even here).
                coarse_x / 2,
                coarse_z / 2,
                lattice_x,
                lattice_z);
            const std::size_t base = lattice_index(lattice_x, lattice_z, 0);

            std::int32_t vertical = -14;
            std::int32_t bottom_fade = 8;
            for (std::int32_t y = 0; y < kLatticeHeight; ++y) {
                const std::size_t index = base + static_cast<std::size_t>(y);
                // Native VFP: selector * 0.05F + 0.5F.  The two literal
                // values deliberately are not interchangeable here.
                // VFP: selector * 0.05F + 0.5F.  fp::mul_add is
                // accumulator + a * b, so the offset is its first operand.
                const float blend = detail::fp::mul_add(
                    0.5F, selector[index], 0.05F);
                float density = detail::fp::mul(lower[index], 0.001953125F);
                if (blend >= 0.0F) {
                    if (blend < 1.0F) {
                        const float delta = detail::fp::mul(
                            detail::fp::sub(upper[index], lower[index]),
                            0.001953125F);
                        density = detail::fp::mul_add(density, delta, blend);
                    } else {
                        density = detail::fp::mul(
                            upper[index], 0.001953125F);
                    }
                }
                density = detail::fp::add(
                    detail::fp::sub(island_height, 8.0F), density);

                if (vertical + 14 < 15) {
                    if (vertical + 14 < 8) {
                        const float fade = static_cast<float>(bottom_fade);
                        const float multiplier = detail::fp::mul_add(
                            1.0F, fade, -0.14285715F);
                        density = detail::fp::mul_add(
                            detail::fp::mul(density, multiplier),
                            fade,
                            -4.2857146F);
                    }
                } else {
                    float fade = detail::fp::mul(
                        static_cast<float>(vertical), 0.015625F);
                    if (fade < 0.0F) {
                        fade = 0.0F;
                    }
                    if (fade > 1.0F) {
                        fade = 1.0F;
                    }
                    density = detail::fp::mul_add(
                        detail::fp::mul(density, detail::fp::sub(1.0F, fade)),
                        fade,
                        -3000.0F);
                }
                output[index] = density;
                ++vertical;
                --bottom_fade;
            }
        }
    }
}

float EndTerrain::island_height_value(
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    std::int32_t local_x,
    std::int32_t local_z) const noexcept {
    const std::int32_t base_x = detail::wrapping_add(
        detail::wrapping_mul(chunk_x, 2), local_x);
    const std::int32_t base_z = detail::wrapping_add(
        detail::wrapping_mul(chunk_z, 2), local_z);
    const float base_x_float = static_cast<float>(base_x);
    const float base_z_float = static_cast<float>(base_z);
    float result = clamp_island_height(detail::fp::add(
        100.0F,
        detail::fp::mul(distance(base_x_float, base_z_float), -8.0F)));

    for (std::int32_t offset_x = -12; offset_x <= 12; ++offset_x) {
        const std::int32_t candidate_x = detail::wrapping_add(chunk_x, offset_x);
        const std::uint32_t abs_x = absolute_bits(candidate_x);
        const std::uint64_t seed_x = static_cast<std::uint64_t>(abs_x) * 3439U;
        const std::int32_t local_delta_x = detail::wrapping_add(
            local_x, detail::wrapping_mul(offset_x, -2));

        for (std::int32_t offset_z = -12; offset_z <= 12; ++offset_z) {
            const std::int32_t candidate_z = detail::wrapping_add(chunk_z, offset_z);
            const std::uint64_t squared_distance =
                static_cast<std::uint64_t>(absolute_bits(candidate_x))
                    * absolute_bits(candidate_x)
                + static_cast<std::uint64_t>(absolute_bits(candidate_z))
                    * absolute_bits(candidate_z);
            if (squared_distance <= 4096U
                || island_noise_.value(
                    static_cast<float>(candidate_x),
                    static_cast<float>(candidate_z)) >= -0.9F) {
                continue;
            }

            const std::uint64_t island_seed = seed_x
                + static_cast<std::uint64_t>(absolute_bits(candidate_z)) * 147U;
            const float radius = static_cast<float>(island_seed % 13U + 9U);
            const std::int32_t local_delta_z = detail::wrapping_add(
                local_z, detail::wrapping_mul(offset_z, -2));
            const float candidate = detail::fp::sub(
                100.0F,
                detail::fp::mul(
                    radius,
                    distance(
                        static_cast<float>(local_delta_x),
                        static_cast<float>(local_delta_z))));
            const float clamped = clamp_island_height(candidate);
            if (result < clamped) {
                result = clamped;
            }
        }
    }
    return result;
}

Chunk EndTerrain::generate_base_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) const {
    std::vector<float> density;
    density_lattice(
        density,
        detail::wrapping_mul(chunk_x, 2),
        detail::wrapping_mul(chunk_z, 2));

    Chunk chunk{};
    chunk.x = chunk_x;
    chunk.z = chunk_z;
    chunk.biomes.fill(kEndBiome);
    chunk.temperatures.fill(0.5F);
    chunk.rainfall.fill(0.5F);

    for (std::int32_t cell_x = 0; cell_x < 2; ++cell_x) {
        for (std::int32_t cell_z = 0; cell_z < 2; ++cell_z) {
            for (std::int32_t cell_y = 0; cell_y < 32; ++cell_y) {
                float z1_x0 = density[lattice_index(cell_x, cell_z + 1, cell_y)];
                float z1_x1 = density[lattice_index(
                    cell_x + 1, cell_z + 1, cell_y)];
                float z0_x0 = density[lattice_index(cell_x, cell_z, cell_y)];
                float z0_x1 = density[lattice_index(
                    cell_x + 1, cell_z, cell_y)];
                const float z1_x0_y_step = detail::fp::mul(
                    detail::fp::sub(
                        density[lattice_index(cell_x, cell_z + 1, cell_y + 1)],
                        z1_x0),
                    0.25F);
                const float z1_x1_y_step = detail::fp::mul(
                    detail::fp::sub(
                        density[lattice_index(
                            cell_x + 1, cell_z + 1, cell_y + 1)],
                        z1_x1),
                    0.25F);
                const float z0_x0_y_step = detail::fp::mul(
                    detail::fp::sub(
                        density[lattice_index(cell_x, cell_z, cell_y + 1)],
                        z0_x0),
                    0.25F);
                const float z0_x1_y_step = detail::fp::mul(
                    detail::fp::sub(
                        density[lattice_index(
                            cell_x + 1, cell_z, cell_y + 1)],
                        z0_x1),
                    0.25F);

                for (std::int32_t local_y = 0; local_y < 4; ++local_y) {
                    float z1 = z1_x0;
                    float z0 = z0_x0;
                    // The native inner loop recalculates the horizontal
                    // slopes after each vertical interpolation step. Keeping
                    // these outside this loop is algebraically similar but
                    // changes binary32 threshold decisions.
                    const float z1_x_step = detail::fp::mul(
                        detail::fp::sub(z1_x1, z1_x0), 0.125F);
                    const float z0_x_step = detail::fp::mul(
                        detail::fp::sub(z0_x1, z0_x0), 0.125F);
                    const std::int32_t y = cell_y * 4 + local_y;
                    for (std::int32_t local_x = 0; local_x < 8; ++local_x) {
                        const float z_step = detail::fp::mul(
                            detail::fp::sub(z1, z0), 0.125F);
                        const std::int32_t x = cell_x * 8 + local_x;
                        float value = z0;
                        for (std::int32_t local_z = 0; local_z < 8; ++local_z) {
                            if (value > 0.0F) {
                                chunk.set_block(
                                    x,
                                    y,
                                    cell_z * 8 + local_z,
                                    block::end_stone);
                            }
                            if (local_z != 7) {
                                value = detail::fp::add(value, z_step);
                            }
                        }
                        z1 = detail::fp::add(z1, z1_x_step);
                        z0 = detail::fp::add(z0, z0_x_step);
                    }
                    z1_x1 = detail::fp::add(z1_x1, z1_x1_y_step);
                    z0_x1 = detail::fp::add(z0_x1, z0_x1_y_step);
                    z1_x0 = detail::fp::add(z1_x0, z1_x0_y_step);
                    z0_x0 = detail::fp::add(z0_x0, z0_x0_y_step);
                }
            }
        }
    }

    recalculate_heightmap(chunk);
    return chunk;
}

void EndTerrain::recalculate_heightmap(Chunk& chunk) noexcept {
    std::int32_t minimum_height = Chunk::height - 1;
    for (std::int32_t x = 0; x < Chunk::width; ++x) {
        for (std::int32_t z = 0; z < Chunk::width; ++z) {
            std::int32_t height = Chunk::height - 1;
            while (height != 0 && chunk.block(x, height - 1, z) == 0U) {
                --height;
            }
            chunk.heightmap[Chunk::column_index(x, z)] =
                static_cast<std::uint8_t>(height);
            minimum_height = std::min(minimum_height, height);
        }
    }
    chunk.minimum_height = minimum_height;
}

} // namespace mcpe::worldgen::v1_1_5_0
