// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "async.h"
#include "benchmark_util.h"
#include "ums.h"

using namespace ums;

static void BM_task_exec_long_tasks(benchmark::State& state)
{
    init_ums([&] {
        std::vector<std::shared_ptr<Task>> tasks;

        for (auto _ : state) {
            for (int i = 0; i < schedulers->cpus_count(); ++i)
                tasks.push_back(async([&] { hard_work(milliseconds(state.range())); }));

            for (auto task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_task_exec_long_tasks)
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->DenseRange(1, 16, 1);

BENCHMARK_MAIN();

// NOLINTEND
