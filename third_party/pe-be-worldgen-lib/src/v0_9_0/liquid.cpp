#include "v0_9_0/liquid.hpp"

#include "detail/liquid_access.hpp"
#include "detail/wrap.hpp"
#include "v0_9_0/blocks.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mcpe::worldgen::v0_9_0::liquid {
namespace {

struct Position final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
};

struct PendingTick final {
    Position position{};
    std::uint8_t id{};
    std::int32_t scheduled_time{};
};

enum class Fluid : std::uint8_t {
    water,
    lava,
};

[[nodiscard]] constexpr std::uint8_t flowing_id(Fluid fluid) noexcept {
    return fluid == Fluid::water ? block::water : block::lava;
}

[[nodiscard]] constexpr std::uint8_t still_id(Fluid fluid) noexcept {
    return fluid == Fluid::water ? block::calm_water : block::calm_lava;
}

[[nodiscard]] constexpr std::int32_t flow_step(Fluid fluid) noexcept {
    // In an ordinary PE world lava gains two depth levels per update; water
    // gains one. The alternate lava step is for a different level type, not
    // the Overworld generator represented here.
    return fluid == Fluid::water ? 1 : 2;
}

[[nodiscard]] constexpr std::int32_t tick_delay(Fluid fluid) noexcept {
    return fluid == Fluid::water ? 5 : 30;
}

[[nodiscard]] constexpr bool is_fluid(
    std::uint8_t id,
    Fluid fluid) noexcept {
    return fluid == Fluid::water ? block::is_water(id) : block::is_lava(id);
}

[[nodiscard]] constexpr Position offset(
    Position position,
    std::size_t direction) noexcept {
    if (direction == 0U) {
        position.x = detail::wrapping_sub(position.x, 1);
    } else if (direction == 1U) {
        position.x = detail::wrapping_add(position.x, 1);
    } else if (direction == 2U) {
        position.z = detail::wrapping_sub(position.z, 1);
    } else {
        position.z = detail::wrapping_add(position.z, 1);
    }
    return position;
}

[[nodiscard]] constexpr bool reverses_direction(
    std::size_t direction,
    std::int32_t prior_direction) noexcept {
    return (direction == 0U && prior_direction == 1)
        || (direction == 1U && prior_direction == 0)
        || (direction == 2U && prior_direction == 3)
        || (direction == 3U && prior_direction == 2);
}

// This is LiquidTileDynamic::_isWaterBlocking.  The explicit tile checks
// remain necessary even though their material predicates are non-blocking.
// The remaining test is Material::blocksMotion || isSolid.
[[nodiscard]] constexpr bool water_blocking(std::uint8_t id) noexcept {
    return id == block::wooden_door
        || id == block::sign
        || id == block::ladder
        || id == block::reeds
        || block::material_blocks_motion(id)
        || block::material_is_solid(id);
}

[[nodiscard]] std::int32_t fluid_depth(
    const detail::LiquidAccess& world,
    Position position,
    Fluid fluid) noexcept {
    return is_fluid(world.block(position.x, position.y, position.z), fluid)
        ? static_cast<std::int32_t>(
              world.data(position.x, position.y, position.z))
        : -1;
}

class LiquidTickQueue final {
public:
    LiquidTickQueue(
        detail::LiquidAccess& world,
        detail::Mt19937& random,
        std::int32_t lava_flow_step = 2) noexcept
        : world_(world),
          random_(random),
          lava_flow_step_(lava_flow_step) {
    }

    void add(
        Position position,
        Fluid fluid,
        std::int32_t delay) noexcept {
        // TileTickingQueue::add validates the same +/-8 tile cube before it
        // creates a TickNextTickData.  The vertical coordinates do not alter
        // ChunkSource availability.
        if (!world_.has_chunks_at(position.x, position.z, 8)) {
            return;
        }
        heap_.push_back({position, flowing_id(fluid),
                         detail::wrapping_add(current_time_, delay)});
        push_heap_native();
    }

