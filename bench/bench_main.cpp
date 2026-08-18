// BrickStack metrics driver — runs the correctness, determinism, and perf
// measurements for real on this machine and writes results/*.json plus a
// human-readable summary to stdout. No test framework; pure measured output.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "brickstack/collapse.hpp"
#include "brickstack/scenarios.hpp"
#include "brickstack/sim.hpp"
#include "brickstack/stability.hpp"

#if defined(__APPLE__)
#include <mach/mach.h>
#endif
#include <sys/resource.h>

using namespace brickstack;
using Clock = std::chrono::steady_clock;

namespace {

double msSince(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

uint64_t currentFootprintBytes() {
#if defined(__APPLE__)
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        return static_cast<uint64_t>(info.phys_footprint);
    }
    return 0;
#else
    return 0;
#endif
}

uint64_t peakRssBytes() {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
#if defined(__APPLE__)
    return static_cast<uint64_t>(ru.ru_maxrss);  // bytes on Darwin
#else
    return static_cast<uint64_t>(ru.ru_maxrss) * 1024ull;  // KiB on Linux
#endif
}

double toMB(uint64_t bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0); }

double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// Fill a solid W x W x H box with exactly `target` bricks (z-major order so
// each placement is adjacent to what came before). Returns bricks placed and
// the footprint side S and layer-0 count.
struct BoxInfo { int32_t placed = 0; int32_t side = 0; int32_t layer0 = 0; int32_t height = 0; };

BoxInfo buildSolidBox(World& w, int32_t target, int32_t height) {
    const int32_t side = static_cast<int32_t>(
        std::ceil(std::sqrt(static_cast<double>(target) / height)));
    BoxInfo info;
    info.side = side;
    info.height = height;
    int32_t placed = 0;
    for (int32_t z = 0; z < height && placed < target; ++z)
        for (int32_t y = 0; y < side && placed < target; ++y)
            for (int32_t x = 0; x < side && placed < target; ++x) {
                if (w.place({x, y, z}, {1, 1, 1}, Material::Wood, nullptr) ==
                    PlaceResult::Ok) {
                    ++placed;
                    if (z == 0) ++info.layer0;
                }
            }
    info.placed = placed;
    return info;
}

struct PerfRow {
    int32_t n = 0;
    int32_t placed = 0;
    int32_t supported = 0;
    int32_t failing = 0;
    double tick_ms_median = 0.0;
    double tick_ms_min = 0.0;
    double pieces_per_sec = 0.0;
    double cascade_detect_ms = 0.0;   // one solve after foundation removed
    double collapse_settle_ms = 0.0;  // full re-settle
    int32_t collapse_ticks = 0;
    int32_t cascade_pieces = 0;       // bricks that lost support and detached
    double mem_delta_mb = 0.0;
    double mem_footprint_mb = 0.0;
};

PerfRow measurePerf(int32_t n) {
    PerfRow row;
    row.n = n;
    constexpr int32_t kHeight = 10;

    const uint64_t footBefore = currentFootprintBytes();
    World world;
    const BoxInfo box = buildSolidBox(world, n, kHeight);
    const uint64_t footAfter = currentFootprintBytes();
    row.placed = box.placed;
    row.mem_delta_mb = toMB(footAfter - footBefore);
    row.mem_footprint_mb = toMB(footAfter);

    CollapseSystem collapse;
    // Warm up + confirm the box is fully stable (a clean per-tick solve).
    StabilityStats s0 = StabilitySolver{}.evaluate(world);
    row.supported = s0.supported;
    row.failing = s0.failing;

    // Tick timing: a stable box -> each step is a full solve with 0 detachment.
    std::vector<double> samples;
    const int32_t reps = (n >= 100000) ? 7 : 15;
    for (int32_t i = 0; i < reps; ++i) {
        const auto t0 = Clock::now();
        collapse.step(world);
        samples.push_back(msSince(t0));
    }
    row.tick_ms_median = median(samples);
    row.tick_ms_min = *std::min_element(samples.begin(), samples.end());
    if (row.tick_ms_median > 0.0)
        row.pieces_per_sec =
            static_cast<double>(row.placed) / (row.tick_ms_median / 1000.0);

    // Collapse: pull the entire foundation layer, forcing every brick above it
    // to lose its anchor path in a single cascade re-evaluation, then re-settle.
    for (BrickId id = 0; id < static_cast<BrickId>(box.layer0); ++id) world.remove(id);
    row.cascade_pieces = box.placed - box.layer0;

    const auto tCascade = Clock::now();
    StabilitySolver{}.evaluate(world);  // the cascade detection pass alone
    row.cascade_detect_ms = msSince(tCascade);

    const auto tSettle = Clock::now();
    int32_t ticks = 0;
    collapse.settle(world, 100000, &ticks);
    row.collapse_settle_ms = msSince(tSettle);
    row.collapse_ticks = ticks;
    return row;
}

