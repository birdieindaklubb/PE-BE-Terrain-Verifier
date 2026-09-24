#pragma once

#include "mcpe/worldgen/chunk.hpp"
#include "mcpe/worldgen/generated_content.hpp"
#include "mcpe/worldgen/mob.hpp"
#include "mcpe/worldgen/position.hpp"
#include "mcpe/worldgen/terrain_stage.hpp"
#include "mcpe/worldgen/version.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace mcpe::worldgen {

enum class GameMode : std::uint8_t {
    survival = 0,
    creative = 1,
};

// LevelData::dimension selects the world source in versions which expose more
// than one generated dimension.  Leaving it unset retains the historical API
// default: the overworld for 0.6.1/0.9.0 and the End for 1.1.5.0, which was
// the first 1.1.5 backend offered by this library.
enum class DimensionId : std::int8_t {
    overworld = 0,
    nether = 1,
    end = 2,
};

// The End fight owns whether the central exit podium is traversable. It is
// not a seed choice: the inactive geometry exists before the dragon dies and
// the active state replaces its central air with End-portal blocks.
enum class EndExitPortalState : std::uint8_t {
    inactive,
    active,
};

// LevelData's three PE 0.9.0 generator codes. The names intentionally retain
// the game-facing distinction: legacy and infinite both use random terrain,
// but select different biome-layer configurations; flat uses its stored
// y-indexed block column instead.
enum class Pe090GeneratorType : std::uint8_t {
    legacy = 0,
    infinite = 1,
    flat = 2,
};

// Parses PE 0.9.0's persisted `game_flatworldlayers` JSON integer array into
// the y-indexed column accepted by GenerationOptions. Returns false for an
// invalid list and leaves fallback selection to the caller.
[[nodiscard]] bool parse_pe_0_9_0_flat_layers(
    std::string_view json,
    std::array<std::uint8_t, Chunk::height>& output) noexcept;

struct GenerationOptions final {
    // PE 0.6.1 derives LevelData::spawnMobs from this setting. Creative mode
    // also suppresses direct Tile::popResource drops, so the choice can alter
    // the Level RNG seen by later springs and initial-spawn selection.
    GameMode game_mode{GameMode::survival};

    // PE 0.6.1 accidentally lets the graphics setting affect generation.
    // LevelRenderer copies it into LeafTile before fresh-world preparation:
    // fancy leaves are not solid-render for spawn and snow-support checks,
    // while the immutable Tile::solid[] table used by trees is unchanged.
    // true is the native Options constructor default; Android can override it
    // from gfx_fancygraphics before a world is selected.
    bool fancy_graphics{true};

    // PE 0.9.0 LevelData::generator. It is ignored by other versions. The
    // native default is the infinite source.
    Pe090GeneratorType pe_0_9_0_generator{Pe090GeneratorType::infinite};

    // The persisted flat-world column, indexed by y. Flat worlds require an
    // explicit value so this library never guesses a storage-layer fallback.
    // For the other PE 0.9.0 sources it is ignored, exactly as it is by the
    // game. A short persisted list is represented by zero-filled tail bytes.
    std::optional<std::array<std::uint8_t, Chunk::height>>
        pe_0_9_0_flat_layers{};

    // An explicit dimension is currently supported for PE 1.1.5.0's End and
    // Nether source.  Selecting an unavailable source is an error; the
    // library never silently substitutes a different dimension.
    std::optional<DimensionId> dimension{};
};

class Generator final {
public:
    Generator(
        Version version,
        std::int32_t seed,
        GenerationOptions options = {});
    ~Generator();

    Generator(Generator&&) noexcept;
    Generator& operator=(Generator&&) noexcept;

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    [[nodiscard]] Version version() const noexcept;
    [[nodiscard]] DimensionId dimension() const noexcept;
    [[nodiscard]] std::int32_t seed() const noexcept;
    [[nodiscard]] GenerationOptions options() const noexcept;

