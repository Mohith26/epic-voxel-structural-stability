// Collapse system — fixed-timestep gravity + cascading re-evaluation.
//
// One step(dt) does exactly:
//   1. Run the stability solver over the current resting set.
//   2. Detach every Unsupported or Failing brick into the "falling" set
//      (removing its cells from the grid). Removing support here is what makes
//      collapse CASCADE: a brick that loses its only support becomes
//      unsupported on the next evaluation and detaches in turn.
//   3. Apply gravity: falling bricks are processed bottom-up (min-z, then id)
//      and each descends one cell if the cells directly below are free of
//      resting bricks; otherwise it lands and rejoins the resting structure.
//      A brick at z == 0 rests on the ground floor.
//
// dt is a fixed integer number of ticks (game-style fixed timestep). Gravity is
// exactly one cell per tick, so the whole thing is integer and deterministic.
#pragma once

#include <cstdint>

#include "brickstack/stability.hpp"
#include "brickstack/world.hpp"

namespace brickstack {

struct StepReport {
    StabilityStats stability{};
    int32_t detached = 0;  // bricks that left the resting set this step
    int32_t fell = 0;      // falling bricks that descended a cell this step
    int32_t landed = 0;    // falling bricks that came to rest this step
    int32_t stillFalling = 0;  // falling bricks still airborne after this step
    bool quiescent = false;    // nothing detached and nothing is falling
};

class CollapseSystem {
public:
    // Advance one fixed tick.
    StepReport step(World& world);

    // Advance ticks until quiescent (steady state) or maxTicks is hit. Returns
    // the number of ticks actually run.
    int32_t settle(World& world, int32_t maxTicks, int32_t* outTicks = nullptr);

private:
    StabilitySolver solver_;
};

}  // namespace brickstack
