#include "v0_9_0/biome_layers.hpp"

#include "v0_9_0/biome_ids.hpp"
#include "v0_9_0/layer_rng.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <memory>
#include <utility>

namespace mcpe::worldgen::v0_9_0 {
namespace {

struct Area final {
    Area(std::int32_t width, std::int32_t height)
        : width(width), height(height), cells(
              static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        assert(width >= 0 && height >= 0);
    }

    [[nodiscard]] std::int32_t& at(std::int32_t x, std::int32_t z) noexcept {
        return cells[static_cast<std::size_t>(z) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x)];
    }

    [[nodiscard]] std::int32_t at(std::int32_t x, std::int32_t z) const noexcept {
        return cells[static_cast<std::size_t>(z) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x)];
    }

    std::int32_t width;
    std::int32_t height;
    std::vector<std::int32_t> cells;
};

[[nodiscard]] constexpr std::int32_t wrapping_add(
    std::int32_t left,
    std::int32_t right) noexcept {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(left)
        + std::bit_cast<std::uint32_t>(right);
    return std::bit_cast<std::int32_t>(bits);
}

// ARM arithmetic in the target is modulo 2^32.  C++ signed overflow is
// undefined, so world-coordinate operations deliberately use their bit pattern.
[[nodiscard]] constexpr std::int32_t wrapping_multiply(
    std::int32_t left,
    std::int32_t right) noexcept {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(left)
        * std::bit_cast<std::uint32_t>(right);
    return std::bit_cast<std::int32_t>(bits);
}

[[nodiscard]] constexpr std::int32_t wrapping_negate(std::int32_t value) noexcept {
    return std::bit_cast<std::int32_t>(
        std::uint32_t{0} - std::bit_cast<std::uint32_t>(value));
}

[[nodiscard]] constexpr std::int32_t arithmetic_shift_right(
    std::int32_t value,
    unsigned amount) noexcept {
    if (value >= 0) {
        return value >> amount;
    }
    const std::int64_t magnitude = -static_cast<std::int64_t>(value);
    return static_cast<std::int32_t>(-
        ((magnitude + ((std::int64_t{1} << amount) - 1)) >> amount));
}

class Layer;
using LayerPtr = std::shared_ptr<Layer>;

class Layer {
public:
    Layer(std::int64_t base_seed, LayerPtr parent = {})
        : random_(base_seed), parent_(std::move(parent)) {
    }
    virtual ~Layer() = default;

    virtual void init_world_seed(std::uint32_t world_seed) {
        if (parent_ != nullptr) {
            parent_->init_world_seed(world_seed);
        }
        random_.set_world_seed(world_seed);
    }

    [[nodiscard]] virtual Area get(
        std::int32_t x,
        std::int32_t z,
        std::int32_t width,
        std::int32_t height) = 0;

protected:
    [[nodiscard]] Area parent(
        std::int32_t x,
        std::int32_t z,
        std::int32_t width,
        std::int32_t height) {
        assert(parent_ != nullptr);
        return parent_->get(x, z, width, height);
    }

    void init_cell(std::int32_t x, std::int32_t z) noexcept {
        random_.set_chunk_seed(x, z);
    }

    [[nodiscard]] std::int32_t next(std::int32_t bound) noexcept {
        return random_.next_int(bound);
    }

    [[nodiscard]] std::int32_t choose(
        std::int32_t first,
        std::int32_t second) noexcept {
        return next(2) == 0 ? first : second;
    }

    [[nodiscard]] std::int32_t mode_or_random(
        std::int32_t north_west,
        std::int32_t north_east,
        std::int32_t south_west,
        std::int32_t south_east) noexcept {
        if (north_east == south_west && south_west == south_east) {
            return north_east;
        }
        if (north_west == north_east && north_west == south_west) {
            return north_west;
        }
        if (north_west == north_east && north_west == south_east) {
            return north_west;
        }
        if (north_west == south_west && north_west == south_east) {
            return north_west;
        }
        if (north_west == north_east && south_west != south_east) {
            return north_west;
        }
        if (north_west == south_west && north_east != south_east) {
            return north_west;
        }
        if (north_west == south_east && north_east != south_west) {
            return north_west;
        }
        if (north_east == south_west && north_west != south_east) {
            return north_east;
        }
        if (north_east == south_east && north_west != south_west) {
            return north_east;
        }
        if (south_west == south_east && north_west != north_east) {
            return south_west;
        }
        switch (next(4)) {
        case 0:
            return north_west;
        case 1:
            return north_east;
        case 2:
            return south_west;
        default:
            return south_east;
        }
    }

private:
    LayerRandom random_;
    LayerPtr parent_;
};

class IslandLayer final : public Layer {
public:
    explicit IslandLayer(std::int64_t seed) : Layer(seed) {
    }

