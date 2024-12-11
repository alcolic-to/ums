// NOLINTBEGIN

#include <atomic>
#include <benchmark/benchmark.h>
#include <chrono>
#include <deque>
#include <memory>

static void BM_vector_push_back(benchmark::State& state)
{
    std::vector<int> v;

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i)
            v.push_back(42);
        benchmark::ClobberMemory(); // Force 42 to be written to memory.
    }
}

BENCHMARK(BM_vector_push_back)->Range(1, 1024 * 1024);

static void BM_deque_push_back(benchmark::State& state)
{
    std::deque<int> q;

    for (auto _ : state) {
        for (int i = 0; i < state.range(0); ++i)
            q.push_back(42);
        benchmark::ClobberMemory(); // Force 42 to be written to memory.
    }
}

BENCHMARK(BM_deque_push_back)->Range(1, 1024 * 1024);

static void BM_vector_push_front(benchmark::State& state)
{
    std::vector<uint64_t> v;

    for (auto _ : state) {
        v.insert(v.begin(), 42);
        benchmark::ClobberMemory(); // Force 42 to be written to memory.

        if (v.size() >= state.range(0)) {
            state.PauseTiming();
            v.clear();
            state.ResumeTiming();
        }
    }
}

BENCHMARK(BM_vector_push_front)->RangeMultiplier(2)->Range(1, 1024 * 1024);

static void BM_deque_push_front(benchmark::State& state)
{
    std::deque<uint64_t> q;

    for (auto _ : state) {
        q.push_front(42);
        benchmark::ClobberMemory(); // Force 42 to be written to memory.

        if (q.size() >= state.range(0)) {
            state.PauseTiming();
            q.clear();
            state.ResumeTiming();
        }
    }
}

BENCHMARK(BM_deque_push_front)->RangeMultiplier(2)->Range(1, 1024 * 1024);

static void BM_vector_pop_back(benchmark::State& state)
{
    std::vector<uint64_t> v;
    benchmark::DoNotOptimize(v);
    for (int i = 0; i < state.range(0) - 1; ++i)
        v.push_back(42);

    for (auto _ : state) {
        state.PauseTiming();
        v.push_back(42);
        benchmark::ClobberMemory();
        state.ResumeTiming();

        v.pop_back();
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_vector_pop_back)->RangeMultiplier(2)->Range(1, 1024);

static void BM_deque_pop_back(benchmark::State& state)
{
    std::deque<uint64_t> q;
    benchmark::DoNotOptimize(q);
    for (int i = 0; i < state.range(0) - 1; ++i)
        q.push_back(42);

    for (auto _ : state) {
        state.PauseTiming();
        q.push_back(42);
        benchmark::ClobberMemory();
        state.ResumeTiming();

        q.pop_back();
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_deque_pop_back)->RangeMultiplier(2)->Range(1, 1024);

static void BM_vector_pop_front(benchmark::State& state)
{
    std::vector<uint64_t> v;
    benchmark::DoNotOptimize(v);
    for (int i = 0; i < state.range(0) - 1; ++i)
        v.push_back(42);

    for (auto _ : state) {
        state.PauseTiming();
        v.push_back(42);
        benchmark::ClobberMemory();
        state.ResumeTiming();

        v.erase(v.begin());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_vector_pop_front)->RangeMultiplier(2)->Range(1, 1024);

static void BM_deque_pop_front(benchmark::State& state)
{
    std::deque<uint64_t> q;
    benchmark::DoNotOptimize(q);
    for (int i = 0; i < state.range(0) - 1; ++i)
        q.push_back(42);

    for (auto _ : state) {
        state.PauseTiming();
        q.push_back(42);
        benchmark::ClobberMemory();
        state.ResumeTiming();

        q.pop_front();
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_deque_pop_front)->RangeMultiplier(2)->Range(1, 1024);

BENCHMARK_MAIN();

// NOLINTEND
