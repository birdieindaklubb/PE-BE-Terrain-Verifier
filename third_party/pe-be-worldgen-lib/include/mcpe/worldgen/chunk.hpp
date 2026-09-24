#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mcpe::worldgen {

struct Chunk {
    static constexpr std::int32_t width = 16;
    static constexpr std::int32_t height = 128;
    static constexpr std::size_t block_count = width * width * height;

    std::int32_t x{};
    std::int32_t z{};
    std::array<std::uint8_t, block_count> blocks{};
    std::array<std::uint8_t, block_count / 2> data{};
    std::array<std::uint8_t, block_count / 2> sky_light{};
    std::array<std::uint8_t, block_count / 2> block_light{};
    // LevelChunk stores columns as x | (z << 4), unlike the x-major block
    // array.  Heights are bytes in 0.6.1, including their wrap at 128.
    std::array<std::uint8_t, width * width> heightmap{};
    std::int32_t minimum_height{127};
    std::array<std::uint8_t, width * width> biomes{};
    std::array<float, width * width> temperatures{};
    std::array<float, width * width> rainfall{};

    [[nodiscard]] static constexpr std::size_t index(
        std::int32_t local_x,
        std::int32_t y,
        std::int32_t local_z) noexcept {
        return (static_cast<std::size_t>(local_x) * width
                + static_cast<std::size_t>(local_z))
                * height
            + static_cast<std::size_t>(y);
    }

    [[nodiscard]] static constexpr std::size_t column_index(
        std::int32_t local_x,
        std::int32_t local_z) noexcept {
        return static_cast<std::size_t>(local_x)
            | (static_cast<std::size_t>(local_z) << 4U);
    }

    [[nodiscard]] constexpr std::uint8_t block(
        std::int32_t local_x,
        std::int32_t y,
        std::int32_t local_z) const noexcept {
        return blocks[index(local_x, y, local_z)];
    }

    constexpr void set_block(
        std::int32_t local_x,
        std::int32_t y,
        std::int32_t local_z,
        std::uint8_t id) noexcept {
        blocks[index(local_x, y, local_z)] = id;
    }

    [[nodiscard]] constexpr std::uint8_t block_data(
        std::int32_t local_x,
        std::int32_t y,
        std::int32_t local_z) const noexcept {
        const std::size_t block_index = index(local_x, y, local_z);
        const std::uint8_t packed = data[block_index >> 1U];
        const std::uint32_t shift = (block_index & 1U) == 0U ? 0U : 4U;
        return static_cast<std::uint8_t>((packed >> shift) & 0x0fU);
    }

    constexpr void set_block_data(
        std::int32_t local_x,
        std::int32_t y,
        std::int32_t local_z,
        std::uint8_t value) noexcept {
        const std::size_t block_index = index(local_x, y, local_z);
        std::uint8_t& packed = data[block_index >> 1U];
        const std::uint32_t shift = (block_index & 1U) == 0U ? 0U : 4U;
        const std::uint8_t mask = static_cast<std::uint8_t>(0x0fU << shift);
        packed = static_cast<std::uint8_t>(
            (packed & static_cast<std::uint8_t>(~mask))
            | static_cast<std::uint8_t>((value & 0x0fU) << shift));
    }

    constexpr void set_block_and_data(
        std::int32_t local_x,
        std::int32_t y,
        std::int32_t local_z,
        std::uint8_t id,
        std::uint8_t value) noexcept {
        set_block(local_x, y, local_z, id);
        set_block_data(local_x, y, local_z, value);
    }
};

} // namespace mcpe::worldgen