    [[nodiscard]] Area get(
        std::int32_t x,
        std::int32_t z,
        std::int32_t width,
        std::int32_t height) override {
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t world_x = wrapping_add(x, dx);
                const std::int32_t world_z = wrapping_add(z, dz);
                init_cell(world_x, world_z);
                result.at(dx, dz) = next(10) == 0 ? 1 : 0;
            }
        }
        if (x <= 0 && wrapping_add(x, width) > 0
            && z <= 0 && wrapping_add(z, height) > 0) {
            result.at(wrapping_negate(x), wrapping_negate(z)) = 1;
        }
        return result;
    }
};

class AddIslandLayer final : public Layer {
public:
    AddIslandLayer(std::int64_t seed, LayerPtr parent)
        : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(
        std::int32_t x,
        std::int32_t z,
        std::int32_t width,
        std::int32_t height) override {
        Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t north_west = input.at(dx, dz);
                const std::int32_t north_east = input.at(dx + 2, dz);
                const std::int32_t south_west = input.at(dx, dz + 2);
                const std::int32_t south_east = input.at(dx + 2, dz + 2);
                const std::int32_t center = input.at(dx + 1, dz + 1);
                init_cell(wrapping_add(x, dx), wrapping_add(z, dz));

                if (center == 0 && (north_west != 0 || north_east != 0
                    || south_west != 0 || south_east != 0)) {
                    std::int32_t candidate = 1;
                    std::int32_t count = 1;
                    if (north_west != 0 && next(count++) == 0) {
                        candidate = north_west;
                    }
                    if (north_east != 0 && next(count++) == 0) {
                        candidate = north_east;
                    }
                    if (south_west != 0 && next(count++) == 0) {
                        candidate = south_west;
                    }
                    if (south_east != 0 && next(count++) == 0) {
                        candidate = south_east;
                    }
                    result.at(dx, dz) = next(3) == 0 || candidate == 4
                        ? candidate : 0;
                } else if (center > 0 && (north_west == 0 || north_east == 0
                    || south_west == 0 || south_east == 0) && next(5) == 0) {
                    result.at(dx, dz) = center == 4 ? 4 : 0;
                } else {
                    result.at(dx, dz) = center;
                }
            }
        }
        return result;
    }
};

class ZoomLayer final : public Layer {
public:
    ZoomLayer(std::int64_t seed, LayerPtr parent, bool fuzzy = false)
        : Layer(seed, std::move(parent)), fuzzy_(fuzzy) {
    }

    [[nodiscard]] Area get(
        std::int32_t x,
        std::int32_t z,
        std::int32_t width,
        std::int32_t height) override {
        const std::int32_t parent_x = arithmetic_shift_right(x, 1);
        const std::int32_t parent_z = arithmetic_shift_right(z, 1);
        const std::int32_t parent_width = arithmetic_shift_right(width, 1) + 2;
        const std::int32_t parent_height = arithmetic_shift_right(height, 1) + 2;
        Area input = parent(parent_x, parent_z, parent_width, parent_height);
        const std::int32_t expanded_width = (parent_width - 1) * 2;
        Area expanded{expanded_width, (parent_height - 1) * 2};

        for (std::int32_t pz = 0; pz < parent_height - 1; ++pz) {
            for (std::int32_t px = 0; px < parent_width - 1; ++px) {
                const std::int32_t north_west = input.at(px, pz);
                const std::int32_t north_east = input.at(px + 1, pz);
                const std::int32_t south_west = input.at(px, pz + 1);
                const std::int32_t south_east = input.at(px + 1, pz + 1);
                init_cell(
                    wrapping_multiply(wrapping_add(parent_x, px), 2),
                    wrapping_multiply(wrapping_add(parent_z, pz), 2));
                expanded.at(px * 2, pz * 2) = north_west;
                expanded.at(px * 2, pz * 2 + 1) = choose(north_west, south_west);
                expanded.at(px * 2 + 1, pz * 2) = choose(north_west, north_east);
                expanded.at(px * 2 + 1, pz * 2 + 1) = fuzzy_
                    ? fuzzy_pick(north_west, north_east, south_west, south_east)
                    : mode_or_random(north_west, north_east, south_west, south_east);
            }
        }

        Area result{width, height};
        const std::int32_t offset_x = x & 1;
        const std::int32_t offset_z = z & 1;
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                result.at(dx, dz) = expanded.at(dx + offset_x, dz + offset_z);
            }
        }
        return result;
    }

private:
    [[nodiscard]] std::int32_t fuzzy_pick(
        std::int32_t north_west,
        std::int32_t north_east,
        std::int32_t south_west,
        std::int32_t south_east) noexcept {
        switch (next(4)) {
        case 0:
            return north_west;
        case 1:
            return north_east;
        case 2:
            return south_west;
        default:
            return south_east;
        }
    }

    bool fuzzy_;
};

class RemoveTooMuchOceanLayer final : public Layer {
public:
    RemoveTooMuchOceanLayer(std::int64_t seed, LayerPtr parent)
        : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t center = input.at(dx + 1, dz + 1);
                const std::int32_t north_west = input.at(dx, dz);
                const std::int32_t north_east = input.at(dx + 2, dz);
                const std::int32_t south_west = input.at(dx, dz + 2);
                const std::int32_t south_east = input.at(dx + 2, dz + 2);
                result.at(dx, dz) = center;
                init_cell(wrapping_add(x, dx), wrapping_add(z, dz));
                if (center == 0 && north_west == 0 && north_east == 0
                    && south_west == 0 && south_east == 0) {
                    if (next(2) == 0) {
                        result.at(dx, dz) = 1;
                    }
                }
            }
        }
        return result;
    }
};

