#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace mcpe::worldgen::v0_9_0 {

inline constexpr std::int32_t kChunkWidth = 16;
inline constexpr std::int32_t kChunkHeight = 128;
inline constexpr std::size_t kChunkBlockCount =
    static_cast<std::size_t>(kChunkWidth) * kChunkWidth * kChunkHeight;

// Only the density-cell interpolation belongs to this shared helper.  Each
// source supplies its own two base materials and liquid level; that avoids
// representing Nether lava as overworld water during an intermediate stage.
struct BaseTerrainMaterials final {
    std::uint8_t solid{};
    std::uint8_t liquid{};
    std::int32_t liquid_level{};
};

// Translates RandomLevelSource::prepareHeights.  density_lattice uses the
// `(coarse_x * 5 + coarse_z) * 17 + coarse_y` layout returned by TerrainNoise.
void interpolate_base_terrain(
    std::span<std::uint8_t> blocks,
    std::span<const float> density_lattice,
    BaseTerrainMaterials materials) noexcept;

// PE 0.9.0's overworld spelling of the generic interpolation.
void interpolate_base_terrain(
    std::span<std::uint8_t> blocks,
    std::span<const float> density_lattice) noexcept;

} // namespace mcpe::worldgen::v0_9_0
