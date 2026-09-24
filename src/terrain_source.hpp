#pragma once

#include "mcpe/worldgen/chunk.hpp"
#include "mcpe/worldgen/generator.hpp"

#include <cstdint>
#include <memory>

namespace mcpe::terrain_verifier {

struct TerrainSourceOptions final {
    worldgen::Version version{};
    worldgen::DimensionId dimension{worldgen::DimensionId::overworld};
    worldgen::Pe090GeneratorType pe090_source{
        worldgen::Pe090GeneratorType::infinite};
};

// Thin dispatch layer over the shared versioned terrain implementations. It
// intentionally exposes only operations used by the air/non-air verifier.
class TerrainSource final {
public:
    TerrainSource(std::uint32_t seed, TerrainSourceOptions options);
    ~TerrainSource();

    TerrainSource(TerrainSource&&) noexcept;
    TerrainSource& operator=(TerrainSource&&) noexcept;

    TerrainSource(const TerrainSource&) = delete;
    TerrainSource& operator=(const TerrainSource&) = delete;

    [[nodiscard]] worldgen::Chunk terrain_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z);

    void populate_chunk(std::int32_t chunk_x, std::int32_t chunk_z);
    [[nodiscard]] const worldgen::Chunk* find_chunk(
        std::int32_t chunk_x,
        std::int32_t chunk_z) const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mcpe::terrain_verifier