class AddSnowLayer final : public Layer {
public:
    AddSnowLayer(std::int64_t seed, LayerPtr parent) : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t value = input.at(dx + 1, dz + 1);
                init_cell(wrapping_add(x, dx), wrapping_add(z, dz));
                if (value == 0) {
                    result.at(dx, dz) = 0;
                } else {
                    const std::int32_t choice = next(6);
                    result.at(dx, dz) = choice == 0 ? 4 : choice == 1 ? 3 : 1;
                }
            }
        }
        return result;
    }
};

enum class EdgeMode { cool_warm, heat_ice, special };

class EdgeLayer final : public Layer {
public:
    EdgeLayer(std::int64_t seed, LayerPtr parent, EdgeMode mode)
        : Layer(seed, std::move(parent)), mode_(mode) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        if (mode_ == EdgeMode::special) {
            Area input = parent(x, z, width, height);
            Area result{width, height};
            for (std::int32_t dz = 0; dz < height; ++dz) {
                for (std::int32_t dx = 0; dx < width; ++dx) {
                    std::int32_t value = input.at(dx, dz);
                    init_cell(wrapping_add(x, dx), wrapping_add(z, dz));
                    if (value != 0 && next(13) == 0) {
                        value |= (next(15) + 1) << 8;
                    }
                    result.at(dx, dz) = value;
                }
            }
            return result;
        }

        Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t center = input.at(dx + 1, dz + 1);
                const std::int32_t north = input.at(dx + 1, dz);
                const std::int32_t east = input.at(dx + 2, dz + 1);
                const std::int32_t west = input.at(dx, dz + 1);
                const std::int32_t south = input.at(dx + 1, dz + 2);
                if (mode_ == EdgeMode::cool_warm && center == 1
                    && (north == 3 || east == 3 || west == 3 || south == 3
                        || north == 4 || east == 4 || west == 4 || south == 4)) {
                    result.at(dx, dz) = 2;
                } else if (mode_ == EdgeMode::heat_ice && center == 4
                    && (north == 1 || east == 1 || west == 1 || south == 1)) {
                    result.at(dx, dz) = 3;
                } else if (mode_ == EdgeMode::heat_ice && center == 4
                    && (north == 2 || east == 2 || west == 2 || south == 2)) {
                    result.at(dx, dz) = 3;
                } else {
                    result.at(dx, dz) = center;
                }
            }
        }
        return result;
    }

private:
    EdgeMode mode_;
};

class AddMushroomIslandLayer final : public Layer {
public:
    AddMushroomIslandLayer(std::int64_t seed, LayerPtr parent) : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t center = input.at(dx + 1, dz + 1);
                const std::int32_t north_west = input.at(dx, dz);
                const std::int32_t north_east = input.at(dx + 2, dz);
                const std::int32_t south_west = input.at(dx, dz + 2);
                const std::int32_t south_east = input.at(dx + 2, dz + 2);
                init_cell(wrapping_add(x, dx), wrapping_add(z, dz));
                if (center == 0 && north_west == 0 && north_east == 0
                    && south_west == 0 && south_east == 0 && next(100) == 0) {
                    result.at(dx, dz) = biome::mushroom_island;
                } else {
                    result.at(dx, dz) = center;
                }
            }
        }
        return result;
    }
};

class AddDeepOceanLayer final : public Layer {
public:
    AddDeepOceanLayer(std::int64_t seed, LayerPtr parent) : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t center = input.at(dx + 1, dz + 1);
                const int ocean_neighbors = (input.at(dx, dz + 1) == 0)
                    + (input.at(dx + 2, dz + 1) == 0)
                    + (input.at(dx + 1, dz) == 0)
                    + (input.at(dx + 1, dz + 2) == 0);
                result.at(dx, dz) = center == 0 && ocean_neighbors > 3
                    ? biome::deep_ocean : center;
            }
        }
        return result;
    }
};

