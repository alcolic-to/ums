// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "benchmark_util.h"
#include "ums.h"

static void BM_task_exec_single_cpu(benchmark::State& state)
{
    init_ums([&] {
        for (auto _ : state)
            task_manager->execute_task<false>([&] { hard_work(microseconds(state.range())); });
    });
}

BENCHMARK(BM_task_exec_single_cpu)
    ->Unit(benchmark::kMicrosecond)
    ->MeasureProcessCPUTime()
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_MAIN();

// NOLINTEND
