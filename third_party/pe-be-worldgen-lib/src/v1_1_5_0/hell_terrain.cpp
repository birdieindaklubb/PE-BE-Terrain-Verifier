#include "v1_1_5_0/hell_terrain.hpp"

#include "detail/fp.hpp"
#include "detail/wrap.hpp"
#include "v0_6_1/legacy_math.hpp"
#include "v0_9_0/base_terrain.hpp"
#include "v0_9_0/blocks.hpp"
#include "v1_1_5_0/blocks.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mcpe::worldgen::v1_1_5_0 {
namespace {

constexpr std::int32_t kLatticeWidth = 5;
constexpr std::int32_t kLatticeHeight = 17;
constexpr std::int32_t kSeaLevel = 32;
constexpr std::uint8_t kHellBiome = 8;

constexpr float kSelectorScaleX = 8.55515F;
constexpr float kSelectorScaleY = 34.2205963F;
constexpr float kSelectorScaleZ = 8.55515F;
constexpr float kDensityScaleX = 684.412F;
constexpr float kDensityScaleY = 2053.23584F;
constexpr float kDensityScaleZ = 684.412F;

[[nodiscard]] constexpr std::size_t lattice_index(
    std::int32_t x,
    std::int32_t z,
    std::int32_t y) noexcept {
    return (static_cast<std::size_t>(x) * kLatticeWidth
            + static_cast<std::size_t>(z))
        * kLatticeHeight
        + static_cast<std::size_t>(y);
}

[[nodiscard]] constexpr std::size_t surface_index(
    std::int32_t x,
    std::int32_t z) noexcept {
    return static_cast<std::size_t>(x) * Chunk::width
        + static_cast<std::size_t>(z);
}

[[nodiscard]] constexpr std::uint32_t surface_seed(
    std::int32_t chunk_x,
    std::int32_t chunk_z) noexcept {
    // HellRandomLevelSource::loadChunk seeds its ThreadData Random directly
    // from the two signed ChunkPos words. Arithmetic is intentionally modulo
    // 2^32, as in the native ARM multiply/add sequence.
    return detail::bits(chunk_x) * 0x9939'f508U
        + detail::bits(chunk_z) * 0xf156'5bd5U;
}

[[nodiscard]] float vertical_density_offset(std::int32_t y) noexcept {
    const float height = static_cast<float>(y);
    float edge_distance = height;
    if (y > 8) {
        edge_distance = detail::fp::sub(16.0F, height);
    }

    const float angle = detail::fp::mul(
        detail::fp::mul(height, 0.3529412F), 3.1415927F);
    float result = detail::fp::add(
        v0_6_1::legacy_math::cos(angle),
        v0_6_1::legacy_math::cos(angle));
    if (edge_distance < 4.0F) {
        const float edge = detail::fp::sub(4.0F, edge_distance);
        // HellRandomLevelSource::getHeights adds this cubic correction to
        // the cosine pair.  It does not scale the pair by the correction;
        // that superficially similar expression moves density thresholds
        // throughout the entire chunk.
        const float squared = detail::fp::mul(edge, edge);
        const float correction = detail::fp::mul(
            detail::fp::mul(squared, -10.0F), edge);
        result = detail::fp::add(result, correction);
    }
    return result;
}

[[nodiscard]] std::uint8_t block_at(
    const Chunk& chunk,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    return chunk.block(x, y, z);
}

} // namespace

HellTerrain::HellTerrain(std::uint32_t world_seed)
    : random_(world_seed),
      density_a_(random_, 16),
      density_b_(random_, 16),
      density_selector_(random_, 8),
      surface_noise_(random_, 4),
      surface_depth_noise_(random_, 4),
      scale_noise_(random_, 10),
      depth_noise_(random_, 16) {
}

