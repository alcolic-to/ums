#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>

#include "ums.h"
#include "util.h"
#include "worker.h"

using namespace std::chrono_literals;

// NOLINTBEGIN

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

        task_manager->execute_task<false>([&] { a = 1; });
        ASSERT_TRUE(a == 1);

        auto task{task_manager->execute_task<true>([&] { a = 0; })};
        task->wait();
        ASSERT_TRUE(a == 0);

        std::atomic<int> b{0};

        task_manager->execute_tasks<false>([&] { ++b; }, [&] { ++b; }, [&] { ++b; }, [&] { ++b; });
        ASSERT_TRUE(b == 4);

        const auto f = [&] { --b; };

        auto tasks{task_manager->execute_tasks(f, f, f, f)};

        for (auto task : tasks)
            task->wait();

        ASSERT_TRUE(b == 0);
    };

    init_ums(test);
}

TEST(Scheduler_tests, evenly_scheduled_tasks)
{
    auto test = [&] {
        for (uint32_t cpus = 1; cpus <= schedulers->cpus_count(); ++cpus) {
            Stopwatch<false> s;
            std::vector<std::shared_ptr<Task>> tasks;

            for (uint32_t c = 1; c <= cpus; ++c)
                tasks.push_back(task_manager->execute_task([] { hard_work(1s); }));

            for (auto task : tasks)
                task->wait();

            ASSERT_LE(s.elapsed(), 1s + 1ms);
        }
    };

    init_ums(test);
}

TEST(Scheduler_tests, sequential_task_execution)
{
    auto test = [&] {
        Stopwatch<false> s;

        task_manager->execute_task<false>([] { hard_work(1s); });
        task_manager->execute_task<false>([] { hard_work(1s); });
        task_manager->execute_task<false>([] { hard_work(1s); });
        task_manager->execute_task<false>([] { hard_work(1s); });

        ASSERT_LE(s.elapsed(), 4s + 1ms);
    };

    init_ums(test);
}

TEST(Scheduler_tests, parallel_execution)
{
    auto test = [&] {
        std::vector<std::shared_ptr<Task>> tasks;
        tasks.reserve(100);
        const auto f = [] { hard_work(20ms); };

        Stopwatch<false> s;

        for (uint32_t tasks_count = 0; tasks_count <= 100; ++tasks_count)
            tasks.emplace_back(task_manager->execute_task(f));

        for (auto task : tasks)
            task->wait();

        ASSERT_LE(s.elapsed(), ((100 * 20ms) / schedulers->cpus_count()) + 50ms);
    };

    init_ums(test);
}

TEST(Scheduler_tests, work_stealing)
{
    auto test = [&] {
        std::vector<std::shared_ptr<Task>> tasks;

        Stopwatch<false> s;

        for (uint32_t cpus = 1; cpus <= schedulers->cpus_count(); ++cpus) {
            auto f = [=] {
                uint32_t div = std::pow<uint32_t>(2, cpus - 1); // 1, 2, 4, 8, 16...
                hard_work(1000ms / div);
            };

            tasks.push_back(task_manager->execute_task(f));
        }

        for (uint32_t cpus = 1; cpus <= schedulers->cpus_count(); ++cpus) {
            auto f = [=] {
                uint32_t div = std::pow<uint32_t>(2, cpus - 1); // 1, 2, 4, 8, 16...
                hard_work(1000ms / div);
            };

            tasks.push_back(task_manager->execute_task(f));
        }

        for (auto task : tasks)
            task->wait();

        auto shortest = 1000ms / std::pow<uint32_t>(2, schedulers->cpus_count() - 1);
        ASSERT_LE(s.elapsed(), 1000ms + (2 * shortest) + 20ms);
    };

    init_ums(test);
}

// Test task order execution.
//
TEST(Scheduler_tests, task_order_execution)
{
    auto test = [&] {
        std::vector<std::shared_ptr<Task>> first_tasks;
        std::vector<std::shared_ptr<Task>> second_tasks;
        std::vector<std::shared_ptr<Task>> third_tasks;

        const auto cpus = schedulers->cpus_count();

        Stopwatch<false> s;

        for (uint32_t c = 1; c <= cpus; ++c)
            first_tasks.push_back(task_manager->execute_task([] {
                hard_work(100ms);
                tls_worker->yield();
                hard_work(100ms);
            }));

        for (uint32_t c = 1; c <= cpus; ++c)
            second_tasks.push_back(task_manager->execute_task([] { hard_work(1s); }));

        for (uint32_t c = 1; c <= cpus; ++c)
            third_tasks.push_back(task_manager->execute_task([] { hard_work(1s); }));

        for (auto task : first_tasks)
            task->wait();

        ASSERT_LE(s.elapsed(), 100ms + 1s + 100ms + 2 * 30ms);
    };

    init_ums(test);
}

// NOLINTEND
