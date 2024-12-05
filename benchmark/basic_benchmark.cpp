// NOLINTBEGIN

#include <benchmark/benchmark.h>

#include "ums.h"

// Simulates work with specified duration.
//
void hard_work(std::chrono::steady_clock::duration dur)
{
    Stopwatch<false> s;
    while (s.elapsed() < dur)
        ;
}

static void BM_Basic_task_execution(benchmark::State& state)
{
    auto test = [] { hard_work(1ns); };

    for (auto _ : state)
        init_ums(test);
}

BENCHMARK(BM_Basic_task_execution);

BENCHMARK_MAIN();

// NOLINTEND
