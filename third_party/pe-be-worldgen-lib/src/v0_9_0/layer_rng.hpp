#pragma once

#include <cstdint>

namespace mcpe::worldgen::v0_9_0 {

// The biome layer graph has its own 64-bit generator.  It is not Random
// (MT19937): each layer keeps a base seed, a world seed and a per-cell seed.
class LayerRandom final {
public:
    explicit LayerRandom(std::int64_t base_seed) noexcept;

    void set_world_seed(std::uint32_t world_seed) noexcept;
    void set_chunk_seed(std::int32_t x, std::int32_t z) noexcept;

    [[nodiscard]] std::int32_t next_int(std::int32_t bound) noexcept;

private:
    [[nodiscard]] static std::uint64_t advance(
        std::uint64_t state,
        std::uint64_t additive) noexcept;

    std::uint64_t base_seed_{};
    std::uint64_t world_seed_{};
    std::uint64_t chunk_seed_{};
};

} // namespace mcpe::worldgen::v0_9_0
