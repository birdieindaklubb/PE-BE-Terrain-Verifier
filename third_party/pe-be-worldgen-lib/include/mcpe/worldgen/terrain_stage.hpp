#pragma once

#include <cstdint>

namespace mcpe::worldgen {

// Exact intermediate block states from PE 0.9.0
// RandomLevelSource::loadChunk.  They are ordered by the native calls, not by
// a conceptual rendering pipeline: caves run after biome surface replacement.
enum class Pe090TerrainStage : std::uint8_t {
    // Immediately after prepareHeights: rock, calm water, and air only.
    raw_density,
    // Immediately after buildSurfaces: biome top/filler blocks and bedrock.
    surfaced,
    // Immediately after LargeCaveFeature::apply, before recalcHeightmap.
    cave_carved,
};

} // namespace mcpe::worldgen
