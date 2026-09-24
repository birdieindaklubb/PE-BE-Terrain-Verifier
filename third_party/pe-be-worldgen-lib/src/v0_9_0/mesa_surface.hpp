#pragma once

#include "detail/simplex_noise.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace mcpe::worldgen::detail {
class Mt19937;
}

namespace mcpe::worldgen::v0_9_0 {

// MesaBiome has one world-seeded state shared by its mesa, mesa-bryce and
// mesa-forest variants.  It is refreshed when the level source is created.
class MesaSurface final {
public:
    explicit MesaSurface(std::uint32_t world_seed);

    [[nodiscard]] std::uint8_t band(
        std::int32_t world_x,
        std::int32_t y,
        std::int32_t world_z) const noexcept;

    // MesaBiome::buildSurfaceAt.  world_x/world_z are absolute block
    // coordinates; x/z are the corresponding local positions in blocks.
    void apply(
        detail::Mt19937& random,
        std::span<std::uint8_t> blocks,
        std::span<std::uint8_t> data,
        std::int32_t x,
        std::int32_t z,
        std::int32_t world_x,
        std::int32_t world_z,
        float surface_noise,
        bool bryce_pillars,
        bool forest_variant) const noexcept;

private:
    struct SeedState final {
        std::array<std::uint8_t, 64> bands;
        detail::PerlinSimplexNoise band_offset;
        detail::PerlinSimplexNoise pillar;
        detail::PerlinSimplexNoise pillar_roof;
    };

    [[nodiscard]] static SeedState make_seed_state(std::uint32_t world_seed);

    [[nodiscard]] std::int32_t pillar_height(
        std::int32_t world_x,
        std::int32_t world_z,
        float surface_noise) const noexcept;

    SeedState state_;
};

} // namespace mcpe::worldgen::v0_9_0
