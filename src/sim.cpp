#include "brickstack/sim.hpp"

#include "brickstack/hash.hpp"

namespace brickstack {

BrickId Sim::apply(const Event& e) {
    switch (e.type) {
        case EventType::Place: {
            BrickId id = kInvalidBrick;
            world_.place(e.origin, e.size, e.material, &id);
            return id;  // kInvalidBrick if the placement was rejected
        }
        case EventType::PlaceUnchecked:
            return world_.placeUnchecked(e.origin, e.size, e.material);
        case EventType::Remove:
            world_.remove(e.target);
            return kInvalidBrick;
        case EventType::Step:
            step(e.steps);
            return kInvalidBrick;
    }
    return kInvalidBrick;
}

void Sim::step(int32_t n) {
    for (int32_t i = 0; i < n; ++i) collapse_.step(world_);
}

uint64_t Sim::worldHash() const {
    Fnv1a h;
    const std::vector<BrickId> ids = world_.aliveIds();  // ascending
    const uint64_t count = ids.size();
    h.mixPod(count);
    for (BrickId id : ids) {
        const Brick& b = world_.brick(id);
        // Mix each field individually so struct padding can never perturb the
        // hash across compilers/platforms.
        h.mixPod(b.id);
        h.mixPod(b.origin.x);
        h.mixPod(b.origin.y);
        h.mixPod(b.origin.z);
        h.mixPod(b.size.x);
        h.mixPod(b.size.y);
        h.mixPod(b.size.z);
        h.mixPod(static_cast<uint16_t>(b.material));
        h.mixPod(static_cast<uint8_t>(b.status));
        h.mixPod(static_cast<uint8_t>(b.resting ? 1 : 0));
    }
    return h.value();
}

}  // namespace brickstack
