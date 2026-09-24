#pragma once

#include <cstdint>
#include <span>

namespace mcpe::terrain_verifier {

struct CavePoint final {
    std::int32_t local_x{};
    std::int32_t y{};
    std::int32_t local_z{};
};

// Necessary, not sufficient: returns false only when native PE 0.9.0 cave
// geometry cannot possibly carve every supplied point. Water rejection and
// block-material checks are intentionally deferred to full terrain.
[[nodiscard]] bool pe090_caves_may_reach_all(
    std::uint32_t world_seed,
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    std::span<const CavePoint> points) noexcept;

} // namespace mcpe::terrain_verifier
