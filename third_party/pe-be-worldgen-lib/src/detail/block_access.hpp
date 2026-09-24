#pragma once

#include <cstdint>

namespace mcpe::worldgen::detail {

// The tiny portion of TileSource required by StructurePiece's coordinate and
// block-writing helpers.  It deliberately has no terrain, biome, cache, or
// gameplay responsibilities, so the shared structure helpers can serve more
// than one dimension/version without pretending those worlds are identical.
class BlockAccess {
public:
    virtual ~BlockAccess() = default;

    [[nodiscard]] virtual std::uint8_t block(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept = 0;
    virtual bool set_block_and_data(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::uint8_t id,
        std::uint8_t metadata,
        bool normal_write) noexcept = 0;
};

} // namespace mcpe::worldgen::detail
