// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "async.hpp"
#include "bm_util.hpp"
#include "ums.hpp"

using namespace ums;

static void BM_task_exec_stress(benchmark::State& state)
{
    init_ums([&] {
        std::vector<Task<void>> tasks;

        for (auto _ : state) {
            for (int i = 0; i < 1024 * 1024; ++i)
                tasks.push_back(async([&] { return; }));

            for (auto& task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_task_exec_stress)
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->Repetitions(10)
    ->DisplayAggregatesOnly();

BENCHMARK_MAIN();

// NOLINTEND
