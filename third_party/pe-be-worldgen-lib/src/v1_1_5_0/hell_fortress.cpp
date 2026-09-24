#include "v1_1_5_0/hell_fortress.hpp"

#include "detail/wrap.hpp"
#include "v1_1_5_0/blocks.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace mcpe::worldgen::v1_1_5_0::hell_fortress {
namespace {

using v0_9_0::structure::BoundingBox;
using v0_9_0::structure::Orientation;

[[nodiscard]] constexpr std::int32_t add(
    std::int32_t left,
    std::int32_t right) noexcept {
    return detail::wrapping_add(left, right);
}

[[nodiscard]] constexpr std::int32_t sub(
    std::int32_t left,
    std::int32_t right) noexcept {
    return detail::wrapping_sub(left, right);
}

[[nodiscard]] constexpr std::int32_t native_abs(std::int32_t value) noexcept {
    return value < 0 ? detail::wrapping_sub(0, value) : value;
}

[[nodiscard]] constexpr Orientation orientation_from_word(
    std::uint32_t word) noexcept {
    return static_cast<Orientation>(word & 3U);
}

[[nodiscard]] BoundingBox piece_bounds(
    Build::PieceKind kind,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    Orientation orientation) noexcept {
    using K = Build::PieceKind;
    // These are compact forms of each native createPiece call: the width,
    // height, and depth passed to getComponentToAddBoundingBox are converted
    // to its inclusive endpoints once here.
    switch (kind) {
    case K::bridge_straight:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 1, 3, 18, 6, -3);
    case K::bridge_crossing:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 8, 10, 18, 6, -3);
    case K::large_crossing:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 2, 4, 6, 8, 0);
    case K::bridge_stairs:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 2, 4, 6, 10, 0);
    case K::blaze_spawner:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 2, 4, 8, 7, 0);
    case K::castle_entrance:
    case K::nether_wart_farm:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 5, 7, 12, 10, -3);
    case K::small_corridor:
    case K::small_corridor_crossing:
    case K::small_corridor_right_turn:
    case K::small_corridor_left_turn:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 1, 3, 4, 6, 0);
    case K::stairs_corridor:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 1, 3, 9, 6, -7);
    case K::corridor_balcony:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 3, 5, 8, 6, 0);
    case K::bridge_end:
        return v0_9_0::structure::oriented_box(
            x, y, z, orientation, 1, 3, 7, 6, -3);
    }
    return {};
}

class RawChunkAccess final : public detail::BlockAccess {
public:
    explicit RawChunkAccess(Chunk& chunk) noexcept
        : chunk_(chunk),
          origin_x_(detail::wrapping_mul(chunk.x, Chunk::width)),
          origin_z_(detail::wrapping_mul(chunk.z, Chunk::width)) {
    }

    [[nodiscard]] std::uint8_t block(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept override {
        if (static_cast<std::uint32_t>(y) >= static_cast<std::uint32_t>(Chunk::height)
            || static_cast<std::uint32_t>(sub(x, origin_x_)) >= Chunk::width
            || static_cast<std::uint32_t>(sub(z, origin_z_)) >= Chunk::width) {
            return block::air;
        }
        return chunk_.block(
            static_cast<std::int32_t>(static_cast<std::uint32_t>(x) & 15U),
            y,
            static_cast<std::int32_t>(static_cast<std::uint32_t>(z) & 15U));
    }

    bool set_block_and_data(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::uint8_t id,
        std::uint8_t metadata,
        bool) noexcept override {
        if (static_cast<std::uint32_t>(y) >= static_cast<std::uint32_t>(Chunk::height)
            || static_cast<std::uint32_t>(sub(x, origin_x_)) >= Chunk::width
            || static_cast<std::uint32_t>(sub(z, origin_z_)) >= Chunk::width) {
            return false;
        }
        chunk_.set_block_and_data(
            static_cast<std::int32_t>(static_cast<std::uint32_t>(x) & 15U),
            y,
            static_cast<std::int32_t>(static_cast<std::uint32_t>(z) & 15U),
            id,
            metadata);
        return true;
    }

private:
    Chunk& chunk_;
    std::int32_t origin_x_{};
    std::int32_t origin_z_{};
};

class Painter final {
public:
    Painter(
        RawChunkAccess& access,
        const Build::Piece& piece,
        const BoundingBox& active_chunk,
        detail::Mt19937& structure_random,
        std::vector<BlockPosition>* lava_sources,
        std::vector<BlockPosition>* blaze_spawners,
        std::vector<GeneratedChest>* chests) noexcept
        : access_(access),
          placement_(piece.bounds, piece.orientation),
          active_(active_chunk),
          structure_random_(structure_random),
          lava_sources_(lava_sources),
          blaze_spawners_(blaze_spawners),
          chests_(chests) {
    }

    void solid(
        std::int32_t min_x, std::int32_t min_y, std::int32_t min_z,
        std::int32_t max_x, std::int32_t max_y, std::int32_t max_z,
        std::uint8_t id = block::nether_brick) noexcept {
        placement_.generate_box(
            access_, active_, id, id,
            min_x, min_y, min_z, max_x, max_y, max_z, false);
    }

    void air(
        std::int32_t min_x, std::int32_t min_y, std::int32_t min_z,
        std::int32_t max_x, std::int32_t max_y, std::int32_t max_z) noexcept {
        placement_.generate_air_box(
            access_, active_, min_x, min_y, min_z, max_x, max_y, max_z);
    }

    void tile(
        std::uint8_t id,
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::uint8_t data = 0) noexcept {
        placement_.place_block(access_, active_, id, data, x, y, z);
        const auto position = placement_.world_position(x, y, z);
        if (!active_.contains(position.x, position.y, position.z)) {
            return;
        }
        if (id == block::flowing_lava && lava_sources_ != nullptr) {
            lava_sources_->push_back({position.x, position.y, position.z});
        } else if (id == block::mob_spawner && blaze_spawners_ != nullptr) {
            blaze_spawners_->push_back({position.x, position.y, position.z});
        }
    }

    void support(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
        placement_.fill_column_down(access_, active_, block::nether_brick, 0, x, y, z);
    }

