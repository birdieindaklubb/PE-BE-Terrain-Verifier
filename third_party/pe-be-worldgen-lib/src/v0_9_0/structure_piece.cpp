#include "v0_9_0/structure_piece.hpp"

#include "detail/wrap.hpp"
#include "v0_9_0/blocks.hpp"

namespace mcpe::worldgen::v0_9_0::structure {

bool BoundingBox::contains(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) const noexcept {
    return min_x <= x && x <= max_x
        && min_y <= y && y <= max_y
        && min_z <= z && z <= max_z;
}

bool BoundingBox::overlaps(const BoundingBox& other) const noexcept {
    // StructurePiece::findCollisionPiece uses this inclusive comparison; a
    // touching face therefore counts as a collision.
    return min_x <= other.max_x && max_x >= other.min_x
        && min_y <= other.max_y && max_y >= other.min_y
        && min_z <= other.max_z && max_z >= other.min_z;
}

bool BoundingBox::intersects_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) const noexcept {
    const std::int32_t min_chunk_x = detail::wrapping_mul(chunk_x, Chunk::width);
    const std::int32_t min_chunk_z = detail::wrapping_mul(chunk_z, Chunk::width);
    const std::int32_t max_chunk_x = detail::wrapping_add(min_chunk_x, Chunk::width);
    const std::int32_t max_chunk_z = detail::wrapping_add(min_chunk_z, Chunk::width);
    return min_x <= max_chunk_x && max_x >= min_chunk_x
        && min_z <= max_chunk_z && max_z >= min_chunk_z;
}

void BoundingBox::translate(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    min_x = detail::wrapping_add(min_x, x);
    min_y = detail::wrapping_add(min_y, y);
    min_z = detail::wrapping_add(min_z, z);
    max_x = detail::wrapping_add(max_x, x);
    max_y = detail::wrapping_add(max_y, y);
    max_z = detail::wrapping_add(max_z, z);
}

BoundingBox oriented_box(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    Orientation orientation,
    std::int32_t side_before,
    std::int32_t side_after,
    std::int32_t forward_length,
    std::int32_t top_offset,
    std::int32_t bottom_offset) noexcept {
    // StructurePiece::getComponentToAddBoundingBox. Every rotated native
    // structure factory delegates to this endpoint convention.
    switch (orientation) {
    case Orientation::south:
        return {
            detail::wrapping_sub(x, side_before),
            detail::wrapping_add(y, bottom_offset), z,
            detail::wrapping_add(x, side_after),
            detail::wrapping_add(y, top_offset),
            detail::wrapping_add(z, forward_length),
        };
    case Orientation::west:
        return {
            detail::wrapping_sub(x, forward_length),
            detail::wrapping_add(y, bottom_offset),
            detail::wrapping_sub(z, side_before), x,
            detail::wrapping_add(y, top_offset),
            detail::wrapping_add(z, side_after),
        };
    case Orientation::north:
        return {
            detail::wrapping_sub(x, side_before),
            detail::wrapping_add(y, bottom_offset),
            detail::wrapping_sub(z, forward_length),
            detail::wrapping_add(x, side_after),
            detail::wrapping_add(y, top_offset), z,
        };
    case Orientation::east:
        return {
            x, detail::wrapping_add(y, bottom_offset),
            detail::wrapping_sub(z, side_before),
            detail::wrapping_add(x, forward_length),
            detail::wrapping_add(y, top_offset),
            detail::wrapping_add(z, side_after),
        };
    case Orientation::none:
        return {};
    }
    return {};
}

std::uint8_t chest_orientation_data(
    Orientation orientation,
    std::uint8_t data) noexcept {
    return data == 1U
            && (orientation == Orientation::west || orientation == Orientation::east)
        ? 2U
        : data;
}

PiecePlacement::PiecePlacement(
    BoundingBox bounds,
    Orientation orientation) noexcept
    : bounds_(bounds), orientation_(orientation) {
}

std::int32_t PiecePlacement::world_x(
    std::int32_t local_x,
    std::int32_t local_z) const noexcept {
    // StructurePiece::getWorldX,.
    switch (orientation_) {
    case Orientation::south:
    case Orientation::north:
        return detail::wrapping_add(bounds_.min_x, local_x);
    case Orientation::west:
        return detail::wrapping_sub(bounds_.max_x, local_z);
    case Orientation::east:
        return detail::wrapping_add(bounds_.min_x, local_z);
    case Orientation::none:
        return local_x;
    }
    return local_x;
}

std::int32_t PiecePlacement::world_y(std::int32_t local_y) const noexcept {
    // StructurePiece::getWorldY,.
    return orientation_ == Orientation::none
        ? local_y
        : detail::wrapping_add(bounds_.min_y, local_y);
}

std::int32_t PiecePlacement::world_z(
    std::int32_t local_x,
    std::int32_t local_z) const noexcept {
    // StructurePiece::getWorldZ,.
    switch (orientation_) {
    case Orientation::south:
        return detail::wrapping_add(bounds_.min_z, local_z);
    case Orientation::west:
    case Orientation::east:
        return detail::wrapping_add(bounds_.min_z, local_x);
    case Orientation::north:
        return detail::wrapping_sub(bounds_.max_z, local_z);
    case Orientation::none:
        return local_z;
    }
    return local_z;
}

