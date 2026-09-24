#pragma once

#include <cstdint>

namespace mcpe::worldgen::v0_9_0 {

class World;

namespace lighting {

// LevelChunk::updateLightsAndHeights, expressed against the small mutable
// world abstraction.  It consumes the target chunk's deferred emitters,
// refreshes its column heights, and runs the target's block/sky light update
// over the same one-chunk-visible population neighborhood.
void update_lights_and_heights(
    World& world,
    std::int32_t chunk_x,
    std::int32_t chunk_z);

} // namespace lighting
} // namespace mcpe::worldgen::v0_9_0
