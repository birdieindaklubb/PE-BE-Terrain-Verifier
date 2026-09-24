#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mcpe::worldgen::detail {

class Mt19937 final {
public:
    explicit Mt19937(std::uint32_t seed = 5489U) noexcept;

    void seed(std::uint32_t value) noexcept;

    [[nodiscard]] std::uint32_t next_u32() noexcept;
    [[nodiscard]] float next_float() noexcept;

    [[nodiscard]] std::uint32_t next_bounded(std::uint32_t bound) noexcept {
        return next_u32() % bound;
    }

private:
    void twist() noexcept;

    static constexpr std::size_t state_size = 624;
    static constexpr std::size_t period_offset = 397;

    std::array<std::uint32_t, state_size> state_{};
    std::size_t index_ = state_size + 1;
};

} // namespace mcpe::worldgen::detail