LocalPosition PiecePlacement::world_position(
    std::int32_t local_x,
    std::int32_t local_y,
    std::int32_t local_z) const noexcept {
    return {
        world_x(local_x, local_z),
        world_y(local_y),
        world_z(local_x, local_z),
    };
}

void PiecePlacement::place_block(
    detail::BlockAccess& world,
    const BoundingBox& active_chunk,
    std::uint8_t id,
    std::uint8_t data,
    std::int32_t local_x,
    std::int32_t local_y,
    std::int32_t local_z) const noexcept {
    // StructurePiece::placeBlock,. The active region is a
    // 16-by-16 chunk box rather than the component's own bounds.
    const LocalPosition position = world_position(local_x, local_y, local_z);
    if (active_chunk.contains(position.x, position.y, position.z)) {
        (void)world.set_block_and_data(
            position.x, position.y, position.z, id, data, true);
    }
}

std::uint8_t PiecePlacement::block_at(
    const detail::BlockAccess& world,
    const BoundingBox& active_chunk,
    std::int32_t local_x,
    std::int32_t local_y,
    std::int32_t local_z) const noexcept {
    const LocalPosition position = world_position(local_x, local_y, local_z);
    return active_chunk.contains(position.x, position.y, position.z)
        ? world.block(position.x, position.y, position.z)
        : block::air;
}

bool PiecePlacement::edges_liquid(
    const detail::BlockAccess& world,
    const BoundingBox& active_chunk) const noexcept {
    // StructurePiece::edgesLiquid,. It tests only the six faces
    // of the one-block-expanded component box, clipped to the active chunk.
    const std::int32_t min_x = std::max(
        detail::wrapping_sub(bounds_.min_x, 1), active_chunk.min_x);
    const std::int32_t max_x = std::min(
        detail::wrapping_add(bounds_.max_x, 1), active_chunk.max_x);
    const std::int32_t min_y = std::max(
        detail::wrapping_sub(bounds_.min_y, 1), active_chunk.min_y);
    const std::int32_t max_y = std::min(
        detail::wrapping_add(bounds_.max_y, 1), active_chunk.max_y);
    const std::int32_t min_z = std::max(
        detail::wrapping_sub(bounds_.min_z, 1), active_chunk.min_z);
    const std::int32_t max_z = std::min(
        detail::wrapping_add(bounds_.max_z, 1), active_chunk.max_z);

    for (std::int32_t x = min_x; x <= max_x; ++x) {
        for (std::int32_t z = min_z; z <= max_z; ++z) {
            if (block::is_liquid(world.block(x, min_y, z))
                || block::is_liquid(world.block(x, max_y, z))) {
                return true;
            }
        }
    }
    for (std::int32_t x = min_x; x <= max_x; ++x) {
        for (std::int32_t y = min_y; y <= max_y; ++y) {
            if (block::is_liquid(world.block(x, y, min_z))
                || block::is_liquid(world.block(x, y, max_z))) {
                return true;
            }
        }
    }
    for (std::int32_t z = min_z; z <= max_z; ++z) {
        for (std::int32_t y = min_y; y <= max_y; ++y) {
            if (block::is_liquid(world.block(min_x, y, z))
                || block::is_liquid(world.block(max_x, y, z))) {
                return true;
            }
        }
    }
    return false;
}

void PiecePlacement::generate_box(
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
    bool only_existing) const noexcept {
    // StructurePiece::generateBox,. The native order is y, x, z;
    // notably it consumes no random words and writes metadata zero.
    for (std::int32_t y = min_y; y <= max_y; ++y) {
        for (std::int32_t x = min_x; x <= max_x; ++x) {
            for (std::int32_t z = min_z; z <= max_z; ++z) {
                if (only_existing && block_at(world, active_chunk, x, y, z) == block::air) {
                    continue;
                }
                const bool boundary = x == min_x || x == max_x
                    || y == min_y || y == max_y || z == min_z || z == max_z;
                place_block(
                    world, active_chunk, boundary ? boundary_id : inside_id,
                    0, x, y, z);
            }
        }
    }
}

void PiecePlacement::generate_air_box(
    detail::BlockAccess& world,
    const BoundingBox& active_chunk,
    std::int32_t min_x,
    std::int32_t min_y,
    std::int32_t min_z,
    std::int32_t max_x,
    std::int32_t max_y,
    std::int32_t max_z) const noexcept {
    // StructurePiece::generateAirBox,.
    for (std::int32_t y = min_y; y <= max_y; ++y) {
        for (std::int32_t x = min_x; x <= max_x; ++x) {
            for (std::int32_t z = min_z; z <= max_z; ++z) {
                place_block(world, active_chunk, block::air, 0, x, y, z);
            }
        }
    }
}

