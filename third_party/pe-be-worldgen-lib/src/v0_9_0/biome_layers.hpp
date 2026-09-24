#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace mcpe::worldgen::v0_9_0 {

// The layered biome source used by the persisted PE 0.9.0 generator modes.
class BiomeLayerSource final {
public:
    BiomeLayerSource(std::uint32_t world_seed, std::int32_t generator_type_code);
    ~BiomeLayerSource();

    BiomeLayerSource(const BiomeLayerSource&) = delete;
    BiomeLayerSource& operator=(const BiomeLayerSource&) = delete;
    BiomeLayerSource(BiomeLayerSource&&) noexcept;
    BiomeLayerSource& operator=(BiomeLayerSource&&) noexcept;

    // Matches BiomeSource::fillRawBiomeData: RiverMixer output, before the
    // Voronoi selection used to store a biome in each chunk column.
    [[nodiscard]] std::vector<std::int32_t> raw_biomes(
        std::int32_t x,
        std::int32_t z,
        std::int32_t width,
        std::int32_t height);

    // Matches BiomeSource::fillBiomeData / getBiome: VoronoiZoom output.
    [[nodiscard]] std::vector<std::int32_t> biomes(
        std::int32_t x,
        std::int32_t z,
        std::int32_t width,
        std::int32_t height);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mcpe::worldgen::v0_9_0
