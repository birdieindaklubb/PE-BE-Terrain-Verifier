#include "terrain_source.hpp"

#include "end_terrain_source.hpp"
#include "pe061_terrain_source.hpp"
#include "v0_9_0/terrain_generator.hpp"
#include "v1_1_5_0/hell_random_level_source.hpp"

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace mcpe::terrain_verifier {

class TerrainSource::Impl final {
public:
    using Backend = std::variant<
        Pe061TerrainSource,
        worldgen::v0_9_0::TerrainGenerator,
        EndTerrainSource,
        worldgen::v1_1_5_0::HellRandomLevelSource>;

    Impl(std::uint32_t seed, TerrainSourceOptions options)
        : backend(make_backend(seed, options)) {
    }

    [[nodiscard]] static Backend make_backend(
        std::uint32_t seed,
        TerrainSourceOptions options) {
        switch (options.version) {
        case worldgen::Version::pe_0_6_1:
            return Backend{
                std::in_place_type<Pe061TerrainSource>, seed};
        case worldgen::Version::pe_0_9_0:
            return Backend{
                std::in_place_type<worldgen::v0_9_0::TerrainGenerator>,
                seed,
                static_cast<std::int32_t>(options.pe090_source)};
        case worldgen::Version::pe_1_1_5_0:
            if (options.dimension == worldgen::DimensionId::nether) {
                return Backend{
                    std::in_place_type<
                        worldgen::v1_1_5_0::HellRandomLevelSource>,
                    seed};
            }
            return Backend{
                std::in_place_type<EndTerrainSource>,
                seed};
        case worldgen::Version::pe_1_6_0_15:
            break;
        }
        throw std::invalid_argument("unsupported terrain-verifier version");
    }

    Backend backend;
};

TerrainSource::TerrainSource(std::uint32_t seed, TerrainSourceOptions options)
    : impl_(std::make_unique<Impl>(seed, options)) {
}

TerrainSource::~TerrainSource() = default;
TerrainSource::TerrainSource(TerrainSource&&) noexcept = default;
TerrainSource& TerrainSource::operator=(TerrainSource&&) noexcept = default;

worldgen::Chunk TerrainSource::terrain_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    return std::visit(
        [=](auto& source) -> worldgen::Chunk {
            using Source = std::remove_cvref_t<decltype(source)>;
            if constexpr (std::is_same_v<Source, Pe061TerrainSource>) {
                return source.generate_chunk(chunk_x, chunk_z);
            } else if constexpr (std::is_same_v<
                                     Source,
                                     worldgen::v0_9_0::TerrainGenerator>) {
                return source.generate_stage(
                    worldgen::Pe090TerrainStage::cave_carved,
                    chunk_x,
                    chunk_z);
            } else if constexpr (std::is_same_v<Source, EndTerrainSource>) {
                return source.generate_chunk(chunk_x, chunk_z);
            } else {
                return source.generate_base_chunk(chunk_x, chunk_z);
            }
        },
        impl_->backend);
}

void TerrainSource::populate_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    std::visit(
        [=](auto& source) {
            using Source = std::remove_cvref_t<decltype(source)>;
            if constexpr (std::is_same_v<
                              Source,
                              worldgen::v1_1_5_0::HellRandomLevelSource>) {
                source.populate_chunk(chunk_x, chunk_z);
            } else {
                throw std::logic_error(
                    "population is available only for the Nether backend");
            }
        },
        impl_->backend);
}

const worldgen::Chunk* TerrainSource::find_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) const noexcept {
    return std::visit(
        [=](const auto& source) -> const worldgen::Chunk* {
            using Source = std::remove_cvref_t<decltype(source)>;
            if constexpr (std::is_same_v<
                              Source,
                              worldgen::v1_1_5_0::HellRandomLevelSource>) {
                return source.find_chunk(chunk_x, chunk_z);
            }
            return nullptr;
        },
        impl_->backend);
}

} // namespace mcpe::terrain_verifier
