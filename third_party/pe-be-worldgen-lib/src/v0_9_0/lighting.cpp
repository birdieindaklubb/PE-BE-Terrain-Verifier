#include "v0_9_0/lighting.hpp"

#include "detail/wrap.hpp"
#include "v0_9_0/blocks.hpp"
#include "v0_9_0/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <tuple>
#include <vector>

namespace mcpe::worldgen::v0_9_0::lighting {
namespace {

enum class Layer : std::uint8_t {
    sky,
    block,
};

struct Bounds final {
    std::int32_t min_x{};
    std::int32_t min_y{};
    std::int32_t min_z{};
    std::int32_t max_x{};
    std::int32_t max_y{};
    std::int32_t max_z{};
};

struct LightUpdate final {
    Layer layer{};
    Bounds bounds{};
};

void split_coordinate(
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

[[nodiscard]] constexpr std::uint8_t nibble(
    const std::array<std::uint8_t, Chunk::block_count / 2>& values,
    std::size_t index) noexcept {
    const std::uint8_t shift = (index & 1U) == 0U ? 0U : 4U;
    return static_cast<std::uint8_t>((values[index >> 1U] >> shift) & 0x0fU);
}

void set_nibble(
    std::array<std::uint8_t, Chunk::block_count / 2>& values,
    std::size_t index,
    std::uint8_t value) noexcept {
    std::uint8_t& packed = values[index >> 1U];
    const std::uint8_t shift = (index & 1U) == 0U ? 0U : 4U;
    const std::uint8_t mask = static_cast<std::uint8_t>(0x0fU << shift);
    packed = static_cast<std::uint8_t>(
        (packed & static_cast<std::uint8_t>(~mask))
        | static_cast<std::uint8_t>((value & 0x0fU) << shift));
}

[[nodiscard]] constexpr std::uint8_t outside_brightness(Layer layer) noexcept {
    // TileSource::getBrightness returns LightLayer's default value outside
    // the 0..127 build height: 15 for sky and 0 for block light.
    return layer == Layer::sky ? 15U : 0U;
}

[[nodiscard]] std::uint8_t brightness(
    const World& world,
    Layer layer,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    if (y < 0 || y >= Chunk::height) {
        return outside_brightness(layer);
    }

    std::int32_t chunk_x{};
    std::int32_t local_x{};
    std::int32_t chunk_z{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const Chunk* const chunk = world.find_chunk(chunk_x, chunk_z);
    if (chunk == nullptr) {
        return 0;
    }
    const auto& values = layer == Layer::sky
        ? chunk->sky_light
        : chunk->block_light;
    return nibble(values, Chunk::index(local_x, y, local_z));
}

[[nodiscard]] bool set_brightness(
    World& world,
    Layer layer,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t value) noexcept {
    if (y < 0 || y >= Chunk::height) {
        return false;
    }

    std::int32_t chunk_x{};
    std::int32_t local_x{};
    std::int32_t chunk_z{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    Chunk* const chunk = world.find_chunk(chunk_x, chunk_z);
    if (chunk == nullptr) {
        return false;
    }
    auto& values = layer == Layer::sky ? chunk->sky_light : chunk->block_light;
    const std::size_t index = Chunk::index(local_x, y, local_z);
    if (nibble(values, index) == value) {
        return false;
    }
    set_nibble(values, index, value);
    return true;
}

[[nodiscard]] bool can_see_sky(
    const World& world,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    if (y >= Chunk::height) {
        return true;
    }
    if (y < 0) {
        return false;
    }

    std::int32_t chunk_x{};
    std::int32_t local_x{};
    std::int32_t chunk_z{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    const Chunk* const chunk = world.find_chunk(chunk_x, chunk_z);
    // TileSource considers an unavailable chunk sky-lit. Callers which may
    // schedule work additionally require a writable chunk, so this value
    // cannot manufacture an update for unavailable terrain.
    return chunk == nullptr
        || y >= chunk->heightmap[Chunk::column_index(local_x, local_z)];
}

[[nodiscard]] std::uint8_t desired_brightness(
    const World& world,
    Layer layer,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept {
    const std::uint8_t id = world.block(x, y, z);
    std::uint8_t attenuation = block::light_block(id);
    if (attenuation == 0U) {
        attenuation = 1U;
    }

    std::uint8_t value{};
    if (layer == Layer::sky && can_see_sky(world, x, y, z)) {
        value = 15U;
    } else {
        if (layer == Layer::block) {
            value = block::light_emission(id);
            // Fully opaque non-emitting tiles do not inspect neighbours.
            // The remaining cases follow LightUpdate's unsigned gate before
            // it takes the shared neighbour path.
            if (attenuation == 15U && value == 0U) {
                return 0;
            }
        } else if (attenuation == 15U) {
            return 0;
        }
    }

    constexpr std::array<std::array<std::int32_t, 3>, 6> directions{{
        {{-1, 0, 0}}, {{1, 0, 0}}, {{0, 1, 0}},
        {{0, -1, 0}}, {{0, 0, 1}}, {{0, 0, -1}},
    }};
    std::uint8_t neighbour{};
    for (const auto& direction : directions) {
        neighbour = std::max(
            neighbour,
            brightness(
                world,
                layer,
                detail::wrapping_add(x, direction[0]),
                y + direction[1],
                detail::wrapping_add(z, direction[2])));
    }
    std::uint8_t transmitted = static_cast<std::uint8_t>(
        static_cast<std::uint32_t>(neighbour) - attenuation);
    if (transmitted > 15U) {
        transmitted = 0;
    }
    return std::max(value, transmitted);
}

[[nodiscard]] bool expand_if_close_enough(
    LightUpdate& existing,
    const LightUpdate& incoming) noexcept {
    if (existing.layer != incoming.layer) {
        return false;
    }
    const Bounds& old = existing.bounds;
    const Bounds& add = incoming.bounds;
    if (add.min_x >= old.min_x && add.min_y >= old.min_y && add.min_z >= old.min_z
        && add.max_x <= old.max_x && add.max_y <= old.max_y
        && add.max_z <= old.max_z) {
        return true;
    }
    if (add.min_x < detail::wrapping_sub(old.min_x, 1)
        || add.min_y < old.min_y - 1
        || add.min_z < detail::wrapping_sub(old.min_z, 1)
        || add.max_x > detail::wrapping_add(old.max_x, 1)
        || add.max_y > old.max_y + 1
        || add.max_z > detail::wrapping_add(old.max_z, 1)) {
        return false;
    }

    Bounds expanded{
        std::min(old.min_x, add.min_x), std::min(old.min_y, add.min_y),
        std::min(old.min_z, add.min_z), std::max(old.max_x, add.max_x),
        std::max(old.max_y, add.max_y), std::max(old.max_z, add.max_z),
    };
    const std::int64_t old_volume = static_cast<std::int64_t>(old.max_x - old.min_x)
        * static_cast<std::int64_t>(old.max_y - old.min_y)
        * static_cast<std::int64_t>(old.max_z - old.min_z);
    const std::int64_t expanded_volume =
        static_cast<std::int64_t>(expanded.max_x - expanded.min_x)
        * static_cast<std::int64_t>(expanded.max_y - expanded.min_y)
        * static_cast<std::int64_t>(expanded.max_z - expanded.min_z);
    if (expanded_volume - old_volume >= 3) {
        return false;
    }
    existing.bounds = expanded;
    return true;
}

void enqueue(
    std::vector<LightUpdate>& updates,
    LightUpdate update) {
    const std::size_t lookback = std::min<std::size_t>(5, updates.size());
    for (std::size_t offset = 0; offset < lookback; ++offset) {
        if (expand_if_close_enough(updates[updates.size() - 1U - offset], update)) {
            return;
        }
    }
    updates.push_back(update);
}

void enqueue_if_other_than(
    World& world,
    std::vector<LightUpdate>& updates,
    Layer layer,
    std::int32_t x,
    std::int32_t y,
    std::int32_t z,
    std::uint8_t minimum) {
    if (y < 0 || y >= Chunk::height) {
        return;
    }
    std::int32_t chunk_x{};
    std::int32_t local_x{};
    std::int32_t chunk_z{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    if (world.find_chunk(chunk_x, chunk_z) == nullptr) {
        return;
    }

    if (layer == Layer::sky && can_see_sky(world, x, y, z)) {
        minimum = 15U;
    } else if (layer == Layer::block) {
        minimum = std::max(minimum, block::light_emission(world.block(x, y, z)));
    }
    if (brightness(world, layer, x, y, z) != minimum) {
        enqueue(updates, {layer, {x, y, z, x, y, z}});
    }
}

void run_light_update(
    World& world,
    std::vector<LightUpdate>& queued,
    const LightUpdate& update) {
    const Bounds& bounds = update.bounds;
    for (std::int32_t x = bounds.min_x; x <= bounds.max_x; ++x) {
        for (std::int32_t z = bounds.min_z; z <= bounds.max_z; ++z) {
            if (!world.has_chunks_at(x, z, 1)) {
                continue;
            }
            const std::int32_t min_y = std::max(0, bounds.min_y);
            const std::int32_t max_y = std::min(Chunk::height - 1, bounds.max_y);
            for (std::int32_t y = min_y; y <= max_y; ++y) {
                const std::uint8_t before = brightness(world, update.layer, x, y, z);
                const std::uint8_t after = desired_brightness(
                    world, update.layer, x, y, z);
                if (before == after) {
                    continue;
                }
                (void)set_brightness(world, update.layer, x, y, z, after);
                const std::uint8_t threshold = after == 0U
                    ? 0U
                    : static_cast<std::uint8_t>(after - 1U);
                enqueue_if_other_than(
                    world, queued, update.layer,
                    detail::wrapping_sub(x, 1), y, z, threshold);
                enqueue_if_other_than(
                    world, queued, update.layer,
                    x, y - 1, z, threshold);
                enqueue_if_other_than(
                    world, queued, update.layer,
                    x, y, detail::wrapping_sub(z, 1), threshold);
                if (bounds.max_x <= detail::wrapping_add(x, 1)) {
                    enqueue_if_other_than(
                        world, queued, update.layer,
                        detail::wrapping_add(x, 1), y, z, threshold);
                }
                if (bounds.max_y <= y + 1) {
                    enqueue_if_other_than(
                        world, queued, update.layer,
                        x, y + 1, z, threshold);
                }
                if (bounds.max_z <= detail::wrapping_add(z, 1)) {
                    enqueue_if_other_than(
                        world, queued, update.layer,
                        x, y, detail::wrapping_add(z, 1), threshold);
                }
            }
        }
    }
}

void drain_light_updates(World& world, std::vector<LightUpdate>& queued) {
    // Level's background task consumes its LightUpdate vector from the end.
    // Complete the queued work here because a generated Chunk result has no
    // live game tick thread to execute the asynchronous task later.
    while (!queued.empty()) {
        const LightUpdate update = queued.back();
        queued.pop_back();
        run_light_update(world, queued, update);
    }
}

void recalculate_column(
    Chunk& chunk,
    std::int32_t local_x,
    std::int32_t local_z) noexcept {
    const std::size_t column = Chunk::column_index(local_x, local_z);
    const std::uint8_t old_height = chunk.heightmap[column];

    std::int32_t height = Chunk::height - 1;
    while (height != 0
           && block::light_block(chunk.block(local_x, height - 1, local_z)) == 0U) {
        --height;
    }
    chunk.heightmap[column] = static_cast<std::uint8_t>(height);
    if (old_height < height) {
        for (std::int32_t y = old_height; y < height; ++y) {
            set_nibble(chunk.sky_light, Chunk::index(local_x, y, local_z), 0);
        }
    }

    std::uint8_t light = 15U;
    for (std::int32_t y = height; y > 0 && light != 0U;) {
        --y;
        std::uint8_t attenuation = block::light_block(chunk.block(local_x, y, local_z));
        if (attenuation == 0U) {
            attenuation = 1U;
        }
        light = light > attenuation
            ? static_cast<std::uint8_t>(light - attenuation)
            : 0U;
        set_nibble(chunk.sky_light, Chunk::index(local_x, y, local_z), light);
    }
}

[[nodiscard]] Bounds sky_bounds(
    const Chunk& chunk,
    std::int32_t chunk_x,
    std::int32_t chunk_z) noexcept {
    std::int32_t minimum_height = Chunk::height - 1;
    std::int32_t maximum_height{};
    for (const std::uint8_t height : chunk.heightmap) {
        minimum_height = std::min(minimum_height, static_cast<std::int32_t>(height));
        maximum_height = std::max(maximum_height, static_cast<std::int32_t>(height));
    }
    const std::int32_t origin_x = detail::wrapping_mul(chunk_x, Chunk::width);
    const std::int32_t origin_z = detail::wrapping_mul(chunk_z, Chunk::width);
    return {
        detail::wrapping_sub(origin_x, 15), minimum_height - 4,
        detail::wrapping_sub(origin_z, 15),
        detail::wrapping_add(origin_x, 30), maximum_height,
        detail::wrapping_add(origin_z, 30),
    };
}

} // namespace

void update_lights_and_heights(
    World& world,
    std::int32_t chunk_x,
    std::int32_t chunk_z) {
    Chunk* const target = world.find_chunk(chunk_x, chunk_z);
    if (target == nullptr) {
        return;
    }

    const World::ChunkKey key{chunk_x, chunk_z};
    std::vector<World::BlockKey> emitters;
    if (const auto found = world.deferred_light_emitters_.find(key);
        found != world.deferred_light_emitters_.end()) {
        emitters = std::move(found->second);
        world.deferred_light_emitters_.erase(found);
    }

    std::vector<LightUpdate> queued;
    if (!emitters.empty()) {
        const auto [first_x, first_y, first_z] = emitters.front();
        LightUpdate block_update{
            Layer::block, {first_x, first_y, first_z, first_x, first_y, first_z}};
        for (const World::BlockKey& emitter : emitters) {
            const auto [x, y, z] = emitter;
            (void)set_brightness(
                world, Layer::block, x, y, z,
                block::light_emission(world.block(x, y, z)));
            block_update.bounds.min_x = std::min(
                block_update.bounds.min_x, detail::wrapping_sub(x, 15));
            block_update.bounds.min_y = std::min(block_update.bounds.min_y, y - 15);
            block_update.bounds.min_z = std::min(
                block_update.bounds.min_z, detail::wrapping_sub(z, 15));
            block_update.bounds.max_x = std::max(
                block_update.bounds.max_x, detail::wrapping_add(x, 15));
            block_update.bounds.max_y = std::max(block_update.bounds.max_y, y + 15);
            block_update.bounds.max_z = std::max(
                block_update.bounds.max_z, detail::wrapping_add(z, 15));
        }
        run_light_update(world, queued, block_update);
    }

    // updateLightsAndHeights calls _recalcHeight for each target column with
    // a null TileSource. This preserves existing horizontal sky light rather
    // than clearing a chunk-wide layer before the following LightUpdate.
    for (std::int32_t local_x = 0; local_x < Chunk::width; ++local_x) {
        for (std::int32_t local_z = 0; local_z < Chunk::width; ++local_z) {
            recalculate_column(*target, local_x, local_z);
        }
    }
    run_light_update(world, queued, {Layer::sky, sky_bounds(*target, chunk_x, chunk_z)});
    drain_light_updates(world, queued);
}

} // namespace mcpe::worldgen::v0_9_0::lighting
