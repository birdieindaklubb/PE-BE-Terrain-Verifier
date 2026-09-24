#pragma once

#include "detail/mt19937.hpp"
#include "detail/noise.hpp"
#include "detail/simplex_noise.hpp"

#include <cstdint>

namespace mcpe::terrain_verifier {

// Exact one-block PE 1.1.5 End terrain evaluator used as a scan prefilter.
// It includes both density terrain and the small outer-island End-stone pass.
class EndPointSource final {
public:
    explicit EndPointSource(std::uint32_t world_seed);

    [[nodiscard]] bool non_air(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z);

private:
    [[nodiscard]] float island_height_value(
        std::int32_t chunk_x,
        std::int32_t chunk_z,
        std::int32_t local_x,
        std::int32_t local_z) const noexcept;
    [[nodiscard]] bool base_non_air(
        std::int32_t chunk_x,
        std::int32_t chunk_z,
        std::int32_t local_x,
        std::int32_t y,
        std::int32_t local_z);

    std::uint32_t world_seed_{};
    std::uint32_t population_x_multiplier_{};
    std::uint32_t population_z_multiplier_{};
    worldgen::detail::Mt19937 random_;
    worldgen::detail::PerlinNoise density_lower_;
    worldgen::detail::PerlinNoise density_upper_;
    worldgen::detail::PerlinNoise density_selector_;
    worldgen::detail::SimplexNoise island_noise_;
};

} // namespace mcpe::terrain_verifier
