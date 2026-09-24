#pragma once

#include <bit>
#include <cstdint>

namespace mcpe::worldgen::detail {

[[nodiscard]] constexpr std::uint32_t bits(std::int32_t value) noexcept {
    return std::bit_cast<std::uint32_t>(value);
}

[[nodiscard]] constexpr std::int32_t signed_bits(std::uint32_t value) noexcept {
    return std::bit_cast<std::int32_t>(value);
}

[[nodiscard]] constexpr std::int32_t wrapping_add(
    std::int32_t left,
    std::int32_t right) noexcept {
    return signed_bits(bits(left) + bits(right));
}

[[nodiscard]] constexpr std::int32_t wrapping_mul(
    std::int32_t left,
    std::int32_t right) noexcept {
    return signed_bits(bits(left) * bits(right));
}

[[nodiscard]] constexpr std::int32_t wrapping_sub(
    std::int32_t left,
    std::int32_t right) noexcept {
    return signed_bits(bits(left) - bits(right));
}

} // namespace mcpe::worldgen::detail