void PiecePlacement::generate_maybe_box(
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
    bool only_existing) const noexcept {
    // StructurePiece::generateMaybeBox,. A word is consumed for
    // every local cell, including cells outside the active chunk.
    for (std::int32_t y = min_y; y <= max_y; ++y) {
        for (std::int32_t x = min_x; x <= max_x; ++x) {
            for (std::int32_t z = min_z; z <= max_z; ++z) {
                if (random.next_float() > probability
                    || (only_existing
                        && block_at(world, active_chunk, x, y, z) == block::air)) {
                    continue;
                }
                place_block(world, active_chunk, id, 0, x, y, z);
            }
        }
    }
}

void PiecePlacement::maybe_generate_block(
    detail::BlockAccess& world,
    const BoundingBox& active_chunk,
    detail::Mt19937& random,
    float probability,
    std::uint8_t id,
    std::uint8_t data,
    std::int32_t local_x,
    std::int32_t local_y,
    std::int32_t local_z) const noexcept {
    // StructurePiece::maybeGenerateBlock,. Unlike maybe-box,
    // equality does not place a tile.
    if (random.next_float() < probability) {
        place_block(world, active_chunk, id, data, local_x, local_y, local_z);
    }
}

void PiecePlacement::fill_column_down(
    detail::BlockAccess& world,
    const BoundingBox& active_chunk,
    std::uint8_t id,
    std::uint8_t data,
    std::int32_t local_x,
    std::int32_t local_y,
    std::int32_t local_z) const noexcept {
    // StructurePiece::fillColumnDown,. Material's byte at offset
    // four is its liquid flag in every generation-reachable material.
    const LocalPosition position = world_position(local_x, local_y, local_z);
    if (!active_chunk.contains(position.x, position.y, position.z)) {
        return;
    }
    for (std::int32_t y = position.y;
         y > 1 && (world.block(position.x, y, position.z) == block::air
             || block::is_liquid(world.block(position.x, y, position.z)));
         y = detail::wrapping_sub(y, 1)) {
        (void)world.set_block_and_data(position.x, y, position.z, id, data, true);
    }
    if (world.block(position.x, position.y, position.z) == block::grass) {
        (void)world.set_block_and_data(
            position.x, position.y, position.z, block::dirt, 0, true);
    }
}

void PiecePlacement::generate_air_column_up(
    detail::BlockAccess& world,
    const BoundingBox& active_chunk,
    std::int32_t local_x,
    std::int32_t local_y,
    std::int32_t local_z) const noexcept {
    // StructurePiece::generateAirColumnUp,.
    const LocalPosition position = world_position(local_x, local_y, local_z);
    if (!active_chunk.contains(position.x, position.y, position.z)) {
        return;
    }
    for (std::int32_t y = position.y;
         y < 127 && world.block(position.x, y, position.z) != block::air;
         y = detail::wrapping_add(y, 1)) {
        (void)world.set_block_and_data(position.x, y, position.z, block::air, 0, true);
    }
}

void PiecePlacement::generate_upper_half_sphere(
    detail::BlockAccess& world,
    const BoundingBox& active_chunk,
    std::int32_t min_x,
    std::int32_t min_y,
    std::int32_t min_z,
    std::int32_t max_x,
    std::int32_t max_y,
    std::int32_t max_z,
    std::uint8_t id,
    bool only_existing) const noexcept {
    // StructurePiece::generateUpperHalfSphere,. PE uses binary32
    // throughout this predicate, with the strict boundary `distance < 1.05`.
    const float x_radius = (static_cast<float>(max_x - min_x) + 1.0F) * 0.5F;
    const float y_extent = static_cast<float>(max_y - min_y) + 1.0F;
    const float z_radius = (static_cast<float>(max_z - min_z) + 1.0F) * 0.5F;
    const float x_center = static_cast<float>(min_x) + x_radius;
    const float z_center = static_cast<float>(min_z) + z_radius;

    for (std::int32_t y = min_y; y <= max_y; ++y) {
        const float normalized_y = static_cast<float>(y - min_y) / y_extent;
        const float y_squared = normalized_y * normalized_y;
        for (std::int32_t x = min_x; x <= max_x; ++x) {
            const float normalized_x = (static_cast<float>(x) - x_center) / x_radius;
            const float x_squared = normalized_x * normalized_x;
            for (std::int32_t z = min_z; z <= max_z; ++z) {
                const float normalized_z = (static_cast<float>(z) - z_center) / z_radius;
                if (x_squared + y_squared + normalized_z * normalized_z >= 1.05F) {
                    continue;
                }
                if (!only_existing || block_at(world, active_chunk, x, y, z) != block::air) {
                    place_block(world, active_chunk, id, 0, x, y, z);
                }
            }
        }
    }
}

const BoundingBox& PiecePlacement::bounds() const noexcept {
    return bounds_;
}

Orientation PiecePlacement::orientation() const noexcept {
    return orientation_;
}

void PiecePlacement::translate_y(std::int32_t offset) noexcept {
    bounds_.translate(0, offset, 0);
}

} // namespace mcpe::worldgen::v0_9_0::structure
