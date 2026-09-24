#include "v0_9_0/terrain_noise.hpp"

#include "detail/fp.hpp"
#include "v0_9_0/biome_height.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>

namespace mcpe::worldgen::v0_9_0 {
namespace {

constexpr std::int32_t kLatticeWidth = 5;
constexpr std::int32_t kLatticeHeight = 17;
constexpr std::int32_t kBiomeNeighborhoodWidth = 10;

// RandomLevelSource constructs the usual five-by-five parabolic field, but
// its PE 0.9 getHeights loop observes only the central three-by-three values.
// Keep the exact binary32 constants used by those nine accesses.
constexpr float kBiomeCornerWeight = std::bit_cast<float>(0x40d7be73U);
constexpr float kBiomeEdgeWeight = std::bit_cast<float>(0x41120f31U);
constexpr float kBiomeCenterWeight = std::bit_cast<float>(0x41b2e2acU);
constexpr std::array<float, 9> kBiomeWeights{{
    kBiomeCornerWeight, kBiomeEdgeWeight, kBiomeCornerWeight,
    kBiomeEdgeWeight, kBiomeCenterWeight, kBiomeEdgeWeight,
    kBiomeCornerWeight, kBiomeEdgeWeight, kBiomeCornerWeight,
}};

constexpr float kSelectorScaleX = 8.55515F;
constexpr float kSelectorScaleY = 4.277575F;
constexpr float kSelectorScaleZ = 8.55515F;
constexpr float kDensityScale = 684.412F;
constexpr float kLowerDensityScaleY = 855.515F;

[[nodiscard]] constexpr std::size_t lattice_index(
    std::int32_t x,
    std::int32_t z,
    std::int32_t y) noexcept {
    return (static_cast<std::size_t>(x) * kLatticeWidth
            + static_cast<std::size_t>(z))
        * kLatticeHeight
        + static_cast<std::size_t>(y);
}

[[nodiscard]] constexpr std::size_t biome_index(
    std::int32_t x,
    std::int32_t z) noexcept {
    return static_cast<std::size_t>(z) * kBiomeNeighborhoodWidth
        + static_cast<std::size_t>(x);
}

} // namespace

TerrainNoise::TerrainNoise(std::uint32_t world_seed)
    : random_(world_seed),
      density_lower_(random_, 16),
      density_upper_(random_, 16),
      density_selector_(random_, 8),
      surface_noise_(random_, 4),
      unused_10_octave_noise_(random_, 10),
      terrain_modifier_(random_, 16),
      unused_8_octave_noise_(random_, 8) {
}

std::span<float> TerrainNoise::density_lattice(
    std::vector<float>& storage,
    std::span<const std::int32_t> raw_biomes,
    std::int32_t x,
    std::int32_t z) const {
    assert(raw_biomes.size()
        >= static_cast<std::size_t>(kBiomeNeighborhoodWidth)
            * kBiomeNeighborhoodWidth);

    std::vector<float> terrain_modifier;
    std::vector<float> selector;
    std::vector<float> lower;
    std::vector<float> upper;

    // The first request is the APK's 2-D overload.  Its final 0.5 argument is
    // part of the ARM ABI, but the target's overload does not read it.
    terrain_modifier_.region2d(
        terrain_modifier,
        x,
        z,
        kLatticeWidth,
        kLatticeWidth,
        200.0F,
        200.0F,
        0.5F);
    density_selector_.region(
        selector,
        static_cast<float>(x),
        0.0F,
        static_cast<float>(z),
        kLatticeWidth,
        kLatticeHeight,
        kLatticeWidth,
        kSelectorScaleX,
        kSelectorScaleY,
        kSelectorScaleZ);
    density_lower_.region(
        lower,
        static_cast<float>(x),
        0.0F,
        static_cast<float>(z),
        kLatticeWidth,
        kLatticeHeight,
        kLatticeWidth,
        kDensityScale,
        kLowerDensityScaleY,
        kDensityScale);
    density_upper_.region(
        upper,
        static_cast<float>(x),
        0.0F,
        static_cast<float>(z),
        kLatticeWidth,
        kLatticeHeight,
        kLatticeWidth,
        kDensityScale,
        kDensityScale,
        kDensityScale);

    storage.resize(static_cast<std::size_t>(kLatticeWidth)
        * kLatticeWidth * kLatticeHeight);

    for (std::int32_t lattice_x = 0; lattice_x < kLatticeWidth; ++lattice_x) {
        for (std::int32_t lattice_z = 0; lattice_z < kLatticeWidth; ++lattice_z) {
            const BiomeHeight center = biome_height::for_biome(
                raw_biomes[biome_index(lattice_x + 2, lattice_z + 2)]);

            float weighted_depth = 0.0F;
            float weighted_scale = 0.0F;
            float total_weight = 0.0F;
            for (std::int32_t neighbor_z = -1; neighbor_z <= 1; ++neighbor_z) {
                for (std::int32_t neighbor_x = -1; neighbor_x <= 1; ++neighbor_x) {
                    const BiomeHeight neighbor = biome_height::for_biome(raw_biomes[
                        biome_index(lattice_x + neighbor_x + 2,
                            lattice_z + neighbor_z + 2)]);
                    const float parabolic_weight = kBiomeWeights[
                        static_cast<std::size_t>(neighbor_z + 1) * 3U
                        + static_cast<std::size_t>(neighbor_x + 1)];
                    float weight = detail::fp::div(
                        parabolic_weight,
                        detail::fp::add(neighbor.depth, 2.0F));
                    if (neighbor.depth > center.depth) {
                        weight = detail::fp::mul(weight, 0.5F);
                    }
                    weighted_depth = detail::fp::mul_add(
                        weighted_depth, neighbor.depth, weight);
                    weighted_scale = detail::fp::mul_add(
                        weighted_scale, neighbor.scale, weight);
                    total_weight = detail::fp::add(total_weight, weight);
                }
            }
            const float average_depth = detail::fp::div(weighted_depth, total_weight);
            const float average_scale = detail::fp::div(weighted_scale, total_weight);

            const std::size_t base = lattice_index(lattice_x, lattice_z, 0);
            float modifier = detail::fp::mul(terrain_modifier[
                static_cast<std::size_t>(lattice_x) * kLatticeWidth + lattice_z], 0.000125F);
            if (modifier < 0.0F) {
                modifier = -detail::fp::mul(modifier, 0.3F);
            }
            modifier = detail::fp::sub(detail::fp::mul(modifier, 3.0F), 2.0F);
            if (modifier < 0.0F) {
                modifier = detail::fp::mul(modifier, 0.5F);
                if (modifier < -1.0F) {
                    modifier = -1.0F;
                }
                modifier = detail::fp::mul(modifier, 0.35714287F);
            } else {
                if (modifier > 1.0F) {
                    modifier = 1.0F;
                }
                modifier = detail::fp::mul(modifier, 0.125F);
            }

            // fp::mul_add is accumulator + a*b, matching the VMLA schedule.
            // Native: (averageDepth * 4.0f - 1.0f) * 0.125f.
            float height_offset = detail::fp::mul_add(
                -1.0F, average_depth, 4.0F);
            height_offset = detail::fp::mul(height_offset, 0.125F);
            height_offset = detail::fp::mul_add(height_offset, modifier, 0.2F);
            const float vertical_scale = detail::fp::div(
                12.0F, detail::fp::mul_add(0.1F, average_scale, 0.9F));
            const float vertical_origin = detail::fp::sub(
                -8.5F, detail::fp::mul(height_offset, 4.25F));

            for (std::int32_t lattice_y = 0; lattice_y < kLatticeHeight; ++lattice_y) {
                const std::size_t index = base + static_cast<std::size_t>(lattice_y);
                float vertical = detail::fp::mul(
                    detail::fp::add(static_cast<float>(lattice_y), vertical_origin),
                    vertical_scale);
                if (vertical < 0.0F) {
                    vertical = detail::fp::mul(vertical, 4.0F);
                }

                float blend = detail::fp::mul_add(1.0F, selector[index], 0.1F);
                blend = detail::fp::mul(blend, 0.5F);
                const float lower_density = detail::fp::mul(lower[index], 0.00390625F);
                const float upper_density = detail::fp::mul(upper[index], 0.001953125F);

                float density = lower_density;
                if (blend >= 0.0F) {
                    if (blend > 1.0F) {
                        density = upper_density;
                    } else {
                        density = detail::fp::mul_add(
                            lower_density,
                            detail::fp::sub(upper_density, lower_density),
                            blend);
                    }
                }
                density = detail::fp::sub(density, vertical);

                if (lattice_y > 13) {
                    const float fade = detail::fp::mul(
                        static_cast<float>(lattice_y - 13), 0.33333334F);
                    density = detail::fp::mul_add(
                        detail::fp::mul(fade, -10.0F),
                        density,
                        detail::fp::sub(1.0F, fade));
                }
                storage[index] = density;
            }
        }
    }

    return storage;
}

std::span<float> TerrainNoise::surface_field(
    std::vector<float>& storage,
    std::int32_t block_x,
    std::int32_t block_z) const {
    return surface_noise_.region2d(
        storage,
        block_x,
        block_z,
        16,
        16,
        0.0625F,
        0.0625F,
        1.0F);
}

} // namespace mcpe::worldgen::v0_9_0