class BiomeInitLayer final : public Layer {
public:
    BiomeInitLayer(std::int64_t seed, LayerPtr parent, std::int32_t generator_type_code)
        : Layer(seed, std::move(parent)), generator_type_code_(generator_type_code) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area input = parent(x, z, width, height);
        Area result{width, height};
        static constexpr std::array infinite_medium = {biome::forest, biome::roofed_forest,
            biome::extreme_hills, biome::plains, biome::plains, biome::plains,
            biome::birch_forest, biome::swampland};
        static constexpr std::array legacy_medium = {biome::forest, biome::extreme_hills,
            biome::plains, biome::plains, biome::plains, biome::birch_forest,
            biome::swampland};
        static constexpr std::array warm = {biome::desert, biome::desert, biome::desert,
            biome::savanna, biome::savanna, biome::plains};
        static constexpr std::array cold = {biome::forest, biome::extreme_hills,
            biome::taiga, biome::plains};
        static constexpr std::array ice = {biome::ice_flats, biome::ice_flats,
            biome::ice_flats, biome::cold_taiga};

        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                init_cell(wrapping_add(x, dx), wrapping_add(z, dz));
                const std::int32_t value = input.at(dx, dz);
                const std::int32_t climate = value & ~0xf00;
                const std::int32_t special = (value & 0xf00) >> 8;
                if (biome::is_ocean(climate) || climate == biome::mushroom_island) {
                    result.at(dx, dz) = climate;
                } else if (climate == 1) {
                    result.at(dx, dz) = special > 0
                        ? (next(3) == 0 ? biome::mesa_plateau_forest : biome::mesa_plateau)
                        : warm[next(6)];
                } else if (climate == 2) {
                    result.at(dx, dz) = special > 0 ? biome::jungle
                        : generator_type_code_ == 1
                            ? infinite_medium[static_cast<std::size_t>(next(8))]
                            : legacy_medium[static_cast<std::size_t>(next(7))];
                } else if (climate == 3) {
                    result.at(dx, dz) = special > 0 ? biome::mega_taiga : cold[next(4)];
                } else if (climate == 4) {
                    result.at(dx, dz) = ice[next(4)];
                } else {
                    result.at(dx, dz) = biome::mushroom_island;
                }
            }
        }
        return result;
    }

private:
    std::int32_t generator_type_code_;
};

class RiverInitLayer final : public Layer {
public:
    RiverInitLayer(std::int64_t seed, LayerPtr parent) : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area input = parent(x, z, width, height);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                init_cell(wrapping_add(x, dx), wrapping_add(z, dz));
                result.at(dx, dz) = input.at(dx, dz) > 0 ? next(299999) + 2 : 0;
            }
        }
        return result;
    }
};

class BiomeEdgeLayer final : public Layer {
public:
    BiomeEdgeLayer(std::int64_t seed, LayerPtr parent, bool enable_swamp_edges)
        : Layer(seed, std::move(parent)), enable_swamp_edges_(enable_swamp_edges) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override;

private:
    [[nodiscard]] static bool valid_temperature_edge(std::int32_t a, std::int32_t b) noexcept {
        if (biome::is_same(a, b)) {
            return true;
        }
        const biome::TemperatureCategory left = biome::temperature_category(a);
        const biome::TemperatureCategory right = biome::temperature_category(b);
        return left != biome::TemperatureCategory::unknown
            && right != biome::TemperatureCategory::unknown
            && (left == right || left == biome::TemperatureCategory::medium
                || right == biome::TemperatureCategory::medium);
    }

    [[nodiscard]] static bool all_same_type(
        std::int32_t center,
        std::int32_t north,
        std::int32_t east,
        std::int32_t west,
        std::int32_t south) noexcept {
        return biome::is_same(north, center) && biome::is_same(east, center)
            && biome::is_same(west, center) && biome::is_same(south, center);
    }

    bool enable_swamp_edges_;
};

Area BiomeEdgeLayer::get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) {
    Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
    Area result{width, height};
    for (std::int32_t dz = 0; dz < height; ++dz) {
        for (std::int32_t dx = 0; dx < width; ++dx) {
            const std::int32_t north = input.at(dx + 1, dz);
            const std::int32_t east = input.at(dx + 2, dz + 1);
            const std::int32_t west = input.at(dx, dz + 1);
            const std::int32_t south = input.at(dx + 1, dz + 2);
            const std::int32_t center = input.at(dx + 1, dz + 1);
            init_cell(wrapping_add(x, dx), wrapping_add(z, dz));

            if (biome::is_same(center, biome::extreme_hills)) {
                result.at(dx, dz) = valid_temperature_edge(north, biome::extreme_hills)
                        && valid_temperature_edge(east, biome::extreme_hills)
                        && valid_temperature_edge(west, biome::extreme_hills)
                        && valid_temperature_edge(south, biome::extreme_hills)
                    ? center : biome::smaller_extreme_hills;
            } else if (center == biome::mesa_plateau_forest) {
                result.at(dx, dz) = all_same_type(center, north, east, west, south)
                    ? center : biome::mesa;
            } else if (center == biome::mesa_plateau) {
                result.at(dx, dz) = all_same_type(center, north, east, west, south)
                    ? center : biome::mesa;
            } else if (center == biome::mega_taiga) {
                result.at(dx, dz) = all_same_type(center, north, east, west, south)
                    ? center : biome::taiga;
            } else if (center == biome::desert
                && (north == biome::ice_flats || east == biome::ice_flats
                    || west == biome::ice_flats || south == biome::ice_flats)) {
                result.at(dx, dz) = biome::extreme_hills_plus;
            } else if (!enable_swamp_edges_ || center != biome::swampland) {
                result.at(dx, dz) = center;
            } else if (north == biome::desert || east == biome::desert || west == biome::desert
                || south == biome::desert || north == biome::cold_taiga
                || east == biome::cold_taiga || west == biome::cold_taiga
                || south == biome::cold_taiga || north == biome::ice_flats
                || east == biome::ice_flats || west == biome::ice_flats
                || south == biome::ice_flats) {
                result.at(dx, dz) = biome::plains;
            } else if (north == biome::jungle || east == biome::jungle
                || west == biome::jungle || south == biome::jungle) {
                result.at(dx, dz) = biome::jungle_edge;
            } else {
                result.at(dx, dz) = biome::swampland;
            }
        }
    }
    return result;
}

