// BrickStack — deterministic voxel-build structural-stability core types.
//
// Everything that participates in simulation STATE is integer / fixed-point so
// the world-state hash is bit-identical across runs and machines. There are no
// floats anywhere in the stability, load, collapse, or hashing paths.
#pragma once

#include <cstdint>
#include <functional>

namespace brickstack {

// Integer grid coordinate. z is "up"; the ground/anchor plane is z == 0.
struct Vec3i {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    friend bool operator==(const Vec3i& a, const Vec3i& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    friend bool operator!=(const Vec3i& a, const Vec3i& b) { return !(a == b); }
};

// Stable, monotonically-assigned piece id. Equal to the storage slot; ids are
// never reused, so iteration in id order is a deterministic canonical order.
using BrickId = uint32_t;

inline constexpr BrickId kInvalidBrick = static_cast<BrickId>(-1);

// Material governs a piece's weight (how much load it adds) and strength (the
// support capacity an anchored piece of this material radiates). Integer only.
enum class Material : uint16_t {
    Wood = 0,
    Stone = 1,
    Metal = 2,
};

struct MaterialProps {
    int32_t weight;    // load this piece contributes
    int32_t strength;  // base support an anchor of this material provides
};

// Documented, deterministic constants for the v1 structural model.
// See stability.hpp for the full model description.
inline constexpr MaterialProps materialProps(Material m) {
    switch (m) {
        case Material::Wood:  return MaterialProps{2, 60};
        case Material::Stone: return MaterialProps{5, 200};
        case Material::Metal: return MaterialProps{8, 400};
    }
    return MaterialProps{2, 60};  // unreachable; keeps -Werror happy
}

// Support lost per graph-hop away from an anchor (attenuation with distance).
inline constexpr int32_t kAttenuationPerHop = 4;

// Classification produced by the stability solver.
enum class Status : uint8_t {
    Supported = 0,    // connected to an anchor and support >= load
    Failing = 1,      // connected but overloaded (support < load)
    Unsupported = 2,  // no connected path to any anchor
    Falling = 3,      // detached, currently in gravity fall (not part of the
                      // resting structure)
};

// Persistent per-piece component data. This is a POD-ish "component" struct: it
// maps 1:1 to what an Unreal UActorComponent would carry, plus two fields the
// solver writes (status, resting). No pointers, no owning containers.
struct Brick {
    BrickId id = kInvalidBrick;
    Vec3i origin{};       // minimum corner of the axis-aligned box
    Vec3i size{1, 1, 1};  // extent in cells (>= 1 on each axis)
    Material material = Material::Wood;
    Status status = Status::Unsupported;
    bool alive = true;    // false once removed (tombstone; id never reused)
    bool resting = true;  // true = part of the settled structure; false = falling
};

}  // namespace brickstack
