#pragma once

#include <cstdint>

namespace mcpe::worldgen::v0_6_1::legacy_math {

// Exact Android bionic/FreeBSD-msun single-precision trigonometry on the
// argument range exercised by the supported native world-generation paths.
[[nodiscard]] float bionic_sin(float value) noexcept;
[[nodiscard]] float bionic_cos(float value) noexcept;

// PE's Mth methods read a 65,536-element table constructed by initMth().
[[nodiscard]] float sin(float value) noexcept;
[[nodiscard]] float cos(float value) noexcept;

// Exposed internally so the table can be regression-tested bit for bit.
[[nodiscard]] float sine_table_value(std::uint16_t index) noexcept;

} // namespace mcpe::worldgen::v0_6_1::legacy_math
