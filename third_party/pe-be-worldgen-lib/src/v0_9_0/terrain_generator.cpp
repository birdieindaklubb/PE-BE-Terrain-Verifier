#include "v0_9_0/terrain_generator.hpp"

#include "detail/mt19937.hpp"
#include "detail/wrap.hpp"
#include "v0_9_0/base_terrain.hpp"
#include "v0_9_0/biome_registry.hpp"
#include "v0_9_0/surface.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mcpe::worldgen::v0_9_0 {
namespace {

[[nodiscard]] constexpr std::size_t layer_index(
    std::int32_t x,
    std::int32_t z) noexcept {
    // Layer::get and BiomeSource::fillBiomeData use z-major areas.
    return static_cast<std::size_t>(z) * kChunkWidth
        + static_cast<std::size_t>(x);
}

[[nodiscard]] constexpr std::size_t surface_index(
    std::int32_t x,
    std::int32_t z) noexcept {
    // PerlinSimplexNoise::getRegion uses x, then z, then y.  This is the
    // `z | (x << 4)` index used by RandomLevelSource::buildSurfaces.
    return static_cast<std::size_t>(x) * kChunkWidth
        + static_cast<std::size_t>(z);
}

void build_surface_column(
    detail::Mt19937& random,
    const BiomeProperties& biome,
    const MesaSurface& mesa,
    const SwampSurface& swamp,
    std::span<std::uint8_t> blocks,
    std::span<std::uint8_t> data,
    std::int32_t x,
    std::int32_t z,
    std::int32_t world_x,
    std::int32_t world_z,
    float surface_noise) noexcept {
    switch (biome.surface_kind) {
    case SurfaceKind::swamp:
        swamp.apply_prepass(blocks, x, z, world_x, world_z);
        break;
    case SurfaceKind::mesa:
        mesa.apply(
            random,
            blocks,
            data,
            x,
            z,
            world_x,
            world_z,
            surface_noise,
            biome.mesa_bryce,
            biome.mesa_forest);
        return;
    default:
        break;
    }

    SurfaceProfile profile = biome.surface;
    switch (biome.surface_kind) {
    case SurfaceKind::extreme_hills:
        profile = extreme_hills_surface_profile(
            biome.subtype, surface_noise, profile.temperature);
        break;
    case SurfaceKind::taiga:
        profile = taiga_surface_profile(
            biome.subtype, surface_noise, profile.temperature);
        break;
    case SurfaceKind::mutated_savanna:
        profile = mutated_savanna_surface_profile(surface_noise, profile.temperature);
        break;
    default:
        break;
    }
    apply_default_surface_column(random, blocks, data, x, z, surface_noise, profile);
}

} // namespace

TerrainGenerator::TerrainGenerator(
    std::uint32_t world_seed,
    std::int32_t generator_type_code)
    : world_seed_(world_seed),
      suppress_underground_features_(generator_type_code == 0),
      biome_source_(world_seed, generator_type_code),
      terrain_noise_(world_seed),
      mesa_surface_(world_seed) {
}

Chunk TerrainGenerator::generate_unpopulated_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    std::vector<DeferredLightEmitter>* deferred_light_emitters) {
    return generate_chunk(
        chunk_x,
        chunk_z,
        nullptr,
        deferred_light_emitters,
        Pe090TerrainStage::cave_carved);
}

Chunk TerrainGenerator::generate_stage(
    Pe090TerrainStage stage,
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    switch (stage) {
    case Pe090TerrainStage::raw_density:
    case Pe090TerrainStage::surfaced:
    case Pe090TerrainStage::cave_carved:
        break;
    default:
        throw std::invalid_argument("unknown PE 0.9.0 terrain stage");
    }
    return generate_chunk(chunk_x, chunk_z, nullptr, nullptr, stage);
}

detail::Mt19937 TerrainGenerator::pre_population_random(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    detail::Mt19937 result;
    (void)generate_chunk(
        chunk_x,
        chunk_z,
        &result,
        nullptr,
        Pe090TerrainStage::cave_carved);
    return result;
}

bool TerrainGenerator::suppresses_underground_features() const noexcept {
    return suppress_underground_features_;
}

Chunk TerrainGenerator::generate_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    detail::Mt19937* post_load_random,
    std::vector<DeferredLightEmitter>* deferred_light_emitters,
    Pe090TerrainStage stage) {
    const std::int32_t coarse_x = detail::wrapping_mul(chunk_x, 4);
    const std::int32_t coarse_z = detail::wrapping_mul(chunk_z, 4);
    const std::int32_t block_x = detail::wrapping_mul(chunk_x, 16);
    const std::int32_t block_z = detail::wrapping_mul(chunk_z, 16);

    const std::vector<std::int32_t> raw_biomes = biome_source_.raw_biomes(
        detail::wrapping_sub(coarse_x, 2),
        detail::wrapping_sub(coarse_z, 2),
        10,
        10);
    const std::vector<std::int32_t> final_biomes = biome_source_.biomes(
        block_x,
        block_z,
        kChunkWidth,
        kChunkWidth);

    std::vector<float> densities;
    const std::span<const float> density_lattice = terrain_noise_.density_lattice(
        densities, raw_biomes, coarse_x, coarse_z);

    Chunk chunk{};
    chunk.x = chunk_x;
    chunk.z = chunk_z;
    interpolate_base_terrain(chunk.blocks, density_lattice);

    for (std::int32_t z = 0; z < kChunkWidth; ++z) {
        for (std::int32_t x = 0; x < kChunkWidth; ++x) {
            const BiomeProperties biome = biome_properties(final_biomes[layer_index(x, z)]);
            const std::size_t column = Chunk::column_index(x, z);
            chunk.biomes[column] = static_cast<std::uint8_t>(
                final_biomes[layer_index(x, z)]);
            chunk.temperatures[column] = biome.surface.temperature;
            chunk.rainfall[column] = biome.rainfall;
        }
    }

    if (stage == Pe090TerrainStage::raw_density) {
        return chunk;
    }

    std::vector<float> surface_field;
    const std::span<const float> surface_noise = terrain_noise_.surface_field(
        surface_field, block_x, block_z);
    const std::uint32_t chunk_seed = detail::bits(chunk_x) * 0x14609048U
        + detail::bits(chunk_z) * 0x07ebe2d5U;
    detail::Mt19937 random(chunk_seed);

    // The first byte of ChunkTilePos is X and the second is Z.  Native code
    // therefore advances X in the outer loop while sharing one MT stream.
    for (std::int32_t x = 0; x < kChunkWidth; ++x) {
        for (std::int32_t z = 0; z < kChunkWidth; ++z) {
            const BiomeProperties biome = biome_properties(final_biomes[layer_index(x, z)]);
            build_surface_column(
                random,
                biome,
                mesa_surface_,
                swamp_surface_,
                chunk.blocks,
                chunk.data,
                x,
                z,
                detail::wrapping_add(block_x, x),
                detail::wrapping_add(block_z, z),
                surface_noise[surface_index(x, z)]);
        }
    }

    if (stage == Pe090TerrainStage::surfaced) {
        return chunk;
    }

    if (suppress_underground_features_) {
        if (post_load_random != nullptr) {
            *post_load_random = random;
        }
    } else {
        cave_carver_.carve(
            world_seed_,
            chunk.blocks,
            chunk_x,
            chunk_z,
            post_load_random,
            deferred_light_emitters);
    }
    return chunk;
}

} // namespace mcpe::worldgen::v0_9_0
