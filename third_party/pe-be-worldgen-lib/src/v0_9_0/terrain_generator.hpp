#pragma once

#include "mcpe/worldgen/chunk.hpp"
#include "mcpe/worldgen/terrain_stage.hpp"
#include "detail/mt19937.hpp"
#include "v0_9_0/biome_layers.hpp"
#include "v0_9_0/caves.hpp"
#include "v0_9_0/mesa_surface.hpp"
#include "v0_9_0/surface_overrides.hpp"
#include "v0_9_0/terrain_noise.hpp"

#include <cstdint>
#include <vector>

namespace mcpe::worldgen::v0_9_0 {

// RandomLevelSource::loadChunk through its cave pass, immediately before the
// native LevelChunk::recalcHeightmap call.  The cave pass also returns its
// deferred lava emitters to the stateful World path so its later lighting pass
// can consume the same queue without a second terrain implementation.
class TerrainGenerator final {
public:
    TerrainGenerator(std::uint32_t world_seed, std::int32_t generator_type_code);

    [[nodiscard]] Chunk generate_unpopulated_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z,
        std::vector<DeferredLightEmitter>* deferred_light_emitters = nullptr);

    [[nodiscard]] Chunk generate_stage(
        Pe090TerrainStage stage,
        std::int32_t chunk_x,
        std::int32_t chunk_z);

    // The lake prepass consumes the Random state left by loadChunk(), after
    // surface generation and (where enabled) LargeCaveFeature.  It is kept
    // as a separate query so population can reproduce that state without
    // coupling mutable World storage to terrain generation.
    [[nodiscard]] detail::Mt19937 pre_population_random(
        std::int32_t chunk_x,
        std::int32_t chunk_z);

    [[nodiscard]] bool suppresses_underground_features() const noexcept;

private:
    [[nodiscard]] Chunk generate_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z,
        detail::Mt19937* post_load_random,
        std::vector<DeferredLightEmitter>* deferred_light_emitters,
        Pe090TerrainStage stage);

    std::uint32_t world_seed_;
    // Level::_createGenerator passes true only for GeneratorType 0.  In this
    // APK that flag suppresses caves, lava lakes, mineshafts, strongholds,
    // and monster rooms; it is not the separate FlatLevelSource (type 2).
    bool suppress_underground_features_;
    BiomeLayerSource biome_source_;
    TerrainNoise terrain_noise_;
    MesaSurface mesa_surface_;
    SwampSurface swamp_surface_;
    CaveCarver cave_carver_;
};

} // namespace mcpe::worldgen::v0_9_0
