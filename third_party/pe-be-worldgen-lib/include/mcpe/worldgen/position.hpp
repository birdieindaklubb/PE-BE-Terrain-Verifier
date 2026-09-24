#pragma once

#include <cstdint>

namespace mcpe::worldgen {

struct BlockPosition final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
};

} // namespace mcpe::worldgen
