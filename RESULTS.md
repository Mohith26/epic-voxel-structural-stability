# BrickStack — Measured Results

**Date measured:** 2026-08-17
**Machine:** Apple **M5 Pro** (arm64), macOS 26.4.1, 18 cores.
**Compiler:** Apple Clang 21.0.0 (`clang-2100.0.123.102`), C++20, **Release `-O3`**,
built with `-Wall -Wextra -Werror` on all project targets.
**Test framework:** GoogleTest v1.15.2 (fetched via CMake FetchContent).

Every number below comes from a real run of `./build/brickstack_bench` and
`ctest` on this machine. Machine-readable values are committed under
`results/*.json`. **Perf numbers are hardware-dependent** (single Apple M5 Pro
core, `-O3`) and will differ on other machines; correctness and determinism
results are hardware-independent.

---

## How to reproduce (exact commands)

```bash
# 1. configure (fetches GoogleTest on first run; needs network) + build Release
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 2. unit tests  ->  "100% tests passed out of 24"
ctest --test-dir build

# 3. measured metrics -> prints the tables and writes results/*.json
#    (run from the project root; it writes results/ relative to cwd)
./build/brickstack_bench
```

---

## 1. Determinism / replay  — `results/determinism.json`

Record an event stream → replay it in a fresh Sim → compare the world-state hash.
Each scenario is run **3 times** and the full per-event hash sequence is compared
across **2 independent in-process Sim instances** plus a repeat run.

| Metric | Value |
|---|---|
| Scenarios bit-identical (hash match) | **12 / 12 (100%)** |
| Runs compared per scenario | 3 |
| Independent sim instances compared | 2 |
| Cross-**process** determinism (two separate runs of the binary) | **byte-identical** (final-hash diff empty) |
| CTest determinism cases | 3 / 3 pass (12-scenario suite + stable-fingerprint + distinct-structures) |

Scenarios span towers, an over-reaching cantilever, a two-sided bridge, a
floating island (all with collapse), and 7 seeded random blobs (several with an
anchor pulled mid-stream to force a cascade). Example committed final hashes:
`cantilever_6_18 = 0x3ea7d0bfc7931b00`, `blob_s99_collapse = 0x2e20d36c99c5dd1f`.

---

## 2. Stability + collapse correctness — `results/correctness.json`

Hand-built structures classified by the solver, checked against expectations
hand-derived from the documented integer model (Wood w2/S60, Stone w5/S200,
ATTEN 4).

| Scenario | Expected (supported / failing / unsupported) | Result |
|---|---|---|
| tower height 5 (wood) | 5 / 0 / 0 | pass |
| tower height 20 (wood) — top starves | 11 / 9 / 0 | pass |
| cantilever col5 arm10 (within reach) | 15 / 0 / 0 | pass |
| cantilever col5 arm15 (over-reach) | 11 / 9 / 0 | pass |
| bridge span10 (stone, two-sided) | 20 / 0 / 0 | pass |
| floating island 3×3 | 0 / 0 / 9 | pass |
| floating island settles to ground (collapse) | all 9 supported at z=0 | pass |
| cantilever over-reach settles stable (collapse) | 0 failing, 0 unsupported | pass |

**Bench correctness: 8 / 8 scenarios pass (100%).**

Additional collapse/solver behavior is covered by CTest (see §4): gravity + rest,
cascade to steady state, anchor-removal re-anchoring, quiescent idempotency.

---

## 3. Performance — `results/bench.json`

Seeded deterministic scenario: a solid `S×S×10` wood box of **exactly N** bricks
(fully ground-supported, so a tick is a clean full stability solve with zero
detachment). **Tick time** is the median of a full `step()` over N pieces;
**pieces/sec** = placed / median-tick-seconds. **Collapse** removes the entire
foundation layer, forcing every brick above it to lose its anchor path in a
single cascade re-evaluation, then re-settles under gravity. **Memory** is the
`phys_footprint` (mach `TASK_VM_INFO`) growth across building the box.

| N (pieces) | tick ms (median) | tick ms (min) | pieces / sec | collapse settle ms | collapse ticks | cascade pieces | memory Δ (MB) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1,000 | 0.0914 | 0.0897 | 10,943,912 | 0.1950 | 3 | 900 | 0.094 |
| 10,000 | 0.9312 | 0.8876 | 10,738,739 | 1.7378 | 3 | 8,976 | 1.156 |
| 100,000 | 9.2454 | 9.1061 | 10,816,171 | 17.6345 | 3 | 90,000 | 9.313 |

Peak process RSS during the whole bench run: **21.14 MB**.
Cascade-detection pass alone (the single solve that reclassifies every piece as
unsupported after the foundation is removed): 0.068 ms @1k, 0.637 ms @10k,
**6.698 ms @100k**.

Notes:
- Tick cost scales cleanly ~linearly with N (≈ **10.8M pieces/sec** sustained
  across all three sizes), as expected for the O(V+E) BFS + two propagation
  passes.
- Memory is ≈ **93 bytes/piece** (a `Brick` record + its cell entry in the
  occupancy hash map).
- `collapse ticks = 3` because the foundation-removal scenario drops the box one
  layer and re-piles; the *cascade* (marking all 90k upper pieces unsupported)
  happens in the single `cascade_detect` pass, then gravity re-settles in 3 fixed
  ticks. This is honest for this scenario shape — it is a wide, short box, not a
  tall free-fall.

---

## 4. Test suite — CTest

```
100% tests passed out of 24        (total time ~16 s; the determinism suite dominates)
```

Breakdown (GoogleTest, one CTest case per test):
- **Placement (9):** ground/adjacency/overlap/bounds/degenerate rejection,
  multi-cell occupancy, remove frees cells, unchecked bypass.
- **Stability (7):** short tower, tall-tower attenuation, cantilever within/over
  reach, two-sided bridge, floating island, anchor-removal disconnect.
- **Collapse (5):** stable no-move + idempotent re-settle, floating island falls
  to ground, cantilever cascade to steady state, base-removal re-anchor,
  convergence/quiescence.
- **Determinism (3):** 12-scenario 2-instance/3-run suite, stable-fingerprint,
  distinct-structure hashes.

**Build warnings: 0** (project targets compiled with `-Wall -Wextra -Werror`;
the lone warning emitted during a full build is inside GoogleTest's own headers,
to which our warning flags are intentionally not applied).

---

## What is NOT measured (honest gaps)

- **Cross-machine / cross-compiler determinism** was not run (only this one
  Apple M5 Pro / Apple Clang 21). The design is integer-only and uses a
  self-rolled PRNG + FNV-1a specifically so it *should* be portable, but that is
  a design argument, not a measurement here.
- **Collapse over a tall free-fall** is not in the perf table; the perf-collapse
  scenario is a wide short box (3 ticks). Tall multi-tick cascades are covered
  functionally by the CTest collapse scenarios, not timed at 1k/10k/100k.
- Perf is a **single core**, no multithreading (out of scope for v1).
