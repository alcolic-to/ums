// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "async.hpp"
#include "bm_util.hpp"
#include "ums.hpp"

using namespace ums;

static void BM_task_exec_long_tasks(benchmark::State& state)
{
    init_ums([&] {
        std::vector<Task<void>> tasks;

        for (auto _ : state) {
            for (int i = 0; i < sch::cpus_count(); ++i)
                tasks.push_back(async([&] { hard_work(milliseconds(state.range())); }));

            for (auto& task : tasks)
                task.wait();
        }
    });
}

BENCHMARK(BM_task_exec_long_tasks)
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->DenseRange(1, 16, 1);

BENCHMARK_MAIN();

// NOLINTEND
