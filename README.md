# BrickStack

BrickStack is a deterministic voxel building and collapse simulation I wrote in C++20. Bricks snap to an integer grid, structural support propagates from anchored ground blocks through their connections, and any structure that loses support collapses under a fixed-timestep simulation. The whole simulation state is integer / fixed-point, so an identical event stream produces a bit-for-bit identical world-state hash across runs, across two independent in-process sim instances, and across separate OS processes.

I wanted to build the core loop of a brick-building survival game (place, connect, over-reach, collapse) as a standalone, testable systems module: a grid building system, a structural-integrity solver, and a deterministic fixed-tick simulation, unit-tested for correctness and benchmarked for tick performance on large builds. Determinism gets first-class treatment because bit-identical replay is the property multiplayer and live-service games need for rollback and desync detection.

## How the stability model works

A brick is an anchor if any of its cells sit on the ground plane (`z == 0`). Two bricks are connected if any of their cells are face-adjacent, forming an undirected structure graph over resting bricks.

1. **Reachability.** Multi-source BFS from all anchors assigns each reachable brick a hop-distance `d`. A brick with no path to an anchor is Unsupported.
2. **Support** propagates down the hop gradient, processed in increasing-`d` order so it is order-independent:
   - `support(anchor) = strength(material)`
   - `support(p) = max over parents q of floor((support(q) - ATTEN) / branch(q))`

   where a parent is a neighbor at `d-1`, `branch(q)` is the number of children of `q` (clamped to at least 1), and `ATTEN = 4`. Support weakens with every hop and is split among everything a piece holds up.
3. **Load** accumulates up the gradient, processed in decreasing-`d` order: `load(p) = weight(p) + sum over children c of floor(load(c) / parents(c))`.
4. **Classify.** Connected and `support >= load` is Supported; connected and `support < load` is Failing; not connected is Unsupported.

Materials are integer tuples: Wood `weight 2 / strength 60`, Stone `5 / 200`, Metal `8 / 400`. This is an abstract structural-integrity game mechanic, not a physics or FEA solver. It bounds how far a build can reach from its foundations before peripheral pieces starve, which is the lever games use to limit unsupported spans. Because it is a fixpoint over fixed BFS distances, classification is a deterministic function of the resting set alone.

## Collapse

One `step()`:

1. Runs the solver over the resting set.
2. Detaches every Unsupported/Failing brick into a falling set, removing its cells. Removing that support is what makes collapse cascade: dependents lose their support and detach on the next evaluation.
3. Applies gravity. Falling bricks descend exactly one cell per tick (integer), processed bottom-up (min-z then id) so landings are deterministic; a brick rests when the cells below are occupied or it reaches the floor.

`settle()` runs steps until the world is quiescent. This is bounded: the sum of piece heights strictly decreases while anything falls, so it always converges.

## Engine portability

There is no engine dependency; the module is pure C++20 + CMake, so everything is reproducible and CI-verifiable without an engine license. It is written so it could port into an engine like Unreal without restructuring: `Brick` is POD component data, `World` owns the grid and placement rules (the shape of a world subsystem), `CollapseSystem::step()` is a fixed gameplay tick, the `Sim` event stream (`Place`/`Remove`/`Step`) maps to replicated gameplay events, and `Sim::worldHash()` works as a replay / desync checksum. Everything that touches simulation state avoids floating point precisely because that is what replay and rollback need.

## Building and running

Requires CMake 3.20+ and a C++20 compiler. GoogleTest is fetched automatically via CMake `FetchContent` (needs network on first configure); the build directory and fetched deps are git-ignored.

```bash
# configure + build (Release, -O3, warnings-as-errors on our code)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# run the unit tests
ctest --test-dir build            # 24 tests

# run the measured benchmarks + write results/*.json (from the project root)
./build/brickstack_bench
```

All warnings are errors (`-Wall -Wextra -Werror`) on the project's own targets. Measured numbers and exact reproduce commands are in [RESULTS.md](RESULTS.md).

## Layout

```
include/brickstack/   types, grid+world, stability, collapse, sim, scenarios, hash
src/                  world.cpp stability.cpp collapse.cpp sim.cpp scenarios.cpp
tests/                placement / stability / collapse / determinism (GoogleTest)
bench/                bench_main.cpp: steady_clock perf + mach memory, writes results/*.json
results/              measured JSON (committed)
```

## Limitations

- Engine-agnostic by design: there is no actual Unreal integration or build, no rendering, no networking transport, and no physics engine underneath. The mapping above is a porting argument, not shipped integration code.
- Cross-machine / cross-compiler determinism has not been run; all measurements are from one Apple Silicon machine with one compiler. The integer-only design plus a self-rolled PRNG and FNV-1a hashing should make it portable, but that is a design argument, not a measurement.
- Perf numbers are single-core; there is no multithreading.
- The stability model is a game mechanic with tuned integer constants, not a physically meaningful solver.