class RegionHillsLayer final : public Layer {
public:
    RegionHillsLayer(std::int64_t seed, LayerPtr biome_parent, LayerPtr river_parent)
        : Layer(seed, std::move(biome_parent)), river_parent_(std::move(river_parent)) {
    }

    void init_world_seed(std::uint32_t world_seed) override {
        Layer::init_world_seed(world_seed);
        river_parent_->init_world_seed(world_seed);
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override;

private:
    [[nodiscard]] static bool has_biome(std::int32_t id) noexcept;
    [[nodiscard]] std::int32_t hill_variant(std::int32_t biome_id);

    LayerPtr river_parent_;
};

bool RegionHillsLayer::has_biome(std::int32_t id) noexcept {
    switch (id) {
    case biome::plains + 128:
    case biome::desert + 128:
    case biome::extreme_hills + 128:
    case biome::forest + 128:
    case biome::taiga + 128:
    case biome::swampland + 128:
    case biome::ice_flats + 128:
    case biome::jungle + 128:
    case biome::jungle_edge + 128:
    case biome::birch_forest + 128:
    case biome::birch_forest_hills + 128:
    case biome::roofed_forest + 128:
    case biome::cold_taiga + 128:
    case biome::mega_taiga + 128:
    case biome::mega_taiga_hills + 128:
    case biome::extreme_hills_plus + 128:
    case biome::savanna + 128:
    case biome::savanna_plateau + 128:
    case biome::mesa + 128:
    case biome::mesa_plateau_forest + 128:
    case biome::mesa_plateau + 128:
        return true;
    default:
        return false;
    }
}

std::int32_t RegionHillsLayer::hill_variant(std::int32_t biome_id) {
    switch (biome_id) {
    case biome::desert: return biome::desert_hills;
    case biome::forest: return biome::forest_hills;
    case biome::birch_forest: return biome::birch_forest_hills;
    case biome::roofed_forest: return biome::plains;
    case biome::taiga: return biome::taiga_hills;
    case biome::mega_taiga: return biome::mega_taiga_hills;
    case biome::cold_taiga: return biome::cold_taiga_hills;
    case biome::plains: return next(3) == 0 ? biome::forest_hills : biome::forest;
    case biome::ice_flats: return biome::ice_mountains;
    case biome::jungle: return biome::jungle_hills;
    case biome::ocean: return biome::deep_ocean;
    case biome::extreme_hills: return biome::extreme_hills_plus;
    case biome::savanna: return biome::savanna_plateau;
    case biome::deep_ocean:
        if (next(3) == 0) {
            return next(2) == 0 ? biome::plains : biome::forest;
        }
        return biome::deep_ocean;
    default:
        return biome::is_same(biome_id, biome::mesa_plateau_forest)
            ? biome::mesa : biome_id;
    }
}

Area RegionHillsLayer::get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) {
    Area biomes = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
    Area rivers = river_parent_->get(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
    Area result{width, height};
    for (std::int32_t dz = 0; dz < height; ++dz) {
        for (std::int32_t dx = 0; dx < width; ++dx) {
            init_cell(wrapping_add(x, dx), wrapping_add(z, dz));
            const std::int32_t center = biomes.at(dx + 1, dz + 1);
            const std::int32_t river_value = rivers.at(dx + 1, dz + 1);
            const bool mutation = river_value >= 2 && (river_value - 2) % 29 == 0;
            if (center != 0 && river_value >= 2 && (river_value - 2) % 29 == 1
                && center < 128) {
                result.at(dx, dz) = has_biome(center + 128) ? center + 128 : center;
                continue;
            }
            if (next(3) != 0 && !mutation) {
                result.at(dx, dz) = center;
                continue;
            }
            std::int32_t replacement = hill_variant(center);
            if (mutation && replacement != center) {
                replacement += 128;
                if (!has_biome(replacement)) {
                    replacement = center;
                }
            }
            if (replacement != center) {
                const std::int32_t north = biomes.at(dx + 1, dz);
                const std::int32_t east = biomes.at(dx + 2, dz + 1);
                const std::int32_t west = biomes.at(dx, dz + 1);
                const std::int32_t south = biomes.at(dx + 1, dz + 2);
                const int same_neighbors = biome::is_same(north, center)
                    + biome::is_same(east, center) + biome::is_same(west, center)
                    + biome::is_same(south, center);
                result.at(dx, dz) = same_neighbors > 2 ? replacement : center;
            } else {
                result.at(dx, dz) = center;
            }
        }
    }
    return result;
}

class RiverLayer final : public Layer {
public:
    RiverLayer(std::int64_t seed, LayerPtr parent) : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
        Area result{width, height};
        const auto filter = [](std::int32_t value) {
            return value > 1 ? (value & 1) + 2 : value;
        };
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t center = filter(input.at(dx + 1, dz + 1));
                result.at(dx, dz) = center == filter(input.at(dx + 1, dz))
                        && center == filter(input.at(dx + 2, dz + 1))
                        && center == filter(input.at(dx, dz + 1))
                        && center == filter(input.at(dx + 1, dz + 2))
                    ? -1 : biome::river;
            }
        }
        return result;
    }
};

