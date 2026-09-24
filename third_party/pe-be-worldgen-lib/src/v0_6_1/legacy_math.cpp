#include "v0_6_1/legacy_math.hpp"

#include "detail/fp.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>

namespace mcpe::worldgen::v0_6_1::legacy_math {
namespace {

// PE 0.6.1 imports sinf/cosf from Android. These are the coefficients and
// evaluation order in bionic's FreeBSD-msun float kernels at Android 2.3.3.
// The finite PE 0.6.1 paths stay near 2*pi, while PE 0.9.0 cave steering can
// drift into bionic's medium argument-reduction path. The much larger Payne-
// Hanek table is unnecessary for every versioned worldgen caller here.
constexpr double pi_over_two = 0x1.921fb54442d18p+0;
constexpr double pi = 0x1.921fb54442d18p+1;
constexpr double three_pi_over_two = 0x1.2d97c7f3321d2p+2;
constexpr double two_pi = 0x1.921fb54442d18p+2;

struct ReducedAngle final {
    std::int32_t quadrant{};
    float high{};
    float low{};
};

[[nodiscard]] ReducedAngle reduce_medium(float value) noexcept {
    // bionic e_rem_pio2f.c's complete medium-size path. Cave steering can
    // drift beyond 2*pi even though its initial yaw is normalized.
    constexpr double inverse_half_pi = 0x1.45f306dc9c883p-1;
    constexpr double half_pi_high = 0x1.921fb54400000p+0;
    constexpr double half_pi_tail = 0x1.0b4611a626331p-34;

    const bool negative = std::signbit(value);
    const double magnitude = std::fabs(static_cast<double>(value));
    const std::int32_t count = static_cast<std::int32_t>(
        magnitude * inverse_half_pi + 0.5);
    const double count_as_double = static_cast<double>(count);
    const double remainder = magnitude - count_as_double * half_pi_high;
    const double tail = count_as_double * half_pi_tail;
    float high = static_cast<float>(remainder - tail);
    float low = static_cast<float>((remainder - static_cast<double>(high)) - tail);
    if (negative) {
        high = -high;
        low = -low;
        return {-count, high, low};
    }
    return {count, high, low};
}

[[nodiscard]] float kernel_sin(double x) noexcept {
    constexpr double s1 = -0x1.5555554cbac77p-3;
    constexpr double s2 = 0x1.11110896efbb2p-7;
    constexpr double s3 = -0x1.a00f9e2cae774p-13;
    constexpr double s4 = 0x1.6cd878c3b46a7p-19;

    const double z = x * x;
    const double w = z * z;
    const double r = s3 + z * s4;
    const double s = z * x;
    return static_cast<float>((x + s * (s1 + z * s2)) + s * w * r);
}

[[nodiscard]] float kernel_cos(double x) noexcept {
    constexpr double c0 = -0x1.ffffffd0c5e81p-2;
    constexpr double c1 = 0x1.55553e1053a42p-5;
    constexpr double c2 = -0x1.6c087e80f1e27p-10;
    constexpr double c3 = 0x1.99342e0ee5069p-16;

    const double z = x * x;
    const double w = z * z;
    const double r = c2 + z * c3;
    return static_cast<float>(((1.0 + z * c0) + w * c1) + (w * z) * r);
}

[[nodiscard]] const std::array<float, 65'536>& sine_table() noexcept {
    static const std::array<float, 65'536> values = [] {
        std::array<float, 65'536> result{};
        // Math's native static initializer multiplies the integer index by
        // this binary32 step (bits 0x38c90fdb) before calling Android sinf.
        // Dividing by mSinScale is algebraically equivalent but not
        // binary32-equivalent: it changes 8,477 final table entries.
        constexpr float table_step = 9.58738e-05F;
        for (std::int32_t index = 0; index < 65'536; ++index) {
            const float angle = detail::fp::mul(
                static_cast<float>(index), table_step);
            result[static_cast<std::size_t>(index)] = bionic_sin(angle);
        }
        return result;
    }();
    return values;
}

} // namespace

