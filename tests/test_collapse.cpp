// Collapse-simulation correctness: gravity, landing, and cascading re-eval.
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "brickstack/collapse.hpp"
#include "brickstack/scenarios.hpp"
#include "brickstack/sim.hpp"
#include "brickstack/stability.hpp"

using namespace brickstack;

namespace {
StabilityStats evaluateNow(Sim& sim) {
    StabilitySolver solver;
    return solver.evaluate(sim.world());
}
int32_t maxRestingZ(const World& w) {
    int32_t hi = -1;
    for (BrickId id : w.aliveIds())
        if (w.brick(id).resting) hi = std::max(hi, w.brick(id).origin.z);
    return hi;
}
}  // namespace

TEST(Collapse, StableTowerDoesNotMove) {
    Sim sim;
    for (const Event& e : tower(5, Material::Wood)) sim.apply(e);
    // Record physical positions before simulating.
    std::vector<int32_t> zBefore;
    for (BrickId id : sim.world().aliveIds()) zBefore.push_back(sim.world().brick(id).origin.z);

    int32_t ticks = 0;
    sim.settle(1000, &ticks);
    const uint64_t settled = sim.worldHash();

    // Nothing moved: same count, same z per brick.
    ASSERT_EQ(sim.world().aliveCount(), 5u);
    std::size_t i = 0;
    for (BrickId id : sim.world().aliveIds()) {
        EXPECT_EQ(sim.world().brick(id).origin.z, zBefore[i]);
        EXPECT_TRUE(sim.world().brick(id).resting);
        ++i;
    }
    StabilityStats s = evaluateNow(sim);
    EXPECT_EQ(s.supported, 5);
    EXPECT_EQ(s.failing, 0);

    // Re-settling a stable structure is an exact no-op (idempotent steady state).
    sim.settle(1000);
    EXPECT_EQ(sim.worldHash(), settled);
}

TEST(Collapse, FloatingIslandFallsToGround) {
    Sim sim;
    for (const Event& e : floatingIsland(3, 3, 6, Material::Wood)) sim.apply(e);
    ASSERT_EQ(maxRestingZ(sim.world()), 6);
    int32_t ticks = 0;
    sim.settle(1000, &ticks);
    EXPECT_GT(ticks, 1);  // it actually fell over several ticks
    // All 9 pieces land on the floor and become supported ground debris.
    EXPECT_EQ(sim.world().aliveCount(), 9u);
    for (BrickId id : sim.world().aliveIds()) {
        EXPECT_TRUE(sim.world().brick(id).resting);
        EXPECT_EQ(sim.world().brick(id).origin.z, 0);
    }
    StabilityStats s = evaluateNow(sim);
    EXPECT_EQ(s.supported, 9);
    EXPECT_EQ(s.unsupported, 0);
}

TEST(Collapse, CantileverOverreachCascadesToSteadyState) {
    Sim sim;
    for (const Event& e : cantilever(5, 15, Material::Wood)) sim.apply(e);
    StabilityStats before = evaluateNow(sim);
    ASSERT_GT(before.failing, 0);  // starts with failing tips
    const int32_t hiBefore = maxRestingZ(sim.world());

    int32_t ticks = 0;
    sim.settle(1000, &ticks);
    EXPECT_GT(ticks, 0);

    // No pieces leave the world (they pile on the floor); steady state reached.
    EXPECT_EQ(sim.world().aliveCount(), 20u);
    StabilityStats after = evaluateNow(sim);
    EXPECT_EQ(after.failing, 0);
    EXPECT_EQ(after.unsupported, 0);
    // The overreaching arm dropped, so debris now sits at or below the arm level.
    EXPECT_LE(maxRestingZ(sim.world()), hiBefore);
}

TEST(Collapse, RemovingBaseCollapsesColumnByOneAndReanchors) {
    Sim sim;
    for (const Event& e : tower(5, Material::Wood)) sim.apply(e);
    sim.apply(evRemove(0));  // pull the ground brick out
    int32_t ticks = 0;
    sim.settle(1000, &ticks);
    EXPECT_GT(ticks, 0);
    EXPECT_EQ(sim.world().aliveCount(), 4u);  // 4 remaining, none lost
    StabilityStats s = evaluateNow(sim);
    EXPECT_EQ(s.supported, 4);
    EXPECT_EQ(s.unsupported, 0);
    EXPECT_EQ(maxRestingZ(sim.world()), 3);  // was 4, dropped one cell
}

TEST(Collapse, ConvergesAndIsQuiescentAtRest) {
    Sim sim;
    for (const Event& e : floatingIsland(4, 4, 10, Material::Stone)) sim.apply(e);
    int32_t ticks = 0;
    const int32_t ran = sim.settle(1000, &ticks);
    EXPECT_LT(ran, 1000);  // converged well before the safety cap
    // One more settle is a no-op (already at rest).
    const uint64_t rested = sim.worldHash();
    sim.settle(1000);
    EXPECT_EQ(sim.worldHash(), rested);
}
