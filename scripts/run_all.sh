#!/usr/bin/env bash
# Configure, build (Release, -Werror), run the unit tests, and produce the
# measured metrics in results/*.json. Run from the project root.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

ctest --test-dir build --output-on-failure

# The bench writes results/*.json relative to the current directory.
./build/brickstack_bench
