#pragma once

#include "mcpe/worldgen/chunk.hpp"
#include "mcpe/worldgen/generated_content.hpp"
#include "mcpe/worldgen/mob.hpp"
#include "detail/liquid_access.hpp"
#include "v1_1_5_0/hell_caves.hpp"
#include "v1_1_5_0/hell_terrain.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace mcpe::worldgen::v1_1_5_0 {

// The PE 1.1.5.0 HellDimension source. Its base-chunk stage includes the
// native terrain and cave pass; post-process owns structures and feature
// writes through the same small TileSource surface as immediate liquids.
class HellRandomLevelSource final : private detail::LiquidAccess {
public:
    explicit HellRandomLevelSource(std::uint32_t world_seed);

    [[nodiscard]] Chunk generate_base_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z);
    void load_base_chunk(std::int32_t chunk_x, std::int32_t chunk_z);
    void populate_chunk(std::int32_t chunk_x, std::int32_t chunk_z);

    [[nodiscard]] const Chunk* find_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z) const noexcept;
    [[nodiscard]] bool is_populated(
        std::int32_t chunk_x,
        std::int32_t chunk_z) const noexcept;

    [[nodiscard]] std::span<const GeneratedMob> generated_mobs() const noexcept;
    [[nodiscard]] std::span<const GeneratedChest> generated_chests() const noexcept;
    [[nodiscard]] std::span<const GeneratedSpawner> generated_spawners() const noexcept;
    [[nodiscard]] std::span<const GeneratedVillager> generated_villagers()
        const noexcept;

private:
    using ChunkKey = std::pair<std::int32_t, std::int32_t>;

    [[nodiscard]] std::uint8_t block(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept override;
    [[nodiscard]] std::uint8_t data(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept override;
    bool set_block_and_data(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::uint8_t id,
        std::uint8_t metadata,
        bool normal_write) noexcept override;
    [[nodiscard]] bool has_chunks_at(
        std::int32_t x,
        std::int32_t z,
        std::int32_t radius) const noexcept override;

    void post_process(std::int32_t chunk_x, std::int32_t chunk_z);
    void ensure_population_neighborhood(
        std::int32_t chunk_x,
        std::int32_t chunk_z);
    void recalculate_modified_chunks() noexcept;
    static void split_coordinate(
        std::int32_t coordinate,
        std::int32_t& chunk,
        std::int32_t& local) noexcept;

    std::uint32_t world_seed_{};
    HellTerrain terrain_;
    HellCaveCarver cave_carver_;
    std::map<ChunkKey, Chunk> chunks_;
    std::set<ChunkKey> loaded_chunks_;
    std::set<ChunkKey> populated_chunks_;
    std::set<ChunkKey> modified_chunks_;
    std::vector<GeneratedChest> chests_;
    std::vector<GeneratedSpawner> spawners_;
};

} // namespace mcpe::worldgen::v1_1_5_0
