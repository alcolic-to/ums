// NOLINTBEGIN

#include <chrono>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>

#include "async.hpp"
#include "options.hpp"
#include "types.hpp"
#include "ums.hpp"
#include "util.hpp"
#include "worker.hpp"

using namespace ums;
using namespace std::chrono_literals;

// Simulates work with specified duration.
//
void hard_work(std::chrono::steady_clock::duration dur)
{
    Stopwatch<false> s;
    while (s.elapsed() < dur)
        ;
}

TEST(Scheduler_tests, sanity_test)
{
    auto test = [&] {
        int a = 0;

        async<true>([&] { a = 1; });
        ASSERT_TRUE(a == 1);

        auto task{async([&] { a = 0; })};
        task->wait();
        ASSERT_TRUE(a == 0);

        std::atomic<int> b{0};

        asyncs<true>([&] { ++b; }, [&] { ++b; }, [&] { ++b; }, [&] { ++b; });
        ASSERT_TRUE(b == 4);

        const auto f = [&] { --b; };

        auto tasks{asyncs(f, f, f, f)};

        for (auto& task : tasks)
            task->wait();

        ASSERT_TRUE(b == 0);

        Task<int> t1 = async([] { return 5; });

        ASSERT_TRUE(t1->get() == 5);
        ASSERT_ANY_THROW(t1->get());

        Task<int> t2 = async([] { return 5; });
        t2->wait();

        ASSERT_NO_THROW(t2->wait());
        ASSERT_TRUE(t2->get() == 5);
        ASSERT_ANY_THROW(t2->get());
    };

    init_ums(test);
}

TEST(Scheduler_tests, task_type_test)
{
    auto test = [&] {
        Task<std::string> empty;
        ASSERT_THROW(empty->get(), std::logic_error);
        ASSERT_THROW(empty->wait(), std::logic_error);
        ASSERT_FALSE(empty.valid());

        empty = async([] { return std::string{"some string"}; });
        ASSERT_EQ(empty->get(), "some string");
    };

    init_ums(test);
}

TEST(Scheduler_tests, evenly_scheduled_tasks)
{
    auto test = [&] {
        for (u32 cpus = 1; cpus <= sch::cpus_count(); ++cpus) {
            for (auto task_dur = 1ms; task_dur <= 64ms; task_dur *= 2) {
                std::vector<Task<void>> tasks;
                tasks.reserve(cpus);

                Stopwatch<false> s;

                for (u32 c = 1; c <= cpus; ++c)
                    tasks.emplace_back(async([=] { hard_work(task_dur); }));

                for (auto& task : tasks)
                    task->wait();

                ASSERT_LE(s.elapsed(), task_dur + 2ms);
            }
        }
    };

    init_ums(test);
}

TEST(Scheduler_tests, sequential_task_execution)
{
    auto test = [&] {
        Stopwatch<false> s;

        async<true>([] { hard_work(10ms); });
        async<true>([] { hard_work(10ms); });
        async<true>([] { hard_work(10ms); });
        async<true>([] { hard_work(10ms); });

        ASSERT_LE(s.elapsed(), 40ms + 2ms);
    };

    init_ums(test);
}

TEST(Scheduler_tests, parallel_execution)
{
    auto test = [&] {
        std::vector<Task<void>> tasks;
        tasks.reserve(100);

        Stopwatch<false> s;

        for (u32 tasks_count = 0; tasks_count < 100; ++tasks_count)
            tasks.emplace_back(async([] { hard_work(1ms); }));

        for (auto& task : tasks)
            task->wait();

        ASSERT_LE(s.elapsed(), ((100 * 1ms) / sch::cpus_count()) + 10ms);
    };

    init_ums(test);
}

TEST(Scheduler_tests, work_stealing)
{
    auto test = [&] {
        std::vector<Task<void>> tasks;

        Stopwatch<false> s;

        for (u32 cpus = 1; cpus <= sch::cpus_count(); ++cpus) {
            auto f = [=] {
                u32 div = std::pow<u32>(2, cpus - 1); // 1, 2, 4, 8, 16...
                hard_work(1000ms / div);
            };

            tasks.push_back(async(f));
        }

        for (u32 cpus = 1; cpus <= sch::cpus_count(); ++cpus) {
            auto f = [=] {
                u32 div = std::pow<u32>(2, cpus - 1); // 1, 2, 4, 8, 16...
                hard_work(1000ms / div);
            };

            tasks.push_back(async(f));
        }

        for (auto& task : tasks)
            task->wait();

        auto shortest = 1000ms / std::pow<u32>(2, sch::cpus_count() - 1);
        ASSERT_LE(s.elapsed(), 1000ms + (2 * shortest) + 2ms);
    };

    init_ums(test);
}

TEST(Scheduler_tests, task_order_execution)
{
    auto test = [&] {
        std::vector<Task<void>> first_tasks;
        std::vector<Task<void>> second_tasks;

        const auto cpus = sch::cpus_count();

        Stopwatch<false> s;

        for (u32 c = 1; c <= cpus; ++c) {
            first_tasks.push_back(async([] {
                hard_work(100ms);
                worker::yield();
                hard_work(100ms);
            }));
        }

        for (u32 c = 1; c <= cpus; ++c)
            second_tasks.push_back(async([] { hard_work(1s); }));

        for (auto& task : first_tasks)
            task->wait();

        ASSERT_GE(s.elapsed(), 100ms + 1s + 100ms);
        ASSERT_LE(s.elapsed(), 100ms + 1s + 100ms + 2ms);
    };

    init_ums(test);
}

TEST(Scheduler_tests, task_order_execution_extended)
{
    auto test = [&] {
        std::vector<Task<void>> first_tasks;
        std::vector<Task<void>> second_tasks;
        std::vector<Task<void>> third_tasks;

        const auto cpus = sch::cpus_count();

        Stopwatch<false> s;

        for (u32 c = 1; c <= cpus; ++c) {
            first_tasks.push_back(async([] {
                hard_work(1s);
                worker::yield();
                hard_work(1s);
            }));
        }

        for (u32 c = 1; c <= cpus; ++c) {
            second_tasks.push_back(async([] {
                hard_work(1s);
                worker::yield();
                hard_work(1s);
            }));
        }

        for (u32 c = 1; c <= cpus; ++c)
            third_tasks.push_back(async([] { hard_work(100ms); }));

        for (auto& task : third_tasks)
            task->wait();

        /**
         * Bug or feature? When we have 2 long running tasks (even if they yield), they will
         * monopolize scheduler, because we will schedule them without scheduling new tasks until
         * they are done.
         *
         * ASSERT_GE(s.elapsed(), 2s + 100ms);
         * ASSERT_LE(s.elapsed(), 2s + 100ms + 20ms);
         */
        ASSERT_GE(s.elapsed(), 4s + 100ms);
        ASSERT_LE(s.elapsed(), 4s + 100ms + 2ms);
    };

    init_ums(test);
}

// NOLINTEND