    void chest(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
        // StructureHelpers::createChest first clips the transformed position,
        // then skips an already-created chest.  Its loot-table seed is drawn
        // only after a new ChestBlockActor has been installed.
        const auto position = placement_.world_position(x, y, z);
        if (!active_.contains(position.x, position.y, position.z)
            || access_.block(position.x, position.y, position.z) == block::chest) {
            return;
        }
        placement_.place_block(
            access_, active_, block::chest,
            v0_9_0::structure::chest_orientation_data(
                placement_.orientation(), 1),
            x, y, z);
        const std::uint32_t loot_seed = structure_random_.next_u32() >> 1U;
        if (chests_ != nullptr) {
            chests_->push_back({
                {position.x, position.y, position.z},
                {},
                GeneratedLootTable{
                    "loot_tables/chests/nether_bridge.json", loot_seed},
            });
        }
    }

private:
    RawChunkAccess& access_;
    v0_9_0::structure::PiecePlacement placement_;
    const BoundingBox& active_;
    detail::Mt19937& structure_random_;
    std::vector<BlockPosition>* lava_sources_{};
    std::vector<BlockPosition>* blaze_spawners_{};
    std::vector<GeneratedChest>* chests_{};
};

[[nodiscard]] std::uint8_t stair_data(
    Orientation orientation,
    std::uint8_t data) noexcept {
    // StructurePiece::getBlockMeta is shared by NetherBrickStairs and the
    // older stair tiles. The PE orientation values are 0=south, 1=west,
    // 2=north, 3=east.
    switch (orientation) {
    case Orientation::south:
        return data == 2U ? 3U : data == 3U ? 2U : data;
    case Orientation::west: {
        constexpr std::array<std::uint8_t, 4> rotated{{2, 3, 0, 1}};
        return data < rotated.size() ? rotated[data] : data;
    }
    case Orientation::east: {
        constexpr std::array<std::uint8_t, 4> rotated{{2, 3, 1, 0}};
        return data < rotated.size() ? rotated[data] : data;
    }
    case Orientation::north:
    case Orientation::none:
        return data;
    }
    return data;
}

void supports(Painter& paint,
              std::int32_t min_x,
              std::int32_t max_x,
              std::int32_t min_z,
              std::int32_t max_z,
              std::int32_t y = -1) noexcept {
    for (std::int32_t x = min_x; x <= max_x; ++x) {
        for (std::int32_t z = min_z; z <= max_z; ++z) {
            paint.support(x, y, z);
        }
    }
}

void draw_straight_bridge(Painter& paint) noexcept {
    paint.solid(0, 3, 0, 4, 4, 18);
    paint.air(1, 5, 0, 3, 7, 18);
    paint.solid(0, 5, 0, 0, 5, 18);
    paint.solid(4, 5, 0, 4, 5, 18);
    paint.solid(0, 2, 0, 4, 2, 5);
    paint.solid(0, 2, 13, 4, 2, 18);
    paint.solid(0, 0, 0, 4, 1, 3);
    paint.solid(0, 0, 15, 4, 1, 18);
    for (std::int32_t x = 0; x <= 4; ++x) {
        for (std::int32_t z = 0; z <= 2; ++z) {
            paint.support(x, -1, z);
            paint.support(x, -1, 18 - z);
        }
    }
    paint.solid(0, 1, 1, 0, 4, 1, block::nether_brick_fence);
    paint.solid(0, 3, 4, 0, 4, 4, block::nether_brick_fence);
    paint.solid(0, 3, 14, 0, 4, 14, block::nether_brick_fence);
    paint.solid(0, 1, 17, 0, 4, 17, block::nether_brick_fence);
    paint.solid(4, 1, 1, 4, 4, 1, block::nether_brick_fence);
    paint.solid(4, 3, 4, 4, 4, 4, block::nether_brick_fence);
    paint.solid(4, 3, 14, 4, 4, 14, block::nether_brick_fence);
    paint.solid(4, 1, 17, 4, 4, 17, block::nether_brick_fence);
}

void draw_small_corridor(Painter& paint) noexcept {
    paint.solid(0, 0, 0, 4, 1, 4);
    paint.air(0, 2, 0, 4, 5, 4);
    paint.solid(0, 2, 0, 0, 5, 4);
    paint.solid(4, 2, 0, 4, 5, 4);
    paint.solid(0, 3, 1, 0, 4, 1, block::nether_brick_fence);
    paint.solid(0, 3, 3, 0, 4, 3, block::nether_brick_fence);
    paint.solid(4, 3, 1, 4, 4, 1, block::nether_brick_fence);
    paint.solid(4, 3, 3, 4, 4, 3, block::nether_brick_fence);
    paint.solid(0, 6, 0, 4, 6, 4);
    supports(paint, 0, 4, 0, 4);
}

void draw_small_crossing(Painter& paint) noexcept {
    paint.solid(0, 0, 0, 4, 1, 4);
    paint.air(0, 2, 0, 4, 5, 4);
    paint.solid(0, 2, 0, 0, 5, 0);
    paint.solid(4, 2, 0, 4, 5, 0);
    paint.solid(0, 2, 4, 0, 5, 4);
    paint.solid(4, 2, 4, 4, 5, 4);
    paint.solid(0, 6, 0, 4, 6, 4);
    supports(paint, 0, 4, 0, 4);
}

void draw_left_turn(Painter& paint) noexcept {
    paint.solid(0, 0, 0, 4, 1, 4);
    paint.air(0, 2, 0, 4, 5, 4);
    paint.solid(4, 2, 0, 4, 5, 4);
    paint.solid(4, 3, 1, 4, 4, 1, block::nether_brick_fence);
    paint.solid(4, 3, 3, 4, 4, 3, block::nether_brick_fence);
    paint.solid(0, 2, 0, 0, 5, 0);
    paint.solid(0, 2, 4, 3, 5, 4);
    paint.solid(1, 3, 4, 1, 4, 4, block::nether_brick_fence);
    paint.solid(3, 3, 4, 3, 4, 4, block::nether_brick_fence);
    paint.solid(0, 6, 0, 4, 6, 4);
    supports(paint, 0, 4, 0, 4);
}

void draw_right_turn(Painter& paint) noexcept {
    paint.solid(0, 0, 0, 4, 1, 4);
    paint.air(0, 2, 0, 4, 5, 4);
    paint.solid(0, 2, 0, 0, 5, 4);
    paint.solid(0, 3, 1, 0, 4, 1, block::nether_brick_fence);
    paint.solid(0, 3, 3, 0, 4, 3, block::nether_brick_fence);
    paint.solid(4, 2, 0, 4, 5, 0);
    paint.solid(1, 2, 4, 4, 5, 4);
    paint.solid(1, 3, 4, 1, 4, 4, block::nether_brick_fence);
    paint.solid(3, 3, 4, 3, 4, 4, block::nether_brick_fence);
    paint.solid(0, 6, 0, 4, 6, 4);
    supports(paint, 0, 4, 0, 4);
}

void draw_large_crossing(Painter& paint) noexcept {
    paint.solid(0, 0, 0, 6, 1, 6);
    paint.air(0, 2, 0, 6, 7, 6);
    paint.solid(0, 2, 0, 1, 6, 0);
    paint.solid(0, 2, 6, 1, 6, 6);
    paint.solid(5, 2, 0, 6, 6, 0);
    paint.solid(5, 2, 6, 6, 6, 6);
    paint.solid(0, 2, 0, 0, 6, 1);
    paint.solid(0, 2, 5, 0, 6, 6);
    paint.solid(6, 2, 0, 6, 6, 1);
    paint.solid(6, 2, 5, 6, 6, 6);
    paint.solid(2, 6, 0, 4, 6, 0);
    paint.solid(2, 5, 0, 4, 5, 0, block::nether_brick_fence);
    paint.solid(2, 6, 6, 4, 6, 6);
    paint.solid(2, 5, 6, 4, 5, 6, block::nether_brick_fence);
    paint.solid(0, 6, 2, 0, 6, 4);
    paint.solid(0, 5, 2, 0, 5, 4, block::nether_brick_fence);
    paint.solid(6, 6, 2, 6, 6, 4);
    paint.solid(6, 5, 2, 6, 5, 4, block::nether_brick_fence);
    supports(paint, 0, 6, 0, 6);
}

void draw_bridge_stairs(Painter& paint) noexcept {
    paint.solid(0, 0, 0, 6, 1, 6);
    paint.air(0, 2, 0, 6, 10, 6);
    paint.solid(0, 2, 0, 1, 8, 0);
    paint.solid(5, 2, 0, 6, 8, 0);
    paint.solid(0, 2, 1, 0, 8, 6);
    paint.solid(6, 2, 1, 6, 8, 6);
    paint.solid(1, 2, 6, 5, 8, 6);
    paint.solid(0, 3, 2, 0, 5, 4, block::nether_brick_fence);
    paint.solid(6, 3, 2, 6, 5, 2, block::nether_brick_fence);
    paint.solid(6, 3, 4, 6, 5, 4, block::nether_brick_fence);
    paint.tile(block::nether_brick, 5, 2, 5);
    paint.solid(4, 2, 5, 4, 3, 5);
    paint.solid(3, 2, 5, 3, 4, 5);
    paint.solid(2, 2, 5, 2, 5, 5);
    paint.solid(1, 2, 5, 1, 6, 5);
    paint.solid(1, 7, 1, 5, 7, 4);
    paint.air(6, 8, 2, 6, 8, 4);
    paint.solid(2, 6, 0, 4, 8, 0);
    paint.solid(2, 5, 0, 4, 5, 0, block::nether_brick_fence);
    supports(paint, 0, 6, 0, 6);
}

void draw_corridor_balcony(Painter& paint) noexcept {
    paint.solid(0, 0, 0, 8, 1, 8);
    paint.air(0, 2, 0, 8, 5, 8);
    paint.solid(0, 6, 0, 8, 6, 5);
    paint.solid(0, 2, 0, 2, 5, 0);
    paint.solid(6, 2, 0, 8, 5, 0);
    paint.solid(1, 3, 0, 1, 4, 0, block::nether_brick_fence);
    paint.solid(7, 3, 0, 7, 4, 0, block::nether_brick_fence);
    paint.solid(0, 2, 4, 8, 2, 8);
    paint.air(1, 1, 4, 2, 2, 4);
    paint.air(6, 1, 4, 7, 2, 4);
    paint.solid(0, 3, 8, 8, 3, 8, block::nether_brick_fence);
    paint.solid(0, 3, 6, 0, 3, 7, block::nether_brick_fence);
    paint.solid(8, 3, 6, 8, 3, 7, block::nether_brick_fence);
    paint.solid(0, 3, 4, 0, 5, 5);
    paint.solid(8, 3, 4, 8, 5, 5);
    paint.solid(1, 3, 5, 2, 5, 5);
    paint.solid(6, 3, 5, 7, 5, 5);
    paint.solid(1, 4, 5, 1, 5, 5, block::nether_brick_fence);
    paint.solid(7, 4, 5, 7, 5, 5, block::nether_brick_fence);
    supports(paint, 0, 8, 0, 5);
}

void draw_bridge_crossing(Painter& paint) noexcept {
    paint.solid(7, 3, 0, 11, 4, 18);
    paint.solid(0, 3, 7, 18, 4, 11);
    paint.air(8, 5, 0, 10, 7, 18);
    paint.air(0, 5, 8, 18, 7, 10);
    paint.solid(7, 5, 0, 7, 5, 7);
    paint.solid(7, 5, 11, 7, 5, 18);
    paint.solid(11, 5, 0, 11, 5, 7);
    paint.solid(11, 5, 11, 11, 5, 18);
    paint.solid(0, 5, 7, 7, 5, 7);
    paint.solid(11, 5, 7, 18, 5, 7);
    paint.solid(0, 5, 11, 7, 5, 11);
    paint.solid(11, 5, 11, 18, 5, 11);
    paint.solid(7, 2, 0, 11, 2, 5);
    paint.solid(7, 2, 13, 11, 2, 18);
    paint.solid(7, 0, 0, 11, 1, 3);
    paint.solid(7, 0, 15, 11, 1, 18);
    for (std::int32_t x = 7; x <= 11; ++x) {
        for (std::int32_t z = 0; z <= 2; ++z) {
            paint.support(x, -1, z);
            paint.support(x, -1, 18 - z);
        }
    }
    paint.solid(0, 2, 7, 5, 2, 11);
    paint.solid(13, 2, 7, 18, 2, 11);
    paint.solid(0, 0, 7, 3, 1, 11);
    paint.solid(15, 0, 7, 18, 1, 11);
    for (std::int32_t x = 0; x <= 2; ++x) {
        for (std::int32_t z = 7; z <= 11; ++z) {
            paint.support(x, -1, z);
            paint.support(18 - x, -1, z);
        }
    }
}

void draw_stairs_corridor(
    Painter& paint,
    Orientation orientation) noexcept {
    const std::uint8_t stair = stair_data(orientation, 2);
    for (std::int32_t z = 0; z <= 9; ++z) {
        const std::int32_t lower = std::max(1, 7 - z);
        const std::int32_t upper = std::min(std::max(lower + 5, 14 - z), 13);
        paint.solid(0, 0, z, 4, lower, z);
        paint.air(1, lower + 1, z, 3, upper - 1, z);
        if (z <= 6) {
            paint.tile(block::nether_brick_stairs, 1, lower + 1, z, stair);
            paint.tile(block::nether_brick_stairs, 2, lower + 1, z, stair);
            paint.tile(block::nether_brick_stairs, 3, lower + 1, z, stair);
        }
        paint.solid(0, upper, z, 4, upper, z);
        paint.solid(0, lower + 1, z, 0, upper - 1, z);
        paint.solid(4, lower + 1, z, 4, upper - 1, z);
        if ((z & 1) == 0) {
            paint.solid(0, lower + 2, z, 0, lower + 3, z,
                        block::nether_brick_fence);
            paint.solid(4, lower + 2, z, 4, lower + 3, z,
                        block::nether_brick_fence);
        }
        for (std::int32_t x = 0; x <= 4; ++x) {
            paint.support(x, -1, z);
        }
    }
}

void draw_blaze_spawner(Painter& paint) noexcept {
    paint.air(0, 2, 0, 6, 7, 7);
    paint.solid(1, 0, 0, 5, 1, 7);
    paint.solid(1, 2, 1, 5, 2, 7);
    paint.solid(1, 3, 2, 5, 3, 7);
    paint.solid(1, 4, 3, 5, 4, 7);
    paint.solid(1, 2, 0, 1, 4, 2);
    paint.solid(5, 2, 0, 5, 4, 2);
    paint.solid(1, 5, 2, 1, 5, 3);
    paint.solid(5, 5, 2, 5, 5, 3);
    paint.solid(0, 5, 3, 0, 5, 8);
    paint.solid(6, 5, 3, 6, 5, 8);
    paint.solid(1, 5, 8, 5, 5, 8);
    paint.tile(block::nether_brick_fence, 1, 6, 3);
    paint.tile(block::nether_brick_fence, 5, 6, 3);
    paint.solid(0, 6, 3, 0, 6, 8, block::nether_brick_fence);
    paint.solid(6, 6, 3, 6, 6, 8, block::nether_brick_fence);
    paint.solid(1, 6, 8, 5, 7, 8, block::nether_brick_fence);
    paint.solid(2, 8, 8, 4, 8, 8, block::nether_brick_fence);
    // NBMonsterThrone creates the tile entity during the later structure
    // post-process. The raw pass still writes its BlockID deterministically.
    paint.tile(block::mob_spawner, 3, 5, 5);
    supports(paint, 0, 6, 0, 6);
}

void draw_bridge_end(Painter& paint, std::uint32_t seed) noexcept {
    detail::Mt19937 local_random(seed);
    for (std::int32_t x = 0; x <= 4; ++x) {
        for (std::int32_t y = 3; y <= 4; ++y) {
            const std::int32_t end = static_cast<std::int32_t>(
                local_random.next_u32() & 7U);
            paint.solid(x, y, 0, x, y, end);
        }
    }
    paint.solid(0, 5, 0, 0, 5,
                static_cast<std::int32_t>(local_random.next_u32() & 7U));
    paint.solid(4, 5, 0, 4, 5,
                static_cast<std::int32_t>(local_random.next_u32() & 7U));
    for (std::int32_t x = 0; x <= 4; ++x) {
        paint.solid(x, 2, 0, x, 2, static_cast<std::int32_t>(
            local_random.next_u32() % 5U));
    }
    for (std::int32_t x = 0; x <= 4; ++x) {
        for (std::int32_t y = 0; y <= 1; ++y) {
            paint.solid(x, y, 0, x, y, static_cast<std::int32_t>(
                local_random.next_u32() % 3U));
        }
    }
}

void draw_castle_entrance(Painter& paint) noexcept {
    paint.solid(0, 3, 0, 12, 4, 12);
    paint.air(0, 5, 0, 12, 13, 12);
    paint.solid(0, 5, 0, 1, 12, 12);
    paint.solid(11, 5, 0, 12, 12, 12);
    paint.solid(2, 5, 11, 4, 12, 12);
    paint.solid(8, 5, 11, 10, 12, 12);
    paint.solid(5, 9, 11, 7, 12, 12);
    paint.solid(2, 5, 0, 4, 12, 1);
    paint.solid(8, 5, 0, 10, 12, 1);
    paint.solid(5, 9, 0, 7, 12, 1);
    paint.solid(2, 11, 2, 10, 12, 10);
    paint.solid(5, 8, 0, 7, 8, 0, block::nether_brick_fence);
    for (std::int32_t step = 1; step <= 11; step += 2) {
        paint.solid(step, 10, 0, step, 11, 0, block::nether_brick_fence);
        paint.solid(step, 10, 12, step, 11, 12, block::nether_brick_fence);
        paint.solid(0, 10, step, 0, 11, step, block::nether_brick_fence);
        paint.solid(12, 10, step, 12, 11, step, block::nether_brick_fence);
        paint.tile(block::nether_brick, step, 13, 0);
        paint.tile(block::nether_brick, step, 13, 12);
        paint.tile(block::nether_brick, 0, 13, step);
        paint.tile(block::nether_brick, 12, 13, step);
        paint.tile(block::nether_brick_fence, step + 1, 13, 0);
        paint.tile(block::nether_brick_fence, step + 1, 13, 12);
        paint.tile(block::nether_brick_fence, 0, 13, step + 1);
        paint.tile(block::nether_brick_fence, 12, 13, step + 1);
    }
    paint.tile(block::nether_brick_fence, 0, 13, 0);
    paint.tile(block::nether_brick_fence, 0, 13, 12);
    paint.tile(block::nether_brick_fence, 12, 13, 0);
    for (std::int32_t step = 3; step <= 9; step += 2) {
        paint.solid(1, 7, step, 1, 8, step, block::nether_brick_fence);
        paint.solid(11, 7, step, 11, 8, step, block::nether_brick_fence);
    }
    paint.solid(4, 2, 0, 8, 2, 12);
    paint.solid(0, 2, 4, 12, 2, 8);
    paint.solid(4, 0, 0, 8, 1, 3);
    paint.solid(4, 0, 9, 8, 1, 12);
    paint.solid(0, 0, 4, 3, 1, 8);
    paint.solid(9, 0, 4, 12, 1, 8);
    for (std::int32_t x = 4; x <= 8; ++x) {
        for (std::int32_t z = 0; z <= 2; ++z) {
            paint.support(x, -1, z);
            paint.support(x, -1, 12 - z);
        }
    }
    for (std::int32_t x = 0; x <= 2; ++x) {
        for (std::int32_t z = 4; z <= 8; ++z) {
            paint.support(x, -1, z);
            paint.support(12 - x, -1, z);
        }
    }
    paint.solid(5, 5, 5, 7, 5, 7);
    paint.air(6, 1, 6, 6, 4, 6);
    paint.tile(block::nether_brick, 6, 0, 6);
    paint.tile(block::flowing_lava, 6, 5, 6);
}

void draw_nether_wart_farm(Painter& paint, Orientation orientation) noexcept {
    paint.solid(0, 3, 0, 12, 4, 12);
    paint.air(0, 5, 0, 12, 13, 12);
    paint.solid(0, 5, 0, 1, 12, 12);
    paint.solid(11, 5, 0, 12, 12, 12);
    paint.solid(2, 5, 11, 4, 12, 12);
    paint.solid(8, 5, 11, 10, 12, 12);
    paint.solid(5, 9, 11, 7, 12, 12);
    paint.solid(2, 5, 0, 4, 12, 1);
    paint.solid(8, 5, 0, 10, 12, 1);
    paint.solid(5, 9, 0, 7, 12, 1);
    paint.solid(2, 11, 2, 10, 12, 10);
    for (std::int32_t step = 1; step <= 11; step += 2) {
        paint.solid(step, 10, 0, step, 11, 0, block::nether_brick_fence);
        paint.solid(step, 10, 12, step, 11, 12, block::nether_brick_fence);
        paint.solid(0, 10, step, 0, 11, step, block::nether_brick_fence);
        paint.solid(12, 10, step, 12, 11, step, block::nether_brick_fence);
        paint.tile(block::nether_brick, step, 13, 0);
        paint.tile(block::nether_brick, step, 13, 12);
        paint.tile(block::nether_brick, 0, 13, step);
        paint.tile(block::nether_brick, 12, 13, step);
        paint.tile(block::nether_brick_fence, step + 1, 13, 0);
        paint.tile(block::nether_brick_fence, step + 1, 13, 12);
        paint.tile(block::nether_brick_fence, 0, 13, step + 1);
        paint.tile(block::nether_brick_fence, 12, 13, step + 1);
    }
    paint.tile(block::nether_brick_fence, 0, 13, 0);
    paint.tile(block::nether_brick_fence, 0, 13, 12);
    paint.tile(block::nether_brick_fence, 12, 13, 0);
    for (std::int32_t step = 3; step <= 9; step += 2) {
        paint.solid(1, 7, step, 1, 8, step, block::nether_brick_fence);
        paint.solid(11, 7, step, 11, 8, step, block::nether_brick_fence);
    }

    const std::uint8_t climb_stair = stair_data(orientation, 3);
    for (std::int32_t step = 0; step <= 6; ++step) {
        const std::int32_t z = step + 4;
        for (std::int32_t x = 5; x <= 7; ++x) {
            paint.tile(block::nether_brick_stairs, x, 5 + step, z, climb_stair);
        }
        if (z >= 5 && z <= 8) {
            paint.solid(5, 5, z, 7, step + 4, z);
        } else if (z >= 9 && z <= 10) {
            paint.solid(5, 8, z, 7, step + 4, z);
        }
        if (step >= 1) {
            paint.air(5, 6 + step, z, 7, 9 + step, z);
        }
    }
    for (std::int32_t x = 5; x <= 7; ++x) {
        paint.tile(block::nether_brick_stairs, x, 12, 11, climb_stair);
    }
    paint.solid(5, 6, 7, 5, 7, 7, block::nether_brick_fence);
    paint.solid(7, 6, 7, 7, 7, 7, block::nether_brick_fence);
    paint.air(5, 13, 12, 7, 13, 12);
    paint.solid(2, 5, 2, 3, 5, 3);
    paint.solid(2, 5, 9, 3, 5, 10);
    paint.solid(2, 5, 4, 2, 5, 8);
    paint.solid(9, 5, 2, 10, 5, 3);
    paint.solid(9, 5, 9, 10, 5, 10);
    paint.solid(10, 5, 4, 10, 5, 8);
    const std::uint8_t east_stair = stair_data(orientation, 0);
    const std::uint8_t west_stair = stair_data(orientation, 1);
    for (const std::int32_t z : {2, 3, 9, 10}) {
        paint.tile(block::nether_brick_stairs, 4, 5, z, west_stair);
        paint.tile(block::nether_brick_stairs, 8, 5, z, east_stair);
    }
    paint.solid(3, 4, 4, 4, 4, 8, block::soul_sand);
    paint.solid(8, 4, 4, 9, 4, 8, block::soul_sand);
    paint.solid(3, 5, 4, 4, 5, 8, block::nether_wart);
    paint.solid(8, 5, 4, 9, 5, 8, block::nether_wart);
    paint.solid(4, 2, 0, 8, 2, 12);
    paint.solid(0, 2, 4, 12, 2, 8);
    paint.solid(4, 0, 0, 8, 1, 3);
    paint.solid(4, 0, 9, 8, 1, 12);
    paint.solid(0, 0, 4, 3, 1, 8);
    paint.solid(9, 0, 4, 12, 1, 8);
    for (std::int32_t x = 4; x <= 8; ++x) {
        for (std::int32_t z = 0; z <= 2; ++z) {
            paint.support(x, -1, z);
            paint.support(x, -1, 12 - z);
        }
    }
    for (std::int32_t x = 0; x <= 2; ++x) {
        for (std::int32_t z = 4; z <= 8; ++z) {
            paint.support(x, -1, z);
            paint.support(12 - x, -1, z);
        }
    }
}

[[nodiscard]] bool prepare_start_random(
    detail::Mt19937& random,
    std::uint32_t world_seed,
    std::int32_t chunk_x,
    std::int32_t chunk_z) noexcept {
    // NetherBridgeFeature::isFeatureChunk reseeds the very Random later
    // handed to createStructureStart. Leaving it at this exact post-locator
    // state is essential: the fortress grammar must not start from MapGen's
    // outer coordinate stream.
    const std::int32_t region_x = chunk_x >> 4;
    const std::int32_t region_z = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(chunk_z) & ~15U);
    random.seed(static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(region_x)
        ^ static_cast<std::uint32_t>(region_z) ^ world_seed));
    (void)random.next_u32();
    if (random.next_bounded(3U) != 0U) {
        return false;
    }
    const std::int32_t candidate_x = add(
        add(detail::wrapping_mul(region_x, 16), 4),
        static_cast<std::int32_t>(random.next_u32() & 7U));
    const std::int32_t candidate_z = add(
        add(region_z, 4), static_cast<std::int32_t>(random.next_u32() & 7U));
    return chunk_x == candidate_x && chunk_z == candidate_z;
}

} // namespace

