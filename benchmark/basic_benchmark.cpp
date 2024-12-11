// NOLINTBEGIN

#include <atomic>
#include <benchmark/benchmark.h>
#include <chrono>
#include <memory>

#include "benchmark_util.h"
#include "ums.h"
#include "util.h"

static void DISABLED_BM_clear_caches(benchmark::State& state)
{
    for (auto _ : state)
        clear_caches(state.range(0));
}

BENCHMARK(DISABLED_BM_clear_caches)
    ->Unit(benchmark::kMicrosecond)
    ->Range(1, L1_size + L2_size + L3_size);

static void BM_task_scheduling(benchmark::State& state)
{
    init_ums([&] {
        for (auto _ : state)
            task_manager->execute_task([&] { hard_work(nanoseconds(state.range())); });
    });
}

BENCHMARK(BM_task_scheduling)
    ->Unit(benchmark::kMicrosecond)
    ->MeasureProcessCPUTime()
    ->RangeMultiplier(2)
    ->Range(0, 1 << 12);

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

static void BM_task_exec_all_cpus(benchmark::State& state)
{
    init_ums([&] {
        std::vector<std::shared_ptr<Task>> tasks;
        tasks.reserve(state.range(0));

        for (auto _ : state) {
            for (int i = 0; i < schedulers->cpus_count(); ++i)
                tasks.push_back(
                    task_manager->execute_task([&] { hard_work(microseconds(state.range())); }));

            for (auto task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_task_exec_all_cpus)
    ->Unit(benchmark::kMicrosecond)
    ->MeasureProcessCPUTime()
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

static void BM_task_exec_all_cpus_multiple_tasks(benchmark::State& state)
{
    init_ums([&] {
        std::vector<std::shared_ptr<Task>> tasks;
        tasks.reserve(state.range(1));

        for (auto _ : state) {
            for (int i = 0; i < schedulers->cpus_count(); ++i) {
                for (int i = 0; i < state.range(1); ++i) {
                    tasks.push_back(task_manager->execute_task(
                        [&] { hard_work(microseconds(state.range(0))); }));
                }
            }

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

static void BM_task_exec_long_tasks(benchmark::State& state)
{
    init_ums([&] {
        std::vector<std::shared_ptr<Task>> tasks;
        tasks.reserve(state.range(0));

        for (auto _ : state) {
            for (int i = 0; i < schedulers->cpus_count(); ++i)
                tasks.push_back(
                    task_manager->execute_task([&] { hard_work(milliseconds(state.range())); }));

            for (auto task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_task_exec_long_tasks)
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->DenseRange(1, 16, 1);

static void BM_task_exec_short_tasks(benchmark::State& state)
{
    init_ums([&] {
        std::vector<std::shared_ptr<Task>> tasks;
        tasks.reserve(state.range(0));

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

static void BM_real_work_simulation(benchmark::State& state)
{
    init_ums([&] {
        std::vector<std::shared_ptr<Task>> tasks;
        tasks.reserve(state.range(0));

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
    ->Unit(benchmark::kMicrosecond)
    ->MeasureProcessCPUTime()
    ->RangeMultiplier(2)
    ->Range(1, 128 /* tasks per CPU */)
    ->Repetitions(10)
    ->ReportAggregatesOnly();

BENCHMARK_MAIN();

// NOLINTEND
