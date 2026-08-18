// Sim — the deterministic driver: an event stream, a fixed-timestep tick loop,
// and a bit-identical world-state hash. Two Sim instances fed the same events
// produce identical hash sequences.
#pragma once

#include <cstdint>
#include <vector>

#include "brickstack/collapse.hpp"
#include "brickstack/types.hpp"
#include "brickstack/world.hpp"

namespace brickstack {

enum class EventType : uint8_t {
    Place = 0,           // rule-checked placement (adjacency + no-overlap)
    PlaceUnchecked = 1,  // fixture/debris placement (overlap/bounds still enforced)
    Remove = 2,          // remove a brick by id
    Step = 3,            // advance `steps` fixed ticks of collapse simulation
};

struct Event {
    EventType type = EventType::Step;
    Vec3i origin{};
    Vec3i size{1, 1, 1};
    Material material = Material::Wood;
    BrickId target = kInvalidBrick;
    int32_t steps = 1;
};

// Helpers to build events tersely.
inline Event evPlace(Vec3i o, Vec3i s, Material m) {
    Event e; e.type = EventType::Place; e.origin = o; e.size = s; e.material = m; return e;
}
inline Event evPlaceUnchecked(Vec3i o, Vec3i s, Material m) {
    Event e; e.type = EventType::PlaceUnchecked; e.origin = o; e.size = s; e.material = m; return e;
}
inline Event evRemove(BrickId id) {
    Event e; e.type = EventType::Remove; e.target = id; return e;
}
inline Event evStep(int32_t n = 1) {
    Event e; e.type = EventType::Step; e.steps = n; return e;
}

class Sim {
public:
    // Apply one event. Returns the id assigned by a Place/PlaceUnchecked (or
    // kInvalidBrick if it was rejected / not a placement).
    BrickId apply(const Event& e);

    // Advance n fixed ticks of the collapse simulation.
    void step(int32_t n = 1);

    // Run to steady state (bounded). Returns ticks run.
    int32_t settle(int32_t maxTicks = 100000, int32_t* outTicks = nullptr) {
        return collapse_.settle(world_, maxTicks, outTicks);
    }

    // Canonical, bit-identical hash of the entire world (all alive bricks in id
    // order: id, origin, size, material, status, resting).
    uint64_t worldHash() const;

    World& world() { return world_; }
    const World& world() const { return world_; }

private:
    World world_;
    CollapseSystem collapse_;
};

}  // namespace brickstack
