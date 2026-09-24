#pragma once

#include "mcpe/worldgen/chunk.hpp"

#include <cstdint>

namespace mcpe::worldgen::v1_1_5_0 {

// LargeHellCaveFeature, invoked by HellRandomLevelSource::loadChunk after
// density and surface construction. It owns an independent native RNG for
// every source chunk in the 17 by 17 influence square.
class HellCaveCarver final {
public:
    void carve(std::uint32_t world_seed, Chunk& chunk) const noexcept;
};

} // namespace mcpe::worldgen::v1_1_5_0
