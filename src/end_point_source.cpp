#include "end_point_source.hpp"

#include "detail/fp.hpp"
#include "detail/wrap.hpp"
#include "v1_1_5_0/end_features.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mcpe::terrain_verifier {
namespace {

constexpr float kSelectorScaleX = 17.1103F;
constexpr float kSelectorScaleY = 4.277575F;
constexpr float kSelectorScaleZ = 17.1103F;
constexpr float kDensityScaleX = 1368.824F;
constexpr float kDensityScaleY = 684.412F;
constexpr float kDensityScaleZ = 1368.824F;

[[nodiscard]] constexpr std::uint32_t absolute_bits(
    std::int32_t value) noexcept {
    const std::uint32_t bits = worldgen::detail::bits(value);
    return value < 0 ? 0U - bits : bits;
}

[[nodiscard]] float clamp_island_height(float value) noexcept {
    float result = -100.0F;
    if (-100.0F < value) {
        result = value;
    }
    if (80.0F < value) {
        result = 80.0F;
    }
    return result;
}

[[nodiscard]] float distance(float x, float z) noexcept {
    return std::sqrt(worldgen::detail::fp::add(
        worldgen::detail::fp::mul(x, x),
        worldgen::detail::fp::mul(z, z)));
}

void split_coordinate(
    std::int32_t coordinate,
    std::int32_t& chunk,
    std::int32_t& local) noexcept {
    chunk = coordinate / 16;
    local = coordinate % 16;
    if (local < 0) {
        --chunk;
        local += 16;
    }
}

[[nodiscard]] constexpr bool outside_main_island(
    std::int32_t chunk_x,
    std::int32_t chunk_z) noexcept {
    return worldgen::detail::bits(chunk_x) * worldgen::detail::bits(chunk_x)
        + worldgen::detail::bits(chunk_z) * worldgen::detail::bits(chunk_z)
        > 4096U;
}

class PointWriter final : public worldgen::v1_1_5_0::EndBlockWriter {
public:
    PointWriter(std::int32_t x, std::int32_t y, std::int32_t z) noexcept
        : x_(x), y_(y), z_(z) {
    }

    void set_end_block(
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        std::uint8_t,
        std::uint8_t = 0) noexcept override {
        if (x == x_ && y == y_ && z == z_) {
            written_ = true;
        }
    }

    [[nodiscard]] bool written() const noexcept {
        return written_;
    }

private:
    std::int32_t x_{};
    std::int32_t y_{};
    std::int32_t z_{};
    bool written_{};
};

[[nodiscard]] constexpr std::size_t sample_index(
    std::int32_t x,
    std::int32_t z,
    std::int32_t y,
    std::int32_t z_size,
    std::int32_t y_size) noexcept {
    return (static_cast<std::size_t>(x) * static_cast<std::size_t>(z_size)
            + static_cast<std::size_t>(z))
            * static_cast<std::size_t>(y_size)
        + static_cast<std::size_t>(y);
}

} // namespace

EndPointSource::EndPointSource(std::uint32_t world_seed)
    : world_seed_(world_seed),
      random_(world_seed),
      density_lower_(random_, 16),
      density_upper_(random_, 16),
      density_selector_(random_, 8),
      island_noise_(random_) {
    worldgen::detail::Mt19937 population_seed_random(world_seed);
    population_x_multiplier_ =
        (population_seed_random.next_u32() >> 1U) | 1U;
    population_z_multiplier_ =
        (population_seed_random.next_u32() >> 1U) | 1U;
}

float EndPointSource::island_height_value(
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    std::int32_t local_x,
    std::int32_t local_z) const noexcept {
    const std::int32_t base_x = worldgen::detail::wrapping_add(
        worldgen::detail::wrapping_mul(chunk_x, 2), local_x);
    const std::int32_t base_z = worldgen::detail::wrapping_add(
        worldgen::detail::wrapping_mul(chunk_z, 2), local_z);
    float result = clamp_island_height(worldgen::detail::fp::add(
        100.0F,
        worldgen::detail::fp::mul(
            distance(static_cast<float>(base_x), static_cast<float>(base_z)),
            -8.0F)));

    for (std::int32_t offset_x = -12; offset_x <= 12; ++offset_x) {
        const std::int32_t candidate_x = worldgen::detail::wrapping_add(
            chunk_x, offset_x);
        const std::uint32_t abs_x = absolute_bits(candidate_x);
        const std::uint64_t seed_x = static_cast<std::uint64_t>(abs_x) * 3439U;
        const std::int32_t local_delta_x = worldgen::detail::wrapping_add(
            local_x, worldgen::detail::wrapping_mul(offset_x, -2));
        for (std::int32_t offset_z = -12; offset_z <= 12; ++offset_z) {
            const std::int32_t candidate_z = worldgen::detail::wrapping_add(
                chunk_z, offset_z);
            const std::uint64_t squared_distance =
                static_cast<std::uint64_t>(absolute_bits(candidate_x))
                    * absolute_bits(candidate_x)
                + static_cast<std::uint64_t>(absolute_bits(candidate_z))
                    * absolute_bits(candidate_z);
            if (squared_distance <= 4096U
                || island_noise_.value(
                    static_cast<float>(candidate_x),
                    static_cast<float>(candidate_z)) >= -0.9F) {
                continue;
            }
            const std::uint64_t island_seed = seed_x
                + static_cast<std::uint64_t>(absolute_bits(candidate_z)) * 147U;
            const float radius = static_cast<float>(island_seed % 13U + 9U);
            const std::int32_t local_delta_z = worldgen::detail::wrapping_add(
                local_z, worldgen::detail::wrapping_mul(offset_z, -2));
            const float candidate = worldgen::detail::fp::sub(
                100.0F,
                worldgen::detail::fp::mul(
                    radius,
                    distance(
                        static_cast<float>(local_delta_x),
                        static_cast<float>(local_delta_z))));
            result = std::max(result, clamp_island_height(candidate));
        }
    }
    return result;
}

