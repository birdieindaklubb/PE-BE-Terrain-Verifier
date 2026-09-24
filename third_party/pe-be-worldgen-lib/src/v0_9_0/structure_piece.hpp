#pragma once

#include "detail/block_access.hpp"
#include "detail/mt19937.hpp"
#include "v0_9_0/world.hpp"

#include <cstdint>

namespace mcpe::worldgen::v0_9_0::structure {

// Inclusive StructurePiece bounding box, stored in the same min-X, min-Y,
// min-Z, max-X, max-Y, max-Z order used by PE's StructurePiece.
struct BoundingBox final {
    std::int32_t min_x{};
    std::int32_t min_y{};
    std::int32_t min_z{};
    std::int32_t max_x{};
    std::int32_t max_y{};
    std::int32_t max_z{};

    [[nodiscard]] bool contains(
        std::int32_t x, std::int32_t y, std::int32_t z) const noexcept;
    [[nodiscard]] bool overlaps(const BoundingBox& other) const noexcept;
    [[nodiscard]] bool intersects_chunk(
        std::int32_t chunk_x, std::int32_t chunk_z) const noexcept;
    void translate(
        std::int32_t x, std::int32_t y, std::int32_t z) noexcept;
};

// The numeric values are significant: native component constructors store a
// random word modulo four, and StructurePiece's coordinate transforms switch
// on those raw values. `none` is used by unrotated pieces.
enum class Orientation : std::int32_t {
    south = 0,
    west = 1,
    north = 2,
    east = 3,
    none = -1,
};

// StructurePiece::getOrientationData for a chest.  The native fortress,
// village, and stronghold helpers all use this same two-case transform.
[[nodiscard]] std::uint8_t chest_orientation_data(
    Orientation orientation,
    std::uint8_t data) noexcept;

struct LocalPosition final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
};

// StructurePiece's rotated inclusive bounding-box convention.  Nether
// fortresses use the same engine routine, so it lives alongside the shared
// coordinate transforms rather than being reimplemented per structure family.
[[nodiscard]] BoundingBox oriented_box(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    Orientation orientation,
    std::int32_t side_before,
    std::int32_t side_after,
    std::int32_t forward_length,
    std::int32_t top_offset,
    std::int32_t bottom_offset) noexcept;

// The common coordinate and clipped-write behavior used by village,
// mineshaft, and stronghold pieces. The pieces themselves remain independent
// feature classes; this small shared primitive avoids three divergent copies
// of StructurePiece's orientation rules.
class PiecePlacement final {
public:
    PiecePlacement(BoundingBox bounds, Orientation orientation) noexcept;

    [[nodiscard]] std::int32_t world_x(
        std::int32_t local_x, std::int32_t local_z) const noexcept;
    [[nodiscard]] std::int32_t world_y(std::int32_t local_y) const noexcept;
    [[nodiscard]] std::int32_t world_z(
        std::int32_t local_x, std::int32_t local_z) const noexcept;
    [[nodiscard]] LocalPosition world_position(
        std::int32_t local_x,
        std::int32_t local_y,
        std::int32_t local_z) const noexcept;

    void place_block(
        detail::BlockAccess& world,
        const BoundingBox& active_chunk,
        std::uint8_t id,
        std::uint8_t data,
        std::int32_t local_x,
        std::int32_t local_y,
        std::int32_t local_z) const noexcept;
    [[nodiscard]] std::uint8_t block_at(
        const detail::BlockAccess& world,
        const BoundingBox& active_chunk,
        std::int32_t local_x,
        std::int32_t local_y,
        std::int32_t local_z) const noexcept;
    [[nodiscard]] bool edges_liquid(
        const detail::BlockAccess& world,
        const BoundingBox& active_chunk) const noexcept;

    // The following helpers retain StructurePiece's local-coordinate loop
    // order and clipping rules.  Keeping them here is intentional: every
    // native structure family delegates these writes to StructurePiece.
    void generate_box(
        detail::BlockAccess& world,
        const BoundingBox& active_chunk,
        std::uint8_t boundary_id,
        std::uint8_t inside_id,
        std::int32_t min_x,
        std::int32_t min_y,
        std::int32_t min_z,
        std::int32_t max_x,
        std::int32_t max_y,
        std::int32_t max_z,
        bool only_existing) const noexcept;
    void generate_air_box(
        detail::BlockAccess& world,
        const BoundingBox& active_chunk,
        std::int32_t min_x,
        std::int32_t min_y,
        std::int32_t min_z,
        std::int32_t max_x,
        std::int32_t max_y,
        std::int32_t max_z) const noexcept;
    void generate_maybe_box(
        detail::BlockAccess& world,
        const BoundingBox& active_chunk,
        detail::Mt19937& random,
        float probability,
        std::uint8_t id,
        std::int32_t min_x,
        std::int32_t min_y,
        std::int32_t min_z,
        std::int32_t max_x,
        std::int32_t max_y,
        std::int32_t max_z,
        bool only_existing) const noexcept;
    void maybe_generate_block(
        detail::BlockAccess& world,
        const BoundingBox& active_chunk,
        detail::Mt19937& random,
        float probability,
        std::uint8_t id,
        std::uint8_t data,
        std::int32_t local_x,
        std::int32_t local_y,
        std::int32_t local_z) const noexcept;
    void fill_column_down(
        detail::BlockAccess& world,
        const BoundingBox& active_chunk,
        std::uint8_t id,
        std::uint8_t data,
        std::int32_t local_x,
        std::int32_t local_y,
        std::int32_t local_z) const noexcept;
    void generate_air_column_up(
        detail::BlockAccess& world,
        const BoundingBox& active_chunk,
        std::int32_t local_x,
        std::int32_t local_y,
        std::int32_t local_z) const noexcept;
    void generate_upper_half_sphere(
        detail::BlockAccess& world,
        const BoundingBox& active_chunk,
        std::int32_t min_x,
        std::int32_t min_y,
        std::int32_t min_z,
        std::int32_t max_x,
        std::int32_t max_y,
        std::int32_t max_z,
        std::uint8_t id,
        bool only_existing) const noexcept;

    [[nodiscard]] const BoundingBox& bounds() const noexcept;
    [[nodiscard]] Orientation orientation() const noexcept;
    void translate_y(std::int32_t offset) noexcept;

private:
    BoundingBox bounds_;
    Orientation orientation_;
};

} // namespace mcpe::worldgen::v0_9_0::structure
