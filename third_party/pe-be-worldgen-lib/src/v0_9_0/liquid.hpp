#pragma once

#include "detail/liquid_access.hpp"
#include "detail/mt19937.hpp"

#include "v0_9_0/world.hpp"

#include <cstdint>
#include <span>

namespace mcpe::worldgen::v0_9_0::liquid {

// The final phase of RandomLevelSource::postProcess.  PE 0.9.0 walks the
// populated chunk's 60 perimeter columns in a fixed order, changes the first
// still-water tile of every vertical run to flowing water, and drains the
// local TileTickingQueue synchronously.  TileSource was constructed with
// ordinary feature callbacks disabled for population. The source still runs
// this local deferred-fluid pass, including its liquid-neighbor effects; it
// remains a generation-only simulation rather than the runtime liquid system.
void settle_chunk_boundary_water(
    World& world,
    std::int32_t block_x,
    std::int32_t block_z) noexcept;

struct NetherLavaTick final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
    std::int32_t delay{};
};

// HellRandomLevelSource collects its initial lava ticks while all ordinary
// features and fortress pieces are written, then drains the queue in one
// tickAllPendingTicks pass. Nested liquid updates remain queued; they are not
// recursive calls. Hell lava advances one depth level per update.
void settle_nether_lava(
    detail::LiquidAccess& world,
    detail::Mt19937& random,
    std::span<const NetherLavaTick> initial_ticks) noexcept;

} // namespace mcpe::worldgen::v0_9_0::liquid
