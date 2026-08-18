// FNV-1a 64-bit hashing — used both for the deterministic world-state hash and
// for the built-in splitmix64 PRNG that seeds reproducible scenarios.
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace brickstack {

inline constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ull;
inline constexpr uint64_t kFnvPrime = 1099511628211ull;

// Incremental FNV-1a so callers can fold a canonical byte stream in one pass.
class Fnv1a {
public:
    void mix(const void* data, std::size_t len) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        for (std::size_t i = 0; i < len; ++i) {
            state_ ^= bytes[i];
            state_ *= kFnvPrime;
        }
    }

    template <typename T>
    void mixPod(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "mixPod requires a POD");
        mix(&value, sizeof(T));
    }

    uint64_t value() const { return state_; }

private:
    uint64_t state_ = kFnvOffsetBasis;
};

// Deterministic, portable PRNG. We roll our own (rather than <random>) so seeded
// scenarios are bit-identical across compilers and standard-library versions.
class SplitMix64 {
public:
    explicit SplitMix64(uint64_t seed) : state_(seed) {}

    uint64_t next() {
        state_ += 0x9E3779B97F4A7C15ull;
        uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // Uniform-ish integer in [0, bound). bound must be > 0.
    uint32_t nextBounded(uint32_t bound) {
        return static_cast<uint32_t>(next() % bound);
    }

private:
    uint64_t state_;
};

}  // namespace brickstack