    void drain() noexcept {
        // tickAllPendingTicks repeatedly advances the queue clock to the
        // earliest pending deadline. _tickToCurrentTime snapshots the heap
        // size on entry and processes at most that many entries whose deadline
        // is now due. Consequently ticks added by callbacks stay in the heap
        // for a later sweep even when they have the same deadline. Retaining
        // absolute time is also essential when water (delay 5), ordinary lava
        // (delay 30), and lava's delayed transition (delay 120) coexist.
        // Lava's depth-transition branch consumes the queue's separate MT.
        while (!heap_.empty()) {
            current_time_ = heap_.front().scheduled_time;
            const std::size_t sweep_size = heap_.size();
            std::size_t processed = 0U;
            while (processed < sweep_size && !heap_.empty()
                   && heap_.front().scheduled_time <= current_time_) {
                const PendingTick pending = pop_heap_native();
                ++processed;
                if (world_.has_chunks_at(
                        pending.position.x, pending.position.z, 8)
                    && world_.block(
                           pending.position.x,
                           pending.position.y,
                           pending.position.z) == pending.id
                    && pending.id != block::air) {
                    tick(
                        pending.position,
                        pending.id == block::water
                             ? Fluid::water
                             : Fluid::lava);
                }
            }
        }
    }

private:
    // The APK's libstdc++ heap uses std::greater<TickNextTickData>, whose
    // comparison is scheduled_time only.  Keeping the binary heap here (not
    // std::priority_queue) preserves its equal-deadline insertion and child
    // selection behavior.
    [[nodiscard]] static bool later(
        const PendingTick& lhs,
        const PendingTick& rhs) noexcept {
        return lhs.scheduled_time > rhs.scheduled_time;
    }

    void push_heap_native() noexcept {
        push_heap_native(heap_.size() - 1U, 0U, heap_.back());
    }

    void push_heap_native(
        std::size_t hole,
        std::size_t top,
        const PendingTick& value) noexcept {
        while (top < hole) {
            const std::size_t parent = (hole - 1U) / 2U;
            if (!later(heap_[parent], value)) {
                break;
            }
            heap_[hole] = heap_[parent];
            hole = parent;
        }
        heap_[hole] = value;
    }

    [[nodiscard]] PendingTick pop_heap_native() noexcept {
        const PendingTick result = heap_.front();
        const PendingTick value = heap_.back();
        heap_.pop_back();
        if (heap_.empty()) {
            return result;
        }

        // This is libstdc++'s __adjust_heap followed by __push_heap, rather
        // than the superficially similar usual sift-down. The distinction is
        // observable for equal tick deadlines, which occur throughout this
        // phase because every flowing-water delay is five ticks.
        std::size_t hole = 0U;
        const std::size_t length = heap_.size();
        while (hole < (length - 1U) / 2U) {
            std::size_t child = 2U * (hole + 1U);
            if (later(heap_[child], heap_[child - 1U])) {
                --child;
            }
            heap_[hole] = heap_[child];
            hole = child;
        }
        if ((length & 1U) == 0U && hole == (length - 2U) / 2U) {
            const std::size_t child = 2U * hole + 1U;
            heap_[hole] = heap_[child];
            hole = child;
        }
        push_heap_native(hole, 0U, value);
        return result;
    }

    [[nodiscard]] bool can_spread_to(
        Position position,
        Fluid fluid) const noexcept {
        if (position.y < 0) {
            return false;
        }
        const std::uint8_t target = world_.block(
            position.x, position.y, position.z);
        // Dynamic water may replace another non-lava material; dynamic lava
        // may replace water but never any lava. This is intentionally not a
        // generic "not liquid" test.
        return !is_fluid(target, fluid)
            && !block::is_lava(target)
            && !water_blocking(target);
    }

    [[nodiscard]] std::int32_t get_highest(
        Position position,
        std::int32_t current,
        Fluid fluid) noexcept {
        std::int32_t candidate = fluid_depth(world_, position, fluid);
        if (candidate < 0) {
            return current;
        }
        if (candidate == 0) {
            ++adjacent_sources_;
        } else if (candidate > 7) {
            candidate = 0;
        }
        return current < 0 || candidate < current ? candidate : current;
    }

