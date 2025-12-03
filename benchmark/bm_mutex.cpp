// NOLINTBEGIN

#include <atomic>
#include <benchmark/benchmark.h>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "async.hpp"
#include "bm_util.hpp"
#include "mutex.hpp"
#include "task.hpp"
#include "types.hpp"
#include "ums.hpp"

using namespace ums;

static void BM_spinlock_plain(benchmark::State& state)
{
    static Spinlock spinlock;
    [[maybe_unused]] static u64 counter = 0;

    for (auto _ : state) {
        const std::scoped_lock<Spinlock> lock{spinlock};
        ++counter;
    }
}

BENCHMARK(BM_spinlock_plain)
    ->UseRealTime()
    ->DenseThreadRange(1, std::thread::hardware_concurrency())
    ->Unit(benchmark::kNanosecond);

static void BM_std_mutex_plain(benchmark::State& state)
{
    static std::mutex mtx;
    [[maybe_unused]] static u64 counter = 0;

    for (auto _ : state) {
        const std::scoped_lock<std::mutex> lock{mtx};
        ++counter;
    }
}

BENCHMARK(BM_std_mutex_plain)
    ->UseRealTime()
    ->DenseThreadRange(1, std::thread::hardware_concurrency())
    ->Unit(benchmark::kNanosecond);

constexpr auto work_dur = 1us;

static void BM_spinlock_plain_with_work(benchmark::State& state)
{
    static Spinlock spinlock;
    [[maybe_unused]] static u64 counter = 0;

    for (auto _ : state) {
        const std::scoped_lock<Spinlock> lock{spinlock};
        hard_work(work_dur);
    }
}

BENCHMARK(BM_spinlock_plain_with_work)
    ->UseRealTime()
    ->DenseThreadRange(1, std::thread::hardware_concurrency())
    ->Unit(benchmark::kMicrosecond);

static void BM_std_mutex_plain_with_work(benchmark::State& state)
{
    static std::mutex mtx;
    [[maybe_unused]] static u64 counter = 0;

    for (auto _ : state) {
        const std::scoped_lock<std::mutex> lock{mtx};
        hard_work(work_dur);
    }
}

BENCHMARK(BM_std_mutex_plain_with_work)
    ->UseRealTime()
    ->DenseThreadRange(1, std::thread::hardware_concurrency())
    ->Unit(benchmark::kMicrosecond);

const int iter_count = 1024;

static void BM_spinlock(benchmark::State& state)
{
    init_ums([&] {
        Spinlock spinlock;
        int counter = 0;

        auto f = [&] {
            for (int i = 0; i < iter_count; ++i) {
                std::scoped_lock<Spinlock> lock{spinlock};
                ++counter;
            }
        };

        std::vector<Task<void>> tasks;
        tasks.reserve(state.range(0));

        for (auto _ : state) {
            for (int i = 0; i < state.range(0); ++i)
                tasks.emplace_back(async(f));

            for (auto& task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_spinlock)->UseRealTime()->Unit(benchmark::kMillisecond)->Range(1, 128);

static void BM_mutex(benchmark::State& state)
{
    init_ums([&] {
        Mutex mutex;
        int counter = 0;

        auto f = [&] {
            for (int i = 0; i < iter_count; ++i) {
                std::scoped_lock<Mutex> lock{mutex};
                ++counter;
            }
        };

        std::vector<Task<void>> tasks;
        tasks.reserve(state.range(0));

        for (auto _ : state) {
            for (int i = 0; i < state.range(0); ++i)
                tasks.emplace_back(async(f));

            for (auto& task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_mutex)->UseRealTime()->Unit(benchmark::kMillisecond)->Range(1, 128);

static void BM_std_mutex(benchmark::State& state)
{
    std::mutex mutex;
    int counter = 0;

    auto f = [&] {
        for (int i = 0; i < iter_count; ++i) {
            std::scoped_lock<std::mutex> lock{mutex};
            ++counter;
        }
    };

    std::vector<std::future<void>> tasks;
    tasks.reserve(state.range(0));

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i)
            tasks.emplace_back(std::async(f));

        for (auto& task : tasks)
            task.wait();
    }
}

BENCHMARK(BM_std_mutex)->UseRealTime()->Unit(benchmark::kMillisecond)->Range(1, 128);

const int work_iter_count = 64;

static void BM_spinlock_with_work(benchmark::State& state)
{
    init_ums([&] {
        Spinlock spinlock;

        auto f = [&] {
            for (int i = 0; i < work_iter_count; ++i) {
                std::scoped_lock<Spinlock> lock{spinlock};
                hard_work(work_dur);
            }
        };

        std::vector<Task<void>> tasks;
        tasks.reserve(state.range(0));

        for (auto _ : state) {
            for (int i = 0; i < state.range(0); ++i)
                tasks.emplace_back(async(f));

            for (auto& task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_spinlock_with_work)->UseRealTime()->Unit(benchmark::kMillisecond)->Range(1, 128);

static void BM_mutex_with_work(benchmark::State& state)
{
    init_ums([&] {
        Mutex mutex;

        auto f = [&] {
            for (int i = 0; i < work_iter_count; ++i) {
                std::scoped_lock<Mutex> lock{mutex};
                hard_work(work_dur);
            }
        };

        std::vector<Task<void>> tasks;
        tasks.reserve(state.range(0));

        for (auto _ : state) {
            for (int i = 0; i < state.range(0); ++i)
                tasks.emplace_back(async(f));

            for (auto& task : tasks)
                task->wait();
        }
    });
}

BENCHMARK(BM_mutex_with_work)->UseRealTime()->Unit(benchmark::kMillisecond)->Range(1, 128);

static void BM_std_mutex_with_work(benchmark::State& state)
{
    std::mutex mutex;

    auto f = [&] {
        for (int i = 0; i < work_iter_count; ++i) {
            std::scoped_lock<std::mutex> lock{mutex};
            hard_work(work_dur);
        }
    };

    std::vector<std::future<void>> tasks;
    tasks.reserve(state.range(0));

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i)
            tasks.emplace_back(std::async(f));

        for (auto& task : tasks)
            task.wait();
    }
}

BENCHMARK(BM_std_mutex_with_work)->UseRealTime()->Unit(benchmark::kMillisecond)->Range(1, 128);

BENCHMARK_MAIN();

// NOLINTEND
