#include "end_terrain_source.hpp"

#include "detail/fp.hpp"
#include "detail/wrap.hpp"
#include "v1_1_5_0/blocks.hpp"
#include "v1_1_5_0/end_features.hpp"

#include <cstdint>

namespace mcpe::terrain_verifier {
namespace {

[[nodiscard]] constexpr bool outside_main_island(
    std::int32_t chunk_x,
    std::int32_t chunk_z) noexcept {
    const std::uint32_t squared_distance = worldgen::detail::bits(chunk_x)
            * worldgen::detail::bits(chunk_x)
        + worldgen::detail::bits(chunk_z)
            * worldgen::detail::bits(chunk_z);
    return squared_distance > 4096U;
}

[[nodiscard]] constexpr std::int32_t chunk_origin(
    std::int32_t chunk) noexcept {
    return worldgen::detail::wrapping_mul(chunk, worldgen::Chunk::width);
}

void split_coordinate(
    std::int32_t coordinate,
    std::int32_t& chunk,
    std::int32_t& local) noexcept {
    chunk = coordinate / worldgen::Chunk::width;
    local = coordinate % worldgen::Chunk::width;
    if (local < 0) {
        --chunk;
        local += worldgen::Chunk::width;
    }
}

class TargetChunkWriter final : public worldgen::v1_1_5_0::EndBlockWriter {
public:
    TargetChunkWriter(
        worldgen::Chunk& target,
        std::int32_t target_chunk_x,
        std::int32_t target_chunk_z) noexcept
        : target_(target),
          target_chunk_x_(target_chunk_x),
          target_chunk_z_(target_chunk_z) {
    }

    void set_end_block(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::uint8_t block,
        std::uint8_t data = 0) noexcept override {
        if (y < 0 || y >= worldgen::Chunk::height) {
            return;
        }
        std::int32_t chunk_x{};
        std::int32_t local_x{};
        std::int32_t chunk_z{};
        std::int32_t local_z{};
        split_coordinate(x, chunk_x, local_x);
        split_coordinate(z, chunk_z, local_z);
        if (chunk_x == target_chunk_x_ && chunk_z == target_chunk_z_) {
            target_.set_block_and_data(local_x, y, local_z, block, data);
        }
    }

private:
    worldgen::Chunk& target_;
    std::int32_t target_chunk_x_{};
    std::int32_t target_chunk_z_{};
};

} // namespace

EndTerrainSource::EndTerrainSource(std::uint32_t world_seed)
    : world_seed_(world_seed),
      terrain_(world_seed) {
    worldgen::detail::Mt19937 seed_random(world_seed);
    x_multiplier_ = (seed_random.next_u32() >> 1U) | 1U;
    z_multiplier_ = (seed_random.next_u32() >> 1U) | 1U;
}

void EndTerrainSource::apply_outer_island_source(
    worldgen::Chunk& target,
    std::int32_t target_chunk_x,
    std::int32_t target_chunk_z,
    std::int32_t source_chunk_x,
    std::int32_t source_chunk_z) const {
    if (!outside_main_island(source_chunk_x, source_chunk_z)) {
        return;
    }

    const std::uint32_t coordinate_term =
        worldgen::detail::bits(source_chunk_x) * x_multiplier_
        + worldgen::detail::bits(source_chunk_z) * z_multiplier_;
    worldgen::detail::Mt19937 random(world_seed_ ^ coordinate_term);

    // Preserve the native short-circuit order. Although the height query and
    // local population stream appear independent, this is an observable
    // translation and optimization must not reorder source decisions.
    if (terrain_.island_height_value(
            source_chunk_x, source_chunk_z, 1, 1) >= -20.0F
        || random.next_bounded(14U) != 0U) {
        return;
    }

    const std::int32_t x = worldgen::detail::wrapping_add(
        chunk_origin(source_chunk_x),
        static_cast<std::int32_t>(random.next_u32() & 15U) + 8);
    const std::int32_t y = static_cast<std::int32_t>(random.next_u32() & 15U) + 55;
    const std::int32_t z = worldgen::detail::wrapping_add(
        chunk_origin(source_chunk_z),
        static_cast<std::int32_t>(random.next_u32() & 15U) + 8);
    TargetChunkWriter writer(target, target_chunk_x, target_chunk_z);
    worldgen::v1_1_5_0::place_end_island(writer, random, x, y, z);
    if ((random.next_u32() & 3U) == 0U) {
        worldgen::v1_1_5_0::place_end_island(writer, random, x, y, z);
    }
}

worldgen::Chunk EndTerrainSource::generate_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) const {
    worldgen::Chunk result = terrain_.generate_base_chunk(chunk_x, chunk_z);
    for (std::int32_t source_offset_z = -1; source_offset_z <= 0;
         ++source_offset_z) {
        for (std::int32_t source_offset_x = -1; source_offset_x <= 0;
             ++source_offset_x) {
            apply_outer_island_source(
                result,
                chunk_x,
                chunk_z,
                worldgen::detail::wrapping_add(chunk_x, source_offset_x),
                worldgen::detail::wrapping_add(chunk_z, source_offset_z));
        }
    }
    return result;
}

} // namespace mcpe::terrain_verifier
