#pragma once

#include "detail/simplex_noise.hpp"
#include "v0_9_0/surface.hpp"

#include <cstdint>
#include <span>

namespace mcpe::worldgen::detail {
class Mt19937;
}

namespace mcpe::worldgen::v0_9_0 {

// Profile choices made immediately before Biome::buildSurfaceAtDefault by the
// native overridden biome methods.  The returned records can then be passed to
// apply_default_surface_column without changing its random draw sequence.
[[nodiscard]] SurfaceProfile extreme_hills_surface_profile(
    std::int32_t type,
    float surface_noise,
    float temperature) noexcept;

[[nodiscard]] SurfaceProfile taiga_surface_profile(
    std::int32_t type,
    float surface_noise,
    float temperature) noexcept;

[[nodiscard]] SurfaceProfile mutated_savanna_surface_profile(
    float surface_noise,
    float temperature) noexcept;

// The swamp profile owns an independently seeded one-octave simplex instance.
// Its prepass runs before the ordinary surface algorithm.
class SwampSurface final {
public:
    SwampSurface();

    void apply_prepass(
        std::span<std::uint8_t> blocks,
        std::int32_t x,
        std::int32_t z,
        std::int32_t world_x,
        std::int32_t world_z) const noexcept;

private:
    detail::PerlinSimplexNoise noise_;
};

} // namespace mcpe::worldgen::v0_9_0
