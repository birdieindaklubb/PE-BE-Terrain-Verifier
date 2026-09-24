#pragma once

#include "mcpe/worldgen/chunk.hpp"
#include "mcpe/worldgen/generated_content.hpp"
#include "mcpe/worldgen/terrain_stage.hpp"
#include "detail/liquid_access.hpp"
#include "v0_9_0/terrain_generator.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace mcpe::worldgen::v0_9_0 {

class World;

namespace lighting {
void update_lights_and_heights(
    World& world,
    std::int32_t chunk_x,
    std::int32_t chunk_z);
}

namespace liquid {
void settle_chunk_boundary_water(
    World& world,
    std::int32_t block_x,
    std::int32_t block_z) noexcept;
}

// The generation-facing subset of TileSource/LevelChunk.  It deliberately
// models storage and height/rain caches only. Feature callbacks remain outside
// this abstraction; the generation-specific deferred-fluid pass reproduces
// its own liquid writes, scheduled ticks, and neighbor effects locally.
class World final : public detail::LiquidAccess {
public:
    using ChunkKey = std::pair<std::int32_t, std::int32_t>;

    World(std::uint32_t world_seed, std::int32_t generator_type_code);

    // These expose the two deterministic RandomLevelSource inputs while
    // keeping World responsible only for loaded-chunk storage. They do not
    // insert a chunk, so callers can implement the native cache lifecycle
    // without treating a pure base-chunk query as a load event.
    [[nodiscard]] Chunk generate_base_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z);
    [[nodiscard]] Chunk generate_terrain_stage(
        Pe090TerrainStage stage,
        std::int32_t chunk_x,
        std::int32_t chunk_z);
    [[nodiscard]] detail::Mt19937 pre_population_random(
        std::int32_t chunk_x,
        std::int32_t chunk_z);
    [[nodiscard]] bool suppresses_underground_features() const noexcept;

    [[nodiscard]] Chunk& writable_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z);
    [[nodiscard]] Chunk* find_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z) noexcept;
    [[nodiscard]] const Chunk* find_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z) const noexcept;

    // TileSource::hasChunksAt only asks its ChunkSource whether every
    // horizontal chunk touched by the requested block cube is available.
    // Population's deferred-fluid pass uses an eight-block radius.
    [[nodiscard]] bool has_chunks_at(
        std::int32_t x,
        std::int32_t z,
        std::int32_t radius) const noexcept override;

    [[nodiscard]] std::uint8_t block(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept override;
    [[nodiscard]] std::uint8_t data(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept override;

    // Feature::setTileAndData, TileSource::setTileNoUpdate, and
    // TileSource::setTileAndDataNoUpdate ultimately make this same storage
    // update.  `normal_write` is retained at the call site to document which
    // native entry point was used; general callbacks remain outside this
    // generation-only abstraction.
    bool set_block_and_data(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::uint8_t id,
        std::uint8_t metadata,
        bool normal_write) noexcept override;

    [[nodiscard]] std::int32_t heightmap(
        std::int32_t x,
        std::int32_t z) const noexcept;
    [[nodiscard]] std::int32_t top_solid_block(
        std::int32_t x,
        std::int32_t z,
        bool include_water) const noexcept;
    [[nodiscard]] std::int32_t top_rain_height(
        std::int32_t x,
        std::int32_t z) noexcept;
    [[nodiscard]] std::uint8_t sky_brightness(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept;
    [[nodiscard]] std::uint8_t raw_brightness(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z) const noexcept;
    [[nodiscard]] float biome_temperature(
        std::int32_t x,
        std::int32_t z) const noexcept;
    [[nodiscard]] std::int32_t biome_id(
        std::int32_t x,
        std::int32_t z) const noexcept;

    // TileEntity creation is generation-visible for monster rooms and
    // structures. These routines retain the exact state written into a fresh
    // chest/spawner while keeping gameplay ticking outside this world model.
    [[nodiscard]] bool place_chest(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::uint8_t metadata = 0) noexcept;
    [[nodiscard]] bool place_mob_spawner(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        SpawnerType type) noexcept;
    void set_chest_slot(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::size_t slot,
        GeneratedItemStack item) noexcept;
    void add_villager(GeneratedVillager villager) noexcept;
    [[nodiscard]] std::span<const GeneratedChest> generated_chests()
        const;
    [[nodiscard]] std::span<const GeneratedSpawner> generated_spawners()
        const;
    [[nodiscard]] std::span<const GeneratedVillager> generated_villagers()
        const noexcept;

    static void recalculate_heightmap(Chunk& chunk) noexcept;

    // The source invokes LevelChunk::updateLightsAndHeights after every
    // completed population pass, rather than merely recalculating columns.
    void update_lights_and_heights(
        std::int32_t chunk_x,
        std::int32_t chunk_z);

private:
    using BlockKey = std::tuple<std::int32_t, std::int32_t, std::int32_t>;
    using ChestSlots = std::array<std::optional<GeneratedItemStack>, 27>;

    static void split_coordinate(
        std::int32_t coordinate,
        std::int32_t& chunk,
        std::int32_t& local) noexcept;
    static void set_nibble(
        std::array<std::uint8_t, Chunk::block_count / 2>& layer,
        std::size_t index,
        std::uint8_t value) noexcept;
    [[nodiscard]] static std::uint8_t nibble(
        const std::array<std::uint8_t, Chunk::block_count / 2>& layer,
        std::size_t index) noexcept;

    friend void lighting::update_lights_and_heights(
        World& world,
        std::int32_t chunk_x,
        std::int32_t chunk_z);
    friend void liquid::settle_chunk_boundary_water(
        World& world,
        std::int32_t block_x,
        std::int32_t block_z) noexcept;

    TerrainGenerator terrain_generator_;
    // Level owns a separate MT stream, initialized from the level seed. The
    // population-only liquid queue needs it for a lava depth transition;
    // normal terrain and feature streams never consume it.
    detail::Mt19937 deferred_liquid_random_;
    std::map<ChunkKey, Chunk> chunks_;
    // LevelChunk uses -999 to invalidate this signed-short cache.  A separate
    // map avoids expanding the public Chunk representation for an internal
    // transient cache.
    std::map<ChunkKey, std::array<std::int16_t, Chunk::width * Chunk::width>>
        rain_heights_;
    std::map<BlockKey, ChestSlots> chests_;
    std::map<BlockKey, SpawnerType> spawners_;
    // At population time LevelChunk is not active for normal light callbacks.
    // _placeCallbacks therefore retains each emitting write for the later
    // updateLightsAndHeights call on the chunk that owns the tile.
    std::map<ChunkKey, std::vector<BlockKey>> deferred_light_emitters_;
    std::vector<GeneratedVillager> villagers_;
    mutable std::vector<GeneratedChest> generated_chests_;
    mutable std::vector<GeneratedSpawner> generated_spawners_;
    mutable bool generated_chests_dirty_{true};
    mutable bool generated_spawners_dirty_{true};
};

} // namespace mcpe::worldgen::v0_9_0
