#pragma once

#include <cstdint>
#include <span>

namespace mcpe::worldgen::detail {
class Mt19937;
}

namespace mcpe::worldgen::v0_9_0 {

// The fields held at Biome offsets 0x38..0x3a, plus the virtual temperature
// query used when an ordinary surface needs ocean water or ice.
struct SurfaceProfile final {
    std::uint8_t top_block;
    std::uint8_t top_data;
    std::uint8_t filler_block;
    float temperature;
};

// Biome::buildSurfaceAtDefault for one local chunk column.  blocks has the
// LevelChunk x * 2048 + z * 128 + y order; data is its packed nibble array.
void apply_default_surface_column(
    detail::Mt19937& random,
    std::span<std::uint8_t> blocks,
    std::span<std::uint8_t> data,
    std::int32_t x,
    std::int32_t z,
    float surface_noise,
    const SurfaceProfile& profile) noexcept;

} // namespace mcpe::worldgen::v0_9_0