bool is_start_chunk(
    std::uint32_t world_seed,
    std::int32_t chunk_x,
    std::int32_t chunk_z) noexcept {
    detail::Mt19937 random;
    return prepare_start_random(random, world_seed, chunk_x, chunk_z);
}

Build::Build(
    detail::Mt19937& random,
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    using K = PieceKind;
    bridge_weights_ = {
        {K::bridge_straight, 30, 0, 0, true},
        {K::bridge_crossing, 10, 0, 4, false},
        {K::large_crossing, 10, 0, 4, false},
        {K::bridge_stairs, 10, 0, 3, false},
        {K::blaze_spawner, 5, 0, 2, false},
        {K::castle_entrance, 5, 0, 1, false},
    };
    castle_weights_ = {
        {K::small_corridor, 25, 0, 0, true},
        {K::small_corridor_crossing, 15, 0, 5, false},
        {K::small_corridor_right_turn, 5, 0, 10, false},
        {K::small_corridor_left_turn, 5, 0, 10, false},
        {K::stairs_corridor, 10, 0, 3, true},
        {K::corridor_balcony, 7, 0, 2, false},
        {K::nether_wart_farm, 5, 0, 2, false},
    };

    const std::int32_t block_x = add(detail::wrapping_mul(chunk_x, 16), 2);
    const std::int32_t block_z = add(detail::wrapping_mul(chunk_z, 16), 2);
    pieces_.push_back({
        K::bridge_crossing,
        {block_x, 64, block_z, add(block_x, 18), 73, add(block_z, 18)},
        orientation_from_word(random.next_u32()),
        0,
        0,
    });
    pending_.push_back(0);

    // NBStartPiece stores its child list in insertion order. Removing the
    // random element shifts later entries, rather than swapping it out.
    while (!pending_.empty()) {
        const std::size_t selected = static_cast<std::size_t>(
            random.next_u32() % static_cast<std::uint32_t>(pending_.size()));
        const std::size_t child = pending_[selected];
        pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(selected));
        add_children(child, random);
    }

    calculate_bounds();
    const std::int32_t available = sub(sub(70, 48), sub(bounds_.max_y, bounds_.min_y));
    std::int32_t target_min_y = 48;
    if (available > 1) {
        target_min_y = add(
            target_min_y,
            static_cast<std::int32_t>(
                random.next_u32() % static_cast<std::uint32_t>(available)));
    }
    const std::int32_t shift = sub(target_min_y, bounds_.min_y);
    for (Piece& piece : pieces_) {
        piece.bounds.translate(0, shift, 0);
    }
    bounds_.translate(0, shift, 0);
}

