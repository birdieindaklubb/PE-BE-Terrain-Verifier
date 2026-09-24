#include "detail/mt19937.hpp"

namespace mcpe::worldgen::detail {

Mt19937::Mt19937(std::uint32_t seed_value) noexcept {
    seed(seed_value);
}

void Mt19937::seed(std::uint32_t value) noexcept {
    state_[0] = value;
    for (std::uint32_t i = 1; i < state_size; ++i) {
        const std::uint32_t previous = state_[i - 1];
        state_[i] = 1'812'433'253U * (previous ^ (previous >> 30U)) + i;
    }
    index_ = state_size;
}

void Mt19937::twist() noexcept {
    constexpr std::uint32_t upper_mask = 0x8000'0000U;
    constexpr std::uint32_t lower_mask = 0x7fff'ffffU;
    constexpr std::uint32_t matrix_a = 0x9908'b0dfU;

    for (std::size_t i = 0; i < state_size - period_offset; ++i) {
        const std::uint32_t joined =
            (state_[i] & upper_mask) | (state_[i + 1] & lower_mask);
        state_[i] = state_[i + period_offset] ^ (joined >> 1U)
            ^ ((joined & 1U) == 0U ? 0U : matrix_a);
    }
    for (std::size_t i = state_size - period_offset; i < state_size - 1; ++i) {
        const std::uint32_t joined =
            (state_[i] & upper_mask) | (state_[i + 1] & lower_mask);
        state_[i] = state_[i + period_offset - state_size] ^ (joined >> 1U)
            ^ ((joined & 1U) == 0U ? 0U : matrix_a);
    }

    const std::uint32_t joined =
        (state_[state_size - 1] & upper_mask) | (state_[0] & lower_mask);
    state_[state_size - 1] = state_[period_offset - 1] ^ (joined >> 1U)
        ^ ((joined & 1U) == 0U ? 0U : matrix_a);
    index_ = 0;
}

std::uint32_t Mt19937::next_u32() noexcept {
    if (index_ >= state_size) {
        // The APK initializes an untouched Random object to seed 5489 before
        // twisting. Normal generator paths always call seed explicitly.
        if (index_ == state_size + 1) {
            seed(5489U);
        }
        twist();
    }

    std::uint32_t value = state_[index_++];
    value ^= value >> 11U;
    value ^= (value << 7U) & 0x9d2c'5680U;
    value ^= (value << 15U) & 0xefc6'0000U;
    value ^= value >> 18U;
    return value;
}

float Mt19937::next_float() noexcept {
    // Random::nextFloat uses VCVT.F64.U32, multiplies the exact binary64
    // integer by 2^-32, then executes one VCVT.F32.F64.  Do not introduce an
    // intermediate binary32 integer rounding: cave angles and noise offsets
    // observe the low bits retained by the direct integer-to-double path.
    constexpr double unit = 0x1p-32;
    return static_cast<float>(static_cast<double>(next_u32()) * unit);
}

} // namespace mcpe::worldgen::detail