void HellTerrain::density_lattice(
    std::vector<float>& output,
    std::int32_t coarse_x,
    std::int32_t coarse_z) const {
    std::vector<float> scale;
    std::vector<float> depth;
    std::vector<float> selector;
    std::vector<float> density_a;
    std::vector<float> density_b;

    // These calls, including the two dimensional shapes, follow
    // HellRandomLevelSource::getHeights.  The first two sources are retained
    // even though this PE build's density equation does not subsequently use
    // their samples; constructing and sampling them is native observable work.
    scale_noise_.region(
        scale,
        static_cast<float>(coarse_x),
        0.0F,
        static_cast<float>(coarse_z),
        kLatticeWidth,
        1,
        kLatticeWidth,
        1.0F,
        0.0F,
        1.0F);
    depth_noise_.region(
        depth,
        static_cast<float>(coarse_x),
        0.0F,
        static_cast<float>(coarse_z),
        kLatticeWidth,
        1,
        kLatticeWidth,
        100.0F,
        0.0F,
        100.0F);
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
    density_a_.region(
        density_a,
        static_cast<float>(coarse_x),
        0.0F,
        static_cast<float>(coarse_z),
        kLatticeWidth,
        kLatticeHeight,
        kLatticeWidth,
        kDensityScaleX,
        kDensityScaleY,
        kDensityScaleZ);
    density_b_.region(
        density_b,
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
            const std::size_t column = lattice_index(lattice_x, lattice_z, 0);
            for (std::int32_t lattice_y = 0;
                 lattice_y < kLatticeHeight;
                 ++lattice_y) {
                const std::size_t index = column
                    + static_cast<std::size_t>(lattice_y);
                const float blend = detail::fp::mul_add(
                    0.5F, selector[index], 0.05F);
                // getHeights starts with its first 16-octave field and
                // interpolates toward the second one.  The two calls are
                // adjacent in the decompilation, but their stack buffers are
                // assigned through separate pointers in the inner loop; do
                // not reverse this selector direction.
                float density = detail::fp::mul(density_a[index], 0.001953125F);
                if (blend >= 0.0F) {
                    // The native VFP condition is <=, not <. At exactly one
                    // it still takes the explicit interpolation path, whose
                    // intermediate rounding is observable at a threshold.
                    if (blend <= 1.0F) {
                        const float difference = detail::fp::mul(
                            detail::fp::sub(density_b[index], density_a[index]),
                            0.001953125F);
                        density = detail::fp::mul_add(density, difference, blend);
                    } else {
                        density = detail::fp::mul(
                            density_b[index], 0.001953125F);
                    }
                }
                density = detail::fp::sub(
                    density, vertical_density_offset(lattice_y));
                if (lattice_y > 13) {
                    const float edge = static_cast<float>(lattice_y - 13);
                    const float retained = detail::fp::mul_add(
                        1.0F, edge, -0.33333334F);
                    // The APK multiplies -3.3333335 by the raw distance
                    // above lattice Y=13, not by the already divided fade.
                    // In source form this is the familiar -10*fade term,
                    // but retaining the native operands preserves its two
                    // binary32 rounding boundaries.
                    density = detail::fp::mul_add(
                        detail::fp::mul(density, retained),
                        edge,
                        -3.3333335F);
                }
                output[index] = density;
            }
        }
    }
}

