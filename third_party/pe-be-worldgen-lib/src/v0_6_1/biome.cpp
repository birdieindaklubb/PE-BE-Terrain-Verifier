#include "v0_6_1/biome.hpp"

#include "detail/fp.hpp"
#include "v0_6_1/blocks.hpp"

#include <algorithm>
#include <cassert>

namespace mcpe::worldgen::v0_6_1 {
namespace {

using detail::fp::add;
using detail::fp::mul;
using detail::fp::mul_add;
using detail::fp::sub;

constexpr std::array<Biome, 11> biomes_by_id{{
    {BiomeId::rain_forest, block::grass, block::dirt},
    {BiomeId::swampland, block::grass, block::dirt},
    {BiomeId::seasonal_forest, block::grass, block::dirt},
    {BiomeId::forest, block::grass, block::dirt},
    {BiomeId::savanna, block::grass, block::dirt},
    {BiomeId::shrubland, block::grass, block::dirt},
    {BiomeId::taiga, block::grass, block::dirt},
    {BiomeId::desert, block::sand, block::sand},
    {BiomeId::plains, block::grass, block::dirt},
    {BiomeId::ice_desert, block::sand, block::sand},
    {BiomeId::tundra, block::grass, block::dirt},
}};

[[nodiscard]] float clamp_unit(float value) noexcept {
    if (value < 0.0F) {
        return 0.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

[[nodiscard]] float curve_temperature(float mixed) noexcept {
    const float inverse = sub(1.0F, mixed);
    return sub(1.0F, mul(inverse, inverse));
}

} // namespace

const Biome& biome(BiomeId id) noexcept {
    return biomes_by_id[static_cast<std::size_t>(id)];
}

BiomeSource::BiomeSource(std::uint32_t world_seed)
    : temperature_random_(world_seed * 9'871U),
      rainfall_random_(world_seed * 39'811U),
      detail_random_(world_seed * 543'321U),
      temperature_noise_(temperature_random_, 4),
      rainfall_noise_(rainfall_random_, 4),
      detail_noise_(detail_random_, 2) {
}

BiomeId BiomeSource::select_direct(float temperature, float rainfall) noexcept {
    if (temperature < 0.1F) {
        return BiomeId::tundra;
    }

    const float humidity = mul(rainfall, temperature);
    if (humidity < 0.2F) {
        if (temperature < 0.5F) {
            return BiomeId::tundra;
        }
        if (temperature < 0.95F) {
            return BiomeId::savanna;
        }
        return BiomeId::desert;
    }
    if (humidity > 0.5F && temperature < 0.7F) {
        return BiomeId::swampland;
    }
    if (temperature < 0.5F) {
        return BiomeId::taiga;
    }
    if (temperature < 0.97F) {
        return humidity < 0.35F ? BiomeId::shrubland : BiomeId::forest;
    }
    if (humidity < 0.45F) {
        return BiomeId::plains;
    }
    if (humidity < 0.9F) {
        return BiomeId::seasonal_forest;
    }
    return BiomeId::rain_forest;
}

const std::array<BiomeId, 64 * 64>& BiomeSource::lookup() noexcept {
    static const std::array<BiomeId, 64 * 64> table = [] {
        std::array<BiomeId, 64 * 64> result{};
        for (std::int32_t temperature_index = 0; temperature_index < 64;
             ++temperature_index) {
            const float temperature = detail::fp::div(
                static_cast<float>(temperature_index), 63.0F);
            for (std::int32_t rainfall_index = 0; rainfall_index < 64;
                 ++rainfall_index) {
                const float rainfall = detail::fp::div(
                    static_cast<float>(rainfall_index), 63.0F);
                result[static_cast<std::size_t>(temperature_index)
                       + static_cast<std::size_t>(rainfall_index) * 64U]
                    = select_direct(temperature, rainfall);
            }
        }
        return result;
    }();
    return table;
}

BiomeId BiomeSource::select_quantized(float temperature, float rainfall) noexcept {
    const auto temperature_index = static_cast<std::int32_t>(mul(temperature, 63.0F));
    const auto rainfall_index = static_cast<std::int32_t>(mul(rainfall, 63.0F));
    assert(temperature_index >= 0 && temperature_index < 64);
    assert(rainfall_index >= 0 && rainfall_index < 64);
    return lookup()[static_cast<std::size_t>(temperature_index)
                    + static_cast<std::size_t>(rainfall_index) * 64U];
}

std::span<const float> BiomeSource::temperatures(
    std::int32_t x,
    std::int32_t z,
    std::int32_t width,
    std::int32_t height) {
    temperature_noise_.region2d(
        temperature_, x, z, width, height, 0.025F, 0.025F, 0.25F);
    detail_noise_.region2d(
        detail_, x, z, width, height, 0.25F, 0.25F, 0.588F);

    const std::size_t size = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    for (std::size_t index = 0; index < size; ++index) {
        const float base = mul_add(0.7F, temperature_[index], 0.15F);
        const float adjustment = mul_add(0.5F, detail_[index], 1.1F);
        const float mixed = mul_add(
            mul(adjustment, 0.01F),
            base,
            0.99F);
        temperature_[index] = clamp_unit(curve_temperature(mixed));
    }
    return {temperature_.data(), size};
}

std::span<const BiomeId> BiomeSource::biomes(
    std::int32_t x,
    std::int32_t z,
    std::int32_t width,
    std::int32_t height) {
    // The original overload accidentally uses width for both noise dimensions.
    // Generator calls are square, but preserving the bug avoids a silent ABI
    // difference for direct callers.
    assert(width == height);
    temperature_noise_.region2d(
        temperature_, x, z, width, width, 0.025F, 0.025F, 0.25F);
    rainfall_noise_.region2d(
        rainfall_, x, z, width, width, 0.05F, 0.05F, 0.3333F);
    detail_noise_.region2d(
        detail_, x, z, width, width, 0.25F, 0.25F, 0.588F);

    const std::size_t size = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    biomes_.resize(size);
    for (std::size_t index = 0; index < size; ++index) {
        const float adjustment = mul_add(0.5F, detail_[index], 1.1F);

        const float rainfall_base = mul_add(0.5F, rainfall_[index], 0.15F);
        float rainfall = mul(adjustment, 0.002F);
        rainfall = mul_add(rainfall, rainfall_base, 0.998F);

        const float temperature_base =
            mul_add(0.7F, temperature_[index], 0.15F);
        float temperature = mul(adjustment, 0.01F);
        temperature = mul_add(temperature, temperature_base, 0.99F);
        temperature = curve_temperature(temperature);

        temperature = clamp_unit(temperature);
        rainfall = clamp_unit(rainfall);
        temperature_[index] = temperature;
        rainfall_[index] = rainfall;
        biomes_[index] = select_quantized(temperature, rainfall);
    }
    return biomes_;
}

} // namespace mcpe::worldgen::v0_6_1
