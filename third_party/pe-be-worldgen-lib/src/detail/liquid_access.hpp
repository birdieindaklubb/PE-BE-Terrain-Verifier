#pragma once

#include "detail/block_access.hpp"

namespace mcpe::worldgen::detail {

// The additional TileSource operations used by the dynamic-liquid generator.
// Keeping this separate from BlockAccess lets terrain and structure writers
// stay deliberately small, while both the overworld and Nether can share the
// same liquid state machine.
class LiquidAccess : public BlockAccess {
public:
    ~LiquidAccess() override = default;

    [[nodiscard]] virtual std::uint8_t data(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept = 0;

    [[nodiscard]] virtual bool has_chunks_at(
        std::int32_t x,
        std::int32_t z,
        std::int32_t radius) const noexcept = 0;
};

} // namespace mcpe::worldgen::detail
