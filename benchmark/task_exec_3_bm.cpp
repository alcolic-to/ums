// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "async.h"
#include "benchmark_util.h"
#include "ums.h"

using namespace ums;

static void BM_task_exec_all_cpus_multiple_tasks(benchmark::State& state)
{
    init_ums([&] {
        std::vector<Task<void>> tasks;
        tasks.reserve(state.range(1));

        for (auto _ : state) {
            for (int i = 0; i < schedulers->cpus_count(); ++i)
                for (int i = 0; i < state.range(1); ++i)
                    tasks.push_back(async([&] { hard_work(microseconds(state.range(0))); }));

            for (auto task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_task_exec_all_cpus_multiple_tasks)
    ->Unit(benchmark::kMicrosecond)
    ->MeasureProcessCPUTime()
    ->RangeMultiplier(8)
    ->Ranges({/* task duration in us */ {1, 1 << 14}, /* tasks count */ {1, 128}});

BENCHMARK_MAIN();

// NOLINTEND
