#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace mcpe::worldgen::detail::fp {

// These helpers spell out the VFPv3 operation boundaries found in the APK.
// The build disables FP contraction, so multiply-add has two binary32
// roundings just like VMLA/VMLS (as opposed to VFMA/VFMS).
[[nodiscard]] inline float add(float a, float b) noexcept {
    return a + b;
}

[[nodiscard]] inline float sub(float a, float b) noexcept {
    return a - b;
}

[[nodiscard]] inline float mul(float a, float b) noexcept {
    return a * b;
}

[[nodiscard]] inline float div(float a, float b) noexcept {
    return a / b;
}

[[nodiscard]] inline float mul_add(float accumulator, float a, float b) noexcept {
    const float product = mul(a, b);
    return add(accumulator, product);
}

[[nodiscard]] inline float mul_sub(float accumulator, float a, float b) noexcept {
    const float product = mul(a, b);
    return sub(accumulator, product);
}

[[nodiscard]] inline std::int32_t trunc_to_i32(float value) noexcept {
    // VCVT.S32.F32 rounds toward zero and saturates on overflow.  NaNs are
    // unpacked as 0.0 by the ARMv7 floating-point pseudocode.  Spell those
    // cases out because an out-of-range C++ floating-to-integer cast is UB.
    if (std::isnan(value)) {
        return 0;
    }
    constexpr float positive_limit = 2'147'483'648.0F;
    constexpr float negative_limit = -2'147'483'648.0F;
    if (value >= positive_limit) {
        return std::numeric_limits<std::int32_t>::max();
    }
    if (value <= negative_limit) {
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] inline std::int32_t floor_to_i32(float value) noexcept {
    std::int32_t integer = trunc_to_i32(value);
    if (value < static_cast<float>(integer)) {
        // Mth::floor uses a plain ARM SUB after VCVT, including its wrap at
        // INT32_MIN for an input below the conversion range.
        if (integer == std::numeric_limits<std::int32_t>::min()) {
            return std::numeric_limits<std::int32_t>::max();
        }
        --integer;
    }
    return integer;
}

} // namespace mcpe::worldgen::detail::fp
