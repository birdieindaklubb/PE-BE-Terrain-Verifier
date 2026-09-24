#include "v1_1_5_0/hell_random_level_source.hpp"

#include "detail/wrap.hpp"
#include "v0_9_0/features.hpp"
#include "v0_9_0/liquid.hpp"
#include "v1_1_5_0/blocks.hpp"
#include "v1_1_5_0/hell_features.hpp"
#include "v1_1_5_0/hell_fortress.hpp"

#include <vector>

namespace mcpe::worldgen::v1_1_5_0 {

HellRandomLevelSource::HellRandomLevelSource(std::uint32_t world_seed)
    : world_seed_(world_seed),
      terrain_(world_seed) {
}

Chunk HellRandomLevelSource::generate_base_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    Chunk chunk = terrain_.generate_base_chunk(chunk_x, chunk_z);
    cave_carver_.carve(world_seed_, chunk);
    HellTerrain::recalculate_heightmap(chunk);
    return chunk;
}

void HellRandomLevelSource::load_base_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    const ChunkKey key{chunk_x, chunk_z};
    if (loaded_chunks_.contains(key)) {
        return;
    }
    chunks_.try_emplace(key, generate_base_chunk(chunk_x, chunk_z));
    loaded_chunks_.insert(key);
}

void HellRandomLevelSource::populate_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    ensure_population_neighborhood(chunk_x, chunk_z);
    const ChunkKey key{chunk_x, chunk_z};
    if (populated_chunks_.contains(key)) {
        return;
    }
    populated_chunks_.insert(key);
    post_process(chunk_x, chunk_z);
}

const Chunk* HellRandomLevelSource::find_chunk(
    std::int32_t chunk_x,
    std::int32_t chunk_z) const noexcept {
    const auto found = chunks_.find({chunk_x, chunk_z});
    return found == chunks_.end() ? nullptr : &found->second;
}

bool HellRandomLevelSource::is_populated(
    std::int32_t chunk_x,
    std::int32_t chunk_z) const noexcept {
    return populated_chunks_.contains({chunk_x, chunk_z});
}

std::span<const GeneratedMob> HellRandomLevelSource::generated_mobs() const noexcept {
    return {};
}

std::span<const GeneratedChest>
HellRandomLevelSource::generated_chests() const noexcept {
    return chests_;
}

std::span<const GeneratedSpawner>
HellRandomLevelSource::generated_spawners() const noexcept {
    return spawners_;
}

std::span<const GeneratedVillager>
HellRandomLevelSource::generated_villagers() const noexcept {
    return {};
}

