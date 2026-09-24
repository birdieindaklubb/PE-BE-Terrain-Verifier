#pragma once

#include "detail/mt19937.hpp"
#include "mcpe/worldgen/chunk.hpp"
#include "mcpe/worldgen/generated_content.hpp"
#include "mcpe/worldgen/position.hpp"
#include "v0_9_0/structure_piece.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mcpe::worldgen::v1_1_5_0::hell_fortress {

// The locator and component graph behind PE 1.1.5.0's NetherBridgeFeature.
// It purposefully contains no block writes: the source owns the load and
// population lifecycle, while this class retains only deterministic structure
// selection and geometry.
class Build final {
public:
    enum class PieceKind : std::uint8_t {
        bridge_crossing,
        bridge_straight,
        large_crossing,
        bridge_stairs,
        blaze_spawner,
        castle_entrance,
        small_corridor,
        small_corridor_crossing,
        small_corridor_right_turn,
        small_corridor_left_turn,
        stairs_corridor,
        corridor_balcony,
        nether_wart_farm,
        bridge_end,
    };

    struct Piece final {
        PieceKind kind{};
        v0_9_0::structure::BoundingBox bounds{};
        v0_9_0::structure::Orientation orientation{
            v0_9_0::structure::Orientation::none};
        std::int32_t depth{};
        // BridgeEnd owns an independent MT stream initialized from this word.
        // The regular pieces do not use it.
        std::uint32_t local_seed{};
        // Each small castle turn samples this flag in its native constructor.
        // The chest itself is deferred until the piece's chunk is populated.
        bool chest_pending{};
    };

    Build(
        detail::Mt19937& random,
        std::int32_t chunk_x,
        std::int32_t chunk_z);

    [[nodiscard]] const std::vector<Piece>& pieces() const noexcept;
    [[nodiscard]] const v0_9_0::structure::BoundingBox& bounds() const noexcept;

private:
    struct Weight {
        PieceKind kind{};
        std::int32_t probability{};
        std::int32_t generated{};
        std::int32_t limit{};
        bool allow_consecutive{};
    };

    [[nodiscard]] Piece* append_candidate(
        detail::Mt19937& random,
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        v0_9_0::structure::Orientation orientation,
        std::int32_t source_depth,
        bool castle_piece);
    [[nodiscard]] Piece* append_end(
        detail::Mt19937& random,
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        v0_9_0::structure::Orientation orientation,
        std::int32_t depth);
    [[nodiscard]] Piece* append_piece(
        detail::Mt19937& random,
        PieceKind kind,
        std::int32_t x,
        std::int32_t y,
        std::int32_t z,
        v0_9_0::structure::Orientation orientation,
        std::int32_t depth);

    void add_children(std::size_t piece_index, detail::Mt19937& random);
    void forward(
        std::size_t parent,
        detail::Mt19937& random,
        std::int32_t offset,
        std::int32_t y_offset,
        bool castle_piece);
    void left(
        std::size_t parent,
        detail::Mt19937& random,
        std::int32_t y_offset,
        std::int32_t offset,
        bool castle_piece);
    void right(
        std::size_t parent,
        detail::Mt19937& random,
        std::int32_t y_offset,
        std::int32_t offset,
        bool castle_piece);
    [[nodiscard]] bool collides(
        const v0_9_0::structure::BoundingBox& candidate) const noexcept;
    void calculate_bounds() noexcept;

    std::vector<Piece> pieces_;
    std::vector<std::size_t> pending_;
    std::vector<Weight> bridge_weights_;
    std::vector<Weight> castle_weights_;
    v0_9_0::structure::BoundingBox bounds_{};
    std::int32_t previous_weight_{-1};
};

// NetherBridgeFeature::isFeatureChunk. The candidate test is pure and can be
// used without instantiating a component graph.
[[nodiscard]] bool is_start_chunk(
    std::uint32_t world_seed,
    std::int32_t chunk_x,
    std::int32_t chunk_z) noexcept;

// StructureFeature's clipped block pass. `lava_sources` and
// `blaze_spawners`, when requested, retain the post-process order of the two
// writes whose side effects are observable outside raw block storage.
void apply_to_chunk(
    const Build& build,
    Chunk& chunk,
    detail::Mt19937& structure_random,
    std::vector<BlockPosition>* lava_sources = nullptr,
    std::vector<BlockPosition>* blaze_spawners = nullptr,
    std::vector<GeneratedChest>* chests = nullptr) noexcept;

// StructureFeature's surrounding-start traversal for one post-process chunk.
// MapGenBase reaches an eight-chunk radius before it invokes a start.
void apply_nearby_starts(
    std::uint32_t world_seed,
    Chunk& chunk,
    detail::Mt19937& population_random,
    std::vector<BlockPosition>* lava_sources = nullptr,
    std::vector<BlockPosition>* blaze_spawners = nullptr,
    std::vector<GeneratedChest>* chests = nullptr) noexcept;

} // namespace mcpe::worldgen::v1_1_5_0::hell_fortress
