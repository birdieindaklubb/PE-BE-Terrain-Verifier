#pragma once

#include "v0_9_0/surface.hpp"

#include <cstdint>

namespace mcpe::worldgen::v0_9_0 {

// This is the generation-relevant subset of Biome::initBiomes.  The native
// object also owns colours, mobs and decoration settings; those are recovered
// with the population pass rather than being duplicated here.
enum class SurfaceKind : std::uint8_t {
    default_surface,
    swamp,
    extreme_hills,
    taiga,
    mesa,
    mutated_savanna,
};

struct BiomeProperties final {
    SurfaceProfile surface;
    float rainfall;
    SurfaceKind surface_kind;
    std::int32_t subtype;
    bool mesa_bryce;
    bool mesa_forest;
};

// Returns the actual fields observed by terrain and surface construction for
// a biome id, including the mutations produced by RegionHillsLayer.
[[nodiscard]] BiomeProperties biome_properties(std::int32_t id) noexcept;

} // namespace mcpe::worldgen::v0_9_0
