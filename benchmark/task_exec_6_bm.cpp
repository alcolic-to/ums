// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "async.h"
#include "benchmark_util.h"
#include "ums.h"

static void BM_task_exec_stress(benchmark::State& state)
{
    init_ums([&] {
        std::vector<std::shared_ptr<Task>> tasks;

        for (auto _ : state) {
            for (int i = 0; i < 1024 * 1024; ++i)
                tasks.push_back(async([&] { return; }));

            for (auto task : tasks)
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
