#include "pe061_terrain_source.hpp"

#include "detail/fp.hpp"
#include "detail/wrap.hpp"
#include "v0_6_1/blocks.hpp"

#include <cassert>
#include <cstddef>

namespace mcpe::terrain_verifier {
namespace {

using worldgen::detail::fp::add;
using worldgen::detail::fp::div;
using worldgen::detail::fp::mul;
using worldgen::detail::fp::mul_add;
using worldgen::detail::fp::sub;

[[nodiscard]] constexpr std::size_t field_index(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::int32_t y_size,
    std::int32_t z_size) noexcept {
    return (static_cast<std::size_t>(x) * static_cast<std::size_t>(z_size)
            + static_cast<std::size_t>(z))
            * static_cast<std::size_t>(y_size)
        + static_cast<std::size_t>(y);
}

[[nodiscard]] constexpr std::size_t map_index(
    std::int32_t x,
    std::int32_t z,
    std::int32_t z_size) noexcept {
    return static_cast<std::size_t>(x) * static_cast<std::size_t>(z_size)
        + static_cast<std::size_t>(z);
}

} // namespace

Pe061TerrainSource::Pe061TerrainSource(std::uint32_t world_seed)
    : random_(world_seed),
      lower_noise_(random_, 16),
      upper_noise_(random_, 16),
      selector_noise_(random_, 8),
      surface_noise_(random_, 4),
      surface_depth_noise_(random_, 4),
      depth_noise_(random_, 10),
      scale_noise_(random_, 16),
      biome_source_(world_seed) {
}

void Pe061TerrainSource::get_heights(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::int32_t x_size,
    std::int32_t y_size,
    std::int32_t z_size) {
    assert(x_size > 0 && y_size > 0 && z_size > 0);

    depth_noise_.region2d(
        depth_field_, x, z, x_size, z_size, 1.12100005F, 1.12100005F, 0.5F);
    scale_noise_.region2d(
        scale_field_, x, z, x_size, z_size, 200.0F, 200.0F, 0.5F);
    selector_noise_.region(
        selector_field_,
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(z),
        x_size,
        y_size,
        z_size,
        8.55515003F,
        4.27757502F,
        8.55515003F);
    lower_noise_.region(
        lower_field_,
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(z),
        x_size,
        y_size,
        z_size,
        684.411987F,
        684.411987F,
        684.411987F);
    upper_noise_.region(
        upper_field_,
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(z),
        x_size,
        y_size,
        z_size,
        684.411987F,
        684.411987F,
        684.411987F);

    heights_.resize(
        static_cast<std::size_t>(x_size)
        * static_cast<std::size_t>(y_size)
        * static_cast<std::size_t>(z_size));

    const std::int32_t climate_step = 16 / x_size;
    const std::int32_t climate_half = climate_step / 2;
    const auto temperatures = biome_source_.temperature_map();
    const auto rainfall = biome_source_.rainfall_map();

    for (std::int32_t ix = 0; ix < x_size; ++ix) {
        for (std::int32_t iz = 0; iz < z_size; ++iz) {
            const std::int32_t climate_x = ix * climate_step + climate_half;
            const std::int32_t climate_z = iz * climate_step + climate_half;
            const std::size_t climate_index = map_index(climate_x, climate_z, 16);

            float climate = mul(temperatures[climate_index], rainfall[climate_index]);
            climate = sub(1.0F, climate);
            climate = mul(climate, climate);
            climate = sub(1.0F, mul(climate, climate));

            const std::size_t column = map_index(ix, iz, z_size);
            float depth = add(depth_field_[column], 256.0F);
            depth = mul(depth, 0.001953125F);
            depth = mul(depth, climate);
            if (depth > 1.0F) {
                depth = 1.0F;
            }

            float scale = div(scale_field_[column], 8000.0F);
            if (scale < 0.0F) {
                scale = -mul(scale, 0.300000012F);
            }
            scale = sub(mul(scale, 3.0F), 2.0F);

            if (scale < 0.0F) {
                scale = mul(scale, 0.5F);
                if (scale < -1.0F) {
                    scale = -1.0F;
                }
                scale = div(scale, 1.39999998F);
                scale = mul(scale, 0.5F);
                depth = 0.0F;
            } else {
                if (scale > 1.0F) {
                    scale = 1.0F;
                }
                scale = mul(scale, 0.125F);
                if (depth < 0.0F) {
                    depth = 0.0F;
                }
            }

            const float y_size_f = static_cast<float>(y_size);
            scale = mul(scale, y_size_f);
            scale = mul(scale, 0.0625F);
            const float center = mul_add(mul(scale, 4.0F), y_size_f, 0.5F);
            depth = add(depth, 0.5F);

            for (std::int32_t iy = 0; iy < y_size; ++iy) {
                float vertical = sub(static_cast<float>(iy), center);
                vertical = mul(vertical, 12.0F);
                vertical = div(vertical, depth);
                if (vertical < 0.0F) {
                    vertical = mul(vertical, 4.0F);
                }

                const std::size_t index = field_index(ix, iy, iz, y_size, z_size);
                float blend = div(selector_field_[index], 10.0F);
                blend = add(blend, 1.0F);
                blend = mul(blend, 0.5F);

                float density = mul(lower_field_[index], 0.001953125F);
                if (blend >= 0.0F) {
                    const float upper = mul(upper_field_[index], 0.001953125F);
                    if (blend <= 1.0F) {
                        density = mul_add(density, sub(upper, density), blend);
                    } else {
                        density = upper;
                    }
                }
                density = sub(density, vertical);

                if (iy >= y_size - 3) {
                    const float fade = div(
                        static_cast<float>(iy - (y_size - 4)), 3.0F);
                    density = mul_add(
                        mul(-10.0F, fade), density, sub(1.0F, fade));
                }
                heights_[index] = density;
            }
        }
    }
}

void Pe061TerrainSource::prepare_heights(
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    std::span<const float> temperatures,
    worldgen::Chunk& chunk) {
    constexpr std::int32_t coarse_x_size = 5;
    constexpr std::int32_t coarse_y_size = 17;
    constexpr std::int32_t coarse_z_size = 5;

    get_heights(
        worldgen::detail::wrapping_mul(chunk_x, 4),
        0,
        worldgen::detail::wrapping_mul(chunk_z, 4),
        coarse_x_size,
        coarse_y_size,
        coarse_z_size);

    for (std::int32_t cell_x = 0; cell_x < 4; ++cell_x) {
        for (std::int32_t cell_z = 0; cell_z < 4; ++cell_z) {
            for (std::int32_t cell_y = 0; cell_y < 16; ++cell_y) {
                float x0_z0 = heights_[field_index(
                    cell_x, cell_y, cell_z, coarse_y_size, coarse_z_size)];
                float x1_z0 = heights_[field_index(
                    cell_x + 1, cell_y, cell_z, coarse_y_size, coarse_z_size)];
                float x0_z1 = heights_[field_index(
                    cell_x, cell_y, cell_z + 1, coarse_y_size, coarse_z_size)];
                float x1_z1 = heights_[field_index(
                    cell_x + 1, cell_y, cell_z + 1, coarse_y_size, coarse_z_size)];

                const float x0_z0_y_step = mul(
                    sub(heights_[field_index(
                            cell_x, cell_y + 1, cell_z,
                            coarse_y_size, coarse_z_size)], x0_z0),
                    0.125F);
                const float x1_z0_y_step = mul(
                    sub(heights_[field_index(
                            cell_x + 1, cell_y + 1, cell_z,
                            coarse_y_size, coarse_z_size)], x1_z0),
                    0.125F);
                const float x0_z1_y_step = mul(
                    sub(heights_[field_index(
                            cell_x, cell_y + 1, cell_z + 1,
                            coarse_y_size, coarse_z_size)], x0_z1),
                    0.125F);
                const float x1_z1_y_step = mul(
                    sub(heights_[field_index(
                            cell_x + 1, cell_y + 1, cell_z + 1,
                            coarse_y_size, coarse_z_size)], x1_z1),
                    0.125F);

                for (std::int32_t sub_y = 0; sub_y < 8; ++sub_y) {
                    const float x_step_z0 = mul(sub(x1_z0, x0_z0), 0.25F);
                    const float x_step_z1 = mul(sub(x1_z1, x0_z1), 0.25F);
                    float current_z0 = x0_z0;
                    float current_z1 = x0_z1;
                    const std::int32_t local_y = cell_y * 8 + sub_y;

                    for (std::int32_t sub_x = 0; sub_x < 4; ++sub_x) {
                        float density = current_z0;
                        const float z_step = mul(sub(current_z1, current_z0), 0.25F);
                        const std::int32_t local_x = cell_x * 4 + sub_x;

                        for (std::int32_t sub_z = 0; sub_z < 4; ++sub_z) {
                            const std::int32_t local_z = cell_z * 4 + sub_z;
                            std::uint8_t generated = worldgen::v0_6_1::block::air;
                            if (local_y <= 63) {
                                generated = temperatures[map_index(local_x, local_z, 16)] < 0.5F
                                        && local_y == 63
                                    ? worldgen::v0_6_1::block::ice
                                    : worldgen::v0_6_1::block::still_water;
                            }
                            if (density > 0.0F) {
                                generated = worldgen::v0_6_1::block::stone;
                            }
                            chunk.set_block(local_x, local_y, local_z, generated);
                            density = add(density, z_step);
                        }
                        current_z0 = add(current_z0, x_step_z0);
                        current_z1 = add(current_z1, x_step_z1);
                    }

                    x0_z0 = add(x0_z0, x0_z0_y_step);
                    x1_z0 = add(x1_z0, x1_z0_y_step);
                    x0_z1 = add(x0_z1, x0_z1_y_step);
                    x1_z1 = add(x1_z1, x1_z1_y_step);
                }
            }
        }
    }
}

void Pe061TerrainSource::build_surfaces(
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    std::span<const worldgen::v0_6_1::BiomeId> biomes,
    worldgen::Chunk& chunk) {
    const std::int32_t block_x = worldgen::detail::wrapping_mul(chunk_x, 16);
    const std::int32_t block_z = worldgen::detail::wrapping_mul(chunk_z, 16);

    surface_noise_.region(
        sand_field_, static_cast<float>(block_x), static_cast<float>(block_z),
        0.0F, 16, 16, 1, 0.03125F, 0.03125F, 1.0F);
    surface_noise_.region(
        gravel_field_, static_cast<float>(block_x), 109.013397F,
        static_cast<float>(block_z), 16, 1, 16, 0.03125F, 1.0F, 0.03125F);
    surface_depth_noise_.region(
        surface_depth_field_, static_cast<float>(block_x),
        static_cast<float>(block_z), 0.0F, 16, 16, 1,
        0.0625F, 0.0625F, 0.0625F);

    for (std::int32_t local_z = 0; local_z < 16; ++local_z) {
        for (std::int32_t local_x = 0; local_x < 16; ++local_x) {
            const std::size_t column = map_index(local_x, local_z, 16);
            const bool sand = add(
                sand_field_[column], mul(random_.next_float(), 0.200000003F)) > 0.0F;
            const bool gravel = add(
                gravel_field_[column], mul(random_.next_float(), 0.200000003F)) > 3.0F;
            float depth_value = div(surface_depth_field_[column], 3.0F);
            depth_value = add(depth_value, 3.0F);
            depth_value = add(depth_value, mul(random_.next_float(), 0.25F));
            const std::int32_t surface_depth =
                worldgen::detail::fp::trunc_to_i32(depth_value);

            const auto& column_biome = worldgen::v0_6_1::biome(biomes[column]);
            std::uint8_t top = column_biome.top;
            std::uint8_t filler = column_biome.filler;
            std::int32_t remaining = -1;

            for (std::int32_t block_y = 127; block_y >= 0; --block_y) {
                const std::uint32_t bedrock_depth = random_.next_bounded(5U);
                if (block_y <= static_cast<std::int32_t>(bedrock_depth)) {
                    chunk.set_block(
                        local_x, block_y, local_z,
                        worldgen::v0_6_1::block::bedrock);
                    continue;
                }

                const std::uint8_t current = chunk.block(local_x, block_y, local_z);
                if (current == worldgen::v0_6_1::block::air) {
                    remaining = -1;
                    continue;
                }
                if (current != worldgen::v0_6_1::block::stone) {
                    continue;
                }

                if (remaining == -1) {
                    if (surface_depth <= 0) {
                        top = worldgen::v0_6_1::block::air;
                        filler = worldgen::v0_6_1::block::stone;
                    } else if (block_y >= 60 && block_y <= 65) {
                        top = column_biome.top;
                        filler = column_biome.filler;
                        if (gravel) {
                            top = worldgen::v0_6_1::block::air;
                            filler = worldgen::v0_6_1::block::gravel;
                        }
                        if (sand) {
                            top = worldgen::v0_6_1::block::sand;
                            filler = worldgen::v0_6_1::block::sand;
                        }
                    }

                    if (block_y <= 63 && top == worldgen::v0_6_1::block::air) {
                        top = worldgen::v0_6_1::block::still_water;
                    }
                    chunk.set_block(
                        local_x, block_y, local_z,
                        block_y > 62 ? top : filler);
                    remaining = surface_depth;
                    continue;
                }

                if (remaining <= 0) {
                    continue;
                }
                --remaining;
                chunk.set_block(local_x, block_y, local_z, filler);
                if (remaining == 0 && filler == worldgen::v0_6_1::block::sand) {
                    filler = worldgen::v0_6_1::block::sand_stone;
                    remaining = static_cast<std::int32_t>(random_.next_u32() & 3U);
                }
            }
        }
    }
}

worldgen::Chunk Pe061TerrainSource::generate_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    const std::uint32_t chunk_seed =
        worldgen::detail::bits(chunk_x) * 341'872'712U
        + worldgen::detail::bits(chunk_z) * 132'899'541U;
    random_.seed(chunk_seed);