void HellTerrain::build_surfaces(Chunk& chunk) const {
    const std::int32_t block_x = detail::wrapping_mul(chunk.x, Chunk::width);
    const std::int32_t block_z = detail::wrapping_mul(chunk.z, Chunk::width);
    std::vector<float> soul_sand_field;
    std::vector<float> gravel_field;
    std::vector<float> depth_field;

    // The target routes the second chunk coordinate through the Y argument
    // for both fields.  The second call changes the output shape (16x1x16),
    // not the coordinate routing.  Keep that nonstandard native call shape
    // rather than replacing it with a later Java generator.
    surface_noise_.region(
        soul_sand_field,
        static_cast<float>(block_x),
        static_cast<float>(block_z),
        0.0F,
        Chunk::width,
        Chunk::width,
        1,
        1.0F,
        1.0F,
        1.0F);
    surface_noise_.region(
        gravel_field,
        static_cast<float>(block_x),
        static_cast<float>(block_z),
        0.0F,
        Chunk::width,
        1,
        Chunk::width,
        1.0F,
        1.0F,
        1.0F);
    surface_depth_noise_.region(
        depth_field,
        static_cast<float>(block_x),
        static_cast<float>(block_z),
        0.0F,
        Chunk::width,
        Chunk::width,
        1,
        2.0F,
        2.0F,
        2.0F);

    detail::Mt19937 random(surface_seed(chunk.x, chunk.z));
    for (std::int32_t x = 0; x < Chunk::width; ++x) {
        for (std::int32_t z = 0; z < Chunk::width; ++z) {
            const std::size_t index = surface_index(x, z);
            const bool soul_sand = detail::fp::add(
                soul_sand_field[index],
                detail::fp::mul(random.next_float(), 0.2F)) > 0.0F;
            const bool gravel = detail::fp::add(
                gravel_field[index],
                detail::fp::mul(random.next_float(), 0.2F)) > 0.0F;
            const std::int32_t depth = detail::fp::trunc_to_i32(
                detail::fp::add(
                    detail::fp::add(
                        detail::fp::mul(depth_field[index], 0.33333334F),
                        3.0F),
                    detail::fp::mul(random.next_float(), 0.25F)));

            std::int32_t remaining = -1;
            std::uint8_t top = block::netherrack;
            std::uint8_t filler = block::netherrack;
            for (std::int32_t y = Chunk::height - 1; y >= 0; --y) {
                const std::uint32_t top_bedrock_word = random.next_u32();
                const std::uint32_t bottom_bedrock_word = random.next_u32();
                if (y >= Chunk::height - 1
                        - static_cast<std::int32_t>(top_bedrock_word % 5U)
                    || y <= static_cast<std::int32_t>(bottom_bedrock_word % 5U)) {
                    chunk.set_block(x, y, z, block::bedrock);
                    continue;
                }

                const std::uint8_t current = block_at(chunk, x, y, z);
                if (current == block::air) {
                    remaining = -1;
                    continue;
                }
                if (current != block::netherrack) {
                    continue;
                }
                if (remaining == -1) {
                    top = block::netherrack;
                    filler = block::netherrack;
                    if (depth <= 0) {
                        top = block::air;
                    } else if (y >= Chunk::height - 68
                        && y <= Chunk::height - 63) {
                        if (gravel) {
                            // PE's Hell source applies gravel as the exposed
                            // surface material while its underlying filler
                            // remains netherrack. This is intentionally not
                            // the more intuitive later-generator ordering.
                            top = block::gravel;
                        }
                        if (soul_sand) {
                            top = block::soul_sand;
                            filler = block::soul_sand;
                        }
                    }
                    if (y < Chunk::height - 64 && top == block::air) {
                        top = block::still_lava;
                    }
                    remaining = depth;
                    chunk.set_block(
                        x,
                        y,
                        z,
                        y >= Chunk::height - 65 ? top : filler);
                } else if (remaining > 0) {
                    --remaining;
                    chunk.set_block(x, y, z, filler);
                }
            }
        }
    }
}

Chunk HellTerrain::generate_base_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) const {
    std::vector<float> density;
    density_lattice(
        density,
        detail::wrapping_mul(chunk_x, 4),
        detail::wrapping_mul(chunk_z, 4));

    Chunk chunk{};
    chunk.x = chunk_x;
    chunk.z = chunk_z;
    chunk.biomes.fill(kHellBiome);
    chunk.temperatures.fill(2.0F);
    chunk.rainfall.fill(0.0F);

    // HellRandomLevelSource::prepareHeights uses the same density-cell walk
    // as PE 0.9.0, but writes Hell materials directly: netherrack when
    // positive, still lava below Y=32 otherwise, and air above it.
    v0_9_0::interpolate_base_terrain(
        chunk.blocks,
        density,
        {block::netherrack, block::still_lava, kSeaLevel});
    build_surfaces(chunk);
    recalculate_heightmap(chunk);
    return chunk;
}

void HellTerrain::recalculate_heightmap(Chunk& chunk) noexcept {
    std::int32_t minimum_height = Chunk::height - 1;
    for (std::int32_t x = 0; x < Chunk::width; ++x) {
        for (std::int32_t z = 0; z < Chunk::width; ++z) {
            std::int32_t height = Chunk::height - 1;
            while (height != 0 && chunk.block(x, height - 1, z) == block::air) {
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