// ---- Correctness (hand scenarios vs. hand-derived expectations) ------------

struct CorrCase {
    std::string name;
    int32_t expSupported, expFailing, expUnsupported;
    bool pass;
    int32_t gotSupported, gotFailing, gotUnsupported;
};

CorrCase checkClassify(const std::string& name, const std::vector<Event>& ev,
                       int32_t es, int32_t ef, int32_t eu) {
    Sim sim;
    for (const Event& e : ev) sim.apply(e);
    StabilityStats s = StabilitySolver{}.evaluate(sim.world());
    CorrCase c{name, es, ef, eu, false, s.supported, s.failing, s.unsupported};
    c.pass = (s.supported == es && s.failing == ef && s.unsupported == eu);
    return c;
}

// ---- Determinism -----------------------------------------------------------

std::vector<uint64_t> runAndLog(const std::vector<Event>& events) {
    Sim sim;
    std::vector<uint64_t> log;
    log.push_back(sim.worldHash());
    for (const Event& e : events) {
        sim.apply(e);
        log.push_back(sim.worldHash());
    }
    sim.settle(100000);
    log.push_back(sim.worldHash());
    return log;
}

}  // namespace

int main() {
    // ---------------- Correctness ----------------
    std::vector<CorrCase> corr;
    corr.push_back(checkClassify("tower5", tower(5, Material::Wood), 5, 0, 0));
    corr.push_back(checkClassify("tower20", tower(20, Material::Wood), 11, 9, 0));
    corr.push_back(
        checkClassify("cantilever_5_10", cantilever(5, 10, Material::Wood), 15, 0, 0));
    corr.push_back(
        checkClassify("cantilever_5_15", cantilever(5, 15, Material::Wood), 11, 9, 0));
    corr.push_back(checkClassify("bridge_5_10", bridge(5, 10, Material::Stone), 20, 0, 0));
    corr.push_back(
        checkClassify("floating_island_3x3", floatingIsland(3, 3, 6, Material::Wood),
                      0, 0, 9));
    // Collapse-outcome scenarios.
    {
        Sim sim;
        for (const Event& e : floatingIsland(3, 3, 6, Material::Wood)) sim.apply(e);
        sim.settle(1000);
        StabilityStats s = StabilitySolver{}.evaluate(sim.world());
        CorrCase c{"island_settles_supported", 9, 0, 0, s.supported == 9 && s.failing == 0,
                   s.supported, s.failing, s.unsupported};
        corr.push_back(c);
    }
    {
        Sim sim;
        for (const Event& e : cantilever(5, 15, Material::Wood)) sim.apply(e);
        sim.settle(1000);
        StabilityStats s = StabilitySolver{}.evaluate(sim.world());
        CorrCase c{"cantilever_settles_stable", 0, 0, 0,
                   s.failing == 0 && s.unsupported == 0, s.supported, s.failing,
                   s.unsupported};
        corr.push_back(c);
    }
    int corrPass = 0;
    for (const auto& c : corr) corrPass += c.pass ? 1 : 0;

    // ---------------- Determinism ----------------
    struct DetCase { std::string name; std::vector<Event> ev; };
    std::vector<DetCase> det;
    det.push_back({"tower10", tower(10, Material::Wood)});
    det.push_back({"tower25", tower(25, Material::Wood)});
    det.push_back({"cantilever_6_18", cantilever(6, 18, Material::Wood)});
    det.push_back({"bridge_6_14", bridge(6, 14, Material::Stone)});
    det.push_back({"floating_island", floatingIsland(4, 4, 8, Material::Wood)});
    det.push_back({"blob_s1_200", seededBlob(1, 200)});
    det.push_back({"blob_s2_200", seededBlob(2, 200)});
    det.push_back({"blob_s3_300", seededBlob(3, 300)});
    det.push_back({"blob_s7_collapse", seededBlob(7, 250, 0, 64)});
    det.push_back({"blob_s11_collapse", seededBlob(11, 250, 0, 64)});
    det.push_back({"blob_s42_collapse", seededBlob(42, 400, 0, 96)});
    det.push_back({"blob_s99_collapse", seededBlob(99, 400, 0, 96)});
    int detPass = 0;
    std::vector<uint64_t> detFinal;
    for (const auto& d : det) {
        const auto a = runAndLog(d.ev);
        const auto b = runAndLog(d.ev);  // independent instance
        const auto c = runAndLog(d.ev);  // repeat run
        if (a == b && a == c) ++detPass;
        detFinal.push_back(a.back());
    }

    // ---------------- Perf ----------------
    std::vector<PerfRow> perf;
    for (int32_t n : {1000, 10000, 100000}) perf.push_back(measurePerf(n));
    const double peakRss = toMB(peakRssBytes());

    // ---------------- Print summary ----------------
    std::printf("== BrickStack measured metrics ==\n");
    std::printf("correctness: %d/%zu scenarios pass\n", corrPass, corr.size());
    std::printf("determinism: %d/%zu scenarios bit-identical (3 runs each)\n", detPass,
                det.size());
    std::printf("\n%-8s %-8s %-12s %-14s %-13s %-11s %-12s %-10s\n", "N", "placed",
                "tick_ms(med)", "pieces/sec", "collapse_ms", "casc_ticks", "casc_pieces",
                "mem_MB");
    for (const auto& r : perf) {
        std::printf("%-8d %-8d %-12.4f %-14.0f %-13.4f %-11d %-12d %-10.2f\n", r.n,
                    r.placed, r.tick_ms_median, r.pieces_per_sec, r.collapse_settle_ms,
                    r.collapse_ticks, r.cascade_pieces, r.mem_delta_mb);
    }
    std::printf("peak RSS: %.2f MB\n", peakRss);

    // ---------------- Write JSON ----------------
    auto writeCorrectness = [&]() {
        FILE* f = std::fopen("results/correctness.json", "w");
        if (!f) return;
        std::fprintf(f, "{\n  \"scenarios_total\": %zu,\n  \"scenarios_pass\": %d,\n",
                     corr.size(), corrPass);
        std::fprintf(f, "  \"pass_rate\": %.4f,\n  \"cases\": [\n",
                     static_cast<double>(corrPass) / static_cast<double>(corr.size()));
        for (std::size_t i = 0; i < corr.size(); ++i) {
            const auto& c = corr[i];
            std::fprintf(f,
                         "    {\"name\": \"%s\", \"pass\": %s, \"expected\": "
                         "{\"supported\": %d, \"failing\": %d, \"unsupported\": %d}, "
                         "\"got\": {\"supported\": %d, \"failing\": %d, \"unsupported\": "
                         "%d}}%s\n",
                         c.name.c_str(), c.pass ? "true" : "false", c.expSupported,
                         c.expFailing, c.expUnsupported, c.gotSupported, c.gotFailing,
                         c.gotUnsupported, i + 1 < corr.size() ? "," : "");
        }
        std::fprintf(f, "  ]\n}\n");
        std::fclose(f);
    };

    auto writeDeterminism = [&]() {
        FILE* f = std::fopen("results/determinism.json", "w");
        if (!f) return;
        std::fprintf(f,
                     "{\n  \"scenarios_total\": %zu,\n  \"scenarios_pass\": %d,\n"
                     "  \"runs_per_scenario\": 3,\n  \"instances_compared\": 2,\n"
                     "  \"pass_rate\": %.4f,\n  \"final_hashes\": [\n",
                     det.size(), detPass,
                     static_cast<double>(detPass) / static_cast<double>(det.size()));
        for (std::size_t i = 0; i < det.size(); ++i) {
            std::fprintf(f, "    {\"name\": \"%s\", \"final_hash\": \"0x%016llx\"}%s\n",
                         det[i].name.c_str(),
                         static_cast<unsigned long long>(detFinal[i]),
                         i + 1 < det.size() ? "," : "");
        }
        std::fprintf(f, "  ]\n}\n");
        std::fclose(f);
    };

    auto writeBench = [&]() {
        FILE* f = std::fopen("results/bench.json", "w");
        if (!f) return;
        std::fprintf(f, "{\n  \"compiler\": \"%s\",\n  \"cpp_standard\": %ld,\n",
#if defined(__clang__)
                     "clang " __clang_version__,
#elif defined(__GNUC__)
                     "gcc",
#else
                     "unknown",
#endif
                     static_cast<long>(__cplusplus));
        std::fprintf(f, "  \"box_height_layers\": 10,\n  \"sizes\": [\n");
        for (std::size_t i = 0; i < perf.size(); ++i) {
            const auto& r = perf[i];
            std::fprintf(f,
                         "    {\"n\": %d, \"placed\": %d, \"supported\": %d, "
                         "\"failing\": %d, \"tick_ms_median\": %.6f, \"tick_ms_min\": "
                         "%.6f, \"pieces_per_sec\": %.1f, \"cascade_detect_ms\": %.6f, "
                         "\"collapse_settle_ms\": %.6f, \"collapse_ticks\": %d, "
                         "\"cascade_pieces\": %d, \"mem_delta_mb\": %.3f, "
                         "\"mem_footprint_mb\": %.3f}%s\n",
                         r.n, r.placed, r.supported, r.failing, r.tick_ms_median,
                         r.tick_ms_min, r.pieces_per_sec, r.cascade_detect_ms,
                         r.collapse_settle_ms, r.collapse_ticks, r.cascade_pieces,
                         r.mem_delta_mb, r.mem_footprint_mb,
                         i + 1 < perf.size() ? "," : "");
        }
        std::fprintf(f, "  ],\n  \"peak_rss_mb\": %.3f\n}\n", peakRss);
        std::fclose(f);
    };

    auto writeSummary = [&]() {
        FILE* f = std::fopen("results/summary.json", "w");
        if (!f) return;
        std::fprintf(f,
                     "{\n  \"correctness_pass\": %d,\n  \"correctness_total\": %zu,\n"
                     "  \"determinism_pass\": %d,\n  \"determinism_total\": %zu,\n"
                     "  \"determinism_pass_rate\": %.4f,\n"
                     "  \"max_pieces\": %d,\n  \"peak_rss_mb\": %.3f\n}\n",
                     corrPass, corr.size(), detPass, det.size(),
                     static_cast<double>(detPass) / static_cast<double>(det.size()),
                     perf.empty() ? 0 : perf.back().placed, peakRss);
        std::fclose(f);
    };

    writeCorrectness();
    writeDeterminism();
    writeBench();
    writeSummary();
    std::printf("\nwrote results/{correctness,determinism,bench,summary}.json\n");

    const bool ok = (corrPass == static_cast<int>(corr.size())) &&
                    (detPass == static_cast<int>(det.size()));
    return ok ? 0 : 1;
}
