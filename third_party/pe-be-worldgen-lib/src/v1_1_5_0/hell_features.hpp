#pragma once

#include "detail/liquid_access.hpp"
#include "detail/mt19937.hpp"

#include <cstdint>

namespace mcpe::worldgen::v1_1_5_0::hell_feature {

// Feature routines called by HellRandomLevelSource::postProcess. They work on
// the narrow TileSource surface shared by the generator worlds; no gameplay
// ticking or entity simulation is pulled into the worldgen library.
[[nodiscard]] bool place_spring(
    detail::LiquidAccess& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    bool inside_rock_only) noexcept;

[[nodiscard]] bool place_glowstone(
    detail::BlockAccess& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept;

void place_fire(
    detail::BlockAccess& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept;

void place_mushroom(
    detail::BlockAccess& world,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t mushroom) noexcept;

} // namespace mcpe::worldgen::v1_1_5_0::hell_feature
