// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "benchmark_util.h"
#include "ums.h"

static void BM_task_exec_short_tasks(benchmark::State& state)
{
    init_ums([&] {
        std::vector<std::shared_ptr<Task>> tasks;

        for (auto _ : state) {
            for (int i = 0; i < schedulers->cpus_count(); ++i)
                tasks.push_back(
                    task_manager->execute_task([&] { hard_work(microseconds(state.range())); }));

            for (auto task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_task_exec_short_tasks)
    ->Unit(benchmark::kMicrosecond)
    ->MeasureProcessCPUTime()
    ->RangeMultiplier(2)
    ->Range(1, 1024);

BENCHMARK_MAIN();

// NOLINTEND
