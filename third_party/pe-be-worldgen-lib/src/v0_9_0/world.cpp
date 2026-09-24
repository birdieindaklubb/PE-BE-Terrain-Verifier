#include "v0_9_0/world.hpp"

#include "detail/wrap.hpp"
#include "v0_9_0/blocks.hpp"
#include "v0_9_0/lighting.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace mcpe::worldgen::v0_9_0 {

World::World(std::uint32_t world_seed, std::int32_t generator_type_code)
    : terrain_generator_(world_seed, generator_type_code),
      deferred_liquid_random_(world_seed) {
}

Chunk World::generate_base_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    Chunk chunk = terrain_generator_.generate_unpopulated_chunk(chunk_x, chunk_z);
    recalculate_heightmap(chunk);
    return chunk;
}

Chunk World::generate_terrain_stage(
    Pe090TerrainStage stage,
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    return terrain_generator_.generate_stage(stage, chunk_x, chunk_z);
}

detail::Mt19937 World::pre_population_random(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    return terrain_generator_.pre_population_random(chunk_x, chunk_z);
}

bool World::suppresses_underground_features() const noexcept {
    return terrain_generator_.suppresses_underground_features();
}

Chunk& World::writable_chunk(std::int32_t chunk_x, std::int32_t chunk_z) {
    const ChunkKey key{chunk_x, chunk_z};
    if (const auto found = chunks_.find(key); found != chunks_.end()) {
        return found->second;
    }

    std::vector<DeferredLightEmitter> cave_emitters;
    Chunk chunk = terrain_generator_.generate_unpopulated_chunk(
        chunk_x, chunk_z, &cave_emitters);
    recalculate_heightmap(chunk);
    auto [position, inserted] = chunks_.try_emplace(key, std::move(chunk));
    (void)inserted;
    std::array<std::int16_t, Chunk::width * Chunk::width> invalid{};
    invalid.fill(-999);
    rain_heights_.try_emplace(key, invalid);
    if (!cave_emitters.empty()) {
        auto& emitters = deferred_light_emitters_[key];
        emitters.reserve(cave_emitters.size());
        for (const DeferredLightEmitter& emitter : cave_emitters) {
            emitters.emplace_back(emitter.x, emitter.y, emitter.z);
        }
    }
    return position->second;
}

Chunk* World::find_chunk(std::int32_t chunk_x, std::int32_t chunk_z) noexcept {
    const auto found = chunks_.find({chunk_x, chunk_z});
    return found == chunks_.end() ? nullptr : &found->second;
}

const Chunk* World::find_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) const noexcept {
    const auto found = chunks_.find({chunk_x, chunk_z});
    return found == chunks_.end() ? nullptr : &found->second;
}

bool World::has_chunks_at(
    std::int32_t x,
    std::int32_t z,
    std::int32_t radius) const noexcept {
    std::int32_t min_chunk_x{};
    std::int32_t min_local_x{};
    std::int32_t max_chunk_x{};
    std::int32_t max_local_x{};
    std::int32_t min_chunk_z{};
    std::int32_t min_local_z{};
    std::int32_t max_chunk_z{};
    std::int32_t max_local_z{};
    split_coordinate(detail::wrapping_sub(x, radius), min_chunk_x, min_local_x);
    split_coordinate(detail::wrapping_add(x, radius), max_chunk_x, max_local_x);
    split_coordinate(detail::wrapping_sub(z, radius), min_chunk_z, min_local_z);
    split_coordinate(detail::wrapping_add(z, radius), max_chunk_z, max_local_z);
    (void)min_local_x;
    (void)max_local_x;
    (void)min_local_z;
    (void)max_local_z;

    for (std::int32_t chunk_z = min_chunk_z;; ++chunk_z) {
        for (std::int32_t chunk_x = min_chunk_x;; ++chunk_x) {
            if (find_chunk(chunk_x, chunk_z) == nullptr) {
                return false;
            }
            if (chunk_x == max_chunk_x) {
                break;
            }
        }
        if (chunk_z == max_chunk_z) {
            break;
        }
    }
    return true;
}

void World::split_coordinate(
    std::int32_t coordinate,
    std::int32_t& chunk,
    std::int32_t& local) noexcept {
    // TileSource::getTile uses ASR followed by AND 15, which is floor division
    // and a non-negative remainder even for a negative block coordinate.
    chunk = coordinate >= 0
        ? coordinate / Chunk::width
        : -1 - static_cast<std::int32_t>(
            (-1LL - static_cast<std::int64_t>(coordinate)) / Chunk::width);
    local = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(coordinate) & 15U);
}

std::uint8_t World::block(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) const noexcept {
    if (static_cast<std::uint32_t>(y) >= static_cast<std::uint32_t>(Chunk::height)) {
        return block::air;
    }
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const Chunk* const chunk = find_chunk(chunk_x, chunk_z);
    return chunk == nullptr ? block::air : chunk->block(local_x, y, local_z);
}