class SmoothLayer final : public Layer {
public:
    SmoothLayer(std::int64_t seed, LayerPtr parent) : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t north = input.at(dx + 1, dz);
                const std::int32_t east = input.at(dx + 2, dz + 1);
                const std::int32_t west = input.at(dx, dz + 1);
                const std::int32_t south = input.at(dx + 1, dz + 2);
                std::int32_t value = input.at(dx + 1, dz + 1);
                if (north == south && west == east) {
                    init_cell(wrapping_add(x, dx), wrapping_add(z, dz));
                    value = next(2) == 0 ? north : west;
                } else if (north == south) {
                    value = north;
                } else if (west == east) {
                    value = west;
                }
                result.at(dx, dz) = value;
            }
        }
        return result;
    }
};

class RareBiomeSpotLayer final : public Layer {
public:
    RareBiomeSpotLayer(std::int64_t seed, LayerPtr parent) : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                init_cell(wrapping_add(x, dx), wrapping_add(z, dz));
                const std::int32_t value = input.at(dx + 1, dz + 1);
                result.at(dx, dz) = next(57) == 0 && value == biome::plains
                    ? biome::plains + 128 : value;
            }
        }
        return result;
    }
};

class ShoreLayer final : public Layer {
public:
    ShoreLayer(std::int64_t seed, LayerPtr parent) : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override;

private:
    [[nodiscard]] static bool jungle_compatible(std::int32_t id) noexcept {
        return biome::type_of(id) == biome::Type::jungle || id == biome::jungle_edge
            || id == biome::forest || id == biome::taiga || biome::is_ocean(id);
    }
};

Area ShoreLayer::get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) {
    Area input = parent(wrapping_add(x, -1), wrapping_add(z, -1), width + 2, height + 2);
    Area result{width, height};
    for (std::int32_t dz = 0; dz < height; ++dz) {
        for (std::int32_t dx = 0; dx < width; ++dx) {
            const std::int32_t north = input.at(dx + 1, dz);
            const std::int32_t east = input.at(dx + 2, dz + 1);
            const std::int32_t west = input.at(dx, dz + 1);
            const std::int32_t south = input.at(dx + 1, dz + 2);
            const std::int32_t center = input.at(dx + 1, dz + 1);
            const bool touches_ocean = biome::is_ocean(north) || biome::is_ocean(east)
                || biome::is_ocean(west) || biome::is_ocean(south);
            if (center == biome::mushroom_island) {
                result.at(dx, dz) = touches_ocean ? biome::mushroom_island_shore : center;
            } else if (biome::type_of(center) == biome::Type::jungle) {
                if (!jungle_compatible(north) || !jungle_compatible(east)
                    || !jungle_compatible(west) || !jungle_compatible(south)) {
                    result.at(dx, dz) = biome::jungle_edge;
                } else {
                    result.at(dx, dz) = touches_ocean ? biome::beach : center;
                }
            } else if (center == biome::extreme_hills || center == biome::extreme_hills_plus
                || center == biome::smaller_extreme_hills) {
                result.at(dx, dz) = touches_ocean ? biome::stone_beach : center;
            } else if (biome::is_snow_covered(center)) {
                result.at(dx, dz) = touches_ocean ? biome::cold_beach : center;
            } else if (center == biome::mesa || center == biome::mesa_plateau_forest) {
                if (!touches_ocean && (biome::type_of(north) != biome::Type::mesa
                    || biome::type_of(east) != biome::Type::mesa
                    || biome::type_of(west) != biome::Type::mesa
                    || biome::type_of(south) != biome::Type::mesa)) {
                    result.at(dx, dz) = biome::desert;
                } else {
                    result.at(dx, dz) = center;
                }
            } else if (biome::is_ocean(center) || center == biome::river || center == biome::swampland) {
                result.at(dx, dz) = center;
            } else {
                result.at(dx, dz) = touches_ocean ? biome::beach : center;
            }
        }
    }
    return result;
}

class RiverMixerLayer final : public Layer {
public:
    RiverMixerLayer(std::int64_t seed, LayerPtr biome_parent, LayerPtr river_parent)
        : Layer(seed, std::move(biome_parent)), river_parent_(std::move(river_parent)) {
    }

