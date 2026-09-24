#pragma once

#include "detail/mt19937.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace mcpe::worldgen::v0_9_0 {

// LevelChunk keeps light-emitting direct writes in a small queue until its
// later updateLightsAndHeights pass.  LargeCaveFeature adds every lava block
// it carves below y=10 to that queue.
struct DeferredLightEmitter final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
};

// The LargeCaveFeature pass invoked by RandomLevelSource::loadChunk after
// surfaces and before the heightmap is recalculated.  Its seed is the native
// 32-bit level seed held by BiomeSource.
class CaveCarver final {
public:
    void carve(
        std::uint32_t world_seed,
        std::span<std::uint8_t> blocks,
        std::int32_t chunk_x,
        std::int32_t chunk_z,
        detail::Mt19937* post_carve_random = nullptr,
        std::vector<DeferredLightEmitter>* deferred_light_emitters = nullptr)
        const noexcept;
};

} // namespace mcpe::worldgen::v0_9_0
