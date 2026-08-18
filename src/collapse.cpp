#include "brickstack/collapse.hpp"

#include <algorithm>
#include <vector>

namespace brickstack {

StepReport CollapseSystem::step(World& world) {
    StepReport report;

    // 1. Evaluate stability over the resting set.
    report.stability = solver_.evaluate(world);

    // 2. Detach every unstable resting brick (id order -> deterministic).
    for (BrickId id : world.restingIds()) {
        const Status s = world.brick(id).status;
        if (s == Status::Unsupported || s == Status::Failing) {
            world.detachToFalling(id);
            ++report.detached;
        }
    }

    // 3. Gravity over the falling set, bottom-up then by id.
    std::vector<BrickId> falling;
    for (BrickId id : world.aliveIds())
        if (!world.brick(id).resting) falling.push_back(id);
    const int32_t fallingBefore = static_cast<int32_t>(falling.size());

    std::sort(falling.begin(), falling.end(), [&](BrickId a, BrickId b) {
        const Vec3i& oa = world.brick(a).origin;
        const Vec3i& ob = world.brick(b).origin;
        if (oa.z != ob.z) return oa.z < ob.z;
        return a < b;
    });

    for (BrickId id : falling) {
        const Brick& b = world.brick(id);
        const Vec3i target{b.origin.x, b.origin.y, b.origin.z - 1};
        const bool canDescend = (target.z >= 0) && world.boxIsFree(target, b.size);
        if (canDescend) {
            world.brickMut(id).origin = target;  // still falling (cells stay out)
            ++report.fell;
        } else {
            world.landAsResting(id, b.origin);  // comes to rest
            ++report.landed;
        }
    }

    report.stillFalling = report.fell;  // everything that descended is still airborne
    report.quiescent = (report.detached == 0 && fallingBefore == 0);
    return report;
}

int32_t CollapseSystem::settle(World& world, int32_t maxTicks, int32_t* outTicks) {
    int32_t ticks = 0;
    while (ticks < maxTicks) {
        StepReport r = step(world);
        ++ticks;
        if (r.quiescent) break;
    }
    if (outTicks) *outTicks = ticks;
    return ticks;
}

}  // namespace brickstack
