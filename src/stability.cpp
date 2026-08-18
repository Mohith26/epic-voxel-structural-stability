#include "brickstack/stability.hpp"

#include <algorithm>
#include <cstddef>
#include <queue>

namespace brickstack {

namespace {

// Face neighbors of a single cell.
inline void appendFaceNeighbors(Vec3i c, Vec3i out[6]) {
    out[0] = {c.x + 1, c.y, c.z};
    out[1] = {c.x - 1, c.y, c.z};
    out[2] = {c.x, c.y + 1, c.z};
    out[3] = {c.x, c.y - 1, c.z};
    out[4] = {c.x, c.y, c.z + 1};
    out[5] = {c.x, c.y, c.z - 1};
}

}  // namespace

StabilityStats StabilitySolver::evaluate(World& world) const {
    StabilityStats stats;
    const std::vector<BrickId> resting = world.restingIds();
    const std::size_t n = resting.size();
    stats.resting = static_cast<int32_t>(n);
    if (n == 0) return stats;

    // Compact index per brick id (id == storage slot).
    std::vector<int32_t> slotToIdx(world.slotCount(), -1);
    for (std::size_t i = 0; i < n; ++i) slotToIdx[resting[i]] = static_cast<int32_t>(i);

    // --- Build the structure graph (deduplicated adjacency in id order) ----
    std::vector<std::vector<int32_t>> adj(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Brick& b = world.brick(resting[i]);
        std::vector<int32_t>& nbrs = adj[i];
        for (int32_t dz = 0; dz < b.size.z; ++dz)
            for (int32_t dy = 0; dy < b.size.y; ++dy)
                for (int32_t dx = 0; dx < b.size.x; ++dx) {
                    Vec3i cell{b.origin.x + dx, b.origin.y + dy, b.origin.z + dz};
                    Vec3i faces[6];
                    appendFaceNeighbors(cell, faces);
                    for (const Vec3i& f : faces) {
                        auto occ = world.occupantAt(f);
                        if (!occ) continue;
                        const int32_t other = slotToIdx[*occ];
                        if (other >= 0 && other != static_cast<int32_t>(i))
                            nbrs.push_back(other);
                    }
                }
        std::sort(nbrs.begin(), nbrs.end());
        nbrs.erase(std::unique(nbrs.begin(), nbrs.end()), nbrs.end());
    }

    // --- Multi-source BFS from anchors (z == 0) ---------------------------
    std::vector<int32_t> dist(n, -1);
    std::queue<int32_t> bfs;
    for (std::size_t i = 0; i < n; ++i) {
        if (world.brick(resting[i]).origin.z == 0) {
            dist[i] = 0;
            ++stats.anchors;
            bfs.push(static_cast<int32_t>(i));
        }
    }
    while (!bfs.empty()) {
        const int32_t u = bfs.front();
        bfs.pop();
        for (int32_t v : adj[u]) {
            if (dist[v] == -1) {
                dist[v] = dist[u] + 1;
                stats.maxDistance = std::max(stats.maxDistance, dist[v]);
                bfs.push(v);
            }
        }
    }

    // --- Branch (children count) and parent count per node ----------------
    std::vector<int32_t> branch(n, 0);
    std::vector<int32_t> parents(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (dist[i] < 0) continue;
        for (int32_t v : adj[i]) {
            if (dist[v] == dist[i] + 1) ++branch[i];
            else if (dist[v] == dist[i] - 1) ++parents[i];
        }
    }

    // Process order: connected nodes sorted by ascending distance, ties by
    // compact index (already ascending). This makes both passes order-stable.
    std::vector<int32_t> order;
    order.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (dist[i] >= 0) order.push_back(static_cast<int32_t>(i));
    std::stable_sort(order.begin(), order.end(),
                     [&](int32_t a, int32_t c) { return dist[a] < dist[c]; });

    // --- Support pass (increasing distance) -------------------------------
    std::vector<int64_t> support(n, 0);
    for (int32_t i : order) {
        const Brick& b = world.brick(resting[i]);
        if (dist[i] == 0) {
            support[i] = materialProps(b.material).strength;
            continue;
        }
        int64_t best = 0;
        for (int32_t v : adj[i]) {
            if (dist[v] != dist[i] - 1) continue;
            const int32_t div = branch[v] < 1 ? 1 : branch[v];
            int64_t through = (support[v] - kAttenuationPerHop) / div;
            if (through > best) best = through;
        }
        support[i] = best < 0 ? 0 : best;
    }

    // --- Load pass (decreasing distance) ----------------------------------
    std::vector<int64_t> load(n, 0);
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        const int32_t i = *it;
        const Brick& b = world.brick(resting[i]);
        int64_t acc = materialProps(b.material).weight;
        for (int32_t v : adj[i]) {
            if (dist[v] != dist[i] + 1) continue;
            const int32_t div = parents[v] < 1 ? 1 : parents[v];
            acc += load[v] / div;
        }
        load[i] = acc;
    }

    // --- Classify ---------------------------------------------------------
    for (std::size_t i = 0; i < n; ++i) {
        Brick& b = world.brickMut(resting[i]);
        if (dist[i] < 0) {
            b.status = Status::Unsupported;
            ++stats.unsupported;
        } else if (support[i] < load[i]) {
            b.status = Status::Failing;
            ++stats.failing;
        } else {
            b.status = Status::Supported;
            ++stats.supported;
        }
    }
    return stats;
}

}  // namespace brickstack
