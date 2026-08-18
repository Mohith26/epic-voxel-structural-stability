// World — the integer voxel grid plus piece storage and placement rules.
//
// Storage is a flat vector<Brick> keyed by id (id == slot, never reused). The
// occupancy grid maps packed cell coordinates -> the resting brick occupying
// that cell. Only *resting* bricks appear in the grid; falling bricks are held
// out until they land.
#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "brickstack/types.hpp"

namespace brickstack {

// Result of a placement attempt via the rule-checked build API.
enum class PlaceResult : uint8_t {
    Ok = 0,
    OutOfBounds = 1,   // a cell has z < 0 or a coordinate outside the packable range
    Overlap = 2,       // a cell is already occupied
    NotAdjacent = 3,   // does not touch the ground plane or any existing piece
    DegenerateSize = 4,// size <= 0 on some axis
};

class World {
public:
    // ---- Placement -------------------------------------------------------

    // Rule-checked build placement (adjacency + no-overlap + bounds). On Ok,
    // writes the new id to *outId (if non-null). Deterministic and pure aside
    // from the mutation it performs.
    PlaceResult place(Vec3i origin, Vec3i size, Material material, BrickId* outId);

    // Unchecked placement used to construct test fixtures (e.g. a floating
    // island) and to re-insert landed collapse debris. Bypasses the adjacency
    // rule but still refuses overlap / out-of-bounds so the grid stays sane.
    // Returns kInvalidBrick if the cells are unavailable.
    BrickId placeUnchecked(Vec3i origin, Vec3i size, Material material);

    // Remove a resting brick (tombstone; frees its cells). No-op if already
    // dead. Returns true if a live brick was removed.
    bool remove(BrickId id);

    // ---- Queries ---------------------------------------------------------

    // Which brick, if any, occupies a cell (resting bricks only).
    std::optional<BrickId> occupantAt(Vec3i cell) const;

    // Would a box of `size` at `origin` fit against the resting grid (all cells
    // free and in bounds)? Does not consider adjacency.
    bool boxIsFree(Vec3i origin, Vec3i size) const;

    const Brick& brick(BrickId id) const { return bricks_[id]; }
    Brick& brickMut(BrickId id) { return bricks_[id]; }

    std::size_t slotCount() const { return bricks_.size(); }

    // Alive resting brick ids, ascending — the canonical deterministic order.
    std::vector<BrickId> restingIds() const;

    // Alive bricks (resting or falling), ascending id.
    std::vector<BrickId> aliveIds() const;

    std::size_t aliveCount() const { return aliveCount_; }

    // ---- Grid maintenance for the collapse system ------------------------

    void detachToFalling(BrickId id);  // remove cells from grid, mark falling
    void landAsResting(BrickId id, Vec3i newOrigin);  // re-insert at newOrigin

    // ---- Cell packing (public for the solver / tests) --------------------

    static bool coordInRange(const Vec3i& origin, const Vec3i& size);
    static uint64_t cellKey(Vec3i cell);

private:
    void writeCells(BrickId id, const Brick& b);
    void eraseCells(const Brick& b);

    std::vector<Brick> bricks_;
    std::unordered_map<uint64_t, BrickId> grid_;
    std::size_t aliveCount_ = 0;
};

}  // namespace brickstack