const std::vector<Build::Piece>& Build::pieces() const noexcept {
    return pieces_;
}

const BoundingBox& Build::bounds() const noexcept {
    return bounds_;
}

Build::Piece* Build::append_piece(
    detail::Mt19937& random,
    PieceKind kind,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    Orientation orientation,
    std::int32_t depth) {
    const BoundingBox candidate = piece_bounds(kind, x, y, z, orientation);
    // Every Nether fortress factory checks StructurePiece::isAboveGround
    // before testing its inclusive collision box.
    if (candidate.min_y <= 10 || collides(candidate)) {
        return nullptr;
    }
    const std::uint32_t local_seed = kind == PieceKind::bridge_end
        ? random.next_u32() >> 1U
        : 0U;
    const bool chest_pending = (kind == PieceKind::small_corridor_right_turn
            || kind == PieceKind::small_corridor_left_turn)
        && random.next_bounded(3U) == 0U;
    pieces_.push_back({
        kind, candidate, orientation, depth, local_seed, chest_pending});
    return &pieces_.back();
}

Build::Piece* Build::append_end(
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    Orientation orientation,
    std::int32_t depth) {
    return append_piece(
        random, PieceKind::bridge_end, x, y, z, orientation, depth);
}

Build::Piece* Build::append_candidate(
    detail::Mt19937& random,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    Orientation orientation,
    std::int32_t source_depth,
    bool castle_piece) {
    const Piece& root = pieces_.front();
    const std::int32_t depth = add(source_depth, 1);
    if (native_abs(sub(x, root.bounds.min_x)) >= 113
        || native_abs(sub(z, root.bounds.min_z)) >= 113) {
        return append_end(random, x, y, z, orientation, depth);
    }

    std::vector<Weight>& weights = castle_piece ? castle_weights_ : bridge_weights_;
    bool has_limited_weight{};
    std::int32_t total_weight{};
    for (const Weight& weight : weights) {
        total_weight = add(total_weight, weight.probability);
        has_limited_weight |= weight.limit > 0 && weight.generated < weight.limit;
    }
    if (!has_limited_weight || total_weight <= 0 || depth >= 31) {
        return append_end(random, x, y, z, orientation, depth);
    }

    for (std::int32_t attempt = 0; attempt < 5; ++attempt) {
        std::int32_t selected = static_cast<std::int32_t>(
            random.next_u32() % static_cast<std::uint32_t>(total_weight));
        for (std::size_t index = 0; index < weights.size(); ++index) {
            Weight& weight = weights[index];
            selected = sub(selected, weight.probability);
            if (selected >= 0) {
                continue;
            }
            if ((weight.limit != 0 && weight.generated >= weight.limit)
                || (previous_weight_ == static_cast<std::int32_t>(weight.kind)
                    && !weight.allow_consecutive)) {
                break;
            }
            if (Piece* const piece = append_piece(
                    random, weight.kind, x, y, z, orientation, depth)) {
                ++weight.generated;
                previous_weight_ = static_cast<std::int32_t>(weight.kind);
                if (weight.limit != 0 && weight.generated >= weight.limit) {
                    weights.erase(weights.begin() + static_cast<std::ptrdiff_t>(index));
                }
                pending_.push_back(pieces_.size() - 1U);
                return piece;
            }
            break;
        }
    }
    return append_end(random, x, y, z, orientation, depth);
}