void HellRandomLevelSource::post_process(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    modified_chunks_.clear();

    // Every post-process operation receives the same chunk-coordinate MT
    // stream. Structure writes happen before the ordinary Nether features,
    // matching HellRandomLevelSource::postProcess.
    detail::Mt19937 multiplier_random{world_seed_};
    const std::uint32_t x_multiplier =
        (multiplier_random.next_u32() >> 1U) | 1U;
    const std::uint32_t z_multiplier =
        (multiplier_random.next_u32() >> 1U) | 1U;
    const std::uint32_t coordinate_term = detail::bits(chunk_x) * x_multiplier
        + detail::bits(chunk_z) * z_multiplier;
    detail::Mt19937 random{world_seed_ ^ coordinate_term};

    Chunk& target = chunks_.at({chunk_x, chunk_z});
    std::vector<BlockPosition> fortress_lava_sources;
    std::vector<BlockPosition> fortress_spawners;
    std::vector<GeneratedChest> fortress_chests;
    hell_fortress::apply_nearby_starts(
        world_seed_, target, random,
        &fortress_lava_sources, &fortress_spawners,
        &fortress_chests);
    modified_chunks_.insert({chunk_x, chunk_z});

    // NBCastleEntrance creates its flowing lava during the structure pass.
    // These vectors also avoid treating terrain's existing flowing lava as a
    // new source merely because it occupies the same raw chunk.
    for (const BlockPosition position : fortress_spawners) {
        spawners_.push_back({position, SpawnerType::blaze});
    }
    chests_.insert(
        chests_.end(), fortress_chests.begin(), fortress_chests.end());
    std::vector<v0_9_0::liquid::NetherLavaTick> lava_ticks;
    lava_ticks.reserve(fortress_lava_sources.size() + 24U);
    for (const BlockPosition position : fortress_lava_sources) {
        // A fortress write reaches LiquidBlockDynamic::onPlace, which uses
        // Nether's ten-tick liquid delay.
        lava_ticks.push_back({position.x, position.y, position.z, 10});
    }

    const std::int32_t origin_x = detail::wrapping_mul(chunk_x, Chunk::width);
    const std::int32_t origin_z = detail::wrapping_mul(chunk_z, Chunk::width);
    const auto feature_x = [&]() noexcept {
        return detail::wrapping_add(
            detail::wrapping_add(origin_x, 8),
            static_cast<std::int32_t>(random.next_u32() & 15U));
    };
    const auto feature_z = [&]() noexcept {
        return detail::wrapping_add(
            detail::wrapping_add(origin_z, 8),
            static_cast<std::int32_t>(random.next_u32() & 15U));
    };
    const auto local_x = [&]() noexcept {
        return detail::wrapping_add(
            origin_x,
            static_cast<std::int32_t>(random.next_u32() & 15U));
    };
    const auto local_z = [&]() noexcept {
        return detail::wrapping_add(
            origin_z,
            static_cast<std::int32_t>(random.next_u32() & 15U));
    };

    for (std::int32_t attempt = 0; attempt < 8; ++attempt) {
        const std::int32_t x = feature_x();
        const std::int32_t y = static_cast<std::int32_t>(
            random.next_bounded(120U)) + 4;
        const std::int32_t z = feature_z();
        (void)hell_feature::place_spring(*this, x, y, z, false);
        // postProcess queues every attempted position with delay zero. The
        // queue validates the block ID only when it is drained.
        lava_ticks.push_back({x, y, z, 0});
    }

    const std::int32_t fire_count = static_cast<std::int32_t>(
        random.next_bounded(random.next_bounded(10U) + 1U)) + 1;
    for (std::int32_t attempt = 0; attempt < fire_count; ++attempt) {
        const std::int32_t x = feature_x();
        const std::int32_t y = static_cast<std::int32_t>(
            random.next_bounded(120U)) + 4;
        const std::int32_t z = feature_z();
        hell_feature::place_fire(*this, random, x, y, z);
    }

    const std::int32_t glowstone_count = static_cast<std::int32_t>(
        random.next_bounded(random.next_bounded(10U) + 1U));
    for (std::int32_t attempt = 0; attempt < glowstone_count; ++attempt) {
        const std::int32_t x = feature_x();
        const std::int32_t y = static_cast<std::int32_t>(
            random.next_bounded(120U)) + 4;
        const std::int32_t z = feature_z();
        (void)hell_feature::place_glowstone(*this, random, x, y, z);
    }
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const std::int32_t x = feature_x();
        const std::int32_t y = static_cast<std::int32_t>(random.next_u32() & 127U);
        const std::int32_t z = feature_z();
        (void)hell_feature::place_glowstone(*this, random, x, y, z);
    }

    // The nextInt(1) conditions are always true, but each performs its own
    // MT draw before the origin passed to PlantFeature.
    (void)random.next_bounded(1U);
    const std::int32_t brown_x = feature_x();
    const std::int32_t brown_y = static_cast<std::int32_t>(
        random.next_u32() & 127U);
    const std::int32_t brown_z = feature_z();
    hell_feature::place_mushroom(
        *this, random, brown_x, brown_y, brown_z,
        block::brown_mushroom);
    (void)random.next_bounded(1U);
    const std::int32_t red_x = feature_x();
    const std::int32_t red_y = static_cast<std::int32_t>(
        random.next_u32() & 127U);
    const std::int32_t red_z = feature_z();
    hell_feature::place_mushroom(
        *this, random, red_x, red_y, red_z,
        block::red_mushroom);

    for (std::int32_t attempt = 0; attempt < 16; ++attempt) {
        const std::int32_t x = local_x();
        const std::int32_t y = static_cast<std::int32_t>(
            random.next_bounded(108U)) + 10;
        const std::int32_t z = local_z();
        v0_9_0::feature::place_ore(
            *this, random, x, y, z, block::quartz_ore, 0, 14,
            block::netherrack, -2, v0_9_0::block::rock, false);
    }

    for (std::int32_t attempt = 0; attempt < 16; ++attempt) {
        const std::int32_t x = local_x();
        const std::int32_t y = static_cast<std::int32_t>(
            random.next_bounded(108U)) + 10;
        const std::int32_t z = local_z();
        (void)hell_feature::place_spring(*this, x, y, z, true);
        lava_ticks.push_back({x, y, z, 0});
    }

    for (std::int32_t attempt = 0; attempt < 9; ++attempt) {
        const std::int32_t x = local_x();
        const std::int32_t y = static_cast<std::int32_t>(
            random.next_bounded(14U)) + 23;
        const std::int32_t z = local_z();
        v0_9_0::feature::place_ore(
            *this, random, x, y, z, block::magma_block, 0, 28,
            block::netherrack, -2, v0_9_0::block::rock, false);
    }

    // BlockTickingQueue owns a Random separate from the coordinate stream.
    // Its runtime seed comes from random_device; the final fully-drained lava
    // state is independent of delay-only draws, so a fixed MT seed makes the
    // observable reimplementation reproducible without altering world RNG.
    detail::Mt19937 liquid_random{5489U};
    v0_9_0::liquid::settle_nether_lava(
        *this, liquid_random, lava_ticks);

    recalculate_modified_chunks();
}

