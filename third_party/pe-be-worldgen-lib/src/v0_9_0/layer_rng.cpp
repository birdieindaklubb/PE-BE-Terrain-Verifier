#include "v0_9_0/layer_rng.hpp"

#include <bit>
#include <cassert>

namespace mcpe::worldgen::v0_9_0 {
namespace {

constexpr std::uint64_t multiplier = 0x5851'f42d'4c95'7f2dULL;
constexpr std::uint64_t increment = 0x1405'7b7e'f767'814fULL;

[[nodiscard]] constexpr std::uint64_t bits(std::int64_t value) noexcept {
    return std::bit_cast<std::uint64_t>(value);
}

[[nodiscard]] constexpr std::int64_t signed_bits(std::uint64_t value) noexcept {
    return std::bit_cast<std::int64_t>(value);
}

[[nodiscard]] constexpr std::int64_t arithmetic_shift_right_24(
    std::uint64_t value) noexcept {
    const std::int64_t signed_value = signed_bits(value);
    if (signed_value >= 0) {
        return signed_value >> 24;
    }
    return -(((-signed_value) + ((std::int64_t{1} << 24) - 1)) >> 24);
}

[[nodiscard]] constexpr std::uint64_t coordinate_bits(
    std::int32_t coordinate) noexcept {
    return bits(static_cast<std::int64_t>(coordinate));
}

} // namespace

LayerRandom::LayerRandom(std::int64_t base_seed) noexcept
    : base_seed_(bits(base_seed)) {
    base_seed_ = advance(base_seed_, bits(base_seed));
    base_seed_ = advance(base_seed_, bits(base_seed));
    base_seed_ = advance(base_seed_, bits(base_seed));
}

void LayerRandom::set_world_seed(std::uint32_t world_seed) noexcept {
    world_seed_ = world_seed;
    world_seed_ = advance(world_seed_, base_seed_);
    world_seed_ = advance(world_seed_, base_seed_);
    world_seed_ = advance(world_seed_, base_seed_);
}

void LayerRandom::set_chunk_seed(std::int32_t x, std::int32_t z) noexcept {
    chunk_seed_ = world_seed_;
    chunk_seed_ = advance(chunk_seed_, coordinate_bits(x));
    chunk_seed_ = advance(chunk_seed_, coordinate_bits(z));
    chunk_seed_ = advance(chunk_seed_, coordinate_bits(x));
    chunk_seed_ = advance(chunk_seed_, coordinate_bits(z));
}

std::int32_t LayerRandom::next_int(std::int32_t bound) noexcept {
    assert(bound > 0);

    std::int64_t value = arithmetic_shift_right_24(chunk_seed_) % bound;
    if (value < 0) {
        value += bound;
    }
    chunk_seed_ = advance(chunk_seed_, world_seed_);
    return static_cast<std::int32_t>(value);
}

std::uint64_t LayerRandom::advance(
    std::uint64_t state,
    std::uint64_t additive) noexcept {
    return state * (state * multiplier + increment) + additive;
}

} // namespace mcpe::worldgen::v0_9_0