void Build::forward(
    std::size_t parent_index,
    detail::Mt19937& random,
    std::int32_t offset,
    std::int32_t y_offset,
    bool castle_piece) {
    const Piece parent = pieces_[parent_index];
    const BoundingBox& box = parent.bounds;
    switch (parent.orientation) {
    case Orientation::north:
        (void)append_candidate(random, add(box.min_x, offset), add(box.min_y, y_offset),
            sub(box.min_z, 1), parent.orientation, parent.depth, castle_piece);
        break;
    case Orientation::south:
        (void)append_candidate(random, add(box.min_x, offset), add(box.min_y, y_offset),
            add(box.max_z, 1), parent.orientation, parent.depth, castle_piece);
        break;
    case Orientation::west:
        (void)append_candidate(random, sub(box.min_x, 1), add(box.min_y, y_offset),
            add(box.min_z, offset), parent.orientation, parent.depth, castle_piece);
        break;
    case Orientation::east:
        (void)append_candidate(random, add(box.max_x, 1), add(box.min_y, y_offset),
            add(box.min_z, offset), parent.orientation, parent.depth, castle_piece);
        break;
    case Orientation::none:
        break;
    }
}

void Build::left(
    std::size_t parent_index,
    detail::Mt19937& random,
    std::int32_t y_offset,
    std::int32_t offset,
    bool castle_piece) {
    const Piece parent = pieces_[parent_index];
    const BoundingBox& box = parent.bounds;
    switch (parent.orientation) {
    case Orientation::north:
    case Orientation::south:
        (void)append_candidate(random, sub(box.min_x, 1), add(box.min_y, y_offset),
            add(box.min_z, offset), Orientation::west, parent.depth, castle_piece);
        break;
    case Orientation::west:
    case Orientation::east:
        (void)append_candidate(random, add(box.min_x, offset), add(box.min_y, y_offset),
            sub(box.min_z, 1), Orientation::north, parent.depth, castle_piece);
        break;
    case Orientation::none:
        break;
    }
}

