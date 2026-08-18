#include "brickstack/world.hpp"

namespace brickstack {

namespace {
// 21-bit signed field per axis, biased so negatives pack cleanly. Range is
// [-(1<<20)+1, (1<<20)-1] per axis — comfortably beyond the benchmark sizes.
constexpr int32_t kCoordBias = 1 << 20;   // 1,048,576
constexpr int32_t kCoordMax = (1 << 20) - 1;
constexpr int32_t kCoordMin = -(kCoordMax);
constexpr uint64_t kFieldMask = (1ull << 21) - 1;
}  // namespace

bool World::coordInRange(const Vec3i& origin, const Vec3i& size) {
    const int32_t maxX = origin.x + size.x - 1;
    const int32_t maxY = origin.y + size.y - 1;
    const int32_t maxZ = origin.z + size.z - 1;
    auto ok = [](int32_t v) { return v >= kCoordMin && v <= kCoordMax; };
    return ok(origin.x) && ok(origin.y) && ok(origin.z) && ok(maxX) && ok(maxY) &&
           ok(maxZ);
}

uint64_t World::cellKey(Vec3i cell) {
    const uint64_t xf = static_cast<uint64_t>(cell.x + kCoordBias) & kFieldMask;
    const uint64_t yf = static_cast<uint64_t>(cell.y + kCoordBias) & kFieldMask;
    const uint64_t zf = static_cast<uint64_t>(cell.z + kCoordBias) & kFieldMask;
    return (xf << 42) | (yf << 21) | zf;
}

std::optional<BrickId> World::occupantAt(Vec3i cell) const {
    auto it = grid_.find(cellKey(cell));
    if (it == grid_.end()) return std::nullopt;
    return it->second;
}

bool World::boxIsFree(Vec3i origin, Vec3i size) const {
    if (!coordInRange(origin, size)) return false;
    if (origin.z < 0) return false;  // z == 0 is the ground floor; nothing below
    for (int32_t dz = 0; dz < size.z; ++dz)
        for (int32_t dy = 0; dy < size.y; ++dy)
            for (int32_t dx = 0; dx < size.x; ++dx) {
                Vec3i c{origin.x + dx, origin.y + dy, origin.z + dz};
                if (grid_.find(cellKey(c)) != grid_.end()) return false;
            }
    return true;
}

void World::writeCells(BrickId id, const Brick& b) {
    for (int32_t dz = 0; dz < b.size.z; ++dz)
        for (int32_t dy = 0; dy < b.size.y; ++dy)
            for (int32_t dx = 0; dx < b.size.x; ++dx) {
                Vec3i c{b.origin.x + dx, b.origin.y + dy, b.origin.z + dz};
                grid_[cellKey(c)] = id;
            }
}

void World::eraseCells(const Brick& b) {
    for (int32_t dz = 0; dz < b.size.z; ++dz)
        for (int32_t dy = 0; dy < b.size.y; ++dy)
            for (int32_t dx = 0; dx < b.size.x; ++dx) {
                Vec3i c{b.origin.x + dx, b.origin.y + dy, b.origin.z + dz};
                grid_.erase(cellKey(c));
            }
}

BrickId World::placeUnchecked(Vec3i origin, Vec3i size, Material material) {
    if (size.x <= 0 || size.y <= 0 || size.z <= 0) return kInvalidBrick;
    if (!boxIsFree(origin, size)) return kInvalidBrick;
    const BrickId id = static_cast<BrickId>(bricks_.size());
    Brick b;
    b.id = id;
    b.origin = origin;
    b.size = size;
    b.material = material;
    b.status = Status::Unsupported;
    b.alive = true;
    b.resting = true;
    bricks_.push_back(b);
    writeCells(id, b);
    ++aliveCount_;
    return id;
}

PlaceResult World::place(Vec3i origin, Vec3i size, Material material,
                         BrickId* outId) {
    if (size.x <= 0 || size.y <= 0 || size.z <= 0) return PlaceResult::DegenerateSize;
    if (!coordInRange(origin, size) || origin.z < 0) return PlaceResult::OutOfBounds;
    if (!boxIsFree(origin, size)) return PlaceResult::Overlap;

    // Adjacency: the box either touches the ground plane (z == 0) or is
    // face-adjacent to an existing resting brick.
    bool adjacent = (origin.z == 0);
    if (!adjacent) {
        for (int32_t dz = 0; dz < size.z && !adjacent; ++dz)
            for (int32_t dy = 0; dy < size.y && !adjacent; ++dy)
                for (int32_t dx = 0; dx < size.x && !adjacent; ++dx) {
                    Vec3i c{origin.x + dx, origin.y + dy, origin.z + dz};
                    const Vec3i faces[6] = {{c.x + 1, c.y, c.z}, {c.x - 1, c.y, c.z},
                                            {c.x, c.y + 1, c.z}, {c.x, c.y - 1, c.z},
                                            {c.x, c.y, c.z + 1}, {c.x, c.y, c.z - 1}};
                    for (const Vec3i& f : faces) {
                        if (grid_.find(cellKey(f)) != grid_.end()) {
                            adjacent = true;
                            break;
                        }
                    }
                }
    }
    if (!adjacent) return PlaceResult::NotAdjacent;

    const BrickId id = placeUnchecked(origin, size, material);
    if (outId) *outId = id;
    return PlaceResult::Ok;
}

bool World::remove(BrickId id) {
    if (id >= bricks_.size()) return false;
    Brick& b = bricks_[id];
    if (!b.alive) return false;
    if (b.resting) eraseCells(b);
    b.alive = false;
    --aliveCount_;
    return true;
}

std::vector<BrickId> World::restingIds() const {
    std::vector<BrickId> ids;
    ids.reserve(aliveCount_);
    for (const Brick& b : bricks_)
        if (b.alive && b.resting) ids.push_back(b.id);
    return ids;  // ascending by construction (id == slot)
}

std::vector<BrickId> World::aliveIds() const {
    std::vector<BrickId> ids;
    ids.reserve(aliveCount_);
    for (const Brick& b : bricks_)
        if (b.alive) ids.push_back(b.id);
    return ids;
}

void World::detachToFalling(BrickId id) {
    Brick& b = bricks_[id];
    if (!b.alive || !b.resting) return;
    eraseCells(b);
    b.resting = false;
    b.status = Status::Falling;
}

void World::landAsResting(BrickId id, Vec3i newOrigin) {
    Brick& b = bricks_[id];
    if (!b.alive) return;
    b.origin = newOrigin;
    b.resting = true;
    writeCells(id, b);
}

}  // namespace brickstack
