# Résumé Bullets — BrickStack (filled strictly from measured results)

> Measured 2026-08-17 on an **Apple M5 Pro (arm64)**, macOS 26.4.1, Apple Clang
> 21.0.0, C++20 Release `-O3`. Every number traces to `results/*.json` and the
> `ctest` run. Perf numbers are **machine-specific** (single core, `-O3`) and
> tagged as such. Nothing here is estimated.

## Filled bullets

- Built a **deterministic voxel-build structural-integrity & collapse system in
  C++20** (engine-agnostic, Unreal-portable gameplay module) simulating
  **100,000 pieces at 9.25 ms/tick (~10.8M pieces/sec)** with an
  anchor-reachability + integer load-propagation stability solver and
  fixed-timestep cascading collapse.
  <br>_(MEASURED: tick median 9.245 ms @100k, 0.931 ms @10k, 0.091 ms @1k;
  10,816,171 pieces/sec @100k; ~93 B/piece, 21.1 MB peak RSS.
  ⚠️ machine-specific — Apple M5 Pro, single core, `-O3`.)_

- Guaranteed **bit-identical replay determinism across 12 scenarios and 2
  independent sim instances** (per-event world-state-hash match, 3 runs each,
  and byte-identical across separate OS processes) via a fixed-timestep,
  integer-only simulation — the correctness property live-service / multiplayer
  gameplay requires.
  <br>_(MEASURED: 12/12 scenarios = 100%, `results/determinism.json`; hashes
  reproduce byte-for-byte across two separate process runs; hardware-independent.)_

- Verified structural-stability + cascading-collapse correctness across **12
  hand-built unit scenarios (cantilever / tower / bridge / floating-island)**
  within a **24-test CTest suite that passes 100%**, with **0 build warnings
  (`-Wall -Wextra -Werror`)** and a reproducible CMake/CTest build (GoogleTest via
  FetchContent).
  <br>_(MEASURED: 8/8 bench correctness scenarios + 24/24 CTest tests pass; 0
  warnings on project targets; hardware-independent.)_

## Measured-value ledger

| Placeholder | Value | Status |
|---|---|---|
| pieces simulated (max) | 100,000 | MEASURED |
| tick time @100k / @10k / @1k | 9.245 / 0.931 / 0.091 ms | MEASURED (machine-specific) |
| pieces / sec @100k | 10,816,171 | MEASURED (machine-specific) |
| collapse settle @100k | 17.63 ms, 3 ticks, 90k cascade pieces | MEASURED (machine-specific) |
| memory per piece / peak RSS | ~93 B / 21.14 MB | MEASURED (machine-specific) |
| determinism scenarios | 12 / 12 (100%), 2 instances, 3 runs | MEASURED |
| cross-process determinism | byte-identical | MEASURED |
| stability/collapse unit scenarios | 12 hand-built (100% pass) | MEASURED |
| bench correctness scenarios | 8 / 8 (100%) | MEASURED |
| CTest total | 24 / 24 pass | MEASURED |
| build warnings (`-Werror`) | 0 (project targets) | MEASURED |

## Honesty tags

- ✅ MEASURED on real hardware; correctness + determinism are hardware-independent.
- ⚠️ All perf numbers (ms/tick, pieces/sec, memory) are **machine-specific**
  (Apple M5 Pro, single core, `-O3`) and will differ elsewhere.
- ⚠️ Determinism was measured on **one machine/compiler only**; integer-only
  design should port, but cross-machine parity was not run (see RESULTS.md).
- ❌ Not a real Unreal Engine build — engine-agnostic module, Unreal-portable by
  design (POD component data + `tick`), not integrated into UE.
