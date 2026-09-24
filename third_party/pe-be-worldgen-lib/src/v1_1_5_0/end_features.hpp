#pragma once

#include "detail/mt19937.hpp"
#include "mcpe/worldgen/generated_content.hpp"

#include <array>
#include <cstdint>

namespace mcpe::worldgen::v1_1_5_0 {

class EndBlockWriter {
public:
    virtual ~EndBlockWriter() = default;

    virtual void set_end_block(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::uint8_t block,
        std::uint8_t data = 0) noexcept = 0;
};

// A small block-level view is sufficient for the two recursive Chorus-flower
// routines.  Keeping it separate from Chunk avoids coupling a feature to the
// library's storage layout and makes cross-chunk writes explicit.
class EndBlockAccess : public EndBlockWriter {
public:
    [[nodiscard]] virtual std::uint8_t end_block(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept = 0;
};

[[nodiscard]] std::array<GeneratedEndPillar, 10> end_pillars(
    std::uint32_t world_seed) noexcept;

void place_end_island(
    EndBlockWriter& writer,
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept;

void place_end_pillar(
    EndBlockWriter& writer,
    const GeneratedEndPillar& pillar) noexcept;

// ChorusFlowerBlock::generatePlant.  This is the one-shot terrain population
// routine, not the later random-tick lifecycle.  A generated terminal flower
// has metadata 5, exactly as it does in the native implementation.
void place_end_chorus_plant(
    EndBlockAccess& blocks,
    detail::Mt19937& random,
    BlockPosition root,
    std::int32_t spread_limit) noexcept;

// EndGatewayFeature::place writes its fixed, three-by-five-by-three gateway
// shell around the supplied centre.  Selection of a natural gateway centre is
// part of the per-chunk post-processing stream; this primitive intentionally
// also supports the runtime-created gateways whose centres are not seed-only.
void place_end_gateway(
    EndBlockWriter& writer,
    BlockPosition center) noexcept;

// The End fight places this non-seeded geometry at its chosen centre. The
// boolean only controls the five-by-five portal interior; the rest of the
// podium is identical in both fight states.
void place_end_exit_podium(
    EndBlockWriter& writer,
    BlockPosition center,
    bool active) noexcept;

// The first End arrival clears headroom over, then lays, the fixed 5 by 5
// obsidian safety pad. The fixed location is intentional game behavior.
void place_end_arrival_platform(EndBlockWriter& writer) noexcept;

} // namespace mcpe::worldgen::v1_1_5_0
