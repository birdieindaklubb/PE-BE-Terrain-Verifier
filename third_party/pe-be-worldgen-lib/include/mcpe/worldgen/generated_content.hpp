#pragma once

#include "mcpe/worldgen/position.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mcpe::worldgen {

// The ItemInstance fields written by a generated container.  `data` is the
// item's auxiliary value (damage / variant), not a block-state nibble.
struct GeneratedItemStack final {
    std::int16_t item_id{};
    std::int16_t count{};
    std::int16_t data{};
};

// Modern structure chests defer their contents to a named loot table.  The
// table seed is written when the chest block entity is created; the game does
// not roll item stacks at terrain-population time.
struct GeneratedLootTable final {
    std::string_view name{};
    std::uint32_t seed{};
};

// The entity identifier selected while generating a mob-spawner tile.  This
// is intentionally a generated-state record, not a simulation of the
// spawner's later ticking or mob AI.
enum class SpawnerType : std::uint8_t {
    skeleton,
    zombie,
    spider,
    cave_spider,
    silverfish,
    blaze,
};

struct GeneratedChest final {
    BlockPosition position{};
    std::array<std::optional<GeneratedItemStack>, 27> slots{};
    std::optional<GeneratedLootTable> deferred_loot{};
};

struct GeneratedSpawner final {
    BlockPosition position{};
    SpawnerType type{};
};

// Village components create these during their distinct post-population mob
// phase. Their coordinates are entity coordinates, so they intentionally
// retain the original binary32 representation instead of being rounded to a
// block position.
struct GeneratedVillager final {
    float x{};
    float y{};
    float z{};
    std::int32_t profession{};
};

// The deterministic layout selected for an End obsidian spike.  The
// corresponding block geometry is written by PE 1.1.5.0's End population
// pass; this record makes the seed-visible layout available without requiring
// callers to scan a region of chunks.
struct GeneratedEndPillar final {
    std::int32_t center_x{};
    std::int32_t center_z{};
    std::int32_t radius{};
    std::int32_t height{};
    bool guarded{};
};

} // namespace mcpe::worldgen