void Build::right(
    std::size_t parent_index,
    detail::Mt19937& random,
    std::int32_t y_offset,
    std::int32_t offset,
    bool castle_piece) {
    const Piece parent = pieces_[parent_index];
    const BoundingBox& box = parent.bounds;
    switch (parent.orientation) {
    case Orientation::north:
    case Orientation::south:
        (void)append_candidate(random, add(box.max_x, 1), add(box.min_y, y_offset),
            add(box.min_z, offset), Orientation::east, parent.depth, castle_piece);
        break;
    case Orientation::west:
    case Orientation::east:
        (void)append_candidate(random, add(box.min_x, offset), add(box.min_y, y_offset),
            add(box.max_z, 1), Orientation::south, parent.depth, castle_piece);
        break;
    case Orientation::none:
        break;
    }
}

void Build::add_children(std::size_t piece_index, detail::Mt19937& random) {
    const PieceKind kind = pieces_[piece_index].kind;
    using K = PieceKind;
    switch (kind) {
    case K::bridge_crossing:
        forward(piece_index, random, 8, 3, false);
        left(piece_index, random, 3, 8, false);
        right(piece_index, random, 3, 8, false);
        break;
    case K::bridge_straight:
        forward(piece_index, random, 1, 3, false);
        break;
    case K::large_crossing:
        forward(piece_index, random, 2, 0, false);
        left(piece_index, random, 0, 2, false);
        right(piece_index, random, 0, 2, false);
        break;
    case K::bridge_stairs:
        right(piece_index, random, 6, 2, false);
        break;
    case K::castle_entrance:
        forward(piece_index, random, 5, 3, true);
        break;
    case K::small_corridor:
    case K::stairs_corridor:
        forward(piece_index, random, 1, 0, true);
        break;
    case K::small_corridor_crossing:
        forward(piece_index, random, 1, 0, true);
        left(piece_index, random, 0, 1, true);
        right(piece_index, random, 0, 1, true);
        break;
    case K::small_corridor_right_turn:
        right(piece_index, random, 0, 1, true);
        break;
    case K::small_corridor_left_turn:
        left(piece_index, random, 0, 1, true);
        break;
    case K::corridor_balcony: {
        const Orientation facing = pieces_[piece_index].orientation;
        const std::int32_t offset = (facing == Orientation::west
            || facing == Orientation::north) ? 5 : 1;
        left(piece_index, random, 0, offset, random.next_u32() % 8U != 0U);
        right(piece_index, random, 0, offset, random.next_u32() % 8U != 0U);
        break;
    }
    case K::nether_wart_farm:
        forward(piece_index, random, 5, 3, true);
        forward(piece_index, random, 5, 11, true);
        break;
    case K::blaze_spawner:
    case K::bridge_end:
        break;
    }
}