std::uint8_t World::data(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) const noexcept {
    if (static_cast<std::uint32_t>(y) >= static_cast<std::uint32_t>(Chunk::height)) {
        return 0;
    }
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const Chunk* const chunk = find_chunk(chunk_x, chunk_z);
    return chunk == nullptr ? 0 : chunk->block_data(local_x, y, local_z);
}

bool World::set_block_and_data(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t id,
    std::uint8_t metadata,
    bool normal_write) noexcept {
    (void)normal_write;
    if (static_cast<std::uint32_t>(y) >= static_cast<std::uint32_t>(Chunk::height)) {
        return false;
    }
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    Chunk* const chunk = find_chunk(chunk_x, chunk_z);
    if (chunk == nullptr) {
        return false;
    }

    const std::uint8_t old_id = chunk->block(local_x, y, local_z);
    const std::uint8_t old_data = chunk->block_data(local_x, y, local_z);
    if (old_id == id && old_data == (metadata & 0x0fU)) {
        return false;
    }
    chunk->set_block_and_data(local_x, y, local_z, id, metadata);
    const BlockKey key{x, y, z};
    if (id != block::chest) {
        generated_chests_dirty_ |= chests_.erase(key) != 0U;
    }
    if (id != block::mob_spawner) {
        generated_spawners_dirty_ |= spawners_.erase(key) != 0U;
    }
    if (block::light_emission(id) != 0U) {
        deferred_light_emitters_[{chunk_x, chunk_z}].push_back(key);
    }

    // LevelChunk::setTileAndData invalidates only the rain-height cache when
    // a write reaches at or above the cached column height.
    auto cache = rain_heights_.find({chunk_x, chunk_z});
    if (cache != rain_heights_.end()
        && cache->second[Chunk::column_index(local_x, local_z)] - 1 <= y) {
        cache->second[Chunk::column_index(local_x, local_z)] = -999;
    }
    return true;
}

void World::set_nibble(
    std::array<std::uint8_t, Chunk::block_count / 2>& layer,
    std::size_t index,
    std::uint8_t value) noexcept {
    std::uint8_t& packed = layer[index >> 1U];
    const std::uint8_t shift = (index & 1U) == 0U ? 0U : 4U;
    const std::uint8_t mask = static_cast<std::uint8_t>(0x0fU << shift);
    packed = static_cast<std::uint8_t>(
        (packed & static_cast<std::uint8_t>(~mask))
        | static_cast<std::uint8_t>((value & 0x0fU) << shift));
}

std::uint8_t World::nibble(
    const std::array<std::uint8_t, Chunk::block_count / 2>& layer,
    std::size_t index) noexcept {
    const std::uint8_t shift = (index & 1U) == 0U ? 0U : 4U;
    return static_cast<std::uint8_t>((layer[index >> 1U] >> shift) & 0x0fU);
}

void World::recalculate_heightmap(Chunk& chunk) noexcept {
    chunk.sky_light.fill(0);
    std::int32_t minimum_height = 127;
    for (std::int32_t local_x = 0; local_x < Chunk::width; ++local_x) {
        for (std::int32_t local_z = 0; local_z < Chunk::width; ++local_z) {
            // LevelChunk::recalcHeightmap starts at 127, reads y-1, and so
            // deliberately excludes the physical y=127 layer.
            std::int32_t height = 127;
            while (height != 0
                   && block::light_block(
                       chunk.block(local_x, height - 1, local_z)) == 0U) {
                --height;
            }
            chunk.heightmap[Chunk::column_index(local_x, local_z)] =
                static_cast<std::uint8_t>(height);
            minimum_height = std::min(minimum_height, height);

            // This direct skylight pass runs before a LevelChunk enters the
            // cache. It covers the complete stored vertical range; later
            // LightUpdate work only has to propagate across columns.
            std::int32_t light = 15;
            for (std::int32_t y = 127; y >= 0 && light > 0; --y) {
                light -= block::light_block(chunk.block(local_x, y, local_z));
                if (light > 0) {
                    set_nibble(
                        chunk.sky_light,
                        Chunk::index(local_x, y, local_z),
                        static_cast<std::uint8_t>(light));
                }
            }
        }
    }
    chunk.minimum_height = minimum_height;
}

void World::update_lights_and_heights(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    lighting::update_lights_and_heights(*this, chunk_x, chunk_z);
}

std::int32_t World::heightmap(std::int32_t x, std::int32_t z) const noexcept {
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const Chunk* const chunk = find_chunk(chunk_x, chunk_z);
    return chunk == nullptr
        ? 0
        : chunk->heightmap[Chunk::column_index(local_x, local_z)];
}

