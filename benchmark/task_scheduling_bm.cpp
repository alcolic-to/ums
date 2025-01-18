// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "async.h"
#include "benchmark_util.h"
#include "ums.h"

static void BM_task_scheduling(benchmark::State& state)
{
    init_ums([&] {
        for (auto _ : state)
            async([&] { hard_work(nanoseconds(state.range())); });
    });
}

BENCHMARK(BM_task_scheduling)
    ->Unit(benchmark::kMicrosecond)
    ->MeasureProcessCPUTime()
    ->RangeMultiplier(2)
    ->Range(0, 1 << 12);

BENCHMARK_MAIN();

// NOLINTEND