    [[nodiscard]] std::int32_t slope_distance(
        Position position,
        std::int32_t distance,
        std::int32_t prior_direction,
        Fluid fluid) const noexcept {
        std::int32_t best = 1000;
        for (std::size_t direction = 0U; direction < 4U; ++direction) {
            if (reverses_direction(direction, prior_direction)) {
                continue;
            }
            const Position next = offset(position, direction);
            if (water_blocking(world_.block(next.x, next.y, next.z))) {
                continue;
            }
            if (is_fluid(world_.block(next.x, next.y, next.z), fluid)
                && world_.data(next.x, next.y, next.z) == 0U) {
                continue;
            }

            const Position below{next.x, next.y - 1, next.z};
            if (!water_blocking(world_.block(below.x, below.y, below.z))) {
                return distance;
            }
            if (distance < 4) {
                const std::int32_t candidate = slope_distance(
                    next, distance + 1, static_cast<std::int32_t>(direction),
                    fluid);
                if (candidate <= best) {
                    best = candidate;
                }
            }
        }
        return best;
    }

    [[nodiscard]] const std::array<bool, 4>& spread_for(
        Position position,
        Fluid fluid) noexcept {
        for (std::size_t direction = 0U; direction < 4U; ++direction) {
            const Position next = offset(position, direction);
            slope_distance_[direction] = 1000;
            if (water_blocking(world_.block(next.x, next.y, next.z))) {
                continue;
            }
            if (is_fluid(world_.block(next.x, next.y, next.z), fluid)
                && world_.data(next.x, next.y, next.z) == 0U) {
                continue;
            }

            const Position below{next.x, next.y - 1, next.z};
            slope_distance_[direction] = water_blocking(
                world_.block(below.x, below.y, below.z))
                ? slope_distance(
                      next, 1, static_cast<std::int32_t>(direction), fluid)
                : 0;
        }

        std::int32_t minimum = slope_distance_[0];
        for (std::size_t direction = 1U; direction < 4U; ++direction) {
            if (slope_distance_[direction] < minimum) {
                minimum = slope_distance_[direction];
            }
        }
        for (std::size_t direction = 0U; direction < 4U; ++direction) {
            spread_[direction] = slope_distance_[direction] == minimum;
        }
        return spread_;
    }

    void set_static(Position position, Fluid fluid) noexcept {
        (void)world_.set_block_and_data(
            position.x,
            position.y,
            position.z,
            still_id(fluid),
            world_.data(position.x, position.y, position.z),
            false);
    }

    [[nodiscard]] bool solidify_lava(
        Position lava,
        Position changed) noexcept {
        if (!block::is_water(world_.block(changed.x, changed.y, changed.z))) {
            return false;
        }
        const std::uint8_t depth = world_.data(lava.x, lava.y, lava.z);
        if (depth == 0U) {
            (void)world_.set_block_and_data(
                lava.x, lava.y, lava.z, block::obsidian, 0, false);
            return true;
        }
        if (depth <= 4U) {
            (void)world_.set_block_and_data(
                lava.x, lava.y, lava.z, block::cobblestone, 0, false);
            return true;
        }
        return false;
    }

    void notify_neighbors(Position position, Fluid changed_fluid) noexcept {
        // An instant liquid tick sends six neighbour notifications. A still
        // liquid neighbour first handles water/lava solidification, then, if
        // still present, becomes flowing with its existing depth and receives
        // its normal delay. That conversion has no further notification.
        // TileSource::updateNeighborsAt does not use the conventional
        // coordinate order. Its x-/x+, z+/z-, y+/y- sequence determines the
        // insertion order of equal-deadline liquid ticks.
        constexpr std::array<Position, 6> neighbors{{
            {-1, 0, 0}, {1, 0, 0}, {0, 0, 1},
            {0, 0, -1}, {0, 1, 0}, {0, -1, 0},
        }};
        for (const Position delta : neighbors) {
            const Position neighbor{
                detail::wrapping_add(position.x, delta.x),
                detail::wrapping_add(position.y, delta.y),
                detail::wrapping_add(position.z, delta.z),
            };
            const std::uint8_t neighbor_id = world_.block(
                neighbor.x, neighbor.y, neighbor.z);
            if (block::is_lava(neighbor_id)
                && changed_fluid == Fluid::water
                && solidify_lava(neighbor, position)) {
                continue;
            }
            Fluid neighbor_fluid{};
            if (neighbor_id == block::calm_water) {
                neighbor_fluid = Fluid::water;
            } else if (neighbor_id == block::calm_lava) {
                neighbor_fluid = Fluid::lava;
            } else {
                continue;
            }
            const std::uint8_t depth = world_.data(
                neighbor.x, neighbor.y, neighbor.z);
            if (world_.set_block_and_data(
                    neighbor.x, neighbor.y, neighbor.z,
                    flowing_id(neighbor_fluid), depth, false)) {
                add(neighbor, neighbor_fluid, tick_delay(neighbor_fluid));
            }
        }
    }

