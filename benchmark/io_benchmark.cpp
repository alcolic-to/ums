// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>

#include "benchmark_util.h"
#include "file.h"
#include "ums.h"
#include "util.h"

using namespace ums;
namespace fs = std::filesystem;

// Writes/reads total_bytes bytes at the offset by issuing sync/async I/O requests with io_size.
//
void sequential_ios(File_handle& file, std::vector<std::vector<char>>& io_v, bool write,
                    uint64_t offset = 0, bool async = false)
{
    std::vector<std::shared_ptr<Task>> tasks;

    uint64_t idx = 0;
    for (auto& io_data : io_v) {
        IO_Buffer io_buf{io_data.data(), io_data.size()};

        auto io_func = [&] {
            if (write)
                cos_write_file(file, io_buf, offset + idx * io_data.size());
            else
                cos_read_file(file, io_buf, offset + idx * io_data.size());
        };

        if (async)
            tasks.push_back(async(io_func));
        else
            io_func();

        ++idx;
    }

    if (async)
        for (auto& task : tasks)
            task->wait();
}

// Creates file and calls sequential_ios.
//
void sequential_ios(std::vector<std::vector<char>>& io_v, bool write, uint64_t offset = 0,
                    bool async = false)
{
    const fs::path file_path{"io_file"};

    {
        File_handle file{file_path};
        sequential_ios(file, io_v, write, offset, async);
    }

    fs::remove(file_path);
}

static void BM_sequential_ios(benchmark::State& state, uint64_t total_bytes, uint64_t io_size,
                              bool write, bool async = false)
{
    std::vector<std::vector<char>> io_v(total_bytes / io_size, std::vector(io_size, 'a'));
    const fs::path file_path{"io_file"};

    {
        File_handle file{file_path};

        // Fill file before reading.
        //
        if (!write)
            sequential_ios(file, io_v, true, 0, true);

        for (auto _ : state)
            sequential_ios(file, io_v, write, 0, async);
    }

    fs::remove(file_path);
}

static void BM_single_write(benchmark::State& state)
{
    init_ums([&] { BM_sequential_ios(state, state.range(0), state.range(0), true); });
}

BENCHMARK(BM_single_write)
    ->UseRealTime()
    ->RangeMultiplier(2)
    ->Range(1024, 1024 * 1024)
    ->Unit(benchmark::kMillisecond);

static void BM_single_read(benchmark::State& state)
{
    init_ums([&] { BM_sequential_ios(state, state.range(0), state.range(0), false); });
}

BENCHMARK(BM_single_read)
    ->UseRealTime()
    ->RangeMultiplier(2)
    ->Range(1024, 1024 * 1024)
    ->Unit(benchmark::kMillisecond);

static void BM_multiple_writes(benchmark::State& state)
{
    init_ums([&] {
        BM_sequential_ios(state, uint64_t(state.range(0)) * 1024 * 1024,
                          uint64_t(state.range(1)) * 1024, true);
    });
}

BENCHMARK(BM_multiple_writes)
    ->UseRealTime()
    ->Ranges({{1, 16}, {1, 1024}})
    ->Unit(benchmark::kMillisecond);

static void BM_multiple_reads(benchmark::State& state)
{
    init_ums([&] {
        BM_sequential_ios(state, uint64_t(state.range(0)) * 1024 * 1024,
                          uint64_t(state.range(1)) * 1024, false);
    });
}

BENCHMARK(BM_multiple_reads)
    ->UseRealTime()
    ->Ranges({{1, 16}, {1, 1024}})
    ->Unit(benchmark::kMillisecond);

static void BM_multiple_writes_async(benchmark::State& state)
{
    init_ums([&] {
        BM_sequential_ios(state, uint64_t(1024) * 1024 * 1024, uint64_t(state.range(0)) * 1024,
                          true, true);
    });
}

BENCHMARK(BM_multiple_writes_async)
    ->UseRealTime()
    ->RangeMultiplier(2)
    ->Range(1, 8)
    ->Unit(benchmark::kSecond);

static void BM_multiple_reads_async(benchmark::State& state)
{
    init_ums([&] {
        BM_sequential_ios(state, uint64_t(1024) * 1024 * 1024, uint64_t(state.range(0)) * 1024,
                          false, true);
    });
}

BENCHMARK(BM_multiple_reads_async)
    ->UseRealTime()
    ->RangeMultiplier(2)
    ->Range(1, 8)
    ->Unit(benchmark::kSecond);

static void BM_multiple_writes_large_file_async(benchmark::State& state)
{
    init_ums([&] {
        BM_sequential_ios(state, uint64_t(state.range(0)) * 1024 * 1024 * 1024,
                          uint64_t(state.range(1)) * 1024, true, true);
    });
}

BENCHMARK(BM_multiple_writes_large_file_async)
    ->UseRealTime()
    ->RangeMultiplier(2)
    ->Ranges({{1, 4}, {4, 64}})
    ->Unit(benchmark::kSecond);

static void BM_multiple_reads_large_file_async(benchmark::State& state)
{
    init_ums([&] {
        BM_sequential_ios(state, uint64_t(state.range(0)) * 1024 * 1024 * 1024,
                          uint64_t(state.range(1)) * 1024, false, true);
    });
}

BENCHMARK(BM_multiple_reads_large_file_async)
    ->UseRealTime()
    ->RangeMultiplier(2)
    ->Ranges({{1, 4}, {4, 64}})
    ->Unit(benchmark::kSecond);

BENCHMARK_MAIN();

// NOLINTEND
