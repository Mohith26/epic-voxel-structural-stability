// Reproducible scenario builders — the hand-constructed structures the solver
// tests assert against, plus a seeded random-growth generator used by the
// determinism and benchmark drivers. Every builder returns a deterministic
// event stream, so replaying it in any Sim yields identical state.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "brickstack/sim.hpp"
#include "brickstack/types.hpp"

namespace brickstack {

// Vertical column of `height` 1x1x1 bricks on the ground at (0,0,z).
std::vector<Event> tower(int32_t height, Material m = Material::Wood);

// A column of `columnHeight` topped by a horizontal arm of `armLength` bricks
// extending in +x — the classic overhang that fails past a certain reach.
std::vector<Event> cantilever(int32_t columnHeight, int32_t armLength,
                              Material m = Material::Wood);

// Two ground pillars of `pillarHeight` separated by `span`, joined by a deck at
// the top — supported from BOTH ends.
std::vector<Event> bridge(int32_t pillarHeight, int32_t span,
                          Material m = Material::Stone);

// A connected cluster of bricks placed (unchecked) in mid-air with no path to
// the ground — every piece is Unsupported.
std::vector<Event> floatingIsland(int32_t sizeX = 3, int32_t sizeZ = 3,
                                  int32_t height = 6, Material m = Material::Wood);

// Deterministic connected random blob of exactly `targetBricks` 1x1x1 bricks,
// grown by adjacency from the ground so no placement is ever rejected. If
// `collapseAnchorId` >= 0, appends a Remove of that brick followed by a Step
// burst so the event stream also exercises the collapse path.
std::vector<Event> seededBlob(uint64_t seed, int32_t targetBricks,
                              int32_t collapseAnchorId = -1,
                              int32_t collapseSteps = 64);

}  // namespace brickstack