bool EndPointSource::base_non_air(
    std::int32_t chunk_x,
    std::int32_t chunk_z,
    std::int32_t local_x,
    std::int32_t y,
    std::int32_t local_z) {
    if (y < 0 || y >= 128) {
        return false;
    }
    const std::int32_t cell_x = local_x / 8;
    const std::int32_t cell_z = local_z / 8;
    const std::int32_t cell_y = y / 4;
    const std::int32_t sub_x = local_x & 7;
    const std::int32_t sub_z = local_z & 7;
    const std::int32_t sub_y = y & 3;
    const std::int32_t coarse_x = worldgen::detail::wrapping_mul(chunk_x, 2);
    const std::int32_t coarse_z = worldgen::detail::wrapping_mul(chunk_z, 2);
    const std::int32_t x_size = cell_x + 2;
    const std::int32_t z_size = cell_z + 2;
    const std::int32_t y_size = cell_y + 2;

    std::vector<float> selector;
    std::vector<float> lower;
    std::vector<float> upper;
    density_selector_.region(
        selector,
        static_cast<float>(coarse_x), 0.0F, static_cast<float>(coarse_z),
        x_size, y_size, z_size,
        kSelectorScaleX, kSelectorScaleY, kSelectorScaleZ);
    density_lower_.region(
        lower,
        static_cast<float>(coarse_x), 0.0F, static_cast<float>(coarse_z),
        x_size, y_size, z_size,
        kDensityScaleX, kDensityScaleY, kDensityScaleZ);
    density_upper_.region(
        upper,
        static_cast<float>(coarse_x), 0.0F, static_cast<float>(coarse_z),
        x_size, y_size, z_size,
        kDensityScaleX, kDensityScaleY, kDensityScaleZ);

    auto density_at = [&](std::int32_t lattice_x,
                          std::int32_t lattice_z,
                          std::int32_t lattice_y) {
        const std::size_t index = sample_index(
            lattice_x, lattice_z, lattice_y, z_size, y_size);
        const float island_height = island_height_value(
            chunk_x, chunk_z, lattice_x, lattice_z);
        const float blend = worldgen::detail::fp::mul_add(
            0.5F, selector[index], 0.05F);
        float density = worldgen::detail::fp::mul(
            lower[index], 0.001953125F);
        if (blend >= 0.0F) {
            if (blend < 1.0F) {
                const float delta = worldgen::detail::fp::mul(
                    worldgen::detail::fp::sub(upper[index], lower[index]),
                    0.001953125F);
                density = worldgen::detail::fp::mul_add(
                    density, delta, blend);
            } else {
                density = worldgen::detail::fp::mul(
                    upper[index], 0.001953125F);
            }
        }
        density = worldgen::detail::fp::add(
            worldgen::detail::fp::sub(island_height, 8.0F), density);

        const std::int32_t vertical = lattice_y - 14;
        const std::int32_t bottom_fade = 8 - lattice_y;
        if (lattice_y < 15) {
            if (lattice_y < 8) {
                const float fade = static_cast<float>(bottom_fade);
                const float multiplier = worldgen::detail::fp::mul_add(
                    1.0F, fade, -0.14285715F);
                density = worldgen::detail::fp::mul_add(
                    worldgen::detail::fp::mul(density, multiplier),
                    fade, -4.2857146F);
            }
        } else {
            float fade = worldgen::detail::fp::mul(
                static_cast<float>(vertical), 0.015625F);
            fade = std::clamp(fade, 0.0F, 1.0F);
            density = worldgen::detail::fp::mul_add(
                worldgen::detail::fp::mul(
                    density, worldgen::detail::fp::sub(1.0F, fade)),
                fade, -3000.0F);
        }
        return density;
    };

    float z1_x0 = density_at(cell_x, cell_z + 1, cell_y);
    float z1_x1 = density_at(cell_x + 1, cell_z + 1, cell_y);
    float z0_x0 = density_at(cell_x, cell_z, cell_y);
    float z0_x1 = density_at(cell_x + 1, cell_z, cell_y);
    const float z1_x0_y_step = worldgen::detail::fp::mul(
        worldgen::detail::fp::sub(
            density_at(cell_x, cell_z + 1, cell_y + 1), z1_x0), 0.25F);
    const float z1_x1_y_step = worldgen::detail::fp::mul(
        worldgen::detail::fp::sub(
            density_at(cell_x + 1, cell_z + 1, cell_y + 1), z1_x1), 0.25F);
    const float z0_x0_y_step = worldgen::detail::fp::mul(
        worldgen::detail::fp::sub(
            density_at(cell_x, cell_z, cell_y + 1), z0_x0), 0.25F);
    const float z0_x1_y_step = worldgen::detail::fp::mul(
        worldgen::detail::fp::sub(
            density_at(cell_x + 1, cell_z, cell_y + 1), z0_x1), 0.25F);
    for (std::int32_t step_y = 0; step_y < sub_y; ++step_y) {
        z1_x0 = worldgen::detail::fp::add(z1_x0, z1_x0_y_step);
        z1_x1 = worldgen::detail::fp::add(z1_x1, z1_x1_y_step);
        z0_x0 = worldgen::detail::fp::add(z0_x0, z0_x0_y_step);
        z0_x1 = worldgen::detail::fp::add(z0_x1, z0_x1_y_step);
    }

    const float z1_x_step = worldgen::detail::fp::mul(
        worldgen::detail::fp::sub(z1_x1, z1_x0), 0.125F);
    const float z0_x_step = worldgen::detail::fp::mul(
        worldgen::detail::fp::sub(z0_x1, z0_x0), 0.125F);
    float z1 = z1_x0;
    float z0 = z0_x0;
    for (std::int32_t step_x = 0; step_x < sub_x; ++step_x) {
        z1 = worldgen::detail::fp::add(z1, z1_x_step);
        z0 = worldgen::detail::fp::add(z0, z0_x_step);
    }
    const float z_step = worldgen::detail::fp::mul(
        worldgen::detail::fp::sub(z1, z0), 0.125F);
    float value = z0;
    for (std::int32_t step_z = 0; step_z < sub_z; ++step_z) {
        value = worldgen::detail::fp::add(value, z_step);
    }
    return value > 0.0F;
}

