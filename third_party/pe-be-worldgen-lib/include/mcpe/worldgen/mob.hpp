#pragma once

#include <cstdint>

namespace mcpe::worldgen {

// Numeric values are the PE 0.6.1 EntityType IDs used by MobFactory.
enum class MobType : std::uint8_t {
    chicken = 10,
    cow = 11,
    pig = 12,
    sheep = 13,
};

// A passive mob added by MobSpawner::postProcessSpawnMobs. These are the
// seed/world-state-derived fields established by the population pass itself.
struct GeneratedMob final {
    MobType type{};
    float x{};
    float y{};
    float z{};
    float yaw{};
    // MobFactory replaces Mob's constructor default with getMaxHealth().
    // This value is persisted as the entity's Health short.
    std::int16_t health{};
    std::int32_t age{};
    // Used only by sheep; zero is also the native default for other animals.
    std::uint8_t wool_color{};
};

} // namespace mcpe::worldgen
