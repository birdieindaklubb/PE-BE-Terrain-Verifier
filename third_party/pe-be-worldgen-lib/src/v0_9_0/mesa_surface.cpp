#include "v0_9_0/mesa_surface.hpp"

#include "detail/fp.hpp"
#include "detail/mt19937.hpp"
#include "v0_6_1/legacy_math.hpp"
#include "v0_9_0/base_terrain.hpp"
#include "v0_9_0/blocks.hpp"

#include <cassert>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace mcpe::worldgen::v0_9_0 {
namespace {

constexpr float kBandScale = 0.001953125F;
constexpr float kPillarScale = 0.25F;
constexpr float kPi = std::bit_cast<float>(0x40490fdbU);

[[nodiscard]] constexpr std::size_t block_index(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    return (static_cast<std::size_t>(x) << 11U)
        | (static_cast<std::size_t>(z) << 7U)
        | static_cast<std::size_t>(y);
}

void set_data(
    std::span<std::uint8_t> data,
    std::size_t index,
    std::uint8_t value) noexcept {
    std::uint8_t& packed = data[index >> 1U];
    if ((index & 1U) == 0U) {
        packed = static_cast<std::uint8_t>((packed & 0xf0U) | (value & 0x0fU));
    } else {
        packed = static_cast<std::uint8_t>((packed & 0x0fU) | ((value & 0x0fU) << 4U));
    }
}

void set_block(
    std::span<std::uint8_t> blocks,
    std::size_t index,
    std::uint8_t tile) noexcept {
    blocks[index] = tile;
}

void set_block_data(
    std::span<std::uint8_t> blocks,
    std::span<std::uint8_t> data,
    std::size_t index,
    std::uint8_t tile,
    std::uint8_t metadata) noexcept {
    set_block(blocks, index, tile);
    set_data(data, index, metadata);
}

[[nodiscard]] std::int32_t wrapping_add(
    std::int32_t left,
    std::int32_t right) noexcept {
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}

// This mirrors the ARM signed remainder sequence emitted for getBand.  The
// inputs produced by a chunk's y range remain non-negative, but retaining the
// target's signed operation prevents a quiet behaviour change at far coords.
[[nodiscard]] std::int32_t remainder_64(std::int32_t value) noexcept {
    return value % 64;
}

} // namespace