    void init_world_seed(std::uint32_t world_seed) override {
        Layer::init_world_seed(world_seed);
        river_parent_->init_world_seed(world_seed);
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override {
        Area biomes = parent(x, z, width, height);
        Area rivers = river_parent_->get(x, z, width, height);
        Area result{width, height};
        for (std::int32_t dz = 0; dz < height; ++dz) {
            for (std::int32_t dx = 0; dx < width; ++dx) {
                const std::int32_t biome_id = biomes.at(dx, dz);
                const std::int32_t river_id = rivers.at(dx, dz);
                if (biome_id == biome::ocean || biome_id == biome::deep_ocean
                    || river_id != biome::river) {
                    result.at(dx, dz) = biome_id;
                } else if (biome_id == biome::ice_flats) {
                    result.at(dx, dz) = biome::frozen_river;
                } else if (biome_id == biome::mushroom_island
                    || biome_id == biome::mushroom_island_shore) {
                    result.at(dx, dz) = biome::mushroom_island_shore;
                } else {
                    result.at(dx, dz) = river_id & 0xff;
                }
            }
        }
        return result;
    }

private:
    LayerPtr river_parent_;
};

class VoronoiZoomLayer final : public Layer {
public:
    VoronoiZoomLayer(std::int64_t seed, LayerPtr parent) : Layer(seed, std::move(parent)) {
    }

    [[nodiscard]] Area get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) override;
};

Area VoronoiZoomLayer::get(std::int32_t x, std::int32_t z, std::int32_t width, std::int32_t height) {
    const std::int32_t parent_x = arithmetic_shift_right(wrapping_add(x, -2), 2);
    const std::int32_t parent_z = arithmetic_shift_right(wrapping_add(z, -2), 2);
    const std::int32_t parent_width = arithmetic_shift_right(width, 2) + 2;
    const std::int32_t parent_height = arithmetic_shift_right(height, 2) + 2;
    Area input = parent(parent_x, parent_z, parent_width, parent_height);
    const std::int32_t expanded_width = (parent_width - 1) * 4;
    Area expanded{expanded_width, (parent_height - 1) * 4};

    const auto jitter = [this]() { return (static_cast<float>(next(1024)) / 1024.0F - 0.5F) * 3.6F; };
    for (std::int32_t pz = 0; pz < parent_height - 1; ++pz) {
        for (std::int32_t px = 0; px < parent_width - 1; ++px) {
            const std::int32_t north_west = input.at(px, pz);
            const std::int32_t north_east = input.at(px + 1, pz);
            const std::int32_t south_west = input.at(px, pz + 1);
            const std::int32_t south_east = input.at(px + 1, pz + 1);
            init_cell(
                wrapping_multiply(wrapping_add(parent_x, px), 4),
                wrapping_multiply(wrapping_add(parent_z, pz), 4));
            const float nw_x = jitter();
            const float nw_z = jitter();
            init_cell(
                wrapping_multiply(wrapping_add(parent_x, px + 1), 4),
                wrapping_multiply(wrapping_add(parent_z, pz), 4));
            const float ne_x = jitter() + 4.0F;
            const float ne_z = jitter();
            init_cell(
                wrapping_multiply(wrapping_add(parent_x, px), 4),
                wrapping_multiply(wrapping_add(parent_z, pz + 1), 4));
            const float sw_x = jitter();
            const float sw_z = jitter() + 4.0F;
            init_cell(
                wrapping_multiply(wrapping_add(parent_x, px + 1), 4),
                wrapping_multiply(wrapping_add(parent_z, pz + 1), 4));
            const float se_x = jitter() + 4.0F;
            const float se_z = jitter() + 4.0F;

            for (std::int32_t dz = 0; dz < 4; ++dz) {
                for (std::int32_t dx = 0; dx < 4; ++dx) {
                    const float fx = static_cast<float>(dx);
                    const float fz = static_cast<float>(dz);
                    const float d_nw = (fx - nw_x) * (fx - nw_x) + (fz - nw_z) * (fz - nw_z);
                    const float d_ne = (fx - ne_x) * (fx - ne_x) + (fz - ne_z) * (fz - ne_z);
                    const float d_sw = (fx - sw_x) * (fx - sw_x) + (fz - sw_z) * (fz - sw_z);
                    const float d_se = (fx - se_x) * (fx - se_x) + (fz - se_z) * (fz - se_z);
                    expanded.at(px * 4 + dx, pz * 4 + dz) = d_nw < d_ne && d_nw < d_sw && d_nw < d_se
                        ? north_west : d_ne < d_nw && d_ne < d_sw && d_ne < d_se
                        ? north_east : d_sw < d_nw && d_sw < d_ne && d_sw < d_se
                        ? south_west : south_east;
                }
            }
        }
    }

    Area result{width, height};
    const std::int32_t offset_x = wrapping_add(x, -2) & 3;
    const std::int32_t offset_z = wrapping_add(z, -2) & 3;
    for (std::int32_t dz = 0; dz < height; ++dz) {
        for (std::int32_t dx = 0; dx < width; ++dx) {
            result.at(dx, dz) = expanded.at(dx + offset_x, dz + offset_z);
        }
    }
    return result;
}