    void spread(
        Position position,
        std::int32_t depth,
        Fluid fluid) noexcept {
        (void)world_.set_block_and_data(
            position.x,
            position.y,
            position.z,
            flowing_id(fluid),
            static_cast<std::uint8_t>(depth),
            false);
        notify_neighbors(position, fluid);
        add(position, fluid, tick_delay(fluid));
    }

    void spread_down(
        Position position,
        std::int32_t depth,
        Fluid fluid) noexcept {
        if (fluid == Fluid::lava
            && block::is_water(world_.block(
                position.x, position.y, position.z))) {
            (void)world_.set_block_and_data(
                position.x, position.y, position.z, block::rock, 0, false);
            return;
        }
        spread(position, depth, fluid);
    }

    void try_spread_to(
        Position position,
        std::int32_t depth,
        Fluid fluid) noexcept {
        if (can_spread_to(position, fluid)) {
            spread(position, depth, fluid);
        }
    }

    void tick(Position position, Fluid fluid) noexcept {
        std::int32_t current = fluid_depth(world_, position, fluid);
        std::int32_t delay = tick_delay(fluid);
        if (current < 1) {
            set_static(position, fluid);
            const Position below{position.x, position.y - 1, position.z};
            if (can_spread_to(below, fluid)) {
                spread_down(below, current + 8, fluid);
                return;
            }
            if (current != 0) {
                return;
            }
        } else {
            adjacent_sources_ = 0;
            std::int32_t smallest = -100;
            smallest = get_highest(offset(position, 0U), smallest, fluid);
            smallest = get_highest(offset(position, 1U), smallest, fluid);
            smallest = get_highest(offset(position, 2U), smallest, fluid);
            smallest = get_highest(offset(position, 3U), smallest, fluid);

            const std::int32_t step = fluid == Fluid::water
                ? 1
                : lava_flow_step_;
            std::int32_t next = smallest + step;
            if (next > 7 || smallest < 0) {
                next = -1;
            }
            const Position above{position.x, position.y + 1, position.z};
            const std::int32_t above_depth = fluid_depth(world_, above, fluid);
            if (above_depth >= 0) {
                next = above_depth <= 7 ? above_depth + 8 : above_depth;
            }
            if (fluid == Fluid::water && adjacent_sources_ >= 2) {
                const Position below{position.x, position.y - 1, position.z};
                if (water_blocking(world_.block(below.x, below.y, below.z))
                    || (is_fluid(
                            world_.block(below.x, below.y, below.z), fluid)
                        // The second source-formation clause reads the
                        // current cell's depth, not the cell below it.
                        && world_.data(position.x, position.y, position.z) == 0U)) {
                    next = 0;
                }
            }

            // Lava delays a newly deepening flow by four times its ordinary
            // period three out of four times. This is the only random draw in
            // the instant liquid queue; its fire routine is disabled there.
            if (fluid == Fluid::lava) {
                const std::int32_t comparison_depth = current < 8
                    ? next
                    : current;
                if (comparison_depth <= 7 && next > current
                    && (random_.next_u32() & 3U) != 0U) {
                    delay = detail::wrapping_mul(delay, 4);
                }
            }

            if (next == current) {
                set_static(position, fluid);
            } else {
                if (next < 0) {
                    (void)world_.set_block_and_data(
                        position.x, position.y, position.z,
                        block::air, 0, false);
                    const Position below{position.x, position.y - 1, position.z};
                    if (!can_spread_to(below, fluid)) {
                        return;
                    }
                    current = next;
                    spread_down(below, current + 8, fluid);
                    return;
                }
                (void)world_.set_block_and_data(
                    position.x, position.y, position.z,
                    flowing_id(fluid), static_cast<std::uint8_t>(next), false);
                notify_neighbors(position, fluid);
                add(position, fluid, delay);
                current = next;
            }
        }

        const Position below{position.x, position.y - 1, position.z};
        if (can_spread_to(below, fluid)) {
            spread_down(
                below, current > 7 ? current : current + 8, fluid);
            return;
        }
        if (current < 0 || (current > 0
                            && !water_blocking(
                                world_.block(below.x, below.y, below.z)))) {
            return;
        }

        const std::array<bool, 4>& candidates = spread_for(position, fluid);
        std::int32_t horizontal_depth = current > 7
            ? 1
            : current + (fluid == Fluid::water ? 1 : lava_flow_step_);
        if (horizontal_depth > 7) {
            return;
        }
        // _getSpread stores these bytes on each liquid singleton. Static
        // liquid callbacks enqueue later ticks, so they cannot overwrite this
        // call's directions before the loop completes.
        for (std::size_t direction = 0U; direction < 4U; ++direction) {
            if (candidates[direction]) {
                try_spread_to(
                    offset(position, direction), horizontal_depth, fluid);
            }
        }
    }

