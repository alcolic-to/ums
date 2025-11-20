// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "async.h"
#include "benchmark_util.h"
#include "ums.h"

using namespace ums;

static void BM_e2e(benchmark::State& state)
{
    for (auto _ : state) {
        init_ums([&] {
            u32 cpus = schedulers->cpus_count();

            std::vector<Task<void>> tasks;
            tasks.reserve(cpus);

            for (u32 i = 0; i < cpus; ++i)
                tasks.emplace_back(async([&] { hard_work(microseconds(state.range())); }));

            for (auto& task : tasks)
                task->wait();
        });
    }
}

BENCHMARK(BM_e2e)
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_MAIN();

// NOLINTEND