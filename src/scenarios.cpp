#include "brickstack/scenarios.hpp"

#include <unordered_set>

#include "brickstack/hash.hpp"
#include "brickstack/world.hpp"

namespace brickstack {

std::vector<Event> tower(int32_t height, Material m) {
    std::vector<Event> ev;
    ev.reserve(static_cast<std::size_t>(height));
    for (int32_t z = 0; z < height; ++z)
        ev.push_back(evPlace({0, 0, z}, {1, 1, 1}, m));
    return ev;
}

std::vector<Event> cantilever(int32_t columnHeight, int32_t armLength, Material m) {
    std::vector<Event> ev;
    for (int32_t z = 0; z < columnHeight; ++z)
        ev.push_back(evPlace({0, 0, z}, {1, 1, 1}, m));
    const int32_t top = columnHeight - 1;
    for (int32_t x = 1; x <= armLength; ++x)
        ev.push_back(evPlace({x, 0, top}, {1, 1, 1}, m));
    return ev;
}

std::vector<Event> bridge(int32_t pillarHeight, int32_t span, Material m) {
    std::vector<Event> ev;
    const int32_t rightX = span + 1;
    for (int32_t z = 0; z < pillarHeight; ++z) {
        ev.push_back(evPlace({0, 0, z}, {1, 1, 1}, m));
        ev.push_back(evPlace({rightX, 0, z}, {1, 1, 1}, m));
    }
    const int32_t top = pillarHeight - 1;
    for (int32_t x = 1; x <= span; ++x)
        ev.push_back(evPlace({x, 0, top}, {1, 1, 1}, m));
    return ev;
}

std::vector<Event> floatingIsland(int32_t sizeX, int32_t sizeZ, int32_t height,
                                  Material m) {
    std::vector<Event> ev;
    for (int32_t z = 0; z < 1; ++z)  // single-layer slab in mid-air
        for (int32_t x = 0; x < sizeX; ++x)
            for (int32_t y = 0; y < sizeZ; ++y)
                ev.push_back(evPlaceUnchecked({x, y, height}, {1, 1, 1}, m));
    return ev;
}

std::vector<Event> seededBlob(uint64_t seed, int32_t targetBricks,
                              int32_t collapseAnchorId, int32_t collapseSteps) {
    std::vector<Event> ev;
    ev.reserve(static_cast<std::size_t>(targetBricks) + 2);
    SplitMix64 rng(seed);

    std::unordered_set<uint64_t> occupied;
    std::vector<Vec3i> placed;
    occupied.reserve(static_cast<std::size_t>(targetBricks) * 2);
    placed.reserve(static_cast<std::size_t>(targetBricks));

    // Seed the blob with a single ground brick.
    Vec3i root{0, 0, 0};
    occupied.insert(World::cellKey(root));
    placed.push_back(root);
    ev.push_back(evPlace(root, {1, 1, 1}, Material::Wood));

    const Vec3i faces[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                            {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    int32_t guard = 0;
    const int32_t guardMax = targetBricks * 40 + 100;
    while (static_cast<int32_t>(placed.size()) < targetBricks && guard < guardMax) {
        ++guard;
        const Vec3i& base = placed[rng.nextBounded(static_cast<uint32_t>(placed.size()))];
        const Vec3i& f = faces[rng.nextBounded(6)];
        Vec3i cand{base.x + f.x, base.y + f.y, base.z + f.z};
        if (cand.z < 0) continue;  // never below the floor
        const uint64_t key = World::cellKey(cand);
        if (occupied.count(key)) continue;  // keep placements always acceptable
        occupied.insert(key);
        placed.push_back(cand);
        ev.push_back(evPlace(cand, {1, 1, 1}, Material::Wood));
    }

    if (collapseAnchorId >= 0) {
        ev.push_back(evRemove(static_cast<BrickId>(collapseAnchorId)));
        ev.push_back(evStep(collapseSteps));
    }
    return ev;
}

}  // namespace brickstack