    detail::LiquidAccess& world_;
    detail::Mt19937& random_;
    std::int32_t lava_flow_step_{};
    std::vector<PendingTick> heap_;
    std::int32_t current_time_{};
    std::int32_t adjacent_sources_{};
    std::array<std::int32_t, 4> slope_distance_{};
    std::array<bool, 4> spread_{};
};

void activate_first_still_water_in_column(
    World& world,
    LiquidTickQueue& ticks,
    std::int32_t x,
    std::int32_t z) noexcept {
    const std::int32_t height = world.heightmap(x, z);
    bool starts_a_run = true;
    for (std::int32_t y = 0; y < height; ++y) {
        if (world.block(x, y, z) != block::calm_water) {
            starts_a_run = true;
            continue;
        }
        if (!starts_a_run) {
            continue;
        }
        starts_a_run = false;
        (void)world.set_block_and_data(x, y, z, block::water, 0, false);
        ticks.add({x, y, z}, Fluid::water, 0);
    }
}

} // namespace

void settle_chunk_boundary_water(
    World& world,
    std::int32_t block_x,
    std::int32_t block_z) noexcept {
    LiquidTickQueue ticks{world, world.deferred_liquid_random_};

    // RandomLevelSource stores this exact perimeter order as a function-local
    // 60-element TilePos table: west edge, north/south interior pairs, then
    // east edge. It is not interchangeable with a generic perimeter loop.
    for (std::int32_t local_z = 0; local_z < Chunk::width; ++local_z) {
        activate_first_still_water_in_column(
            world, ticks, block_x, detail::wrapping_add(block_z, local_z));
    }
    for (std::int32_t local_x = 1; local_x < Chunk::width - 1; ++local_x) {
        const std::int32_t x = detail::wrapping_add(block_x, local_x);
        activate_first_still_water_in_column(world, ticks, x, block_z);
        activate_first_still_water_in_column(
            world, ticks, x, detail::wrapping_add(block_z, Chunk::width - 1));
    }
    for (std::int32_t local_z = 0; local_z < Chunk::width; ++local_z) {
        activate_first_still_water_in_column(
            world,
            ticks,
            detail::wrapping_add(block_x, Chunk::width - 1),
            detail::wrapping_add(block_z, local_z));
    }

    ticks.drain();
}

void settle_nether_lava(
    detail::LiquidAccess& world,
    detail::Mt19937& random,
    std::span<const NetherLavaTick> initial_ticks) noexcept {
    LiquidTickQueue ticks{world, random, 1};
    for (const NetherLavaTick& tick : initial_ticks) {
        ticks.add({tick.x, tick.y, tick.z}, Fluid::lava, tick.delay);
    }
    ticks.drain();
}

} // namespace mcpe::worldgen::v0_9_0::liquid
