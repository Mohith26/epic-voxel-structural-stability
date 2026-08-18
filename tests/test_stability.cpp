// Stability-solver correctness on hand-constructed structures.
// Expected classification counts are derived by hand from the documented model
// (see stability.hpp); constants: Wood w=2/S=60, Stone w=5/S=200, ATTEN=4.
#include <gtest/gtest.h>

#include "brickstack/scenarios.hpp"
#include "brickstack/sim.hpp"
#include "brickstack/stability.hpp"

using namespace brickstack;

namespace {
// Build a world from a place-only event stream and evaluate stability once.
StabilityStats classify(const std::vector<Event>& events) {
    Sim sim;
    for (const Event& e : events) sim.apply(e);
    StabilitySolver solver;
    return solver.evaluate(sim.world());
}
}  // namespace

TEST(Stability, ShortTowerFullySupported) {
    StabilityStats s = classify(tower(5, Material::Wood));
    EXPECT_EQ(s.resting, 5);
    EXPECT_EQ(s.supported, 5);
    EXPECT_EQ(s.failing, 0);
    EXPECT_EQ(s.unsupported, 0);
    EXPECT_EQ(s.anchors, 1);
    EXPECT_EQ(s.maxDistance, 4);
}

TEST(Stability, TallTowerTopFailsFromAttenuation) {
    // Support = 60-4z, load = 2*(20-z). Support >= load only for z <= 10.
    StabilityStats s = classify(tower(20, Material::Wood));
    EXPECT_EQ(s.resting, 20);
    EXPECT_EQ(s.supported, 11);  // z = 0..10
    EXPECT_EQ(s.failing, 9);     // z = 11..19
    EXPECT_EQ(s.unsupported, 0);
}

TEST(Stability, CantileverWithinReachSupported) {
    StabilityStats s = classify(cantilever(5, 10, Material::Wood));
    EXPECT_EQ(s.resting, 15);
    EXPECT_EQ(s.supported, 15);
    EXPECT_EQ(s.failing, 0);
}

TEST(Stability, CantileverOverreachTipsFail) {
    // Arm support = 44-4x; fails past x=6.
    StabilityStats s = classify(cantilever(5, 15, Material::Wood));
    EXPECT_EQ(s.resting, 20);
    EXPECT_EQ(s.supported, 11);  // column(5) + arm x=1..6
    EXPECT_EQ(s.failing, 9);     // arm x=7..15
    EXPECT_EQ(s.unsupported, 0);
}

TEST(Stability, TwoSidedBridgeStandsWhereCantileverWouldFail) {
    StabilityStats s = classify(bridge(5, 10, Material::Stone));
    EXPECT_EQ(s.resting, 20);
    EXPECT_EQ(s.supported, 20);  // supported from both ends
    EXPECT_EQ(s.failing, 0);
    EXPECT_EQ(s.anchors, 2);
}

TEST(Stability, FloatingIslandIsUnsupported) {
    StabilityStats s = classify(floatingIsland(3, 3, 6, Material::Wood));
    EXPECT_EQ(s.resting, 9);
    EXPECT_EQ(s.unsupported, 9);
    EXPECT_EQ(s.supported, 0);
    EXPECT_EQ(s.failing, 0);
    EXPECT_EQ(s.anchors, 0);
}

TEST(Stability, RemovingAnchorDisconnectsColumn) {
    // Tower of 5; remove the ground brick -> the rest is no longer reachable.
    Sim sim;
    for (const Event& e : tower(5, Material::Wood)) sim.apply(e);
    sim.apply(evRemove(0));  // remove the (0,0,0) anchor
    StabilitySolver solver;
    StabilityStats s = solver.evaluate(sim.world());
    EXPECT_EQ(s.resting, 4);
    EXPECT_EQ(s.unsupported, 4);
    EXPECT_EQ(s.anchors, 0);
}