bool EndPointSource::non_air(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) {
    std::int32_t chunk_x{};
    std::int32_t local_x{};
    std::int32_t chunk_z{};
    std::int32_t local_z{};
    split_coordinate(x, chunk_x, local_x);
    split_coordinate(z, chunk_z, local_z);
    if (base_non_air(chunk_x, chunk_z, local_x, y, local_z)) {
        return true;
    }

    PointWriter writer(x, y, z);
    for (std::int32_t source_offset_z = -1; source_offset_z <= 0;
         ++source_offset_z) {
        for (std::int32_t source_offset_x = -1; source_offset_x <= 0;
             ++source_offset_x) {
            const std::int32_t source_x = worldgen::detail::wrapping_add(
                chunk_x, source_offset_x);
            const std::int32_t source_z = worldgen::detail::wrapping_add(
                chunk_z, source_offset_z);
            if (!outside_main_island(source_x, source_z)) {
                continue;
            }
            const std::uint32_t coordinate_term =
                worldgen::detail::bits(source_x) * population_x_multiplier_
                + worldgen::detail::bits(source_z) * population_z_multiplier_;
            worldgen::detail::Mt19937 random(world_seed_ ^ coordinate_term);
            if (island_height_value(source_x, source_z, 1, 1) >= -20.0F
                || random.next_bounded(14U) != 0U) {
                continue;
            }
            const std::int32_t island_x = worldgen::detail::wrapping_add(
                worldgen::detail::wrapping_mul(source_x, 16),
                static_cast<std::int32_t>(random.next_u32() & 15U) + 8);
            const std::int32_t island_y =
                static_cast<std::int32_t>(random.next_u32() & 15U) + 55;
            const std::int32_t island_z = worldgen::detail::wrapping_add(
                worldgen::detail::wrapping_mul(source_z, 16),
                static_cast<std::int32_t>(random.next_u32() & 15U) + 8);
            worldgen::v1_1_5_0::place_end_island(
                writer, random, island_x, island_y, island_z);
            if ((random.next_u32() & 3U) == 0U) {
                worldgen::v1_1_5_0::place_end_island(
                    writer, random, island_x, island_y, island_z);
            }
            if (writer.written()) {
                return true;
            }
        }
    }
    return false;
}

} // namespace mcpe::terrain_verifier