    // Produces exactly the selected version's pre-population LevelChunk made
    // by getChunk(). It contains terrain and biome surfaces, but not the
    // stateful postProcess features such as ores, structures, trees, plants,
    // springs, or lakes.
    [[nodiscard]] Chunk generate_base_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z);

    // Returns an exact intermediate PE 0.9.0 RandomLevelSource::loadChunk
    // block state. This is available only for the random infinite/legacy
    // sources; flat worlds and other versions have no matching pipeline and
    // throw std::logic_error. Intermediate chunks intentionally precede the
    // native height/light refresh, so only their terrain blocks, metadata,
    // and biome/climate columns are stage-defined.
    [[nodiscard]] Chunk generate_pe_0_9_0_terrain_stage(
        Pe090TerrainStage stage,
        std::int32_t chunk_x,
        std::int32_t chunk_z);

    // PE 1.1.5.0 End's terrain-only chunk: base End terrain plus the seeded
    // outer-island pass which adds End stone. It intentionally excludes End
    // pillars, cities, chorus plants, gateways, the exit podium, and arrival
    // geometry because those are features or runtime scene state rather than
    // the version's terrain predicate. Other versions/dimensions throw
    // std::logic_error.
    [[nodiscard]] Chunk generate_pe_1_1_5_0_end_terrain_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z);

    // PE 1.1.5.0 Nether's complete pre-population terrain chunk: density
    // terrain, surface replacement, and the native large-cave pass. It
    // intentionally excludes fortress pieces and biome-decoration writes
    // (springs, fire, glowstone, mushrooms, quartz, and magma). Other
    // versions/dimensions throw std::logic_error.
    [[nodiscard]] Chunk generate_pe_1_1_5_0_nether_terrain_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z);

    // Reproduces PE 0.6.1's fresh finite-world preparation: load and populate
    // all 16x16 chunks in native cache-trigger order, then choose the initial
    // spawn. PE 0.9.0 and the PE 1.1.5.0 End source are unbounded and have no
    // equivalent finite bootstrap operation, so this throws std::logic_error
    // for those versions. Call this before stateful load/populate operations
    // for PE 0.6.1.
    [[nodiscard]] BlockPosition generate_world();

    // Stateful equivalents of ChunkCache loading and postProcess(). A cache
    // miss can automatically populate a completed version-native loaded-chunk
    // neighborhood. Population writes therefore remain in this generator-owned
    // world rather than being returned as isolated values.
    void load_base_chunk(std::int32_t chunk_x, std::int32_t chunk_z);
    void populate_chunk(std::int32_t chunk_x, std::int32_t chunk_z);
    [[nodiscard]] const Chunk* find_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z) const noexcept;
    [[nodiscard]] bool is_populated(
        std::int32_t chunk_x,
        std::int32_t chunk_z) const noexcept;

    // Passive mobs emitted by PE 0.6.1 survival-mode postProcess calls, in
    // native addEntity order. PE 0.9.0's ordinary mob spawning is a later
    // runtime tick and therefore returns an empty span here. The span remains
    // valid until another population call.
    [[nodiscard]] std::span<const GeneratedMob> generated_mobs()
        const noexcept;

    // Generated tile-entity state and village entities. Chest and spawner
    // records use a stable coordinate order; a deferred modern chest retains
    // its target loot-table seed rather than premature item rolls. Villagers
    // retain their native creation order. The spans remain valid until another
    // population call.
    // Versions without a given feature return an empty span rather than
    // inventing a record.
    [[nodiscard]] std::span<const GeneratedChest> generated_chests()
        const;
    [[nodiscard]] std::span<const GeneratedSpawner> generated_spawners()
        const;
    [[nodiscard]] std::span<const GeneratedVillager> generated_villagers()
        const noexcept;

    // PE 1.1.5.0's ten End-spike layouts, ordered as the native decorator
    // constructs them. Other dimensions and versions return an empty span;
    // the library never substitutes an overworld source for PE 1.1.5.0.
    [[nodiscard]] std::span<const GeneratedEndPillar> generated_end_pillars()
        const noexcept;

    // Reproduces PE 1.1.5.0's End-arrival safety pad at the dimension's fixed
    // spawn location. This is an arrival-time world mutation, not terrain
    // population, so callers opt into it explicitly.
    void place_end_arrival_platform();

    // Applies the non-seeded block state a player sees on first entering a
    // newly created PE 1.1.5.0 End: the inactive central podium and fixed
    // arrival pad. Seeded chunk population remains explicit through
    // populate_chunk(), so callers retain control over the generated area.
    void initialize_fresh_end_entry();

    // Reproduces the End fight's central exit-podium block operation. The
    // state is runtime-owned rather than seed-derived: use inactive while the
    // dragon is alive and active after its defeat. Unsupported versions throw
    // std::logic_error rather than substituting another version's portal.
    void place_end_exit_portal(EndExitPortalState state);

    // Applies PE 1.1.5.0's EndGatewayFeature shell at an explicitly supplied
    // centre. Natural-gateway selection and dragon-fight gateway scheduling
    // are distinct stages; their centre is therefore never guessed from a
    // world seed by this direct block-operation API.
    void place_end_gateway(BlockPosition center);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mcpe::worldgen
