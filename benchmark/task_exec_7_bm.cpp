// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "benchmark_util.h"
#include "ums.h"

static void BM_real_work_simulation(benchmark::State& state)
{
    init_ums([&] {
        std::vector<std::shared_ptr<Task>> tasks;

        for (auto _ : state) {
            auto dur = 20ms;
            for (int i = 0; i < schedulers->cpus_count(); ++i) {
                for (int i = 0; i < state.range(0); ++i) {
                    dur = std::max(1ms, dur - 1ms);
                    tasks.push_back(task_manager->execute_task([&] { hard_work(dur, true); }));
                }
            }

            for (auto task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_real_work_simulation)
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->RangeMultiplier(2)
    ->Range(1, 128 /* tasks per CPU */)
    ->Repetitions(10)
    ->DisplayAggregatesOnly();

BENCHMARK_MAIN();

// NOLINTEND
