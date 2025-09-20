// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "benchmark_util.h"
#include "ums.h"

using namespace ums;

static void DISABLED_BM_clear_caches(benchmark::State& state)
{
    for (auto _ : state)
        clear_caches(state.range(0));
}

BENCHMARK(DISABLED_BM_clear_caches)
    ->Unit(benchmark::kMicrosecond)
    ->Range(1, L1_size + L2_size + L3_size);

BENCHMARK_MAIN();

// NOLINTEND
