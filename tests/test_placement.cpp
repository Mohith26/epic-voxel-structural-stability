// Placement-rule tests: adjacency, no-overlap, bounds.
#include <gtest/gtest.h>

#include "brickstack/world.hpp"

using namespace brickstack;

TEST(Placement, GroundPlacementSucceeds) {
    World w;
    BrickId id = kInvalidBrick;
    EXPECT_EQ(w.place({0, 0, 0}, {1, 1, 1}, Material::Wood, &id), PlaceResult::Ok);
    EXPECT_NE(id, kInvalidBrick);
    EXPECT_EQ(w.aliveCount(), 1u);
    EXPECT_TRUE(w.occupantAt({0, 0, 0}).has_value());
}

TEST(Placement, FloatingPlacementRejectedAsNotAdjacent) {
    World w;
    // z > 0 with nothing beneath and nothing adjacent -> rejected.
    EXPECT_EQ(w.place({0, 0, 5}, {1, 1, 1}, Material::Wood, nullptr),
              PlaceResult::NotAdjacent);
    EXPECT_EQ(w.aliveCount(), 0u);
}

TEST(Placement, AdjacentPlacementSucceeds) {
    World w;
    ASSERT_EQ(w.place({0, 0, 0}, {1, 1, 1}, Material::Wood, nullptr), PlaceResult::Ok);
    // Stacked on top (face-adjacent below).
    EXPECT_EQ(w.place({0, 0, 1}, {1, 1, 1}, Material::Wood, nullptr), PlaceResult::Ok);
    // Beside it at z=0 (grounded).
    EXPECT_EQ(w.place({1, 0, 0}, {1, 1, 1}, Material::Wood, nullptr), PlaceResult::Ok);
    // Floating beside the stack but face-adjacent to (1,0,0)+(0,0,1) region.
    EXPECT_EQ(w.place({1, 0, 1}, {1, 1, 1}, Material::Wood, nullptr), PlaceResult::Ok);
}

TEST(Placement, OverlapRejected) {
    World w;
    ASSERT_EQ(w.place({0, 0, 0}, {2, 1, 1}, Material::Wood, nullptr), PlaceResult::Ok);
    // Overlaps the existing 2-wide brick's second cell.
    EXPECT_EQ(w.place({1, 0, 0}, {1, 1, 1}, Material::Wood, nullptr),
              PlaceResult::Overlap);
}

TEST(Placement, BelowFloorRejected) {
    World w;
    EXPECT_EQ(w.place({0, 0, -1}, {1, 1, 1}, Material::Wood, nullptr),
              PlaceResult::OutOfBounds);
}

TEST(Placement, DegenerateSizeRejected) {
    World w;
    EXPECT_EQ(w.place({0, 0, 0}, {0, 1, 1}, Material::Wood, nullptr),
              PlaceResult::DegenerateSize);
}

TEST(Placement, MultiCellBrickOccupiesAllCells) {
    World w;
    BrickId id = kInvalidBrick;
    ASSERT_EQ(w.place({0, 0, 0}, {3, 1, 1}, Material::Stone, &id), PlaceResult::Ok);
    for (int32_t x = 0; x < 3; ++x) {
        auto occ = w.occupantAt({x, 0, 0});
        ASSERT_TRUE(occ.has_value());
        EXPECT_EQ(*occ, id);
    }
    EXPECT_FALSE(w.occupantAt({3, 0, 0}).has_value());
}

TEST(Placement, RemoveFreesCells) {
    World w;
    BrickId id = kInvalidBrick;
    ASSERT_EQ(w.place({0, 0, 0}, {1, 1, 1}, Material::Wood, &id), PlaceResult::Ok);
    EXPECT_TRUE(w.remove(id));
    EXPECT_FALSE(w.occupantAt({0, 0, 0}).has_value());
    EXPECT_EQ(w.aliveCount(), 0u);
    EXPECT_FALSE(w.remove(id));  // already removed
}

TEST(Placement, UncheckedBypassesAdjacencyButNotOverlap) {
    World w;
    BrickId a = w.placeUnchecked({0, 0, 9}, {1, 1, 1}, Material::Wood);  // floating ok
    EXPECT_NE(a, kInvalidBrick);
    BrickId b = w.placeUnchecked({0, 0, 9}, {1, 1, 1}, Material::Wood);  // overlap
    EXPECT_EQ(b, kInvalidBrick);
}