void HellRandomLevelSource::ensure_population_neighborhood(
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    for (std::int32_t offset_z = -1; offset_z <= 1; ++offset_z) {
        for (std::int32_t offset_x = -1; offset_x <= 1; ++offset_x) {
            load_base_chunk(
                detail::wrapping_add(chunk_x, offset_x),
                detail::wrapping_add(chunk_z, offset_z));
        }
    }
}

void HellRandomLevelSource::recalculate_modified_chunks() noexcept {
    for (const ChunkKey& key : modified_chunks_) {
        if (auto found = chunks_.find(key); found != chunks_.end()) {
            HellTerrain::recalculate_heightmap(found->second);
        }
    }
}

void HellRandomLevelSource::split_coordinate(
    std::int32_t coordinate,
    std::int32_t& chunk,
    std::int32_t& local) noexcept {
    chunk = coordinate >= 0
        ? coordinate / Chunk::width
        : -1 - static_cast<std::int32_t>(
            (-1LL - static_cast<std::int64_t>(coordinate)) / Chunk::width);
    local = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(coordinate) & 15U);
}

std::uint8_t HellRandomLevelSource::block(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) const noexcept {
    if (static_cast<std::uint32_t>(y) >= static_cast<std::uint32_t>(Chunk::height)) {
        return block::air;
    }
    std::int32_t chunk_x{};
    std::int32_t local_x{};
    std::int32_t chunk_z{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const auto found = chunks_.find({chunk_x, chunk_z});
    return found == chunks_.end()
        ? block::air
        : found->second.block(local_x, y, local_z);
}

std::uint8_t HellRandomLevelSource::data(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) const noexcept {
    if (static_cast<std::uint32_t>(y) >= static_cast<std::uint32_t>(Chunk::height)) {
        return 0;
    }
    std::int32_t chunk_x{};
    std::int32_t local_x{};
    std::int32_t chunk_z{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const auto found = chunks_.find({chunk_x, chunk_z});
    return found == chunks_.end()
        ? 0
        : found->second.block_data(local_x, y, local_z);
}

bool HellRandomLevelSource::set_block_and_data(
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
    std::int32_t local_x{};
    std::int32_t chunk_z{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const auto found = chunks_.find({chunk_x, chunk_z});
    if (found == chunks_.end()) {
        return false;
    }
    Chunk& chunk = found->second;
    if (chunk.block(local_x, y, local_z) == id
        && chunk.block_data(local_x, y, local_z) == metadata) {
        return false;
    }
    chunk.set_block_and_data(local_x, y, local_z, id, metadata);
    modified_chunks_.insert({chunk_x, chunk_z});
    return true;
}

bool HellRandomLevelSource::has_chunks_at(
    std::int32_t x,
    std::int32_t z,
    std::int32_t radius) const noexcept {
    std::int32_t min_chunk_x{};
    std::int32_t ignored{};
    std::int32_t max_chunk_x{};
    std::int32_t min_chunk_z{};
    std::int32_t max_chunk_z{};
    split_coordinate(detail::wrapping_sub(x, radius), min_chunk_x, ignored);
    split_coordinate(detail::wrapping_add(x, radius), max_chunk_x, ignored);
    split_coordinate(detail::wrapping_sub(z, radius), min_chunk_z, ignored);
    split_coordinate(detail::wrapping_add(z, radius), max_chunk_z, ignored);
    for (std::int32_t candidate_z = min_chunk_z;; ++candidate_z) {
        for (std::int32_t candidate_x = min_chunk_x;; ++candidate_x) {
            if (!loaded_chunks_.contains({candidate_x, candidate_z})) {
                return false;
            }
            if (candidate_x == max_chunk_x) {
                break;
            }
        }
        if (candidate_z == max_chunk_z) {
            break;
        }
    }
    return true;
}

} // namespace mcpe::worldgen::v1_1_5_0