float bionic_sin(float value) noexcept {
    const std::int32_t signed_bits =
        std::bit_cast<std::int32_t>(value);
    const std::uint32_t magnitude =
        std::bit_cast<std::uint32_t>(value) & 0x7fff'ffffU;

    if (magnitude <= 0x3f49'0fdaU) {
        if (magnitude < 0x3980'0000U
            && static_cast<std::int32_t>(value) == 0) {
            return value;
        }
        return kernel_sin(static_cast<double>(value));
    }
    if (magnitude <= 0x407b'53d1U) {
        if (magnitude <= 0x4016'cbe3U) {
            return signed_bits > 0
                ? kernel_cos(static_cast<double>(value) - pi_over_two)
                : -kernel_cos(static_cast<double>(value) + pi_over_two);
        }
        return kernel_sin(
            (signed_bits > 0 ? pi : -pi) - static_cast<double>(value));
    }
    if (magnitude <= 0x40e2'31d5U) {
        if (magnitude <= 0x40af'eddfU) {
            return signed_bits > 0
                ? -kernel_cos(static_cast<double>(value) - three_pi_over_two)
                : kernel_cos(static_cast<double>(value) + three_pi_over_two);
        }
        return kernel_sin(
            static_cast<double>(value)
            + (signed_bits > 0 ? -two_pi : two_pi));
    }

    if (magnitude <= 0x4949'0f80U) {
        const ReducedAngle reduced = reduce_medium(value);
        const double angle =
            static_cast<double>(reduced.high) + reduced.low;
        switch (reduced.quadrant & 3) {
        case 0:
            return kernel_sin(angle);
        case 1:
            return kernel_cos(angle);
        case 2:
            return kernel_sin(-angle);
        default:
            return -kernel_cos(angle);
        }
    }
    assert(false && "legacy bionic_sin argument outside medium worldgen range");
    return std::numeric_limits<float>::quiet_NaN();
}

float bionic_cos(float value) noexcept {
    const std::int32_t signed_bits =
        std::bit_cast<std::int32_t>(value);
    const std::uint32_t magnitude =
        std::bit_cast<std::uint32_t>(value) & 0x7fff'ffffU;

    if (magnitude <= 0x3f49'0fdaU) {
        if (magnitude < 0x3980'0000U
            && static_cast<std::int32_t>(value) == 0) {
            return 1.0F;
        }
        return kernel_cos(static_cast<double>(value));
    }
    if (magnitude <= 0x407b'53d1U) {
        if (magnitude > 0x4016'cbe3U) {
            return -kernel_cos(
                static_cast<double>(value)
                + (signed_bits > 0 ? -pi : pi));
        }
        return signed_bits > 0
            ? kernel_sin(pi_over_two - static_cast<double>(value))
            : kernel_sin(static_cast<double>(value) + pi_over_two);
    }
    if (magnitude <= 0x40e2'31d5U) {
        if (magnitude > 0x40af'eddfU) {
            return kernel_cos(
                static_cast<double>(value)
                + (signed_bits > 0 ? -two_pi : two_pi));
        }
        return signed_bits > 0
            ? kernel_sin(static_cast<double>(value) - three_pi_over_two)
            : kernel_sin(-three_pi_over_two - static_cast<double>(value));
    }

    if (magnitude <= 0x4949'0f80U) {
        const ReducedAngle reduced = reduce_medium(value);
        const double angle =
            static_cast<double>(reduced.high) + reduced.low;
        switch (reduced.quadrant & 3) {
        case 0:
            return kernel_cos(angle);
        case 1:
            return kernel_sin(-angle);
        case 2:
            return -kernel_cos(angle);
        default:
            return kernel_sin(angle);
        }
    }
    assert(false && "legacy bionic_cos argument outside medium worldgen range");
    return std::numeric_limits<float>::quiet_NaN();
}

float sine_table_value(std::uint16_t index) noexcept {
    return sine_table()[index];
}

float sin(float value) noexcept {
    constexpr float table_scale = 10'430.3779F;
    const float scaled = detail::fp::mul(value, table_scale);
    const auto index = static_cast<std::uint16_t>(
        detail::fp::trunc_to_i32(scaled));
    return sine_table_value(index);
}

float cos(float value) noexcept {
    constexpr float table_scale = 10'430.3779F;
    float scaled = detail::fp::mul(value, table_scale);
    scaled = detail::fp::add(16'384.0F, scaled);
    const auto index = static_cast<std::uint16_t>(
        detail::fp::trunc_to_i32(scaled));
    return sine_table_value(index);
}

} // namespace mcpe::worldgen::v0_6_1::legacy_math