bool Build::collides(const BoundingBox& candidate) const noexcept {
    return std::any_of(pieces_.begin(), pieces_.end(), [&](const Piece& piece) {
        return piece.bounds.overlaps(candidate);
    });
}

void Build::calculate_bounds() noexcept {
    bounds_ = {
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::min() + 1,
        std::numeric_limits<std::int32_t>::min() + 1,
        std::numeric_limits<std::int32_t>::min() + 1,
    };
    for (const Piece& piece : pieces_) {
        bounds_.min_x = std::min(bounds_.min_x, piece.bounds.min_x);
        bounds_.min_y = std::min(bounds_.min_y, piece.bounds.min_y);
        bounds_.min_z = std::min(bounds_.min_z, piece.bounds.min_z);
        bounds_.max_x = std::max(bounds_.max_x, piece.bounds.max_x);
        bounds_.max_y = std::max(bounds_.max_y, piece.bounds.max_y);
        bounds_.max_z = std::max(bounds_.max_z, piece.bounds.max_z);
    }
}

void apply_to_chunk(
    const Build& build,
    Chunk& chunk,
    detail::Mt19937& structure_random,
    std::vector<BlockPosition>* lava_sources,
    std::vector<BlockPosition>* blaze_spawners,
    std::vector<GeneratedChest>* chests) noexcept {
    const std::int32_t min_x = detail::wrapping_mul(chunk.x, Chunk::width);
    const std::int32_t min_z = detail::wrapping_mul(chunk.z, Chunk::width);
    const BoundingBox active_chunk{
        min_x, 1, min_z,
        add(min_x, Chunk::width - 1), 512, add(min_z, Chunk::width - 1),
    };
    RawChunkAccess access(chunk);
    for (const Build::Piece& piece : build.pieces()) {
        if (!piece.bounds.overlaps(active_chunk)) {
            continue;
        }
        Painter paint{
            access, piece, active_chunk, structure_random, lava_sources,
            blaze_spawners, chests};
        switch (piece.kind) {
        case Build::PieceKind::bridge_crossing:
            draw_bridge_crossing(paint);
            break;
        case Build::PieceKind::bridge_straight:
            draw_straight_bridge(paint);
            break;
        case Build::PieceKind::large_crossing:
            draw_large_crossing(paint);
            break;
        case Build::PieceKind::bridge_stairs:
            draw_bridge_stairs(paint);
            break;
        case Build::PieceKind::blaze_spawner:
            draw_blaze_spawner(paint);
            break;
        case Build::PieceKind::castle_entrance:
            draw_castle_entrance(paint);
            break;
        case Build::PieceKind::small_corridor:
            draw_small_corridor(paint);
            break;
        case Build::PieceKind::small_corridor_crossing:
            draw_small_crossing(paint);
            break;
        case Build::PieceKind::small_corridor_right_turn:
            draw_right_turn(paint);
            if (piece.chest_pending) {
                paint.chest(1, 2, 3);
            }
            break;
        case Build::PieceKind::small_corridor_left_turn:
            draw_left_turn(paint);
            if (piece.chest_pending) {
                paint.chest(3, 2, 3);
            }
            break;
        case Build::PieceKind::stairs_corridor:
            draw_stairs_corridor(paint, piece.orientation);
            break;
        case Build::PieceKind::corridor_balcony:
            draw_corridor_balcony(paint);
            break;
        case Build::PieceKind::nether_wart_farm:
            draw_nether_wart_farm(paint, piece.orientation);
            break;
        case Build::PieceKind::bridge_end:
            draw_bridge_end(paint, piece.local_seed);
            break;
        }
    }
}

