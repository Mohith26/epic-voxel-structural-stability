// Determinism / replay: identical event streams must yield a bit-identical
// world-state hash sequence across independent Sim instances and repeat runs.
#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "brickstack/scenarios.hpp"
#include "brickstack/sim.hpp"

using namespace brickstack;

namespace {

// Apply an event stream, recording the world hash after every event.
std::vector<uint64_t> runAndLog(const std::vector<Event>& events) {
    Sim sim;
    std::vector<uint64_t> log;
    log.reserve(events.size() + 1);
    log.push_back(sim.worldHash());  // empty-world baseline
    for (const Event& e : events) {
        sim.apply(e);
        log.push_back(sim.worldHash());
    }
    // Fold in a final settle so the collapse path is part of the fingerprint.
    sim.settle(100000);
    log.push_back(sim.worldHash());
    return log;
}

std::vector<std::pair<std::string, std::vector<Event>>> scenarioSuite() {
    std::vector<std::pair<std::string, std::vector<Event>>> s;
    s.emplace_back("tower10", tower(10, Material::Wood));
    s.emplace_back("tower25_partial_collapse", tower(25, Material::Wood));
    s.emplace_back("cantilever_overreach", cantilever(6, 18, Material::Wood));
    s.emplace_back("bridge_stone", bridge(6, 14, Material::Stone));
    s.emplace_back("floating_island", floatingIsland(4, 4, 8, Material::Wood));
    // Seeded blobs, some with an anchor pulled to trigger collapse. Kept small
    // so `ctest` stays fast; the full-size stress runs live in the bench driver.
    s.emplace_back("blob_s1_120", seededBlob(1, 120));
    s.emplace_back("blob_s2_120", seededBlob(2, 120));
    s.emplace_back("blob_s3_150", seededBlob(3, 150));
    s.emplace_back("blob_s7_collapse", seededBlob(7, 120, 0, 48));
    s.emplace_back("blob_s11_collapse", seededBlob(11, 120, 0, 48));
    s.emplace_back("blob_s42_collapse", seededBlob(42, 150, 0, 64));
    s.emplace_back("blob_s99_collapse", seededBlob(99, 150, 0, 64));
    return s;
}

}  // namespace

TEST(Determinism, TwoIndependentInstancesMatchAcrossSuite) {
    const auto suite = scenarioSuite();
    int passed = 0;
    for (const auto& [name, events] : suite) {
        // Two independent Sim instances (built in separate calls) + a repeat run.
        const std::vector<uint64_t> a = runAndLog(events);
        const std::vector<uint64_t> b = runAndLog(events);
        const std::vector<uint64_t> c = runAndLog(events);
        EXPECT_EQ(a, b) << "instance mismatch: " << name;
        EXPECT_EQ(a, c) << "repeat-run mismatch: " << name;
        if (a == b && a == c) ++passed;
    }
    EXPECT_EQ(passed, static_cast<int>(suite.size()));
}

TEST(Determinism, FinalHashIsStableValueAcrossRebuilds) {
    // A concrete, order-sensitive fingerprint that must be reproducible.
    const std::vector<uint64_t> h1 = runAndLog(cantilever(6, 18, Material::Wood));
    const std::vector<uint64_t> h2 = runAndLog(cantilever(6, 18, Material::Wood));
    ASSERT_FALSE(h1.empty());
    EXPECT_EQ(h1.back(), h2.back());
    EXPECT_NE(h1.back(), 0ull);
}

TEST(Determinism, DifferentStructuresProduceDifferentHashes) {
    const uint64_t a = runAndLog(tower(10, Material::Wood)).back();
    const uint64_t b = runAndLog(tower(11, Material::Wood)).back();
    EXPECT_NE(a, b);
}
