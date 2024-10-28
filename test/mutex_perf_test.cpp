// NOLINTBEGIN

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "condition_variable.h"
#include "gtest/gtest.h"
#include "mutex.h"
#include "task_manager.h"
#include "util.h"

// #define RUN_PERF_TESTS
#ifdef RUN_PERF_TESTS

template<class Lockable>
void run_lock_perf_test(const std::string& test_name, int num_threads)
{
    static Lockable lockable;
    constexpr uint64_t iter_count = 100000;
    uint64_t counter = 0;

    auto lock_fn = [&] {
        for (uint64_t i = 0; i < iter_count; ++i) {
            std::scoped_lock<Lockable> lock{lockable};
            ++counter;
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    // Disabling next line since it causes clang-tidy dump with AV.
    // Stopwatch<std::chrono::microseconds> sw{
    //     std::format("{:8} with {:4} threads", test_name, num_threads)};

    std::stringstream ss;
    ss << std::setw(10) << std::left << test_name << " with " << std::setw(4) << std::right
       << num_threads << " thread(s)";

    Stopwatch<std::chrono::microseconds> sw{ss.str()};

    for (int i = 0; i < num_threads; ++i)
        threads.emplace_back(lock_fn);

    for (auto&& t : threads)
        t.join();

    ASSERT_TRUE(counter == num_threads * iter_count);
}

TEST(Mutex, mutex_vs_spinlock_perf_test_1)
{
    std::cout << "---------------------------------------------------------------\n";
    for (int threads_count = 1; threads_count <= 128; threads_count *= 2) {
        run_lock_perf_test<Spinlock>("Spinlock", threads_count);
        run_lock_perf_test<std::mutex>("std::mutex", threads_count);
        std::cout << "---------------------------------------------------------------\n";
    }
}

TEST(Mutex, mutex_peft_test_1)
{
    Mutex mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        for (int i = 0; i < iterations; ++i) {
            std::scoped_lock<Mutex> lock{mutex};
            ++counter;
        }
    };

    task_manager.execute_tasks<false>(f, f, f, f, f, f, f, f, f, f, f, f, f, f, f, f);
    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, spinlock_peft_test_1)
{
    Spinlock mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        for (int i = 0; i < iterations; ++i) {
            std::scoped_lock<Spinlock> lock{mutex};
            ++counter;
        }
    };

    task_manager.execute_tasks<false>(f, f, f, f, f, f, f, f, f, f, f, f, f, f, f, f);
    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, std_mutex_peft_test_1)
{
    GTEST_SKIP() << "Skipping std::mutex perf test, since it last long.";

    std::mutex mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        for (int i = 0; i < iterations; ++i) {
            std::scoped_lock<std::mutex> lock{mutex};
            ++counter;
        }
    };

    std::vector<std::thread> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i)
        v.emplace_back(std::thread{f});

    for (auto&& thread : v)
        thread.join();

    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, mutex_peft_test_2)
{
    Mutex mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        std::scoped_lock<Mutex> lock{mutex};
        for (int i = 0; i < iterations; ++i)
            ++counter;
    };

    task_manager.execute_tasks<false>(f, f, f, f, f, f, f, f, f, f, f, f, f, f, f, f);
    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, spinlock_peft_test_2)
{
    Spinlock mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        std::scoped_lock<Spinlock> lock{mutex};
        for (int i = 0; i < iterations; ++i)
            ++counter;
    };

    task_manager.execute_tasks<false>(f, f, f, f, f, f, f, f, f, f, f, f, f, f, f, f);
    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, std_mutex_peft_test_2)
{
    std::mutex mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        std::scoped_lock<std::mutex> lock{mutex};
        for (int i = 0; i < iterations; ++i)
            ++counter;
    };

    std::vector<std::thread> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i)
        v.emplace_back(std::thread{f});

    for (auto&& thread : v)
        thread.join();

    ASSERT_TRUE(counter == 16 * iterations);
}

#endif

// NOLINTEND