std::int32_t World::top_solid_block(
    std::int32_t x,
    std::int32_t z,
    bool include_water) const noexcept {
    // LevelChunk::getTopSolidBlock does not consult light opacity.  It skips
    // both leaf materials and accepts Material::blocksMotion; the optional
    // form adds water as a valid surface.
    for (std::int32_t y = 127; y > 0; --y) {
        const std::uint8_t id = block(x, y, z);
        if (id == block::air || block::is_leaf_material(id)) {
            continue;
        }
        if (include_water && (id == block::water || id == block::calm_water)) {
            return y + 1;
        }
        if (block::material_blocks_motion(id)) {
            return y + 1;
        }
    }
    return 0;
}

std::int32_t World::top_rain_height(
    std::int32_t x,
    std::int32_t z) noexcept {
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const ChunkKey key{chunk_x, chunk_z};
    const Chunk* const chunk = find_chunk(chunk_x, chunk_z);
    if (chunk == nullptr) {
        return -1;
    }
    std::int16_t& cached = rain_heights_[key][Chunk::column_index(local_x, local_z)];
    if (cached == -999) {
        std::int32_t y = 127;
        while (y != 0) {
            const std::uint8_t id = chunk->block(local_x, y, local_z);
            if (block::light_block(id) != 0U || id == block::water || id == block::calm_water) {
                ++y;
                break;
            }
            --y;
        }
        cached = static_cast<std::int16_t>(y == 0 ? -1 : y);
    }
    return cached;
}

std::uint8_t World::sky_brightness(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) const noexcept {
    if (y < 0) {
        return 0;
    }
    if (y >= Chunk::height) {
        return 15;
    }
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const Chunk* const chunk = find_chunk(chunk_x, chunk_z);
    return chunk == nullptr
        ? 0
        : nibble(chunk->sky_light, Chunk::index(local_x, y, local_z));
}

std::uint8_t World::raw_brightness(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) const noexcept {
    // Fresh-world population uses skyDarken == 0. LevelChunk::getRawBrightness
    // therefore returns max(sky nibble, block nibble) for ordinary tiles.
    if (y < 0) {
        return 0;
    }
    if (y >= Chunk::height) {
        return 15;
    }
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const Chunk* const chunk = find_chunk(chunk_x, chunk_z);
    if (chunk == nullptr) {
        return 0;
    }
    const std::size_t index = Chunk::index(local_x, y, local_z);
    return std::max(nibble(chunk->sky_light, index),
                    nibble(chunk->block_light, index));
}

float World::biome_temperature(
    std::int32_t x,
    std::int32_t z) const noexcept {
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const Chunk* const chunk = find_chunk(chunk_x, chunk_z);
    return chunk == nullptr
        ? 0.5F
        : chunk->temperatures[Chunk::column_index(local_x, local_z)];
}

std::int32_t World::biome_id(
    std::int32_t x,
    std::int32_t z) const noexcept {
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const Chunk* const chunk = find_chunk(chunk_x, chunk_z);
    return chunk == nullptr
        ? 0
        : static_cast<std::int32_t>(
            chunk->biomes[Chunk::column_index(local_x, local_z)]);
}

bool World::place_chest(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t metadata) noexcept {
    if (!set_block_and_data(x, y, z, block::chest, metadata, true)) {
        return false;
    }
    chests_.try_emplace({x, y, z});
    generated_chests_dirty_ = true;
    return true;
}

bool World::place_mob_spawner(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    SpawnerType type) noexcept {
    if (!set_block_and_data(x, y, z, block::mob_spawner, 0, true)) {
        return false;
    }
    spawners_.insert_or_assign({x, y, z}, type);
    generated_spawners_dirty_ = true;
    return true;
}

void World::set_chest_slot(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::size_t slot,
    GeneratedItemStack item) noexcept {
    const auto found = chests_.find({x, y, z});
    if (found != chests_.end() && slot < found->second.size()) {
        found->second[slot] = item;
        generated_chests_dirty_ = true;
    }
}

void World::add_villager(GeneratedVillager villager) noexcept {
    villagers_.push_back(villager);
}

std::span<const GeneratedChest> World::generated_chests() const {
    if (generated_chests_dirty_) {
        generated_chests_.clear();
        generated_chests_.reserve(chests_.size());
        for (const auto& [position, slots] : chests_) {
            const auto& [x, y, z] = position;
            generated_chests_.push_back({{x, y, z}, slots});
        }
        generated_chests_dirty_ = false;
    }
    return generated_chests_;
}

std::span<const GeneratedSpawner> World::generated_spawners() const {
    if (generated_spawners_dirty_) {
        generated_spawners_.clear();
        generated_spawners_.reserve(spawners_.size());
        for (const auto& [position, type] : spawners_) {
            const auto& [x, y, z] = position;
            generated_spawners_.push_back({{x, y, z}, type});
        }
        generated_spawners_dirty_ = false;
    }
    return generated_spawners_;
}

std::span<const GeneratedVillager> World::generated_villagers() const noexcept {
    return villagers_;
}

} // namespace mcpe::worldgen::v0_9_0
