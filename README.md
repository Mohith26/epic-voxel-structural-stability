# BrickStack — Deterministic Voxel-Build Structural-Stability & Collapse

A deterministic **C++20 gameplay-systems module** that models the core loop of a
LEGO-Fortnite-style building game: bricks snap to an integer voxel grid,
structural support propagates from anchored ground blocks through their
connections, and any structure that loses support **collapses** under a
fixed-timestep simulation. The entire simulation state is integer / fixed-point,
so an identical event stream produces a **bit-for-bit identical world-state hash**
across runs, across two independent in-process sim instances, and across separate
OS processes.

This is a **from-scratch systems/gameplay-programming exercise** aimed at the
Epic Games *Gameplay Programmer Intern (LEGO Fortnite)* role: a grid building
system + a structural-integrity solver + a deterministic fixed-tick simulation,
unit-tested for correctness and benchmarked for tick performance on large builds.

## Engine-agnostic, but Unreal-portable

There is **no Unreal Engine dependency** — the module is pure C++20 + CMake so it
is fully reproducible and CI-verifiable without an engine license. It is written
to port cleanly into Unreal:

| BrickStack | Unreal mapping |
|---|---|
| `brickstack::Brick` (POD component data — id, origin, size, material, status) | fields on a `UActorComponent` / an entity row |
| `World` (grid + placement rules) | a `UWorldSubsystem` owning the voxel grid |
| `CollapseSystem::step(World&)` fixed tick | `UWorldSubsystem::Tick` / a fixed-step gameplay tick |
| `Sim` event stream (`Place`/`Remove`/`Step`) | replicated gameplay events |
| `Sim::worldHash()` | a checksum for replay / netcode desync detection |

Everything that touches simulation state avoids floating point precisely because
that is the property live-service / multiplayer gameplay needs for replay and
rollback.

## The structural-integrity model (deterministic, integer)

A brick is an **anchor** if any of its cells sit on the ground plane (`z == 0`).
Two bricks are **connected** if any of their cells are face-adjacent, forming an
undirected structure graph over resting bricks.

1. **Reachability.** Multi-source BFS from all anchors assigns each reachable
   brick a hop-distance `d`. A brick with no path to an anchor is **Unsupported**.
2. **Support** (propagates down the hop gradient, processed in increasing-`d`
   order so it is order-independent):
   - `support(anchor) = strength(material)`
   - `support(p) = max over parents q of floor((support(q) - ATTEN) / branch(q))`
   where a *parent* is a neighbor at `d-1`, `branch(q)` is the number of children
   of `q` (clamped ≥ 1), and `ATTEN = 4`. Support weakens with every hop and is
   split among everything a piece holds up.
3. **Load** (accumulates up the gradient, processed in decreasing-`d` order):
   `load(p) = weight(p) + sum over children c of floor(load(c) / parents(c))`.
4. **Classify.** Connected & `support ≥ load` → **Supported**; connected &
   `support < load` → **Failing**; not connected → **Unsupported**.

Materials (integer): Wood `weight 2 / strength 60`, Stone `5 / 200`, Metal
`8 / 400`. The model is an abstract **structural-integrity game mechanic**, not a
physics/FEA solver: it bounds how far a build can reach from its foundations
before peripheral pieces starve — the lever games use to limit unsupported spans.
It is a fixpoint over fixed BFS distances, hence a deterministic function of the
resting set alone.

## Collapse (fixed timestep)

One `step()`:
1. Runs the solver over the resting set.
2. Detaches every Unsupported/Failing brick into a *falling* set (removing its
   cells). Removing that support is what makes collapse **cascade** — dependents
   lose their support and detach on the next evaluation.
3. Applies gravity: falling bricks descend exactly one cell per tick (integer),
   processed bottom-up (min-z then id) so landings are deterministic; a brick
   rests when the cells below are occupied or it reaches the floor (`z == 0`).

`settle()` runs steps until the world is quiescent (bounded; the sum of piece
heights strictly decreases while anything falls, so it always converges).

## Layout

```
include/brickstack/   types, grid+world, stability, collapse, sim, scenarios, hash
src/                  world.cpp stability.cpp collapse.cpp sim.cpp scenarios.cpp
tests/                placement / stability / collapse / determinism (GoogleTest)
bench/                bench_main.cpp — steady_clock perf + mach memory, writes results/*.json
results/              measured JSON (committed)
```

## Build, test, benchmark

Requires CMake ≥ 3.20 and a C++20 compiler. GoogleTest is fetched automatically
via CMake `FetchContent` (needs network on first configure); the build directory
and fetched deps are git-ignored.

```bash
# configure + build (Release, -O3, warnings-as-errors on our code)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# run the unit tests
ctest --test-dir build            # 24 tests

# run the measured benchmarks + write results/*.json (from the project root)
./build/brickstack_bench
```

All warnings are errors (`-Wall -Wextra -Werror`) on the project's own targets.
See **RESULTS.md** for measured numbers and the exact reproduce commands, and
**BULLETS.md** for résumé bullets filled strictly from measured values.

## Scope

Implements the spec's Must-have v1 only (grid + placement rules, stability
solver, collapse + determinism, perf benchmark). Out of scope by design: real
Unreal integration/build, rendering, networking transport, a physics engine
(PhysX), and assets.
