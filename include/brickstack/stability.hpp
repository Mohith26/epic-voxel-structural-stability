// Stability solver — classifies every resting brick as Supported, Failing, or
// Unsupported. Deterministic and integer-only.
//
// ============================ THE MODEL =============================
// A brick is an ANCHOR if any of its cells sits on the ground plane (z == 0).
// Two bricks are CONNECTED if any of their cells are face-adjacent. This yields
// an undirected "structure graph" over resting bricks.
//
// 1) REACHABILITY. Multi-source BFS from all anchors over the structure graph
//    assigns each reachable brick a hop-distance d (anchors have d = 0). Any
//    brick with no path to an anchor is UNSUPPORTED (it will detach and fall).
//
// 2) SUPPORT (propagates down the hop gradient, attenuating with distance and
//    branching). Processed in increasing-d order so results are independent of
//    within-layer iteration order:
//        support(anchor) = strength(material)
//        support(p)      = max over parents q (neighbors with d = d(p)-1) of
//                          floor( (support(q) - kAttenuationPerHop) / branch(q) )
//    where branch(q) = number of children of q (neighbors with d = d(q)+1),
//    clamped to >= 1. Support is clamped to >= 0. Intuition: an anchor radiates
//    a finite support budget that weakens with every hop outward and is split
//    among everything a piece holds up — so structures can only reach so far
//    from their foundations before peripheral pieces starve.
//
// 3) LOAD (accumulates up the hop gradient). Processed in decreasing-d order:
//        load(p) = weight(p) + sum over children c of floor( load(c) / parents(c) )
//    where parents(c) = number of neighbors of c with d = d(c)-1, clamped >= 1.
//    A piece carries its own weight plus its share of everything resting on it.
//
// 4) CLASSIFY. Connected AND support(p) >= load(p)  -> Supported.
//              Connected AND support(p) <  load(p)  -> Failing.
//              Not connected                        -> Unsupported.
//
// The model is a fixpoint over fixed BFS distances: every quantity depends only
// on already-processed layers, and max/sum are order-independent, so the result
// is a deterministic function of the resting set alone.
#pragma once

#include <cstdint>
#include <vector>

#include "brickstack/types.hpp"
#include "brickstack/world.hpp"

namespace brickstack {

struct StabilityStats {
    int32_t resting = 0;
    int32_t supported = 0;
    int32_t failing = 0;
    int32_t unsupported = 0;
    int32_t anchors = 0;
    int32_t maxDistance = 0;  // deepest hop-distance reached from an anchor
};

class StabilitySolver {
public:
    // Evaluate the resting set of `world`, writing Status into each resting
    // brick and returning summary counts.
    StabilityStats evaluate(World& world) const;
};

}  // namespace brickstack