[[nodiscard]] LayerPtr zoom_n(std::int64_t seed, LayerPtr parent, std::int32_t count) {
    for (; count > 0; --count) {
        // PE 0.9.0 gives every repeated normal-zoom layer the same base + 1
        // seed. It does not advance the seed between repetitions.
        parent = std::make_shared<ZoomLayer>(seed + 1, std::move(parent));
    }
    return parent;
}

} // namespace

class BiomeLayerSource::Impl final {
public:
    Impl(std::uint32_t seed, std::int32_t generator_type_code) {
        LayerPtr layer = std::make_shared<IslandLayer>(1);
        layer = std::make_shared<ZoomLayer>(2000, std::move(layer), true);
        layer = std::make_shared<AddIslandLayer>(1, std::move(layer));
        layer = std::make_shared<ZoomLayer>(2001, std::move(layer));
        layer = std::make_shared<AddIslandLayer>(2, std::move(layer));
        layer = std::make_shared<AddIslandLayer>(50, std::move(layer));
        layer = std::make_shared<AddIslandLayer>(70, std::move(layer));
        layer = std::make_shared<RemoveTooMuchOceanLayer>(2, std::move(layer));
        layer = std::make_shared<AddSnowLayer>(2, std::move(layer));
        layer = std::make_shared<AddIslandLayer>(3, std::move(layer));
        layer = std::make_shared<EdgeLayer>(2, std::move(layer), EdgeMode::cool_warm);
        layer = std::make_shared<EdgeLayer>(2, std::move(layer), EdgeMode::heat_ice);
        layer = std::make_shared<EdgeLayer>(3, std::move(layer), EdgeMode::special);
        layer = std::make_shared<ZoomLayer>(2002, std::move(layer));
        layer = std::make_shared<ZoomLayer>(2003, std::move(layer));
        layer = std::make_shared<AddIslandLayer>(4, std::move(layer));
        layer = std::make_shared<AddMushroomIslandLayer>(5, std::move(layer));
        layer = std::make_shared<AddDeepOceanLayer>(4, std::move(layer));

        const std::int32_t biome_size = generator_type_code == 0 ? 2 : 4;
        LayerPtr river_init_source = layer;
        LayerPtr river = std::make_shared<RiverInitLayer>(100, river_init_source);
        LayerPtr biome_layer = std::make_shared<BiomeInitLayer>(200, std::move(layer), generator_type_code);
        biome_layer = zoom_n(1000, std::move(biome_layer), 2);
        biome_layer = std::make_shared<BiomeEdgeLayer>(
            1000, std::move(biome_layer), generator_type_code == 1);
        LayerPtr hill_river = zoom_n(1000, river, 2);
        biome_layer = std::make_shared<RegionHillsLayer>(
            1000, std::move(biome_layer), std::move(hill_river));
        river = zoom_n(1000, std::move(river), 2);
        river = zoom_n(1000, std::move(river), biome_size);
        river = std::make_shared<RiverLayer>(1, std::move(river));
        river = std::make_shared<SmoothLayer>(1000, std::move(river));
        biome_layer = std::make_shared<RareBiomeSpotLayer>(1001, std::move(biome_layer));
        for (std::int32_t index = 0; index < biome_size; ++index) {
            biome_layer = std::make_shared<ZoomLayer>(1000 + index, std::move(biome_layer));
            if (index == 0) {
                biome_layer = std::make_shared<AddIslandLayer>(3, std::move(biome_layer));
            }
            if (index == 1) {
                biome_layer = std::make_shared<ShoreLayer>(1000, std::move(biome_layer));
            }
        }
        biome_layer = std::make_shared<SmoothLayer>(1000, std::move(biome_layer));
        raw_ = std::make_shared<RiverMixerLayer>(100, std::move(biome_layer), std::move(river));
        voronoi_ = std::make_shared<VoronoiZoomLayer>(10, raw_);
        raw_->init_world_seed(seed);
        voronoi_->init_world_seed(seed);
    }

    LayerPtr raw_;
    LayerPtr voronoi_;
};

BiomeLayerSource::BiomeLayerSource(
    std::uint32_t world_seed,
    std::int32_t generator_type_code)
    : impl_(std::make_unique<Impl>(world_seed, generator_type_code)) {
}

BiomeLayerSource::~BiomeLayerSource() = default;
BiomeLayerSource::BiomeLayerSource(BiomeLayerSource&&) noexcept = default;
BiomeLayerSource& BiomeLayerSource::operator=(BiomeLayerSource&&) noexcept = default;

std::vector<std::int32_t> BiomeLayerSource::raw_biomes(
    std::int32_t x,
    std::int32_t z,
    std::int32_t width,
    std::int32_t height) {
    return impl_->raw_->get(x, z, width, height).cells;
}

std::vector<std::int32_t> BiomeLayerSource::biomes(
    std::int32_t x,
    std::int32_t z,
    std::int32_t width,
    std::int32_t height) {
    return impl_->voronoi_->get(x, z, width, height).cells;
}

} // namespace mcpe::worldgen::v0_9_0
