// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "async.hpp"
#include "bm_util.hpp"
#include "ums.hpp"

using namespace ums;

static void BM_real_work_simulation(benchmark::State& state)
{
    init_ums([&] {
        std::vector<Task<void>> tasks;

        for (auto _ : state) {
            auto dur = 20ms;
            for (int i = 0; i < sch::cpus_count(); ++i) {
                for (int i = 0; i < state.range(0); ++i) {
                    dur = std::max(1ms, dur - 1ms);
                    tasks.push_back(async([&] { hard_work(dur, true); }));
                }
            }

            for (auto& task : tasks)
                task.wait();
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