    const std::int32_t block_x = worldgen::detail::wrapping_mul(chunk_x, 16);
    const std::int32_t block_z = worldgen::detail::wrapping_mul(chunk_z, 16);
    const auto biomes = biome_source_.biomes(block_x, block_z, 16, 16);
    const auto temperatures = biome_source_.temperature_map();

    worldgen::Chunk chunk{};
    chunk.x = chunk_x;
    chunk.z = chunk_z;
    prepare_heights(chunk_x, chunk_z, temperatures, chunk);
    build_surfaces(chunk_x, chunk_z, biomes, chunk);
    return chunk;
}

std::array<std::uint8_t, worldgen::Chunk::height>
Pe061TerrainSource::generate_first_column(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    const std::uint32_t chunk_seed =
        worldgen::detail::bits(chunk_x) * 341'872'712U
        + worldgen::detail::bits(chunk_z) * 132'899'541U;
    random_.seed(chunk_seed);

    const std::int32_t block_x = worldgen::detail::wrapping_mul(chunk_x, 16);
    const std::int32_t block_z = worldgen::detail::wrapping_mul(chunk_z, 16);
    const auto biomes = biome_source_.biomes(block_x, block_z, 16, 16);
    const auto temperatures = biome_source_.temperature_map();
    const auto rainfall = biome_source_.rainfall_map();

    const std::int32_t coarse_x = worldgen::detail::wrapping_mul(chunk_x, 4);
    const std::int32_t coarse_z = worldgen::detail::wrapping_mul(chunk_z, 4);
    depth_noise_.region2d(
        depth_field_, coarse_x, coarse_z, 1, 1,
        1.12100005F, 1.12100005F, 0.5F);
    scale_noise_.region2d(
        scale_field_, coarse_x, coarse_z, 1, 1,
        200.0F, 200.0F, 0.5F);
    selector_noise_.region(
        selector_field_, static_cast<float>(coarse_x), 0.0F,
        static_cast<float>(coarse_z), 1, 17, 1,
        8.55515003F, 4.27757502F, 8.55515003F);
    lower_noise_.region(
        lower_field_, static_cast<float>(coarse_x), 0.0F,
        static_cast<float>(coarse_z), 1, 17, 1,
        684.411987F, 684.411987F, 684.411987F);
    upper_noise_.region(
        upper_field_, static_cast<float>(coarse_x), 0.0F,
        static_cast<float>(coarse_z), 1, 17, 1,
        684.411987F, 684.411987F, 684.411987F);

    // In the full 5-wide density request, lattice (0,0) samples climate
    // cell (1,1): integer 16/5 gives a step of three and a half-step of one.
    constexpr std::size_t climate_index = map_index(1, 1, 16);
    float climate = mul(temperatures[climate_index], rainfall[climate_index]);
    climate = sub(1.0F, climate);
    climate = mul(climate, climate);
    climate = sub(1.0F, mul(climate, climate));

    float depth = add(depth_field_[0], 256.0F);
    depth = mul(depth, 0.001953125F);
    depth = mul(depth, climate);
    if (depth > 1.0F) {
        depth = 1.0F;
    }
    float scale = div(scale_field_[0], 8000.0F);
    if (scale < 0.0F) {
        scale = -mul(scale, 0.300000012F);
    }
    scale = sub(mul(scale, 3.0F), 2.0F);
    if (scale < 0.0F) {
        scale = mul(scale, 0.5F);
        if (scale < -1.0F) {
            scale = -1.0F;
        }
        scale = div(scale, 1.39999998F);
        scale = mul(scale, 0.5F);
        depth = 0.0F;
    } else {
        if (scale > 1.0F) {
            scale = 1.0F;
        }
        scale = mul(scale, 0.125F);
        if (depth < 0.0F) {
            depth = 0.0F;
        }
    }
    scale = mul(scale, 17.0F);
    scale = mul(scale, 0.0625F);
    const float center = mul_add(mul(scale, 4.0F), 17.0F, 0.5F);
    depth = add(depth, 0.5F);

    std::array<float, 17> density_line{};
    for (std::int32_t y = 0; y < 17; ++y) {
        float vertical = sub(static_cast<float>(y), center);
        vertical = mul(vertical, 12.0F);
        vertical = div(vertical, depth);
        if (vertical < 0.0F) {
            vertical = mul(vertical, 4.0F);
        }
        float blend = div(selector_field_[static_cast<std::size_t>(y)], 10.0F);
        blend = add(blend, 1.0F);
        blend = mul(blend, 0.5F);
        float density = mul(lower_field_[static_cast<std::size_t>(y)], 0.001953125F);
        if (blend >= 0.0F) {
            const float upper = mul(
                upper_field_[static_cast<std::size_t>(y)], 0.001953125F);
            if (blend <= 1.0F) {
                density = mul_add(density, sub(upper, density), blend);
            } else {
                density = upper;
            }
        }
        density = sub(density, vertical);
        if (y >= 14) {
            const float fade = div(static_cast<float>(y - 13), 3.0F);
            density = mul_add(
                mul(-10.0F, fade), density, sub(1.0F, fade));
        }
        density_line[static_cast<std::size_t>(y)] = density;
    }

    std::array<std::uint8_t, worldgen::Chunk::height> column{};
    for (std::int32_t cell_y = 0; cell_y < 16; ++cell_y) {
        float density = density_line[static_cast<std::size_t>(cell_y)];
        const float step = mul(
            sub(density_line[static_cast<std::size_t>(cell_y + 1)], density),
            0.125F);
        for (std::int32_t sub_y = 0; sub_y < 8; ++sub_y) {
            const std::int32_t y = cell_y * 8 + sub_y;
            std::uint8_t block = worldgen::v0_6_1::block::air;
            if (y <= 63) {
                block = temperatures[0] < 0.5F && y == 63
                    ? worldgen::v0_6_1::block::ice
                    : worldgen::v0_6_1::block::still_water;
            }
            if (density > 0.0F) {
                block = worldgen::v0_6_1::block::stone;
            }
            column[static_cast<std::size_t>(y)] = block;
            density = add(density, step);
        }
    }

    surface_noise_.region(
        sand_field_, static_cast<float>(block_x), static_cast<float>(block_z),
        0.0F, 1, 16, 1, 0.03125F, 0.03125F, 1.0F);
    surface_noise_.region(
        gravel_field_, static_cast<float>(block_x), 109.013397F,
        static_cast<float>(block_z), 1, 1, 16,
        0.03125F, 1.0F, 0.03125F);
    surface_depth_noise_.region(
        surface_depth_field_, static_cast<float>(block_x),
        static_cast<float>(block_z), 0.0F, 1, 16, 1,
        0.0625F, 0.0625F, 0.0625F);

    const bool sand = add(
        sand_field_[0], mul(random_.next_float(), 0.200000003F)) > 0.0F;
    const bool gravel = add(
        gravel_field_[0], mul(random_.next_float(), 0.200000003F)) > 3.0F;
    float depth_value = div(surface_depth_field_[0], 3.0F);
    depth_value = add(depth_value, 3.0F);
    depth_value = add(depth_value, mul(random_.next_float(), 0.25F));
    const std::int32_t surface_depth =
        worldgen::detail::fp::trunc_to_i32(depth_value);
    const auto& column_biome = worldgen::v0_6_1::biome(biomes[0]);
    std::uint8_t top = column_biome.top;
    std::uint8_t filler = column_biome.filler;
    std::int32_t remaining = -1;
    for (std::int32_t y = 127; y >= 0; --y) {
        const std::uint32_t bedrock_depth = random_.next_bounded(5U);
        if (y <= static_cast<std::int32_t>(bedrock_depth)) {
            column[static_cast<std::size_t>(y)] =
                worldgen::v0_6_1::block::bedrock;
            continue;
        }
        const std::uint8_t current = column[static_cast<std::size_t>(y)];
        if (current == worldgen::v0_6_1::block::air) {
            remaining = -1;
            continue;
        }
        if (current != worldgen::v0_6_1::block::stone) {
            continue;
        }
        if (remaining == -1) {
            if (surface_depth <= 0) {
                top = worldgen::v0_6_1::block::air;
                filler = worldgen::v0_6_1::block::stone;
            } else if (y >= 60 && y <= 65) {
                top = column_biome.top;
                filler = column_biome.filler;
                if (gravel) {
                    top = worldgen::v0_6_1::block::air;
                    filler = worldgen::v0_6_1::block::gravel;
                }
                if (sand) {
                    top = worldgen::v0_6_1::block::sand;
                    filler = worldgen::v0_6_1::block::sand;
                }
            }
            if (y <= 63 && top == worldgen::v0_6_1::block::air) {
                top = worldgen::v0_6_1::block::still_water;
            }
            column[static_cast<std::size_t>(y)] = y > 62 ? top : filler;
            remaining = surface_depth;
            continue;
        }
        if (remaining <= 0) {
            continue;
        }
        --remaining;
        column[static_cast<std::size_t>(y)] = filler;
        if (remaining == 0 && filler == worldgen::v0_6_1::block::sand) {
            filler = worldgen::v0_6_1::block::sand_stone;
            remaining = static_cast<std::int32_t>(random_.next_u32() & 3U);
        }
    }
    return column;
}

} // namespace mcpe::terrain_verifier