MesaSurface::SeedState MesaSurface::make_seed_state(std::uint32_t world_seed) {
    // MesaBiome::generateBands first creates the offset-noise octave from a
    // freshly seeded native MT19937, then consumes that same stream for bands.
    detail::Mt19937 band_random(world_seed);
    detail::PerlinSimplexNoise band_offset(band_random, 1);

    std::array<std::uint8_t, 64> bands{};
    bands.fill(16U);

    for (std::int32_t index = 0; index < 64;) {
        index += static_cast<std::int32_t>(band_random.next_bounded(5U)) + 1;
        if (index < 64) {
            bands[static_cast<std::size_t>(index)] = 1U;
        }
        ++index;
    }

    const std::uint32_t yellow_runs = band_random.next_bounded(4U) + 2U;
    for (std::uint32_t run = 0; run < yellow_runs; ++run) {
        const std::uint32_t length = band_random.next_bounded(3U) + 1U;
        const std::uint32_t start = band_random.next_u32() & 63U;
        for (std::uint32_t offset = 0;
             offset < length && start + offset < 64U;
             ++offset) {
            bands[start + offset] = 4U;
        }
    }

    const std::uint32_t brown_runs = band_random.next_bounded(4U) + 2U;
    for (std::uint32_t run = 0; run < brown_runs; ++run) {
        const std::uint32_t length = band_random.next_bounded(3U) + 2U;
        const std::uint32_t start = band_random.next_u32() & 63U;
        for (std::uint32_t offset = 0;
             offset < length && start + offset < 64U;
             ++offset) {
            bands[start + offset] = 12U;
        }
    }

    const std::uint32_t red_runs = band_random.next_bounded(4U) + 2U;
    for (std::uint32_t run = 0; run < red_runs; ++run) {
        const std::uint32_t length = band_random.next_bounded(3U) + 1U;
        const std::uint32_t start = band_random.next_u32() & 63U;
        for (std::uint32_t offset = 0;
             offset < length && start + offset < 64U;
             ++offset) {
            bands[start + offset] = 14U;
        }
    }

    const std::uint32_t white_runs = band_random.next_bounded(3U) + 3U;
    std::int32_t white_index = 0;
    for (std::uint32_t run = 0; run < white_runs; ++run) {
        white_index += static_cast<std::int32_t>(band_random.next_u32() & 15U) + 4;
        if (white_index >= 64) {
            continue;
        }

        bands[static_cast<std::size_t>(white_index)] = 0U;
        if ((band_random.next_u32() & 0x0800'0000U) != 0U) {
            bands[static_cast<std::size_t>(white_index - 1)] = 8U;
        }
        if (white_index != 63
            && (band_random.next_u32() & 0x0800'0000U) != 0U) {
            bands[static_cast<std::size_t>(white_index + 1)] = 8U;
        }
    }

    // refreshBiome reseeds separately for both Bryce fields.  Constructing
    // four octaves before one octave is part of the observable RNG stream.
    detail::Mt19937 pillar_random(world_seed);
    detail::PerlinSimplexNoise pillar(pillar_random, 4);
    detail::PerlinSimplexNoise pillar_roof(pillar_random, 1);

    return SeedState{
        std::move(bands),
        std::move(band_offset),
        std::move(pillar),
        std::move(pillar_roof),
    };
}

MesaSurface::MesaSurface(std::uint32_t world_seed)
    : state_(make_seed_state(world_seed)) {}

std::uint8_t MesaSurface::band(
    std::int32_t world_x,
    std::int32_t y,
    std::int32_t world_z) const noexcept {
    const float scaled_x = detail::fp::mul(
        static_cast<float>(world_x), kBandScale);
    const float scaled_z = detail::fp::mul(
        static_cast<float>(world_z), kBandScale);
    const float noise = state_.band_offset.value(scaled_x, scaled_z);
    const double rounded = std::nearbyint(static_cast<double>(detail::fp::add(noise, noise)));
    const auto offset = static_cast<std::int32_t>(rounded);
    const std::int32_t index = remainder_64(wrapping_add(
        wrapping_add(y, offset), 64));
    return state_.bands[static_cast<std::size_t>(index)];
}

std::int32_t MesaSurface::pillar_height(
    std::int32_t world_x,
    std::int32_t world_z,
    float surface_noise) const noexcept {
    const float x = detail::fp::mul(static_cast<float>(world_x), kPillarScale);
    const float z = detail::fp::mul(static_cast<float>(world_z), kPillarScale);

    // This unusual clamp (against the supplied surface field rather than a
    // second absolute noise field) is the PE 0.9.0 ARM implementation.
    float height = state_.pillar.value(x, z);
    const float surface_limit = std::fabs(surface_noise);
    if (height > surface_limit) {
        height = surface_limit;
    }
    if (height <= 0.0F) {
        return 0;
    }

    const float roof_x = detail::fp::mul(
        static_cast<float>(world_x), kBandScale);
    const float roof_z = detail::fp::mul(
        static_cast<float>(world_z), kBandScale);
    const float roof_noise = state_.pillar_roof.value(roof_x, roof_z);

    float squared = detail::fp::mul(height, height);
    squared = detail::fp::mul(squared, 2.5F);
    float roof = std::ceil(detail::fp::mul(std::fabs(roof_noise), 50.0F));
    roof = detail::fp::add(roof, 14.0F);
    if (roof > squared) {
        roof = squared;
    }
    return detail::fp::trunc_to_i32(detail::fp::add(roof, 64.0F));
}

void MesaSurface::apply(
    detail::Mt19937& random,
    std::span<std::uint8_t> blocks,
    std::span<std::uint8_t> data,
    std::int32_t x,
    std::int32_t z,
    std::int32_t world_x,
    std::int32_t world_z,
    float surface_noise,
    bool bryce_pillars,
    bool forest_variant) const noexcept {
    assert(blocks.size() >= kChunkBlockCount);
    assert(data.size() >= kChunkBlockCount / 2U);
    assert(x >= 0 && x < kChunkWidth && z >= 0 && z < kChunkWidth);

    const std::int32_t pillar_top = bryce_pillars
        ? pillar_height(world_x, world_z, surface_noise)
        : 0;

    float thickness_value = detail::fp::mul_add(
        3.0F, surface_noise, 0.33333334F);
    thickness_value = detail::fp::mul_add(
        thickness_value, random.next_float(), 0.25F);
    const std::int32_t thickness = detail::fp::trunc_to_i32(thickness_value);

    const float cosine_argument = detail::fp::mul(surface_noise, kPi);
    // The APK calls bionic cosf here. Host libm implementations are allowed
    // to differ in the last bit, which can flip the strict 0.5 threshold.
    const bool use_colored_bands =
        v0_6_1::legacy_math::bionic_cos(cosine_argument) > 0.5F;

    std::int32_t remaining = -1;
    bool use_white_stained_filler = false;
    std::uint8_t filler = block::hardened_clay;

    for (std::int32_t y = kChunkHeight - 1; y >= 0; --y) {
        const std::size_t index = block_index(x, y, z);

        if (random.next_bounded(5U) >= static_cast<std::uint32_t>(y)) {
            set_block(blocks, index, block::unbreakable);
            continue;
        }

        const std::uint8_t current = blocks[index];
        if (current == block::air) {
            // The native loop deliberately treats a just-created pillar block
            // as air until the next iteration, so it is never surfaced here.
            if (y < pillar_top) {
                set_block(blocks, index, block::rock);
            }
            remaining = -1;
            continue;
        }
        if (current != block::rock) {
            continue;
        }

        if (remaining != -1) {
            if (remaining > 0) {
                --remaining;
                if (use_white_stained_filler) {
                    set_block_data(blocks, data, index, block::stained_clay, 1U);
                } else {
                    const std::uint8_t color = band(world_x, y, world_z);
                    if (color < 16U) {
                        set_block_data(blocks, data, index, block::stained_clay, color);
                    } else {
                        set_block(blocks, index, block::hardened_clay);
                    }
                }
            }
            continue;
        }

        remaining = thickness;
        filler = block::hardened_clay;
        if (thickness < 1) {
            filler = block::rock;
        } else if (y >= 59 && y <= 64) {
            filler = block::hardened_clay;
        }

        if (y >= 63) {
            remaining = wrapping_add(remaining, y - 63);
        }

        if (y < 62) {
            set_block(blocks, index, filler);
            if (filler == block::stained_clay) {
                set_data(data, index, 1U);
            }
            use_white_stained_filler = false;
            continue;
        }

        if (!forest_variant || y <= (thickness + 43) * 2) {
            if (y <= thickness + 66) {
                use_white_stained_filler = true;
                set_block_data(blocks, data, index, block::sand, 1U);
                continue;
            }

            if (y >= 64 && y <= 127) {
                use_white_stained_filler = false;
                if (use_colored_bands) {
                    const std::uint8_t color = band(world_x, y, world_z);
                    if (color < 16U) {
                        set_block_data(blocks, data, index, block::stained_clay, color);
                    } else {
                        set_block(blocks, index, block::hardened_clay);
                    }
                } else {
                    set_block(blocks, index, block::hardened_clay);
                }
                continue;
            }

            use_white_stained_filler = false;
            set_block(blocks, index, block::hardened_clay);
            continue;
        }

        use_white_stained_filler = false;
        if (use_colored_bands) {
            set_block_data(blocks, data, index, block::dirt, 1U);
        } else {
            set_block_data(blocks, data, index, block::grass, 0U);
        }
    }
}

} // namespace mcpe::worldgen::v0_9_0