void apply_nearby_starts(
    std::uint32_t world_seed,
    Chunk& chunk,
    detail::Mt19937& population_random,
    std::vector<BlockPosition>* lava_sources,
    std::vector<BlockPosition>* blaze_spawners,
    std::vector<GeneratedChest>* chests) noexcept {
    // MapGenStructure's fixed influence radius is measured in chunks. Keep
    // the source-order traversal (X outer, Z inner) even though each start's
    // construction stream is independent; it becomes observable if two
    // starts overlap this active chunk.
    constexpr std::int32_t radius = 8;
    const std::int32_t first_x = sub(chunk.x, radius);
    const std::int32_t last_x = add(chunk.x, radius);
    const std::int32_t first_z = sub(chunk.z, radius);
    const std::int32_t last_z = add(chunk.z, radius);
    // StructureFeature::postProcess receives HellRandomLevelSource's
    // already chunk-seeded population stream. A newly-created fortress chest
    // therefore advances the same MT observed by the ordinary feature pass.
    for (std::int32_t source_x = first_x;; source_x = add(source_x, 1)) {
        for (std::int32_t source_z = first_z;; source_z = add(source_z, 1)) {
            detail::Mt19937 start_random;
            if (prepare_start_random(
                    start_random, world_seed, source_x, source_z)) {
                Build start(start_random, source_x, source_z);
                apply_to_chunk(
                    start, chunk, population_random, lava_sources,
                    blaze_spawners, chests);
            }
            if (source_z == last_z) {
                break;
            }
        }
        if (source_x == last_x) {
            break;
        }
    }
}

} // namespace mcpe::worldgen::v1_1_5_0::hell_fortress
